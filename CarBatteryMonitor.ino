#include <WiFi.h>
#include <HTTPClient.h>

//====================================================
// WIFI CONFIGURATION
//====================================================
const char* ssid     = "MyWiFi";
const char* password = "1234567890";

//====================================================
// THINGSPEAK CONFIGURATION
//====================================================
const char* apiKey = "";       // Channel write API key
const char* server = "http://api.thingspeak.com/update";

//====================================================
// HARDWARE
//====================================================
const int analogPin = 34;

// Voltage divider:
// R1 = 330k (high side)
// R2 = 47k  (low side)
//
// Ratio = (330k + 47k) / 47k = 8
//
const float DIVIDER_RATIO = (330.0 + 47.0) / 47.0;

//====================================================
// TIMING
//====================================================
const uint32_t SAMPLE_INTERVAL_MS = 10;
const uint32_t THINGSPEAK_INTERVAL_MS = 15000;

//====================================================
// SAMPLE ACCUMULATION
//====================================================
uint64_t adcAccumulator = 0;
uint32_t adcSamples = 0;

unsigned long lastSampleTime = 0;
unsigned long lastUploadTime = 0;

//====================================================
// WIFI
//====================================================
void connectWiFi()
{
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);

    Serial.print("Connecting to WiFi");

    uint32_t startTime = millis();

    while (WiFi.status() != WL_CONNECTED)
    {
        delay(500);
        Serial.print(".");

        // timeout after 10 seconds
        if (millis() - startTime > 10000)
        {
            Serial.println();
            Serial.println("Connection failed, restarting ESP32...");
            delay(1000);
            ESP.restart();
        }
    }

    Serial.println();
    Serial.println("WiFi connected");
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());
}

//====================================================
// THINGSPEAK UPLOAD
//====================================================
bool sendToThingSpeak(float voltage)
{
    if (WiFi.status() != WL_CONNECTED)
    {
        Serial.println("WiFi disconnected, reconnecting...");
        connectWiFi();
    }

    // Turn LED on during upload
    digitalWrite(LED_BUILTIN, HIGH);

    HTTPClient http;

    String url = String(server) +
                 "?api_key=" + apiKey +
                 "&field1=" + String(voltage, 3);

    http.begin(url);

    int httpCode = http.GET();

    // Turn LED off as soon as the request finishes
    digitalWrite(LED_BUILTIN, LOW);

    if (httpCode > 0)
    {
        String response = http.getString();

        Serial.print("ThingSpeak response: ");
        Serial.println(response);

        http.end();

        // Success
        digitalWrite(LED_BUILTIN, HIGH);
        delay(100);
        digitalWrite(LED_BUILTIN, LOW);
        return true;
    }
    else
    {
        Serial.print("HTTP Error: ");
        Serial.println(httpCode);

        http.end();
 
        // Error
        for(int i=0; i<3; i++)
        {
            digitalWrite(LED_BUILTIN, HIGH);
            delay(100);
            digitalWrite(LED_BUILTIN, LOW);
            delay(100);
        }
        return false;
    }
}

//====================================================
// SETUP
//====================================================
void setup()
{
    Serial.begin(115200);

    delay(1000);

    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, LOW);

    analogReadResolution(12);

    // Maximum ADC range
#if ESP_ARDUINO_VERSION_MAJOR >= 3
    analogSetPinAttenuation(analogPin, ADC_ATTENDB_MAX);
#else
    analogSetPinAttenuation(analogPin, ADC_11db);
#endif

    connectWiFi();

    lastSampleTime = millis();
    lastUploadTime = millis();

    Serial.println();
    Serial.println("System started");
    Serial.println("Measurement range: 0-24V");
}

//====================================================
// LOOP
//====================================================
void loop()
{
    unsigned long now = millis();

    //=========================================
    // CONTINUOUS SAMPLING
    //=========================================
    if (now - lastSampleTime >= SAMPLE_INTERVAL_MS)
    {
        lastSampleTime = now;

        uint32_t adc_mV = analogReadMilliVolts(analogPin);
        adc_mV = adc_mV < 150 ? 0 : adc_mV;

        adcAccumulator += adc_mV;
        adcSamples++;
    }

    //=========================================
    // UPLOAD EVERY 15 SECONDS
    //=========================================
    if (now - lastUploadTime >= THINGSPEAK_INTERVAL_MS)
    {
        lastUploadTime = now;

        if (adcSamples > 0)
        {
            float adcVoltage =
                ((float)adcAccumulator / adcSamples) / 1000.0f;

            float inputVoltage =
                adcVoltage * DIVIDER_RATIO;

            Serial.println("--------------------------------");
            Serial.print("Samples: ");
            Serial.println(adcSamples);

            Serial.print("ADC Voltage: ");
            Serial.print(adcVoltage, 4);
            Serial.println(" V");

            Serial.print("Input Voltage: ");
            Serial.print(inputVoltage, 3);
            Serial.println(" V");

            sendToThingSpeak(inputVoltage);

            // reset accumulation
            adcAccumulator = 0;
            adcSamples = 0;
        }
    }
}
