/*
 * main.ino
 *
 *  Created on: 01.01.2021
 *
 */

#include <Arduino.h>

#include <WiFi.h>
#include <WiFiMulti.h>
#include <WiFiClientSecure.h>

#include <HTTPClient.h>
#include <Arduino_JSON.h>
// #include <WebSocketsClient.h>

#include "user_defines.h"
#include "bme280.h"


WiFiMulti WiFiMulti;
// WebSocketsClient webSocket;

// enable production use, set false for debugging
#define PRODUCTION
//#define TARGET_DEBUG
//#define DEVELOPMENT

const char* THIS_SENSOR_NAME = "esp32-bme280-1";

#ifdef PRODUCTION
// old server: 192.168.0.229:80
const char* post_measurement_server = "http://reipi/post-measurement";
const char* get_status_update_server = "http://reipi/get-status-update";
#endif
#ifdef DEBUG: 
const char* post_measurement_server = "http://reipi:5000/post-measurement";
const char* get_status_update_server = "http://reipi:5000/get-status-update";
#endif
#ifdef DEVELOPMENT
const char* post_measurement_server = "http://192.168.0.100:5000/post-measurement";
const char* get_status_update_server = "http://192.168.0.100:5000/get-status-update";
#endif

String status_update_response;
float status_update_arr[3];

unsigned long last_time = 0;
unsigned long timer_delay = 10000;

#define USE_SERIAL Serial

void hexdump(const void *mem, uint32_t len, uint8_t cols = 16) {
  const uint8_t* src = (const uint8_t*) mem;
  USE_SERIAL.printf("\n[HEXDUMP] Address: 0x%08X len: 0x%X (%d)", (ptrdiff_t)src, len, len);
  for(uint32_t i = 0; i < len; i++) {
    if(i % cols == 0) {
      USE_SERIAL.printf("\n[0x%08X] 0x%08X: ", (ptrdiff_t)src, i);
    }
    USE_SERIAL.printf("%02X ", *src);
    src++;
  }
  USE_SERIAL.printf("\n");
}

/*
void webSocketEvent(WStype_t type, uint8_t * payload, size_t length) {

  switch(type) {
    case WStype_DISCONNECTED:
      USE_SERIAL.printf("[WSc] Disconnected!\n");
      break;
    case WStype_CONNECTED:
      USE_SERIAL.printf("[WSc] Connected to url: %s\n", payload);

      // send message to server when Connected
      webSocket.sendTXT("Connected");
      break;
    case WStype_TEXT:
      USE_SERIAL.printf("[WSc] get text: %s\n", payload);

      // send message to server
      // webSocket.sendTXT("message here");
      break;
    case WStype_BIN:
      USE_SERIAL.printf("[WSc] get binary length: %u\n", length);
      hexdump(payload, length);

      // send data to server
      // webSocket.sendBIN(payload, length);
      break;
    case WStype_ERROR:      
    case WStype_FRAGMENT_TEXT_START:
    case WStype_FRAGMENT_BIN_START:
    case WStype_FRAGMENT:
    case WStype_FRAGMENT_FIN:
      break;
  }

}
*/


String httpGETRequest(const char* serverName) {
  HTTPClient http;
    
  // Your IP address with path or Domain name with URL path 
  http.begin(serverName);
  
  // Send HTTP POST request
  int httpResponseCode = http.GET();
  
  String payload = "{}"; 
  
  if (httpResponseCode>0) {
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

void setup() {
  // USE_SERIAL.begin(921600);
  USE_SERIAL.begin(115200);

  //Serial.setDebugOutput(true);
  USE_SERIAL.setDebugOutput(true);

  USE_SERIAL.println();
  USE_SERIAL.println();
  USE_SERIAL.println();

  for(uint8_t t = 4; t > 0; t--) {
    USE_SERIAL.printf("[SETUP] BOOT WAIT %d...\n", t);
    USE_SERIAL.flush();
    delay(1000);
  }

  WiFiMulti.addAP(SSID, PW);

  //WiFi.disconnect();
  while(WiFiMulti.run() != WL_CONNECTED) {
    delay(100);
  }

  

  /* use http requests instead of websocket
  // server address, port and URL
  webSocket.begin(WS_SERVER_ADDRESS, WS_SERVER_PORT, WS_SERVER_URL);

  // event handler
  webSocket.onEvent(webSocketEvent);

  // use HTTP Basic Authorization this is optional remove if not needed
  //webSocket.setAuthorization("user", "Password");

  // try ever 5000 again if connection has failed 
  webSocket.setReconnectInterval(5000);
  */

  bme280_setup();

  USE_SERIAL.printf("[SETUP] finished\n\n");
}

void loop() {

  float temperature, pressure, humidity = 0;

  if ((millis() - last_time) > timer_delay) {
    //if (WiFiMulti.status() == WL_CONNECTED) {
    if (true) {
      HTTPClient http;
      get_bme280_values(&temperature, &pressure, &humidity);
      
      // post-measurement request ---------------------------------
      http.begin(post_measurement_server);

      // Specify content-type header
      http.addHeader("Content-Type", "application/json");
      // Data to send with HTTP POST
      String httpRequestData = String("{\"api_key\":\"") + 
        String(EXTENSION_API_KEY) + 
        String("\",\"sensor\":\"") + String(THIS_SENSOR_NAME) + 
        String("\",\"temperature\":\"") + temperature + 
        String("\",\"pressure\":\"") + pressure + 
        String("\",\"humidity\":\"") + humidity + 
        String("\"}");          
      // Send HTTP POST request
      int httpResponseCode = http.POST(httpRequestData);
     
      Serial.print("post-measurement HTTP Response code: ");
      Serial.println(httpResponseCode);

      // get-status-update request ----------------------------------
      // http.begin(get_status_update_server);

      const char* get_status_update_request_path = (String(get_status_update_server) + String("?api_key=") + 
        String(EXTENSION_API_KEY) + 
        String("&sensor=") + String(THIS_SENSOR_NAME)).c_str();
      Serial.println("request url:");
      Serial.println(get_status_update_request_path);
      status_update_response = httpGETRequest(get_status_update_request_path);
      Serial.println("response:");
      Serial.println(status_update_response);
      JSONVar myObject = JSON.parse(status_update_response);
  
      // JSON.typeof(jsonVar) can be used to get the type of the var
      if (JSON.typeof(myObject) == "undefined") {
        Serial.println("Parsing input failed!");
        return;
      }
    
      Serial.print("JSON object = ");
      Serial.println(myObject);

      JSONVar keys = myObject.keys();
    
      for (int i = 0; i < keys.length(); i++) {
        JSONVar value = myObject[keys[i]];
        Serial.print(keys[i]);
        Serial.print(" = ");
        Serial.println(value);
        status_update_arr[i] = double(value);
      }
      Serial.print("1 = ");
      Serial.println(status_update_arr[0]);
      Serial.print("2 = ");
      Serial.println(status_update_arr[1]);
      Serial.print("3 = ");
      Serial.println(status_update_arr[2]);
      
      // Free resources
      http.end();
    }
    else {
      Serial.println("WiFi Disconnected");
    }
    last_time = millis();
  }
  
  /*
  // write to websocket server
  // ...
  //String payload = String(temperature) + String(',') + String(pressure) + String(',') + String(humidity);
  USE_SERIAL.printf("#0\n");
  char payload[100];
  size_t payload_length = 0;
  USE_SERIAL.printf("#1\n");
  snprintf(payload, payload_length, "%f.3, %f.3, %f.3", temperature, pressure, humidity);
  USE_SERIAL.printf("Sending to websocket...\n");
  // webSocket.sendTXT(payload, payload_length);
  USE_SERIAL("Waiting ...")
  delay(1000*2); // wait
  */
}
