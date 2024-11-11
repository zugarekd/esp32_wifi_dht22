#include "SPIFFS.h"
#include "FS.h"
#include <WiFi.h>
#include <DHT.h>
#include <HTTPClient.h>

#define DHT22_PIN  22 // ESP32 pin GPIO22 connected to DHT22 sensor

DHT dht22(DHT22_PIN, DHT22);

const char *ssid = "ssid";
const char *password = "password";

const char* homeFileName = "/home.txt";
const char* nameFileName = "/name.txt";
const char* sleepFileName = "/sleep.txt";

long interval = 60000;
unsigned long previousMillis = 0;

String home = "";
String name = "";
String slp = "";
String ip = "";

NetworkServer server(80);

void setup() {
  Serial.begin(9600);

  if (SPIFFS.begin(true)) {
    Serial.println("SPIFFS mounted successfully.");
  } else {
   Serial.println("SPIFFS mount failed. Check your filesystem.");
  }

  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);
  dht22.begin(); // initialize the DHT22 sensor
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected.");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
  ip = WiFi.localIP().toString(false);

  server.begin();

  // Open home file and read contents
  File file = SPIFFS.open(homeFileName, "r");
  if (file) {
    home = file.readString();
    file.close();
  } else {
    Serial.println("Failed to open home.txt, setting default value.");
    home = "";  // Set a default value here if needed
  }

  // Open name file and read contents
  file = SPIFFS.open(nameFileName, "r");
  if (file) {
    name = file.readString();
    file.close();
  } else {
    Serial.println("Failed to open name.txt, setting default to IP address.");
    name = WiFi.localIP().toString();
  }
  if (name == "") {
    name = WiFi.localIP().toString();
  }

  // Open sleep file and read contents
  file = SPIFFS.open(sleepFileName, "r");
  if (file) {
    slp = file.readString();
    file.close();
    if (slp.toInt() > 0) {
      interval = slp.toInt();  // Set interval from file
    }
  } else {
    Serial.println("Failed to open sleep.txt, using default interval.");
    interval = 60000;
  }
  if (slp == "") {
    slp = "60000";
  }
}

void handleFileWrite(const char* fileName, const String& content) {
  File file = SPIFFS.open(fileName, "w");
  if (!file) {
    Serial.println("Failed to open file for writing");
    return;
  }
  file.print(content);
  file.close();
}

String getParamValue(String data, const String &param) {
  int startIndex = data.indexOf(param + "=");
  if (startIndex == -1) return "";
  startIndex += param.length() + 1;
  int endIndex = data.indexOf('&', startIndex);
  if (endIndex == -1) endIndex = data.indexOf(' ', startIndex);
  return data.substring(startIndex, endIndex);
}

void loop() {
  WiFiClient client = server.available();
  if (client) {
    Serial.println("New Client.");
    String request = "";
    while (client.connected() && client.available()) {
      request += (char)client.read();
    }

    // Parse the request path
    if (request.indexOf("GET /name") >= 0) {
      String newName = getParamValue(request, "value");
      if (newName != "") {
        handleFileWrite(nameFileName, newName);
        client.println("HTTP/1.1 200 OK");
        client.println("Content-Type: text/html");
        client.println();
        client.println("Name updated.");
        name = newName;
      }
    } else if (request.indexOf("GET /home") >= 0) {
      String newHome = getParamValue(request, "value");
      if (newHome != "") {
        handleFileWrite(homeFileName, newHome);
        client.println("HTTP/1.1 200 OK");
        client.println("Content-Type: text/html");
        client.println();
        client.println("Home updated.");
        home = newHome;
      }
    } else if (request.indexOf("GET /send") >= 0) {
      makeHttpRequest();
      client.println("HTTP/1.1 200 OK");
      client.println("Content-Type: text/html");
      client.println();
      client.println("Sent.");
    } else if (request.indexOf("GET /sleep") >= 0) {
      String newSleep = getParamValue(request, "value");
      if (newSleep != "") {
        interval = newSleep.toInt();
        handleFileWrite(sleepFileName, newSleep);
        client.println("HTTP/1.1 200 OK");
        client.println("Content-Type: text/html");
        client.println();
        client.println("Sleep interval updated.");
        if (interval < 30000) {
          interval = 60000;     
        }
        slp = newSleep;
      } else {
        Serial.println("Failed to open sleep.txt, using default interval.");
        interval = 60000;
      }
    } else {
      // Default response with DHT data
      client.println("HTTP/1.1 200 OK");
      client.println("Content-Type: text/html");
      client.println();
      float humi = dht22.readHumidity();
      float tempF = dht22.readTemperature(true);
      if (isnan(tempF) || isnan(humi)) {
        client.println("Failed to read from DHT sensor.");
      } else {
        client.print("Humidity: ");
        client.print(humi);
        client.print("% | Temperature: ");
        client.print(tempF);
        client.print("F | ");
        client.print("Name: " + name + " | ");
        client.print("Home: " + home + " | ");
        client.print("Duration: " + slp);
      }
    }

    client.stop();
    Serial.println("Client disconnected.");
  }

  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;
    makeHttpRequest();
  }
}

void makeHttpRequest() {
  if (home != "") {
    if (WiFi.status() == WL_CONNECTED) {
      HTTPClient http;
      // read humidity
      float humi  = dht22.readHumidity();

      float tempF = dht22.readTemperature(true);

      http.begin(home + "?name=" + name + "&h=" + String(humi) + "&f=" + String(tempF) + "&ip=" + ip);

      int httpResponseCode = http.GET();

      if (httpResponseCode > 0) {
        String payload = http.getString();
        Serial.println(payload);
      } else {
        Serial.print("Error on HTTP request: ");
        Serial.println(httpResponseCode);
      }

      http.end();
    } else {
      Serial.println("Wi-Fi Disconnected");
    }
  }
}
