// Copyright 2019 The Chromium Authors
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

function logOutput(s) {
  if (window.domAutomationController)
    window.domAutomationController.log(s);
  else
    console.log(s);
}

function sendResult(status, detail) {
  logOutput(status + ' ' + detail);
  if (window.domAutomationController) {
    window.domAutomationController.send(status);
  }
}

function initGL(canvas, opt_attribs)
{
  try {
    let attribs = Object.assign({ powerPreference: "low-power" },
                                opt_attribs || {});
    gl = canvas.getContext("webgl", attribs);
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

function setup(opt_attribs)
{
  let canvas = document.getElementById("c");
  initGL(canvas, opt_attribs);
  if (gl && setupGL(gl))
    return true;
  if (window.domAutomationController)
    window.domAutomationController.send('FAILURE');
  return false;
}

function drawSomeFrames(callback)
{
  let swapsBeforeCallback = 60;

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

let _runningOnDualGPUSystem = false;

function setRunningOnDualGpuSystem() {
  _runningOnDualGPUSystem = true;
}

function isRunningOnDualGpuSystem() {
  return _runningOnDualGPUSystem;
}

function getUnmaskedVendor() {
  let ext = gl.getExtension('WEBGL_debug_renderer_info');
  let renderer = gl.getParameter(ext.UNMASKED_RENDERER_WEBGL);
  if (renderer.startsWith('ANGLE'))
    return renderer;
  return gl.getParameter(ext.UNMASKED_VENDOR_WEBGL);
}

function getSplitUnmaskedVendor() {
  let vendor = getUnmaskedVendor().toLowerCase();
  // Like:
  //  Intel Inc.
  //  ATI Technologies Inc.
  // Renderer would be like:
  //  Intel(R) HD Graphics 630
  //  AMD Radeon Pro 560 OpenGL Engine
  // Handle parentheses just in case.
  return vendor.split(/[ ()]/);
}

function assertRunningOnLowPowerGpu() {
  if (!isRunningOnDualGpuSystem())
    return false;
  let tokens = getSplitUnmaskedVendor();
  if (tokens.includes('intel')) {
    logOutput('System was correctly running on Intel integrated GPU');
    return true;
  }
  sendResult(
      'FAIL',
      'System wasn\'t running on Intel integrated GPU: vendor = ' +
          getUnmaskedVendor());
  return false;
}

function assertRunningOnHighPerformanceGpu() {
  if (!isRunningOnDualGpuSystem())
    return false;
  let tokens = getSplitUnmaskedVendor();
  if (tokens.includes('ati') || tokens.includes('amd') ||
      tokens.includes('nvidia')) {
    logOutput(
        'System was correctly running on discrete GPU: ' + getUnmaskedVendor());
    return true;
  }
  sendResult(
      'FAIL',
      'System wasn\'t running on discrete GPU: vendor = ' +
          getUnmaskedVendor());
  return false;
}
