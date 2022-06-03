// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let oc;
let gl;
const sz = 256;

let canvas;
let ibrc;

let workerMode = false;
let worker;

//---------------------------------------------------------------------------
// Functions to be called either on the main thread or worker

function setupOffscreenCanvas(highPerformance) {
  oc = new OffscreenCanvas(sz, sz);
  let attribs = {};
  if (highPerformance)
    attribs['powerPreference'] = 'high-performance';
  gl = oc.getContext('webgl', attribs);
}

function doRender() {
  gl.clearColor(0.0, 1.0, 0.0, 1.0);
  gl.clear(gl.COLOR_BUFFER_BIT);
  let ib = oc.transferToImageBitmap();
  if (workerMode) {
    self.postMessage({frame: ib}, [ib]);
  } else {
    ibrc.transferFromImageBitmap(ib);
  }
}

//---------------------------------------------------------------------------
// Functions to be called only on the main thread

function initialize(useWorker, selfURL) {
  canvas = document.createElement('canvas');
  canvas.style = 'position: absolute; top: 0px; left: 0px;';
  canvas.width = 256;
  canvas.height = 256;
  document.body.appendChild(canvas);
  ibrc = canvas.getContext('bitmaprenderer');

  workerMode = useWorker;
  if (workerMode) {
    worker = new Worker(selfURL);
    worker.onmessage = function(e) {
      ibrc.transferFromImageBitmap(e.data.frame);
      waitForFinish();
    }
  }

  sendResult("READY");
}

function setup(highPerformance) {
  if (workerMode) {
    worker.postMessage({command: 'setup', highPerformance: highPerformance});
  } else {
    setupOffscreenCanvas(highPerformance);
  }
}

function render() {
  if (workerMode) {
    worker.postMessage({command: 'render'});
  } else {
    doRender();
    waitForFinish();
  }
}

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

function waitForFinish()
{
  let numFramesBeforeEnd = 15;

  function waitForFinishImpl() {
    if (--numFramesBeforeEnd == 0) {
      sendResult("SUCCESS", "Test complete");
    } else {
      window.requestAnimationFrame(waitForFinishImpl);
    }
  }

  window.requestAnimationFrame(waitForFinishImpl);
}

//---------------------------------------------------------------------------
// Code to be executed only on the worker

if (this.document === undefined) {
  workerMode = true;
  // We're on a worker - set up postMessage handler.
  self.onmessage = function(e) {
    switch (e.data.command) {
    case 'setup':
      setupOffscreenCanvas(e.data.highPerformance);
      break;
    case 'render':
      doRender();
      break;
    }
  }
}
