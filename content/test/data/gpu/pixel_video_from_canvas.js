// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function logOutput(s) {
  if (window.domAutomationController)
    window.domAutomationController.log(s);
  else
    console.log(s);
}

function sendResult(status) {
  if (window.domAutomationController) {
    window.domAutomationController.send(status);
  } else {
    console.log(status);
  }
}

// This is a very basic test for 4 corner pixels, with large tolerance.
function checkFourColorsFrame(video) {
  const width = video.videoWidth;
  const height = video.videoHeight;
  let test_cnv = new OffscreenCanvas(width, height);
  let ctx = test_cnv.getContext('2d');
  ctx.drawImage(video, 0, 0, width, height);

  const kYellow = [0xFF, 0xFF, 0x00, 0xFF];
  const kRed = [0xFF, 0x00, 0x00, 0xFF];
  const kBlue = [0x00, 0x00, 0xFF, 0xFF];
  const kGreen = [0x00, 0xFF, 0x00, 0xFF];

  function checkPixel(x, y, expected_rgba) {
    const settings = {colorSpaceConversion: 'none'};
    const rgba = ctx.getImageData(x, y, 1, 1, settings).data;
    const tolerance = 30;
    for (let i = 0; i < 4; i++) {
      if (Math.abs(rgba[i] - expected_rgba[i]) > tolerance) {
        return false;
      }
    }
    return true;
  }

  let m = 10;  // margin from the frame's edge
  if (!checkPixel(m, m, kYellow)) {
    logOutput('top left corner is not yellow');
    return false;
  }
  if (!checkPixel(width - m, m, kRed)) {
    logOutput('top right corner is not red');
    return false;
  }
  if (!checkPixel(m, height - m, kBlue)) {
    logOutput('bottom left corner is not blue');
    return false;
  }
  if (!checkPixel(width - m, height - m, kGreen)) {
    logOutput('bottom right corner is not green');
    return false;
  }
  return true;
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
  gl.viewport(0, 0, gl.canvas.width, gl.canvas.height);
  gl.enable(gl.SCISSOR_TEST);
}

function fourColorsFrameGL(gl) {
  const width = gl.canvas.width;
  const height = gl.canvas.height;
  gl.scissor(0, 0, width, height);
  gl.clearColor(0, 0, 0, 1);
  gl.clear(gl.COLOR_BUFFER_BIT);

  gl.scissor(width / 2, 0, width, height / 2);
  gl.clearColor(0, 1, 0, 1);
  gl.clear(gl.COLOR_BUFFER_BIT);

  gl.scissor(width / 2, height / 2, width, height);
  gl.clearColor(1, 0, 0, 1);
  gl.clear(gl.COLOR_BUFFER_BIT);

  gl.scissor(0, 0, width / 2, height / 2);
  gl.clearColor(0, 0, 1, 1);
  gl.clear(gl.COLOR_BUFFER_BIT);

  gl.scissor(0, height / 2, width / 2, height);
  gl.clearColor(1, 1, 0, 1);
  gl.clear(gl.COLOR_BUFFER_BIT);

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

    if (checkFourColorsFrame(video)) {
      logOutput('Test completed');
      sendResult('SUCCESS');
    } else {
      logOutput('Test failed. Result mismatch.');
      sendResult('FAILED');
    }
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
    draw = fourColorsFrameGL;
  } else {
    draw = fourColorsFrame;
  }

  draw(ctx);
  let stream = canvas.captureStream(fps);
  video.srcObject = stream;
  video.onerror = e => {
    logOutput(`Test failed: ${e.message}`);
    sendResult('FAIL');
  };
  setupFrameCallback(video);
  video.play();

  while (true) {
    await waitForNextFrame();
    draw(ctx);
  }
}
