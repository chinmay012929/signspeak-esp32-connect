#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <HardwareSerial.h>
#include <TinyGPS++.h>

#define MOTOR_PIN 27

HardwareSerial gpsSerial(1);
TinyGPSPlus gps;

BLECharacteristic *pCharacteristic;
bool deviceConnected = false;

#define SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"

// -------- BLE Callbacks --------
class MyServerCallbacks: public BLEServerCallbacks {
  void onConnect(BLEServer* pServer) {
    deviceConnected = true;
    Serial.println("BLE Connected");
  }

  void onDisconnect(BLEServer* pServer) {
    deviceConnected = false;
    Serial.println("BLE Disconnected");
    BLEDevice::startAdvertising();
  }
};

class MyCallbacks: public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *pCharacteristic) {
    // Trim string to avoid hidden newlines or spaces ruining the match
    String rxValue = pCharacteristic->getValue().c_str();
    rxValue.trim(); 

    if (rxValue.length() > 0) {
      if (rxValue == "VIBRATE_1") {
        Serial.println("Received Voice Command: VIBRATE_1 (HELLO)");
        digitalWrite(MOTOR_PIN, HIGH);
        delay(1000); // Vibrate for 1 second
        digitalWrite(MOTOR_PIN, LOW);
      } 
      else if (rxValue == "VIBRATE_2") {
        Serial.println("Android requested: 2 TIME VIBRATION");
        // --- Vibrate 1! ---
        digitalWrite(MOTOR_PIN, HIGH); // Turn motor ON
        delay(300);                    // Wait 0.3 seconds
        digitalWrite(MOTOR_PIN, LOW);  // Turn motor OFF
        
        // --- Pause! ---
        delay(300);                    // Pause for 0.3 seconds so you can feel the gap
        // --- Vibrate 2! ---
        digitalWrite(MOTOR_PIN, HIGH); // Turn motor ON again
        delay(300);                    // Wait 0.3 seconds
        digitalWrite(MOTOR_PIN, LOW);  // Turn motor OFF
      } 
      else {
        Serial.print("Received Unknown Command: '");
        Serial.print(rxValue);
        Serial.println("'");
      }
    }
  }
};

// -------- Setup --------
void setup() {
  Serial.begin(115200);
  pinMode(MOTOR_PIN, OUTPUT);

  // GPS init
  gpsSerial.begin(9600, SERIAL_8N1, 16, 17);

  // BLE init
  BLEDevice::init("SignSpeak_Band");

  BLEServer *pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  BLEService *pService = pServer->createService(SERVICE_UUID);

  // Set up characteristic to be written to by Android app
  pCharacteristic = pService->createCharacteristic(
                      CHARACTERISTIC_UUID,
                      BLECharacteristic::PROPERTY_WRITE |
                      BLECharacteristic::PROPERTY_NOTIFY
                    );

  pCharacteristic->addDescriptor(new BLE2902());
  pCharacteristic->setCallbacks(new MyCallbacks());

  pService->start();
  
  // FIX: Explicitly advertise the new UUID so Android connects flawlessly
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(true);
  BLEDevice::startAdvertising();

  Serial.println("System Ready. Listening for VIBRATE commands and GPS data...");
}

// -------- Loop --------
void loop() {
#if 1 // Set to 1 for MOCK TEST, set to 0 to use real GPS
  // --- TEMPORARY MOCK TEST ---
  // Instead of waiting for real satellites, just force a fake location every 3 seconds!
  char gpsString[32];
  
  // We will hardcode a mock location to test the Android map animation
  sprintf(gpsString, "GPS:20.2961,85.8245"); 
  
  // 5. Fire it to the Android App over Bluetooth!
  if (deviceConnected && pCharacteristic != nullptr) {
    pCharacteristic->setValue(gpsString);
    pCharacteristic->notify(); // Push to Android
  }
  
  Serial.println("Sent fake GPS to Android over Bluetooth!");
  delay(3000); // Wait 3 seconds
  // ---------------------------
#else
  // 📍 Feed GPS data into TinyGPS++
  while (gpsSerial.available() > 0) {
    char c = gpsSerial.read();
    
    // Uncomment this if you still want to see raw NMEA GPS text
    Serial.write(c); 

    if (gps.encode(c)) {
      // 1. Obtain newly secured lock from GPS Module (TinyGPS++)
      if (gps.location.isValid() && gps.location.isUpdated()) {
        float currentLat = gps.location.lat(); // Live GPS Lat
        float currentLon = gps.location.lng(); // Live GPS Lon
        
        // 2. We need a character array to hold the formatted string
        char gpsString[32];
        
        // 3. Format the numbers into the specific "GPS:lat,lon" string
        sprintf(gpsString, "GPS:%.4f,%.4f", currentLat, currentLon);
        
        // 4. Print it to Serial Monitor so you can verify it locally
        Serial.print("Sending to Android: ");
        Serial.println(gpsString);
        
        // 5. Fire it to the Android App over Bluetooth!
        if (deviceConnected && pCharacteristic != nullptr) {
          pCharacteristic->setValue(gpsString);
          pCharacteristic->notify();
        }
      }
    }
  }
#endif
}