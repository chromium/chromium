// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function logOutput(s) {
  if (window.domAutomationController)
    window.domAutomationController.log(s);
  else
    console.log(s);
}

function fourColorsFrame(ctx) {
  const width = ctx.canvas.width;
  const height = ctx.canvas.height;
  const kYellow = '#FFFF00';
  const kRed = '#FF0000';
  const kBlue = '#0000FF';
  const kGreen = '#00FF00';

  ctx.fillStyle = kYellow;
  ctx.fillRect(0, 0, width / 2, height / 2);

  ctx.fillStyle = kRed;
  ctx.fillRect(width / 2, 0, width / 2, height / 2);

  ctx.fillStyle = kBlue;
  ctx.fillRect(0, height / 2, width / 2, height / 2);

  ctx.fillStyle = kGreen;
  ctx.fillRect(width / 2, height / 2, width / 2, height / 2);
}

function waitForNextFrame() {
  return new Promise((resolve, _) => {
    window.requestAnimationFrame(resolve);
  });
}

function initGL(gl) {
  const fragment_shader = `
      void main(void) {
        gl_FragColor = vec4(1.0, 1.0, 0.0, 1.0);
      }
  `;
  const vertex_shader = `
      attribute vec3 c;
      void main(void) {
        gl_Position = vec4(c, 1.0);
      }
  `;

  gl.viewport(0, 0, gl.canvas.width, gl.canvas.height);

  const vs = gl.createShader(gl.VERTEX_SHADER);
  gl.shaderSource(vs, vertex_shader);
  gl.compileShader(vs);

  const fs = gl.createShader(gl.FRAGMENT_SHADER);
  gl.shaderSource(fs, fragment_shader);
  gl.compileShader(fs);

  gl.getShaderParameter(fs, gl.COMPILE_STATUS);

  const program = gl.createProgram();
  gl.attachShader(program, vs);
  gl.attachShader(program, fs);
  gl.linkProgram(program);
  gl.useProgram(program);

  const vb = gl.createBuffer();
  gl.bindBuffer(gl.ARRAY_BUFFER, vb);
  gl.bufferData(
      gl.ARRAY_BUFFER, new Float32Array([
        -1.0, -1.0, 0.0, 1.0, -1.0, 0.0, -1.0, 0.0, 0.0, -1.0, 0.0, 0.0, 1.0,
        0.0, 0.0, 1.0, -1.0, 0.0
      ]),
      gl.STATIC_DRAW);

  const coordLoc = gl.getAttribLocation(program, 'c');
  gl.vertexAttribPointer(coordLoc, 3, gl.FLOAT, false, 0, 0);
  gl.enableVertexAttribArray(coordLoc);
}

function triangleOnFrame(gl) {
  gl.clearColor(0.0, 0.0, 1, 1);
  gl.clear(gl.COLOR_BUFFER_BIT);

  gl.drawArrays(gl.TRIANGLE_FAN, 0, 6);
  gl.finish();
}

function setupFrameCallback(video) {
  logOutput(`Expecting frames now`);
  let vf_counter = 5;
  function videoFrameCallback(now, md) {
    logOutput(`Got a video frame: ${now}`);
    if (vf_counter > 0) {
      vf_counter--;
      video.requestVideoFrameCallback(videoFrameCallback);
      return;
    }

    logOutput('Test complete.');
    domAutomationController.send('SUCCESS');
  }
  video.requestVideoFrameCallback(videoFrameCallback);
}

async function runTest(context_type, alpha) {
  const canvas = document.getElementById('cnv');
  const video = document.getElementById('vid');
  const ctx = canvas.getContext(context_type, {alpha: alpha});
  const fps = 60;

  let draw = null;
  if (context_type == 'webgl2') {
    initGL(ctx);
    draw = triangleOnFrame;
  } else {
    draw = fourColorsFrame;
  }

  draw(ctx);
  let stream = canvas.captureStream(fps);
  video.srcObject = stream;
  video.onerror = e => {
    logOutput(`Test failed: ${e.message}`);
    domAutomationController.send('FAIL');
  };
  setupFrameCallback(video);
  video.play();

  while (true) {
    await waitForNextFrame();
    draw(ctx);
  }
}
