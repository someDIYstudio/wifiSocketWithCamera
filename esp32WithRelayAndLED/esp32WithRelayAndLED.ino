#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include "esp_camera.h"
#include <ArduinoJson.h>
#include "camera_code.h"

const char *ssid = "YourWiFiSSID";
const char *password = "YourWiFiPassword";
const char *botToken = "YOUR_BOT_TOKEN";
const char *chat_id = "YOUR_CHAT_ID";

#define FLASH_LED_PIN  4
#define LED_LIGHT 12
#define RELAY 13

const unsigned long BOT_MTBS = 3000;

unsigned long bot_lasttime; 
WiFiClientSecure secured_client;
UniversalTelegramBot bot(botToken, secured_client);

camera_config_t config;

void setup()
{
  //Serial.begin(115200);
  //Serial.println();
  pinSetup();

  if (!setupCamera())
  {
    // Serial.println("Camera Setup Failed!");
    while (true)
    {
      delay(100);
    }
  }

  // if(!takePhoto()) Serial.println("can't take a photo");

  wifiSetup();
  timeChek();

  bot.longPoll = 60;
  
  bot.sendMessageWithReplyKeyboard(chat_id, "Camera is online", "", "[[\"start\"]]", true);
}

void loop()
{
  if (millis() - bot_lasttime > BOT_MTBS)
  {
    int numNewMessages = bot.getUpdates(bot.last_message_received + 1);

    while (numNewMessages)
    {
      // Serial.println("got response");
      handleNewMessages(numNewMessages);
      numNewMessages = bot.getUpdates(bot.last_message_received + 1); 
    }

    bot_lasttime = millis();
  }
}

void pinSetup(){
  pinMode(FLASH_LED_PIN, OUTPUT);
  digitalWrite(FLASH_LED_PIN, LOW);
  pinMode(LED_LIGHT, OUTPUT);
  digitalWrite(LED_LIGHT, LOW);
  pinMode(RELAY, OUTPUT);
  digitalWrite(RELAY, HIGH);
}

void wifiSetup(){
  // Serial.print("Connecting to Wifi SSID ");
  // Serial.print(ssid);
  WiFi.begin(ssid, password);
  secured_client.setCACert(TELEGRAM_CERTIFICATE_ROOT);
  while (WiFi.status() != WL_CONNECTED)
  {
    // Serial.print(".");
    delay(500);
  }
  // Serial.print("\nWiFi connected. IP address: ");
  // Serial.println(WiFi.localIP());
}

void timeChek(){
  // Serial.print("Retrieving time: ");
  configTime(0, 0, "pool.ntp.org"); 
  time_t now = time(nullptr);
  while (now < 24 * 3600)
  {
    // Serial.print(".");
    delay(100);
    now = time(nullptr);
  }
  // Serial.println(now);
}

void handleNewMessages(int numNewMessages)
{
  String mainMenu = "[[\"photo\", \"photo with LED\"],[\"photo with flash\"],";
  mainMenu += "[\"printer ON\", \"printer OFF\"],";
  mainMenu += "[\"current resolution\",\"setup frame size\"],";
  mainMenu += "[\"send video\"], [\"restart esp\"]]"; //'send video' in progress
  String frameSizeMenu = "[[\"UXGA\", \"SXGA\", \"XGA\"],[\"SVGA\", \"VGA\"], [\"back\"]]";
  // Serial.println("handleNewMessages");
  // Serial.println(String(numNewMessages));

  for (int i = 0; i < numNewMessages; i++)
  {
    String text = bot.messages[i].text;

    // Serial.println(String(bot.messages[i].chat_id));
    
    if (text == "start" || text == "/start" || text == "back")
    {
      bot.sendMessageWithReplyKeyboard(chat_id, "select an action", "", mainMenu, true);
    }

    if(text == "photo") {
      captureAndSendPhoto();
    }

    if(text == "photo with LED") {
      digitalWrite(LED_LIGHT, HIGH);
      delay(100);
      captureAndSendPhoto();
      delay(100);
      digitalWrite(LED_LIGHT, LOW);
    }

    if (text == "photo with flash")
    {
      digitalWrite(FLASH_LED_PIN, HIGH);
      delay(100);
      captureAndSendPhoto();
      delay(100);
      digitalWrite(FLASH_LED_PIN, LOW);
    }

    //printer shutdown handling
    if(text == "printer OFF"){
      bot.sendMessageWithReplyKeyboard(chat_id, "are you sure?", "", "[[\"yes\"], [\"no\"], [\"back\"]]", true);
    }
      if(text == "yes"){
        digitalWrite(RELAY, LOW);
        bot.sendMessage(chat_id, "printer turned off", "");
        bot.sendMessageWithReplyKeyboard(chat_id, "select an action", "", mainMenu, true);
      }
      else if(text == "no"){
        bot.sendMessage(chat_id, "canceled", "");
        bot.sendMessageWithReplyKeyboard(chat_id, "select an action", "", mainMenu, true);
      }

     if(text == "printer ON"){
      digitalWrite(RELAY, HIGH);
      bot.sendMessage(chat_id, "printer turned on", "");
    }

    //camera resolutions
    if(text == "current resolution") {
        sensor_t *sensor = esp_camera_sensor_get();        
        bot.sendMessage(chat_id, nameOfFramesize(sensor->status.framesize), "");
    }
    if(text =="setup frame size"){
      bot.sendMessage(chat_id, "select image resolution", "");
      bot.sendMessageWithReplyKeyboard(chat_id, "select an action", "", frameSizeMenu, true);
    }
    if(text == "SVGA"){
      changeResolution(FRAMESIZE_SVGA);
      bot.sendMessage(chat_id, "new resolution is SVGA", "");
      bot.sendMessageWithReplyKeyboard(chat_id, "select an action", "", mainMenu, true);
    }
    if(text == "VGA"){
      changeResolution(FRAMESIZE_VGA);
      bot.sendMessage(chat_id, "new resolution is VGA", "");
      bot.sendMessageWithReplyKeyboard(chat_id, "select an action", "", mainMenu, true);
    }
    if(text == "XGA"){
      changeResolution(FRAMESIZE_XGA);
      bot.sendMessage(chat_id, "new resolution is XGA", "");
      bot.sendMessageWithReplyKeyboard(chat_id, "select an action", "", mainMenu, true);
    }
    if(text == "SXGA"){
      changeResolution(FRAMESIZE_SXGA);
      bot.sendMessage(chat_id, "new resolution is SXGA", "");
      bot.sendMessageWithReplyKeyboard(chat_id, "select an action", "", mainMenu, true);
    }
    if(text == "UXGA"){
      changeResolution(FRAMESIZE_UXGA);
      bot.sendMessage(chat_id, "new resolution is UXGA", "");
      bot.sendMessageWithReplyKeyboard(chat_id, "select an action", "", mainMenu, true);
    }
  }
}

bool takePhoto(){
  delay(100);
  camera_fb_t *fb = NULL; 
  fb = esp_camera_fb_get();
  if(!fb) return false;
  esp_camera_fb_return(fb);
  return true;
}


void captureAndSendPhoto() {
  if(!takePhoto()) Serial.println("can't take a photo");

  camera_fb_t *fb = NULL; 
  fb = esp_camera_fb_get();
  if (!fb) {
    // Serial.println("Camera capture failed");
    return;
  }

  sendTelegramPhoto(fb->buf, fb->len);

  esp_camera_fb_return(fb);
}

void sendTelegramPhoto(uint8_t *photoData, size_t photoSize) {
  // Serial.println("Sending photo to Telegram...");
  bot.sendChatAction(chat_id, "upload_photo");

  HTTPClient http;

  String url = "https://api.telegram.org/bot" + String(botToken) + "/sendPhoto";

  String boundary = "------------------------" + String(millis());

  http.begin(url);
  http.addHeader("Content-Type", "multipart/form-data; boundary=" + boundary);

  String body = "--" + boundary + "\r\n";
  body += "Content-Disposition: form-data; name=\"chat_id\"\r\n\r\n";
  body += String(chat_id) + "\r\n";

  body += "--" + boundary + "\r\n";
  body += "Content-Disposition: form-data; name=\"photo\"; filename=\"photo.jpg\"\r\n";
  body += "Content-Type: image/jpeg\r\n\r\n";

  for (size_t i = 0; i < photoSize; i++) {
    body += (char)photoData[i];
  }

  body += "\r\n";
  body += "--" + boundary + "--\r\n";

  int httpResponseCode = http.POST(body);

  if (httpResponseCode == 200) {
    // Serial.println("Photo sent successfully");
  } else {
    // Serial.print("Error sending photo. HTTP response code: ");
    // Serial.println(httpResponseCode);
    bot.sendMessage(chat_id, "Error sending photo.", "");
  }

  http.end();

  // Serial.println("done");
}

void changeResolution(framesize_t newSize) {
  sensor_t *s = esp_camera_sensor_get();
  s->set_framesize(s, newSize);  
}