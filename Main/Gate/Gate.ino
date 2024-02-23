#include <Arduino.h>
#include <LiquidCrystal_I2C.h>
#include <SPI.h>
#include <MFRC522.h>

#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClient.h>
#include <ESP8266WiFiMulti.h>

#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>

#include "Barrier.h" // Handle the servo
#include "index.h"

// Set the LCD number of columns and rows
#define LCD_COLS        16
#define LCD_ROWS        2

// Pins connections
#define LCD_SDA         SDA     // D2
#define LCD_SCL         SCL     // D1

#define RFID_RST_PIN    D4
#define RFID_SDA_PIN    SS      // D8
#define RFID_SCK_PIN    SCK     // D5
#define RFID_MOSI_PIN   MOSI    // D7
#define RFID_MISO_PIN   MISO    // D6

#define SERVO_PIN       D3

// Wifi credential
const char* SSID = "ESP8266_NodeMCU";
const char* PSWD = "12345678";

// Create LCD instance
LiquidCrystal_I2C lcd(0x27, LCD_COLS, LCD_ROWS);

// Create RFID instance
MFRC522 rfid(RFID_SDA_PIN, RFID_RST_PIN);

// Create Servo instance
Barrier barrier(SERVO_PIN);

//Create WiFiMulti instance
ESP8266WiFiMulti WiFiMulti;

// Create AsyncWebServer object on port 80
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

const char* sonar1 = "http://192.168.4.1/sonar1";
const char* sonar2 = "http://192.168.4.1/sonar2";
const char* sonar3 = "http://192.168.4.1/sonar3";
const char* status = "http://192.168.4.1/status";

String distance1;
String distance2;
String distance3;
String status_s;

bool STATE = 1;

void setup() {
    // Initialize Serial Communication
    Serial.begin(115200);
    Serial.println("");

    Serial.print("Connecting to ");
    Serial.println(SSID);
    WiFi.begin(SSID, PSWD);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("");
    Serial.println("Connected");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());

    WiFi.printDiag(Serial);

    // Init Web Socket
    ws.onEvent(onEvent);
    server.addHandler(&ws);

    // Route for root / web page
    server.on("/", HTTP_GET, [] (AsyncWebServerRequest* request) {
        request->send_P(200, "text/html", index_html, processor);
    });

    // Test the servo (Sweep from 0 to 180 degree)
    barrier.Test();

    // Initialize LCD
    lcd.init();
    lcd.backlight();
    LCD_Greeting();

    // Initialize SPI bus
    SPI.begin();

    // Initialize MFRC522 RFID
    rfid.PCD_Init();

    // Start server
    server.begin();

    Serial.println();
    Serial.println("Server Started");
}

void loop() {
    ws.cleanupClients();
    if (STATE) {
        barrier.Open();
    }
    else {
        barrier.Close();
    }

    // Look for new cards
    if (!rfid.PICC_IsNewCardPresent()) {
        return;
    }
    // Select one of the cards
    if (!rfid.PICC_ReadCardSerial()) {
        return;
    }

    String UID = RFID_GetUID();
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print(UID);


    // Check WiFi connection status
    if ((WiFiMulti.run() == WL_CONNECTED)) {
        // distance1 = httpGETRequest(sonar1);
        // distance2 = httpGETRequest(sonar2);
        // distance3 = httpGETRequest(sonar3);
        status_s = httpGETRequest(status);
        Serial.println("Empty: " + status_s);

        lcd.setCursor(0, 1);
        lcd.print("Empty: " + status_s);
        // lcd.print(distance1);
        // lcd.print(" ");
        // lcd.print(distance2);
        // lcd.print(" ");
        // lcd.print(distance3);
        // lcd.print(" ");
    }
    else {
        Serial.println("WiFi Disconnected");
    }
}

String RFID_GetUID() {
    String content = "";
    for (byte i = 0; i < rfid.uid.size; i++) {
        content.concat(String(rfid.uid.uidByte[i] < 0x10 ? " 0" : " "));
        content.concat(String(rfid.uid.uidByte[i], HEX));
    }
    content.toUpperCase();
    return content.substring(1);
}

String httpGETRequest(const char* serverName) {
    WiFiClient client;
    HTTPClient http;

    // Your IP address with path or Domain name with URL path 
    http.begin(client, serverName);

    // Send HTTP POST request
    int httpResponseCode = http.GET();

    String payload = "--";

    if (httpResponseCode > 0) {
        Serial.print("HTTP Response code: ");
        Serial.println(httpResponseCode);
        payload = http.getString();
    }
    else {
        Serial.print("Error code: ");
        Serial.println(httpResponseCode);
    }
    // Free resources
    http.end();

    return payload;
}


void LCD_Greeting() {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("PARKING SYSTEM");
    lcd.setCursor(0, 1);
    lcd.print("HUST PRODUCTION");
    delay(1000);
}

void handleWebSocketMessage(void* arg, uint8_t* data, size_t len) {
    AwsFrameInfo* info = (AwsFrameInfo*) arg;
    if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
        data[len] = 0;
        if (strcmp((char*) data, "toggle") == 0) {
            STATE = !STATE;
            ws.textAll(String(STATE));
        }
    }
}


void onEvent(AsyncWebSocket* server, AsyncWebSocketClient* client, AwsEventType type, void* arg, uint8_t* data, size_t len) {
    switch (type) {
        case WS_EVT_CONNECT:
            Serial.printf("WebSocket client #%u connected from %s\n", client->id(), client->remoteIP().toString().c_str());
            break;
        case WS_EVT_DISCONNECT:
            Serial.printf("WebSocket client #%u disconnected\n", client->id());
            break;
        case WS_EVT_DATA:
            handleWebSocketMessage(arg, data, len);
            break;
        case WS_EVT_PONG: // Response to a ping request
            break;
        case WS_EVT_ERROR: // An error is received from the client
            break;
    }
}


String processor(const String& var) {
    Serial.println(var);
    if (var == "STATE") {
        if (STATE) {
            return "OPEN";
        }
        else {
            return "CLOSE";
        }
    }
    
    return String();
}