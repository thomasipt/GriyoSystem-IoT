#include <Ethernet.h>
#include <SPI.h>
#include <EEPROM.h>
#include <String.h>

#include <OneWire.h>
#include <DallasTemperature.h>
#define ONE_WIRE_BUS 12
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

#include <Wire.h>

//I2C LCD 0x3F
#include <LiquidCrystal_I2C_Manual.h>
LiquidCrystal_I2C_Manual lcd(0x27, 16, 2);
//#include <LiquidCrystal_I2C.h>
//LiquidCrystal_I2C lcd(0x3F, 16, 2);

//I2C RTC 0x68
#include "RTClib.h"
RTC_DS1307 rtc;

//seting modul LAN
byte ip[] = {192, 168, 0, 101};
byte gateway[] = {192, 168, 0, 1};
byte subnet[] = {255, 255, 255, 0};
byte dnsserver[] = {192, 168, 0, 1};
byte mac[] = {0x00, 0x00, 0x00, 0x01, 0x00, 0x01};
EthernetServer server = EthernetServer(80);

int outputQuantity = 9;
boolean outputInverted = false;
int refreshPage = 15;
int switchOnAllPinsButton = false;
int outputAddress[9] = {A0, A1, A2, A3, A10, A11, A12, A13, A14};
String buttonText[9] = {
  "L01 GAPURA",
  "L02 PAVING",
  "L03 TAMAN",
  "L04 PAGAR KANAN",
  "L05 PAGAR KIRI",
  "L06 R SEKRET 1",
  "L07 R SEKRET 2",
  "L08 TRS SEKRET",
  "L09 TEMBOK SEKRET",
};

int retainOutputStatus[9] = {1, 1, 1, 1, 1, 1, 1, 1, 1};
int outp = 0;
boolean printLastCommandOnce = false;
boolean printButtonMenuOnce = false;
boolean initialPrint = true;
String allOn = "";
String allOff = "";
boolean reading = false;
boolean outputStatus[11];

String tanggal;
String hari;
String bulan;
String hari_weton;
byte acuan_pasaran = 1;

unsigned long previousMillis = 0;
const long interval = 1000;
int STS = 1;

unsigned long timeConnectedAt;
boolean writeToEeprom = false;
//const int tempInPin = A1;
int tempInValue = 0; //baca suhu
int tempScaleOutValue = 0; //format suhu
int tempOutValue = 0; //format suhu
int tempOutDeg = 0.0; //satuan celsius
int tempOutFar = 0.0; //satuan farenheit

String readString;
boolean login = false;

char c;

byte newChar1[8] = {B01000, B10100, B01000, B00011, B00100, B00100, B00011, B00000}; //degree centigrade
byte newChar2[8] = {B01000, B10100, B01000, B00011, B00100, B00111, B00100, B00000}; //degree fahrenheit

void setup() {
  Serial.begin(9600);
  PinModeSystem();
  initEepromValues();
  readEepromValues();
  sensors.begin();

  //set pin output
  boolean currentState = false;
  for (int var = 0; var < outputQuantity; var++) {
    pinMode(outputAddress[var], OUTPUT);

    //kondisi ON OFF waktu start
    if (outputInverted == true) {
      if (outputStatus[var] == 0) {
        currentState = true;
      } else {
        currentState = false;
      }
      digitalWrite(outputAddress[var], currentState);
    }
    else {
      //digitalWrite(outputAddress[var], LOW);
      if (outputStatus[var] == 0) {
        currentState = false;
      } else {
        currentState = true;
      }
      digitalWrite(outputAddress[var], currentState);
    }
  }

  //seting IP LAN
  Ethernet.begin(mac, ip, gateway, subnet);

  server.begin();
  Serial.print("Server started at ");
  Serial.println(Ethernet.localIP());

  //lcd.autoAddress();
  lcd.begin();
  rtc.begin();
  //rtc.adjust(DateTime(__DATE__, __TIME__));
  InitSystem();
}

void PinModeSystem() {
  pinMode(A0, OUTPUT);
  pinMode(A1, OUTPUT);
  pinMode(A2, OUTPUT);
  pinMode(A3, OUTPUT);
  pinMode(A4, OUTPUT);
  pinMode(A5, OUTPUT);
  pinMode(A6, OUTPUT);
  pinMode(A7, OUTPUT);
  pinMode(A8, OUTPUT);
  pinMode(A9, OUTPUT);
  pinMode(A10, OUTPUT);
  pinMode(A11, OUTPUT);
  pinMode(A12, OUTPUT);
  pinMode(A13, OUTPUT);
  pinMode(A14, OUTPUT);
}

void InitSystem() {
  lcd.clear();
  BlinkLCD();
  lcd.setCursor(0, 0);
  lcd.print("RUMAH TERAPI ST");
  lcd.setCursor(1, 1);
  lcd.print(Ethernet.localIP());
  BlinkLCD();
  delay(1000);
  BlinkLCD();
  delay(1000);
  BlinkLCD();
  delay(1000);
  BlinkLCD();
  delay(1000);
  BlinkLCD();
  delay(1000);
  lcd.clear();

  //rtc.adjust(DateTime(__DATE__, __TIME__));
  //lcd.setCursor(7, 0);
  //lcd.print("123456789");

  lcd.createChar(1, newChar1);
  lcd.createChar(2, newChar2);
}

void BlinkLCD() {
  lcd.noBacklight(); delay(100);
  lcd.backlight();
}

void loop() {
  sensors.requestTemperatures();
  tempOutDeg = sensors.getTempCByIndex(0);
  tempOutFar = tempOutDeg * 9 / 5 + 32;
  
  checkForClient();// cek status clients dan proses
  
  checkRTC();
  CetakSuhu();
  
  unsigned long currentMillis = millis();
  if(currentMillis - previousMillis > interval) {
    previousMillis = currentMillis;
    if (STS == 1) {
      lcd.setCursor(10, 1);
      lcd.print("      ");
      lcd.setCursor((16 - hari.length()), 1);
      lcd.print(hari);
      STS = 0;
    }
    else {
      lcd.setCursor(10, 1);
      lcd.print("      ");     
      lcd.setCursor((16 - hari_weton.length()), 1);
      lcd.print(hari_weton);
      STS = 1;
    }
  }
}

void checkForClient() {
  EthernetClient client = server.available();
  if (client) {
    boolean currentLineIsBlank = true;
    boolean sentHeader = false;
    boolean login = false;
    while (client.connected()) {
      if (client.available()) {
        char c = client.read();
        readString.concat(c);
        //Serial.print(c);
        if (c == '*') {
          printHtmlHeader(client); //panggil html header dan css
          //printLoginTitle(client); //form LOGIN
          printHtmlFooter(client);
          //sentHeader = true;
          login = false;
          break;
        }
        if (!sentHeader) {
          printHtmlHeader(client); //panggil html header dan css
          printHtmlButtonTitle(client); //cetak button caption
          sentHeader = true;
        }
        if (reading && c == ' ') {
          reading = false;
        }
        if (c == '?') {
          reading = true;
        }
        //UPDATE LOGIN
        if (login == false) {
          if (readString.indexOf("User=admin&Pass=1234") > 0) {
            login = true;
          }
        }
        //

        if (reading) {  //read 2 if user input is H set output to L
          if (c == 'H') {outp = 1;}
          if (c == 'L') {outp = 0;}
          Serial.print(c);
          //Serial.print(outp);
          //Serial.print('\n');
          //---------------------------------------------------------------------------------------------
          // ? H 1 0
          // ^ ^ ^ ^
          // | | | |____________read 4 ( 10,11,12,13....)
          // | | |______________read 3 ( 1....9)
          // | |________________read 2 if user input is H set output to L
          // |__________________read 1
          //---------------------------------------------------------------------------------------------

          if ( c == '1') {  //read 4 ( 10,11,12,13....)
            char c = client.read();
            switch (c) {
              case '0':
                triggerPin(outputAddress[10], client, outp); break;
              case '1':
                triggerPin(outputAddress[11], client, outp); break;
              case '2':
                triggerPin(outputAddress[12], client, outp); break;
              case '3':
                triggerPin(outputAddress[13], client, outp); break;
              case '4':
                triggerPin(outputAddress[14], client, outp); break;
              case '5':
                triggerPin(outputAddress[15], client, outp); break;
              default:
                char c = client.read();
                triggerPin(outputAddress[1], client, outp);
            }
          }
          else {
            switch (c) {  //read 3 ( 1....9)
              case '0':
                triggerPin(outputAddress[0], client, outp); break;
              case '1':
                triggerPin(outputAddress[1], client, outp); break;
              case '2':
                triggerPin(outputAddress[2], client, outp); break;
              case '3':
                triggerPin(outputAddress[3], client, outp); break;
              case '4':
                triggerPin(outputAddress[4], client, outp); break;
              case '5':
                triggerPin(outputAddress[5], client, outp); break;
              case '6':
                triggerPin(outputAddress[6], client, outp); break;
              case '7':
                triggerPin(outputAddress[7], client, outp); break;
              case '8':
                triggerPin(outputAddress[8], client, outp); break;
              case '9':
                triggerPin(outputAddress[9], client, outp); break;
            }
          }
        }
        if (c == '\n' && currentLineIsBlank) {
          printLastCommandOnce = true;
          printButtonMenuOnce = true;
          triggerPin(777, client, outp); //panggil baca input dan cetak menu, 777 untuk update output
          break;
        }
      }
    }
    printHtmlFooter(client);
  }
  else {
    if (millis() > (timeConnectedAt + 60000)) { //cek kondisi idle 1 menit
      if (writeToEeprom == true) {
        writeEepromValues();  //simpan kondisi terakhir ke EEprom jika tidak ada input data
        Serial.println("Writing statuses to Eeprom");
        writeToEeprom = false;
      }
    }
  }
}

void triggerPin(int pin, EthernetClient client, int outp) {
  if (pin != 777) {
    if (outp == 1) {
      if (outputInverted == false) {
        digitalWrite(pin, HIGH);
      } else {
        digitalWrite(pin, LOW);
      }
    }
    if (outp == 0) {
      if (outputInverted == false) {
        digitalWrite(pin, LOW);
      }  else {
        digitalWrite(pin, HIGH);
      }
    }
  }
  readOutputStatuses();
  if (printButtonMenuOnce == true) {
    printHtmlButtons(client);
    printButtonMenuOnce = false;
  }
}

void printHtmlButtons(EthernetClient client) {
  client.println("");
  client.println("<p>");
  client.println("<FORM>");
  client.println("<table border=\"0\" align=\"center\">");

  //cetak suhu
  client.print("<tr>\n");
  client.print("<td><h4>"); client.print("TEMPERATURE"); client.print("</h4></td>\n");
  client.print("<td><h4>"); client.print(tempOutDeg); client.print("&deg;C</h4></td>\n");
  client.print("<td><h4>"); client.print(tempOutFar); client.print("&deg;F</h4></td>\n");

  //CetakSuhu();

  //client.print(tempOutValue);
  client.print("<td></td>");
  client.print("</tr>");

  //looping tombol
  for (int var = 0; var < outputQuantity; var++)  {
    allOn += "H";  allOn  += outputAddress[var];
    allOff += "L"; allOff += outputAddress[var];

    client.print("<tr>\n");
    client.print("<td><h4>"); client.print(buttonText[var]); client.print("</h4></td>\n");

    //tombol ON
    client.print("<td>");
    client.print("<INPUT TYPE=\"button\" VALUE=\"ON ");
    client.print("\" onClick=\"parent.location='/?H");
    client.print(var);
    client.print("'\"></td>\n");

    //tombol OFF
    client.print(" <td><INPUT TYPE=\"button\" VALUE=\"OFF");
    client.print("\" onClick=\"parent.location='/?L");
    client.print(var);
    client.print("'\"></td>\n");
    if (outputStatus[var] == true ) {
      if (outputInverted == false) {
        client.print(" <td><div class='green-circle'><div class='glare'></div></div></td>\n");
        //lcd.setCursor(var + 7, 0);
        //lcd.print("*");
      } else {
        client.print(" <td><div class='black-circle'><div class='glare'></div></div></td>\n");
        //lcd.setCursor(var + 7, 0);
        //lcd.print(" ");
      }
    }
    else {
      if (outputInverted == false) {
        client.print(" <td><div class='black-circle'><div class='glare'></div></div></td>\n");
        //lcd.setCursor(var + 7, 0);
        //lcd.print(" ");
      } else {
        client.print(" <td><div class='green-circle'><div class='glare'></div></div></td>\n");
        //lcd.setCursor(var + 7, 0);
        //lcd.print("*");
      }
    }
    client.print("</tr>\n");
  }

  if (switchOnAllPinsButton == true ) {

    //semua tombol ON
    client.print("<tr>\n<td><INPUT TYPE=\"button\" VALUE=\"Switch ON All Pins");
    client.print("\" onClick=\"parent.location='/?");
    client.print(allOn);
    client.print("'\"></td>\n");

    //semua tombol OFF
    client.print("<td><INPUT TYPE=\"button\" VALUE=\"Switch OFF All Pins");
    client.print("\" onClick=\"parent.location='/?");
    client.print(allOff);
    client.print("'\"></td>\n<td></td>\n<td></td>\n</tr>\n");
  }
  client.println("</table>");
  client.println("</FORM>");
  client.println("</p>");

}

//baca status output
void readOutputStatuses() {
  for (int var = 0; var < outputQuantity; var++)  {
    outputStatus[var] = digitalRead(outputAddress[var]);
  }

}

//baca EEprom dan simpan output
void readEepromValues() {
  for (int adr = 0; adr < outputQuantity; adr++)  {
    outputStatus[adr] = EEPROM.read(adr);
    Serial.print(EEPROM.read(adr));
  }
  Serial.println("readEepromValues");
}

//tulis nilai pada EEprom
void writeEepromValues() {
  for (int adr = 0; adr < outputQuantity; adr++)  {
    EEPROM.write(adr, outputStatus[adr]);
    Serial.print(outputStatus[adr]);
    BlinkLCD();
    BlinkLCD();
    BlinkLCD();
  }
  Serial.println("writeEepromValues");
}

//init fungsi EEpromValues
//jika EEprom format salah, bukan0 atau 1, maka >1 paksa menjadi bernilai 0
void initEepromValues() {
  for (int adr = 0; adr < outputQuantity; adr++) {
    if (EEPROM.read(adr) > 1) {
      EEPROM.write(adr, 0);
    }
  }
}


void printHtmlHeader(EthernetClient client) {
  Serial.print("Serving html Headers at ms -");
  timeConnectedAt = millis();
  Serial.print(timeConnectedAt);
  writeToEeprom = true;
  client.println("HTTP/1.1 200 OK");
  client.println("Content-Type: text/html");
  client.println("Connnection: close");
  client.println();
  client.println("<!DOCTYPE HTML>");
  client.println("<head>");
  client.println("<title>INTELLIGENT SMART SYSTEM</title>");
  client.println("<meta name=\"description\" content=\"IT Solution - EDP_IPT2017\"/>");
  client.print  ("<meta http-equiv=\"refresh\" content=\"");
  client.print  (refreshPage);
  client.println("; url=/\">");
  client.println("<meta name=\"apple-mobile-web-app-capable\" content=\"yes\">");
  client.println("<meta name=\"apple-mobile-web-app-status-bar-style\" content=\"default\">");
  client.println("<meta name=\"viewport\" content=\"width=device-width, user-scalable=no\">");
  client.println("<style type=\"text/css\">");
  client.println("");

  //text-CSS3
  client.println("html {height: 100%;}");
  client.println("body {");
  client.println("	height: 100%;");
  client.println("	margin: 0;");
  client.println("	font-family: helvetica, sans-serif;");
  client.println("	-webkit-text-size-adjust: none;");
  client.println("      }");
  client.println("");
  client.println("body {");
  client.println("	-webkit-background-size: 100% 20px;");
  client.println("	background-color: #c5ccd3;");
  client.println("	background-image:");
  client.println("      -webkit-gradient(linear, left top, right bottom,");
  client.println("      color-stop(.75, transparent),");
  client.println("      color-stop(.75, rgba(0,0,0,.1)));");
  client.println("	-webkit-background-size: 5px;");
  client.println("      }");
  client.println("");
  client.println(".view {");
  client.println("	min-height: 100%;");
  client.println("	overflow: auto;");
  client.println("      }");
  client.println("");
  client.println(".header-wrapper {");
  client.println("	height: 44px;");
  client.println("	font-weight: bold;");
  client.println("	text-shadow: rgba(0,0,0,1) 0 5px 5px;");
  client.println("	border-top: solid 1px rgba(255,255,255,0.6);");
  client.println("	border-bottom: solid 1px rgba(0,0,0,0.6);");
  client.println("	color: #fff;");
  client.println("	background-color: #8195af;");
  client.println("	background-image: ");
  client.println("	-webkit-gradient(linear, left top, left bottom,");
  client.println("	from(rgba(255,255,255,.4)),");
  client.println("	to(rgba(255,255,255,.05)) ),");
  client.println("	-webkit-gradient(linear, left top, left bottom,");
  client.println("	from(transparent),");
  client.println("	to(rgba(0,0,64,.1)) );");
  client.println("	background-repeat: no-repeat;");
  client.println("	background-position: top left, bottom left;");
  client.println("	-webkit-background-size: 100% 21px, 100% 22px;");
  client.println("	-webkit-box-sizing: border-box;");
  client.println("	-webkit-box-shadow: 10px 10px 5px 0px");
  client.println("      rgba(0,0,0,0.75);");
  client.println("      }");
  client.println("");
  client.println(".header-wrapper h1 {");
  client.println("	text-align: center;");
  client.println("	font-size: 20px;");
  client.println("	line-height: 44px;");
  client.println("	margin: 0;");
  client.println("      }");
  client.println("");
  client.println(".group-wrapper {");
  client.println("	margin: 10px;");
  client.println("      }");
  client.println("");
  client.println(".group-wrapper h2 {");
  client.println("	color: #4c566c;");
  client.println("	font-size: 20px;");
  client.println("	line-height: 0.8;");
  client.println("	font-weight: bold;");
  client.println("	text-shadow: #fff 0 1px 0;");
  client.println("	margin: 20px 10px 12px;");
  client.println("      }");
  client.println("");
  client.println(".group-wrapper h3 {");
  client.println("	color: #4c566c;");
  client.println("	font-size: 12px;");
  client.println("	line-height: 1;");
  client.println("	font-weight: bold;");
  client.println("	text-shadow: #fff 0 1px 0;");
  client.println("	margin: 20px 10px 12px;");
  client.println("      }");
  client.println("");
  client.println(".group-wrapper h4 {");
  client.println("	color: #212121;");
  client.println("	font-size: 14px;");
  client.println("	line-height: 1;");
  client.println("	font-weight: bold;");
  client.println("	text-shadow: #aaa 1px 1px 3px;");
  client.println("	margin: 5px 5px 5px;");
  client.println("      }");
  client.println("");
  client.println(".group-wrapper table {");
  client.println("	background-color: #fff;");
  client.println("	-webkit-border-radius: 10px;");
  client.println("	-moz-border-radius: 10px;");
  client.println("	-khtml-border-radius: 10px;");
  client.println("	border-radius: 10px;");
  client.println("	font-size: 20px;");
  client.println("	line-height: 20px;");
  client.println("	margin: 10px 0 20px;");
  client.println("	border: solid 3px #8195af;");
  client.println("	padding: 11px 3px 12px 3px;");
  client.println("	margin-left:auto;");
  client.println("	margin-right:auto;");
  client.println("	-moz-transform :scale(1);"); //Mozilla Firefox
  client.println("	-moz-transform-origin: 0 0;");
  client.println("	box-shadow: 0 10px 100px 10px ");
  client.println("      rgba(0, 0, 0, 0.2), 0 6px 20px 0");
  client.println("      rgba(0, 0, 0, 0.19);");
  client.println("      }");
  client.println("");
  client.println(".green-circle {");
  client.println("	display: block;");
  client.println("	height: 23px;");
  client.println("	width: 23px;");
  client.println("	background-color: #0f0;");
  client.println("	/* background-color: rgba(60, 132, 198, 0.8); */");
  client.println("	-moz-border-radius: 11px;");
  client.println("	-webkit-border-radius: 11px;");
  client.println("	-khtml-border-radius: 11px;");
  client.println("	border-radius: 11px;");
  client.println("	margin-left: 1px;");
  client.println("	background-image: -webkit-gradient(linear, 0% 0%, 0% 90%, from(rgba(46, 184, 0, 0.8)), to(rgba(148, 255, 112, .9)));@");
  client.println("	border: 2px solid #ccc;");
  client.println("	-webkit-box-shadow: rgba(11, 140, 27, 0.5) 0px 10px 16px;");
  client.println("	-moz-box-shadow: rgba(11, 140, 27, 0.5) 0px 10px 16px; /* FF 3.5+ */");
  client.println("	box-shadow: rgba(11, 140, 27, 0.5) 0px 10px 16px; /* FF 3.5+ */");
  client.println("      }");
  client.println("");
  client.println(".black-circle {");
  client.println("	display: block;");
  client.println("	height: 23px;");
  client.println("	width: 23px;");
  client.println("	background-color: #040;");
  client.println("	-moz-border-radius: 11px;");
  client.println("	-webkit-border-radius: 11px;");
  client.println("	-khtml-border-radius: 11px;");
  client.println("	border-radius: 11px;");
  client.println("	margin-left: 1px;");
  client.println("	-webkit-box-shadow: rgba(11, 140, 27, 0.5) 0px 10px 16px;");
  client.println("	-moz-box-shadow: rgba(11, 140, 27, 0.5) 0px 10px 16px; /* FF 3.5+ */ ");
  client.println("	box-shadow: rgba(11, 140, 27, 0.5) 0px 10px 16px; /* FF 3.5+ */");
  client.println("      }");
  client.println("");
  client.println(".glare {");
  client.println("	position: relative;");
  client.println("	top: 1;");
  client.println("	left: 5px;");
  client.println("	-webkit-border-radius: 10px;");
  client.println("	-moz-border-radius: 10px;");
  client.println("	-khtml-border-radius: 10px;");
  client.println("	border-radius: 10px;");
  client.println("	height: 1px;");
  client.println("	width: 13px;");
  client.println("	padding: 5px 0;");
  client.println("	background-color: rgba(200, 200, 200, 0.25);");
  client.println("	background-image: -webkit-gradient(linear, 0% 0%, 0% 95%, from(rgba(255, 255, 255, 0.7)), to(rgba(255, 255, 255, 0)));");
  client.println("      }");
  //client.println("");
  //client.println("");
  //text-css

  //style data+header
  client.println("</style>");
  client.println("</head>");

  client.println("<body>");
  client.println("<div class=\"view\">");
  client.println("    <div class=\"header-wrapper\">");
  client.println("      <h1>MAIN BUILDING</h1>");
  client.println("    </div>");
}

void printHtmlFooter(EthernetClient client) {
  //seting variables sebelum keluar
  printLastCommandOnce = false;
  printButtonMenuOnce = false;
  allOn = "";
  allOff = "";

  client.println("\n<h3 align=\"center\">RUMAH TERAPI ST&copy; 2017</h3>");
  client.println("\n<h2 align=\"center\">" + tanggal + "</h3>");
  client.println("</div>\n</div>\n</body>\n</html>");

  //delay browser terima data
  delay(1);

  //tutup koneksi
  client.stop();
  Serial.println(" - Done, Closing Connection.");

  //delay clear buffer browser terima data
  delay (2);
}

void printHtmlButtonTitle(EthernetClient client) {
  client.println("<div  class=\"group-wrapper\">");
  client.println();
}

void printLoginTitle(EthernetClient client) {
  client.print  ("<form action='192.168.0.101/'>"); //change to your IP
  client.print  ("");
  client.println("<div  class=\"group-wrapper\">");
  client.println("<FORM>");
  client.println("<table border=\"0\" align=\"center\">");
  client.println("  <tr>\n");
  client.print  ("    <td><h4>USERNAME :</h4></td>");
  client.print  ("    <td><h4><input name=\'User\' value=\'\'></h4></td>");
  client.println("  </tr>\n");
  client.println("  <tr>\n");
  client.print  ("    <td><h4>PASSWORD :</h4></td>");
  client.print  ("    <td><h4><input type=\'Password\' name=\'Pass\' value=\'\'></h4></td>");
  client.println("  </tr>\n");
  client.println("  <tr>\n");
  client.print  ("    <td colspan=\"2\" align=\"right\"><INPUT TYPE=\'submit\' value=\'LOGIN\'></td>");
  client.println("  </tr>\n");
  client.println("</table>");
  client.println("</FORM>");
  client.println("</div>");
}

void CetakSuhu() {
  String celcius, farenheit;
  
  if (tempOutDeg > 99) { celcius = "99"; } else { celcius = String(tempOutDeg); }
  if (tempOutFar > 99) { farenheit = "99"; } else { farenheit = String(tempOutFar); }
  
  lcd.setCursor(0, 1);
  //lcd.print("        ");
  //lcd.setCursor(0, 1);
  lcd.print(celcius);  lcd.write(1);
  lcd.print(" ");
  lcd.print(farenheit);  lcd.write(2);
}

void checkRTC() {
  String s, m, d, mth, h;
  
  DateTime now = rtc.now();
  //sprintf(tanggal, "%02hhu/%02hhu/%02hhu", now.day(), now.month(), now.year() );
  if (now.minute() < 10) { m = "0" + String(now.minute()); } else { m = String(now.minute()); }
  if (now.day() < 10) { d = "0" + String(now.day()); } else { d = String(now.day()); }
  if (now.month() < 10) { mth = "0" + String(now.month()); } else { mth = String(now.month()); }
  
  lcd.setCursor(0, 0);
  lcd.print(d + "/" + mth + "/" + String(now.year()));
  lcd.print(" ");
  lcd.print(String(now.hour()) + ":" + m);
  
  switch (now.dayOfWeek()){
    case 0: hari = "MINGGU"; break;
    case 1: hari = "SENIN"; break;
    case 2: hari = "SELASA"; break;
    case 3: hari = "RABU"; break;
    case 4: hari = "KAMIS"; break;
    case 5: hari = "JUMAT"; break;
    case 6: hari = "SABTU"; break;
  }
  
  switch (now.month()){
    case 1: bulan= "JANUARI"; break;
    case 2: bulan = "FEBRUARI"; break;
    case 3: bulan = "MARET"; break;
    case 4: bulan = "APRIL"; break;
    case 5: bulan = "MEI"; break;
    case 6: bulan = "JUNI"; break;
    case 7: bulan = "JULI"; break;
    case 8: bulan = "AGUSTUS"; break;
    case 9: bulan = "SEPTEMBER"; break;
    case 10: bulan = "OKTIBER"; break;
    case 11: bulan = "NOVEMBER"; break;
    case 12: bulan = "DESEMBER"; break;
  }
  
  DateTime acuan ("Jan 01 2000", "00:00:00");
  // sample input: date = "Dec 26 2009", time = "12:34:56"
  byte pasaran = (((now.unixtime() / 86400) - (acuan.unixtime() / 86400)) % 5) + acuan_pasaran;
  if (pasaran > 5) {
    pasaran = pasaran - 5;
  }
  switch (pasaran) {
    case 1: hari_weton = "LEGI"; break;
    case 2: hari_weton = "PAHING"; break;
    case 3: hari_weton = "PON"; break;
    case 4: hari_weton = "WAGE"; break;
    case 5: hari_weton = "KLIWON"; break;
  }
  
  tanggal = d + " " + bulan + " " + String(now.year()) + ", " + hari + " / " + hari_weton;
  
}

