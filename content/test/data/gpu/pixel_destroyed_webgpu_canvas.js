// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const parsedString = new URLSearchParams(window.location.search);
const canvas_string = parsedString.get('canvas');
const method_string = parsedString.get('method');

function is_onscreen() {
  return canvas_string == 'onscreen';
}

function is_offscreen() {
  return canvas_string == 'offscreen';
}

function is_transfer_to_offscreen() {
  return canvas_string == 'transferToOffscreen';
}

function is_drawImage_method() {
  return method_string == 'drawImage';
}

function is_blob_method() {
  return method_string == 'toBlob';
}

function is_toDataURL_method() {
  return method_string == 'toDataURL';
}

function is_transfter_to_image_bitmap_method() {
  return method_string == 'transferToImageBitmap';
}

function is_copy_external_image_to_texture_method() {
  return method_string == 'copyExternalImageToTexture';
}

function is_gl_texImage2d_method() {
  return method_string == 'glTexImage2D';
}

function is_capture_stream_method() {
  return method_string == 'captureStream';
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

function waitForFinish() {
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

function createGpuCanvas(divId) {
  if (is_onscreen() || is_transfer_to_offscreen()) {
    const div = document.getElementById(divId);
    div.insertAdjacentHTML('afterbegin', `
      <canvas id="${divId}_gpu" width="100" height="100" class="nomargin"
        style="background-color: lightgray;"></canvas>`);
    const gpuElement = document.getElementById(divId + "_" + "gpu");
    if (is_transfer_to_offscreen()) {
      return gpuElement.transferControlToOffscreen();
    } else {
      return gpuElement;
    }
  } else if (is_offscreen()) {
    return new OffscreenCanvas(100, 100);
  }
}

function createTargetCanvas(divId) {
  const div = document.getElementById(divId);
  div.insertAdjacentHTML('beforeend', `
    <canvas id="${divId}_destroyed" width="100" height="100" class="nomargin"
        style="background-color: gray;"></canvas>
    <canvas id="${divId}_lost" width="100" height="100" class="nomargin"
        style="background-color: gray;"></canvas>`);
}

function createTargetImg(divId) {
  const div = document.getElementById(divId);
  div.insertAdjacentHTML('beforeend', `
    <img id="${divId}_destroyed" width="100" height="100" class="nomargin"
      style="background-color: gray;"></img>
    <img id="${divId}_lost" width="100" height="100" class="nomargin"
      style="background-color: gray;"></img>`);
}

function createTargetVideo(divId) {
  const div = document.getElementById(divId);
  div.insertAdjacentHTML('beforeend', `
    <video id="${divId}_destroyed" width="100" height="100" class="nomargin"
      playsinline autoplay muted style="background-color: gray;"></video>`);
}

function createTargetNodes(divId) {
  if (is_drawImage_method()) {
    createTargetCanvas(divId);
  } else if (is_blob_method()) {
    createTargetImg(divId);
  } else if (is_toDataURL_method()) {
    createTargetImg(divId);
  } else if (is_transfter_to_image_bitmap_method()) {
    createTargetCanvas(divId);
  } else if (is_copy_external_image_to_texture_method()) {
    createTargetCanvas(divId);
  } else if (is_gl_texImage2d_method()) {
    createTargetCanvas(divId);
  } else if (is_capture_stream_method()) {
    createTargetVideo(divId);
  } else {
    sendResult("FAILURE", "unknown method");
  }
}

function drawImage(canvas, divId, targetId) {
  const targetNode = document.getElementById(divId + "_" + targetId);
  const ctx = targetNode.getContext('2d');
  if (!ctx) {
    sendResult("FAILURE", "getContext(2d) failed");
    return;
  }

  ctx.drawImage(canvas, 0, 0);
}

function toBlob(canvas, divId, targetId) {
  if (is_onscreen()) {
    canvas.toBlob((blob) => {
      const url = URL.createObjectURL(blob);
      const image = document.getElementById(divId + "_" + targetId);
      image.src = url;
    });
  } else {
    canvas.convertToBlob().then((blob) => {
      const url = URL.createObjectURL(blob);
      const image = document.getElementById(divId + "_" + targetId);
      image.src = url;
    });
  }
}

function toDataURL(canvas, divId, targetId) {
  const image = document.getElementById(divId + "_" + targetId);
  image.src = canvas.toDataURL();
}

function transferToImageBitmap(canvas, divId, targetId) {
  const targetNode = document.getElementById(divId + "_" + targetId);
  const ctx = targetNode.getContext('2d');
  if (!ctx) {
    sendResult("FAILURE", "getContext(2d) failed");
    return;
  }

  const imageBitmap = canvas.transferToImageBitmap();
  ctx.drawImage(imageBitmap, 0, 0);
}

async function copyExternalImageToTexture(canvas, divId, targetId) {
  const gpuCanvas = document.getElementById(divId + "_" + targetId);
  const [gpuDevice, gpuContext] = await webGpuUtils.init(gpuCanvas);
  if (!gpuDevice || !gpuContext) {
    sendResult("FAILURE", "Failed to initialize WebGPU");
    return;
  }

  webGpuUtils.uploadToGPUTextureTest(gpuDevice, gpuContext, canvas);
}

function glTexImage2D(canvas, divId, targetId) {
  const glCanvas = document.getElementById(divId + "_" + targetId);
  const gl = glCanvas.getContext('webgl2', { antialias: false });
  if (!gl) {
    sendResult("FAILURE", "getContext(webgl) failed");
    return;
  }

  const tex = gl.createTexture();
  gl.bindTexture(gl.TEXTURE_2D, tex);
  gl.texImage2D(gl.TEXTURE_2D, 0, gl.RGBA, gl.RGBA, gl.UNSIGNED_BYTE, canvas);

  const fbo = gl.createFramebuffer();
  gl.bindFramebuffer(gl.READ_FRAMEBUFFER, fbo);
  gl.framebufferTexture2D(
    gl.READ_FRAMEBUFFER, gl.COLOR_ATTACHMENT0, gl.TEXTURE_2D, tex, 0);

  gl.blitFramebuffer(0, 0, canvas.width, canvas.height, 0, 0,
    glCanvas.width, glCanvas.height, gl.COLOR_BUFFER_BIT,
    gl.LINEAR);

  gl.deleteFramebuffer(fbo);
  gl.deleteTexture(tex);
}

function captureStream(canvas, divId, targetId) {
  if (targetId == 'lost') {
    // When requests a new video frame, it will wait for the canvas to
    // be re-drawn. After re-draw, it will copy the image to the video.
    // But after the device is lost, the canvas will not be re-drawn.
    return;
  }

  const gpuCanvas = document.getElementById(divId + "_" + "gpu");
  const stream = gpuCanvas.captureStream(0);
  const video = document.getElementById(divId + "_" + "destroyed");
  video.srcObject = stream;

  stream.getVideoTracks()[0].requestFrame();
}

function copyCanvasImage(canvas, divId, targetId) {
  if (is_drawImage_method()) {
    drawImage(canvas, divId, targetId);
  } else if (is_blob_method()) {
    toBlob(canvas, divId, targetId);
  } else if (is_toDataURL_method()) {
    toDataURL(canvas, divId, targetId);
  } else if (is_transfter_to_image_bitmap_method()) {
    transferToImageBitmap(canvas, divId, targetId);
  } else if (is_copy_external_image_to_texture_method()) {
    copyExternalImageToTexture(canvas, divId, targetId);
  } else if (is_gl_texImage2d_method()) {
    glTexImage2D(canvas, divId, targetId);
  } else if (is_capture_stream_method()) {
    captureStream(canvas, divId, targetId);
  } else {
    sendResult("FAILURE", "unknown method");
  }
}

var deviceCount = 0;
function waitAllDevicesLost() {
  deviceCount += 1;
  if (deviceCount == 2) {
    waitForFinish();
  }
}

const opaque_id = "opaque";
const transparent_id = "transparent";

let gpuCanvas;
let gpuCanvasAlpha;

function setup() {

  gpuCanvas = createGpuCanvas(opaque_id);
  gpuCanvasAlpha = createGpuCanvas(transparent_id);

  // create target nodes.
  createTargetNodes(opaque_id);
  createTargetNodes(transparent_id);

  sendResult("READY");
}

async function initialize(canvas, divId, alpha) {
  const [gpuDevice, gpuContext] = await webGpuUtils.init(canvas, alpha);
  if (!gpuDevice || !gpuContext) {
    sendResult("FAILURE", "Failed to initialize WebGPU");
    return;
  }

  gpuDevice.lost.then(() => {
    console.log(`WebGPU Device for "${divId}" canvas lost`);

    copyCanvasImage(canvas, divId, "lost");
    waitAllDevicesLost();
  });

  return [gpuDevice, gpuContext];
}

async function render() {
  const [gpuDevice, gpuContext] =
    await initialize(gpuCanvas, opaque_id, false);

  const [gpuDeviceAlpha, gpuContextAlpha] =
    await initialize(gpuCanvasAlpha, transparent_id, true);

  const redColor = { r: 1, g: 0, b: 0, a: 1 };
  const greenColor = { r: 0, g: 1, b: 0, a: 1 };

  var g_renderCount = 1;
  const renderCallback = function () {
    const color = (g_renderCount % 2 == 0) ? redColor : greenColor;

    webGpuUtils.solidColorTest(gpuDevice, gpuContext, color);
    webGpuUtils.solidColorTest(gpuDeviceAlpha, gpuContextAlpha, color);

    if (g_renderCount == 0) {
      gpuDevice.destroy();
      gpuDeviceAlpha.destroy();

      copyCanvasImage(gpuCanvas, opaque_id, "destroyed");
      copyCanvasImage(gpuCanvasAlpha, transparent_id, "destroyed");
    } else {
      g_renderCount--;
      window.requestAnimationFrame(renderCallback);
    }
  };

  window.requestAnimationFrame(renderCallback);
}
