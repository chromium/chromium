// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const vertexShader = [
  "attribute vec3 pos;",
  "void main(void)",
  "{",
  "  gl_Position = vec4(pos, 1.0);",
  "}"
].join("\n");

const fragmentShader = [
  "precision mediump float;",
  "void main(void)",
  "{",
  "  gl_FragColor = vec4(0.0, 1.0, 0.0, 1.0);",
  "}"
].join("\n");

let gl;

function sendResult(status, detail) {
  console.log(detail);
  if (window.domAutomationController) {
    window.domAutomationController.send(status);
  } else {
    console.log(status);
  }
}

function initGL(canvas)
{
  try {
    gl = canvas.getContext("webgl", { powerPreference: "low-power" });
  } catch (e) {}
  return gl;
}

function setupShader(source, type) {
  var shader = gl.createShader(type);
  gl.shaderSource(shader, source);
  gl.compileShader(shader);
  if (!gl.getShaderParameter(shader, gl.COMPILE_STATUS))
    return null;
  return shader;
}

function setupProgram(vs_id, fs_id) {
  var vs = setupShader(vertexShader, gl.VERTEX_SHADER);
  var fs = setupShader(fragmentShader, gl.FRAGMENT_SHADER);
  if (!vs || !fs)
    return null;
  var program = gl.createProgram();
  gl.attachShader(program, vs);
  gl.attachShader(program, fs);
  gl.linkProgram(program);
  if (!gl.getProgramParameter(program, gl.LINK_STATUS))
    return null;
  gl.useProgram(program);
  return program;
}

function setupBuffer(gl) {
  var buffer = gl.createBuffer();
  gl.bindBuffer(gl.ARRAY_BUFFER, buffer);
  var vertexData = [
    // Triangle 1
    -1.0, -1.0, 0.0,
    1.0, 1.0, 0.0,
    -1.0, 1.0, 0.0,

    // Triangle 2
    -1.0, -1.0, 0.0,
    1.0, -1.0, 0.0,
    1.0, 1.0, 0.0
  ];
  gl.bufferData(gl.ARRAY_BUFFER, new Float32Array(vertexData), gl.STATIC_DRAW);
}

function setupGL() {
  var program = setupProgram("shader-vs", "shader-fs");
  if (!program)
    return false;
  var posAttr = gl.getAttribLocation(program, "pos");
  gl.enableVertexAttribArray(posAttr);
  setupBuffer(gl);
  var stride = 3 * Float32Array.BYTES_PER_ELEMENT;
  gl.vertexAttribPointer(posAttr, 3, gl.FLOAT, false, stride, 0);
  gl.clearColor(0.0, 0.0, 0.0, 0.0);
  gl.viewport(0, 0, 300, 300);
  gl.disable(gl.DEPTH_TEST);
  if (gl.getError() != gl.NO_ERROR)
    return false;
  return true;
}

function drawQuad() {
  gl.clear(gl.COLOR_BUFFER_BIT | gl.DEPTH_BUFFER_BIT);
  gl.drawArrays(gl.TRIANGLES, 0, 6);
}

function setup()
{
  let canvas = document.getElementById("c");
  initGL(canvas);
  if (gl && setupGL(gl))
    return true;
  domAutomationController.send("FAILURE");
  return false;
}

function drawSomeFrames(callback)
{
  let swapsBeforeCallback = 15;

  function drawSomeFramesHelper() {
    if (--swapsBeforeCallback == 0) {
      callback();
    } else {
      drawQuad();
      window.requestAnimationFrame(drawSomeFramesHelper);
    }
  }

  window.requestAnimationFrame(drawSomeFramesHelper);
}
