/*
 * Copyright 2013 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

NaClTerm.prefix = 'lua'
NaClTerm.nmf = '//storage.googleapis.com/gonacl/demos/publish/234230_dev/lua/lua.nmf'

window.onload = function() {
  lib.init(function() {
    document.getElementById('shell').addEventListener('click', runLua, false);
    document.getElementById('scimark').addEventListener('click', runSciMark, false);
    document.getElementById('unittests').addEventListener('click', runUnitTests, false);
    document.getElementById('binarytrees').addEventListener('click', runBinaryTrees, false);
    document.getElementById('mandlebrot').addEventListener('click', runMandlebrot, false);
    document.getElementById('donut').addEventListener('click', runDonut, false);
    document.getElementById('fibonacci').addEventListener('click', runFibonacci, false);
    document.getElementById('banner').addEventListener('click', runBanner, false);
    NaClTerm.init();
  });
};

function runLua() {
  NaClTerm.argv = [];
  term_.command.restartNaCl();
}

function runUnitTests() {
  NaClTerm.argv = ['-e_U=true', 'all.lua']
  term_.command.restartNaCl();
  return false;
}

function runSciMark(e) {
  NaClTerm.argv = ['/mnt/http/scimark.lua']
  term_.command.restartNaCl();
  return false;
}

function runBinaryTrees(e) {
  NaClTerm.argv = ['/mnt/http/binarytrees.lua']
  term_.command.restartNaCl();
  return false;
}

function runMandlebrot(e) {
  if (!term_.command.loaded)
     runLua();

  term_.command.sendMessage('for i=-1,1,.08 do for r=-2,1,.04 do I=i R=r n=0 b=I*I while n<26 and R*R+b<4 do I=2*R*I+i R=R*R-b+r b=I*I n=n+1 end io.write(string.char(n+32)) end print() end')

  document.getElementById('terminal').focus();
}

function runFibonacci(e) {
  if (!term_.command.loaded)
     runLua();

  term_.command.sendMessage('function fib(n) return n<2 and n or fib(n-1)+fib(n-2) end\nprint(fib(10))\nprint(fib(30))\n')

  document.getElementById('terminal').focus();
}

function runBanner(e) {
  if (!term_.command.loaded)
     runLua();

  term_.command.sendMessage(
't=0 A={64,36,35,42,33,61,59,58,126,45,44,46,32}L={0,254,195,3,224,135,231,127, \
124,60,128,255,120,158,207,199,241,252,192,121,30,31,61,227,1,223,243,188,63,251 \
,190,193,28,156,62,248,143,7,126}D={2,3,3,4,1,5,6,7,8,9,10,1,11,12,13,14,15,16,4 \
,1,10,14,7,17,18,10,1,19,3,20,21,15,15,3,22,21,11,7,17,18,23,2,24,25,13,14,15,26 \
,27,13,21,11,7,8,28,29,19,7,25,13,2,3,30,3,8,21,11,7,25,10,29,31,7,32,13,21,19, \
27,27,13,33,34,7,25,10,35,15,16,24,20,21,19,24,27,8,36,37,7,25,10,10,31,38,39,13 \
}B=bit32 M=math I=io.write T=string.char P=print S=M.sin C=M.cos F=M.floor b={} \
for i=1,1200 do b[i]=0 end for i,v in ipairs(D)do w=L[v]for j=1,8 do if B.band(w \
,1)==1 then b[160-48+i*8+j]=1 end w=B.rshift(w,1)end end P("\\x1b[2J") for w=1, \
4720 do P("\\x1b[H")s=1.06*S(t*.07)^2 for i=1,1200 do x=s*((i%60)-34)+34 y=s*(F(i \
/60)-15)+15 n=32 if y>=0 and y<20 and x>=0 and x<60 and b[F(y)*60+F(x)]==1 then \
n=A[M.min(F(((x-30+C(t)*20)^2+(y-S(t)^2*10)^2)/(195*S(t*.2)^32+5)),12)+1]end if \
i%60==0 then n=10 end I(T(n))end t=t+.005 end');

  document.getElementById('terminal').focus();
}

function runDonut(e) {
  if (!term_.command.loaded)
     runLua();

  term_.command.sendMessage(
'            A=0 B=0 z={}b=\n\
         {}E={32,46,44,45,126,\n\
       58,59,61,33,42,35,36,64}S\n\
     =math.sin C=math.cos F=math.\n\
  floor I=io.write T=string.char W=60\n\
  P=print H=25 P("\\x1b[2J")for w=1,240\n\
 do for o=0,W*H do b[o]=1 z[o]=0 end e=\n\
 S(A)g=C(A)m=C(B)n=S(B)for j=0,6.28,.09\n\
 do d=C(j)f=S(j)       for i=0,6.28,.04\n\
do c=S(i)h=d+2 D        =1/(c*h*e+f*g+5)\n\
l=C(i)t=c*h*g-            f*e x=F(W/2+W*\n\
.3*D*(l*h*m-t*n          ))y=F(H/2+H*.6*\n\
 D*(l*h*n+t*m))o        =x+W*y  N=math.\n\
 max(0,F(8*((f*e-c*d*g)*m-c*d*e-f*g -l*\n\
 d *n)))+2 if H> y and y>0 and x>0 and\n\
  W>x and D> z[o] then  z[o]=D b[o]=N\n\
   end end  end P( "\\x1b[H")for k=0\n\
     ,W* H do if k%W~=0 then I(T(\n\
       E[b[k]]))else I( T( 10))\n\
         end end A = A + .04\n\
             B=B+.02 end')

  document.getElementById('terminal').focus();
}
