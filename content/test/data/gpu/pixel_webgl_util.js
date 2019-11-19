// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var vertexShader = [
  "attribute vec3 pos;",
  "void main(void)",
  "{",
  "  gl_Position = vec4(pos, 1.0);",
  "}"
].join("\n");

var fragmentShader = [
  "precision mediump float;",
  "void main(void)",
  "{",
  "  gl_FragColor = vec4(0.0, 1.0, 0.0, 1.0);",
  "}"
].join("\n");

// TODO: We should test premultiplyAlpha as well.
function initGL(canvas, antialias, alpha)
{
  var gl = null;
  try {
    gl = canvas.getContext("experimental-webgl",
                           {"alpha": alpha, "antialias":antialias});
  } catch (e) {}
  if (!gl) {
    try {
      gl = canvas.getContext("webgl");
    } catch (e) { }
  }
  return gl;
}

function setupShader(gl, source, type) {
  var shader = gl.createShader(type);
  gl.shaderSource(shader, source);
  gl.compileShader(shader);
  if (!gl.getShaderParameter(shader, gl.COMPILE_STATUS))
    return null;
  return shader;
}

function setupProgram(gl, vs_id, fs_id) {
  var vs = setupShader(gl, vertexShader, gl.VERTEX_SHADER);
  var fs = setupShader(gl, fragmentShader, gl.FRAGMENT_SHADER);
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
    0.0, 0.6, 0.0,  // Vertex 1 position
    -0.6, -0.6, 0.0,  // Vertex 2 position
    0.6, -0.6, 0.0,  // Vertex 3 position
  ];
  gl.bufferData(gl.ARRAY_BUFFER, new Float32Array(vertexData), gl.STATIC_DRAW);
}

function setup(gl) {
  var program = setupProgram(gl, "shader-vs", "shader-fs");
  if (!program)
    return false;
  var posAttr = gl.getAttribLocation(program, "pos");
  gl.enableVertexAttribArray(posAttr);
  setupBuffer(gl);
  var stride = 3 * Float32Array.BYTES_PER_ELEMENT;
  gl.vertexAttribPointer(posAttr, 3, gl.FLOAT, false, stride, 0);
  gl.clearColor(0.0, 0.0, 0.0, 0.0);
  gl.viewport(0, 0, 200, 200);
  gl.disable(gl.DEPTH_TEST);
  if (gl.getError() != gl.NO_ERROR)
    return false;
  return true;
}

function drawTriangle(gl) {
  gl.clear(gl.COLOR_BUFFER_BIT | gl.DEPTH_BUFFER_BIT);
  gl.drawArrays(gl.TRIANGLES, 0, 3);
}

var g_swapsBeforeAckUtil = 15;
var g_glUtil;

function makeMain(antialias, alpha)
{
  return function() {
    var canvas = document.getElementById("c");
    g_glUtil = initGL(canvas, antialias, alpha);
    if (g_glUtil && setup(g_glUtil)) {
      drawSomeFramesUtil();
    } else {
      domAutomationController.send("FAILURE");
    }
  };
}

function drawSomeFramesUtil()
{
  if (g_swapsBeforeAckUtil == 0) {
    domAutomationController.send("SUCCESS");
  } else {
    g_swapsBeforeAckUtil--;
    drawTriangle(g_glUtil);
    window.requestAnimationFrame(drawSomeFramesUtil);
  }
}
