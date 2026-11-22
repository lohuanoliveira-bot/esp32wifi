#include <WiFi.h>
#include <WebServer.h>
#include <ESP32Servo.h>
#include <math.h>
#include <stdlib.h>

// ================= CONFIGURACOES =================
// Altere para o nome e senha da rede Wi-Fi da sua casa:
const char* ssid = "seuWIFI";
const char* password = "suaSENHAWIFI";

const int pinoFL = 13; // Frente esquerda
const int pinoFR = 12; // Frente direita
const int pinoBL = 14; // Traseira esquerda
const int pinoBR = 27; // Traseira direita

// Nomes alterados para evitar conflito com o sistema do ESP32
Servo FL_servo, FR_servo, BL_servo, BR_servo;
WebServer server(80);

const int CENTRO = 90;
const int FRENTE = 50;
const int TRAS = 130;

// Ajuste conforme os limites mecanicos da montagem.
const int ANGULO_MIN = 40;
const int ANGULO_MAX = 140;

float joystickX = 0;
float joystickY = 0;
float fase = 0;
bool andando = false;
unsigned long ultimoComando = 0;
unsigned long ultimoPasso = 0;
const unsigned long TIMEOUT = 700;

// ================= MOVIMENTOS =================
void setAll(int fl, int fr, int bl, int br) {
  FL_servo.write(constrain(fl, ANGULO_MIN, ANGULO_MAX));
  FR_servo.write(constrain(fr, ANGULO_MIN, ANGULO_MAX));
  BL_servo.write(constrain(bl, ANGULO_MIN, ANGULO_MAX));
  BR_servo.write(constrain(br, ANGULO_MIN, ANGULO_MAX));
}

void parar() {
  andando = false;
  joystickX = joystickY = 0;
  setAll(CENTRO, CENTRO, CENTRO, CENTRO);
}

void emPe() {
  parar();
}

void sentar() {
  andando = false;
  joystickX = joystickY = 0;
  setAll(CENTRO, CENTRO, 140, 40);
}

void deitar() {
  andando = false;
  joystickX = joystickY = 0;
  setAll(50, 130, 130, 50);
}

void atualizarMovimento() {
  unsigned long agora = millis();

  if (andando && (agora - ultimoComando > TIMEOUT ||
                   WiFi.status() != WL_CONNECTED)) {
    parar();
  }

  if (agora - ultimoPasso < 20) return;
  float dt = (agora - ultimoPasso) / 1000.0f;
  ultimoPasso = agora;
  if (!andando) return;

  float intensidade = fmaxf(fabsf(joystickX), fabsf(joystickY));
  fase = fmodf(fase + dt * (2.0f + 5.0f * intensidade), 6.2831853f);

  float esquerda = constrain(joystickY + joystickX, -1.0f, 1.0f);
  float direita = constrain(joystickY - joystickX, -1.0f, 1.0f);
  float amplitude = (TRAS - FRENTE) / 2.0f;
  float passo = amplitude * sinf(fase);

  setAll(
    CENTRO + int(esquerda * passo),
    CENTRO - int(direita * passo),
    CENTRO - int(esquerda * passo),
    CENTRO + int(direita * passo)
  );
}

// ================= PAGINA HTML =================
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="pt-BR">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Cachorro Robo</title>
<style>
@import url('https://googleapis.com');
*{box-sizing:border-box}
body{margin:0;padding:24px;background:#f3f2eb;color:#263c30;
font-family:'DM Sans',sans-serif;text-align:center}
main{max-width:440px;margin:0 auto}
h1{font-family:'Space Grotesk',sans-serif;font-size:34px;
letter-spacing:-1px;margin-top:30px}
p{color:#617160;line-height:1.6}
#area{width:260px;height:260px;margin:32px auto;
border-radius:50%;position:relative;touch-action:none;
border:1px solid #bdc9b5;cursor:grab;
background:repeating-radial-gradient(circle,transparent 0 42px,
#cad4bf 43px 44px),#e7ecdf}
#controle{width:82px;height:82px;position:absolute;
left:89px;top:89px;background:#263c30;border:6px solid white;
border-radius:50%;pointer-events:none;box-shadow:0 6px 14px #263c3025}
.botoes{display:flex;gap:8px}
button{font:inherit;padding:14px 10px;border:1px solid #c9d2c1;
border-radius:6px;background:transparent;color:#263c30;
cursor:pointer;flex:1;transition:transform .15s,background .15s}
button:hover{background:#e0e7d5}
button:active{transform:scale(.96)}
button:focus-visible,#area:focus-visible{outline:3px solid #6b8b3c;outline-offset:4px}
#parar{width:100%;background:#d6ee81;border:0;margin-top:12px;
font-weight:600}
#status{min-height:44px;font-size:14px;margin-top:22px}
@keyframes entrada{from{opacity:0;transform:translateY(10px)}
to{opacity:1;transform:none}}
main{animation:entrada .5s both}
@media(prefers-reduced-motion:reduce){main{animation:none}}
</style>
</head>
<body>
<main>
<h1>Cachorro robo</h1>
<p>Arraste para andar e virar. Mantenha pressionado para continuar. Solte para parar.</p>
<div id="area" tabindex="0" role="group" aria-label="Joystick: arraste ou use as setas do teclado">
<div id="controle"></div></div>
<div class="botoes">
<button data-acao="sentar">Sentar</button>
<button data-acao="deitar">Deitar</button>
<button data-acao="empe">Em pe</button>
</div>
<button id="parar">PARAR</button>
<p id="status" role="status">Pronto para brincar!</p>
</main>
<script>
const area=document.getElementById('area');
const controle=document.getElementById('controle');
const statusTexto=document.getElementById('status');
let x=0,y=0,pointer=null,acao='parar';
let revisao=0,enviada=-1,ocupado=false;

function desenhar(){
 controle.style.transform='translate('+(x*85)+'px,'+(-y*85)+'px)';
}

function pararMover(){
 pointer=null;x=y=0;acao='parar';revisao++;
 desenhar();enviar();
}

function mover(e){
 const r=area.getBoundingClientRect();
 let clientX = e.touches ? e.touches.clientX : e.clientX;
 let clientY = e.touches ? e.touches.clientY : e.clientY;
 let a=(clientX-r.left-r.width/2)/85;
 let b=-(clientY-r.top-r.height/2)/85;
 const n=Math.hypot(a,b);
 if(n>1){a/=n;b/=n;}
 x=a;y=b;
 acao=Math.hypot(x,y)>.08?'joy':'parar';
 revisao++;desenhar();enviar();
}

area.addEventListener('mousedown', (e)=>{pointer=e.pointerId;mover(e);});
window.addEventListener('mousemove', (e)=>{if(pointer!==null)mover(e);});
window.addEventListener('mouseup', pararMover);

area.addEventListener('touchstart', (e)=>{pointer=1;mover(e);});
window.addEventListener('touchmove', (e)=>{if(pointer!==null)mover(e);});
window.addEventListener('touchend', pararMover);

document.querySelectorAll('.botoes button').forEach(b=>{
 b.addEventListener('click', ()=>{acao=b.getAttribute('data-acao');revisao++;enviar();});
});
document.getElementById('parar').addEventListener('click', pararMover);

async function enviar(){
 if(ocupado)return;
 if(acao!=='joy' && enviada===revisao)return;
 ocupado=true;
 const rev=revisao,cmd=acao;
 const url=cmd==='joy'
 ? '/joy?x='+x.toFixed(3)+'&y='+y.toFixed(3)
 : '/cmd?acao='+cmd;
 const abort=new AbortController();
 const timer=setTimeout(()=>abort.abort(),550);
 try{
  const r=await fetch(url,{signal:abort.signal,cache:'no-store'});
  if(!r.ok) throw new Error();
  enviada=rev;
 } catch {
  if(cmd==='joy') statusTexto.innerText='Erro de conexao';
 } finally {
  clearTimeout(timer);
  ocupado=false;
  if(revisao!==rev) enviar();
 }
}
</script>
</body>
</html>
)rawliteral";

// ================= SERVIDOR E ROTAS HTTP =================
void handleRoot() {
  server.send(200, "text/html", index_html);
}

void handleJoystick() {
  if (server.hasArg("x") && server.hasArg("y")) {
    joystickX = server.arg("x").toFloat();
    joystickY = server.arg("y").toFloat();
    andando = true;
    ultimoComando = millis();
    server.send(200, "text/plain", "OK");
  } else {
    server.send(400, "text/plain", "Dados Invalidos");
  }
}

void handleCommand() {
  if (server.hasArg("acao")) {
    String acao = server.arg("acao");
    if (acao == "sentar") sentar();
    else if (acao == "deitar") deitar();
    else if (acao == "empe") emPe();
    else parar();
    server.send(200, "text/plain", "OK");
  } else {
    server.send(400, "text/plain", "Comando Ausente");
  }
}

// ================= SETUP E LOOP PADRAO =================
void setup() {
  Serial.begin(115200);

  // Alocacao de timers obrigatorios para o ESP32Servo
  ESP32PWM::allocateTimer(0);
  ESP32PWM::allocateTimer(1);
  ESP32PWM::allocateTimer(2);
  ESP32PWM::allocateTimer(3);

  FL_servo.setPeriodHertz(50);
  FR_servo.setPeriodHertz(50);
  BL_servo.setPeriodHertz(50);
  BR_servo.setPeriodHertz(50);

  FL_servo.attach(pinoFL, 500, 2400);
  FR_servo.attach(pinoFR, 500, 2400);
  BL_servo.attach(pinoBL, 500, 2400);
  BR_servo.attach(pinoBR, 500, 2400);

  parar();

  WiFi.begin(ssid, password);
  Serial.print("Conectando ao WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nConectado!");
  Serial.print("IP do Robo: http://");
  Serial.println(WiFi.localIP());

  server.on("/", handleRoot);
  server.on("/joy", handleJoystick);
  server.on("/cmd", handleCommand);
  server.onNotFound(handleRoot); // Redireciona qualquer erro de link para a página principal
  server.begin();
}

void loop() {
  server.handleClient();
  atualizarMovimento();
}
