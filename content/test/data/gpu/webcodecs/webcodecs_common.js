// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

"use strict";

// Use 16x16 aligned resolution since some platforms require that.
// See https://crbug.com/1084702.
// Also, some platforms require a resolution that isn't tiny (e.g. 160) to
// use hardware acceleration.
const FRAME_WIDTH = 640;
const FRAME_HEIGHT = 480;

class TestHarness {
  finished = false;
  success = false;
  skipped = false;
  message = 'ok';
  logs = [];
  logWindow = null;

  constructor() {}

  skip(message) {
    this.skipped = true;
    this.finished = true;
    this.message = message;
    this.log('Test skipped: ' + message);
  }

  reportSuccess() {
    this.finished = true;
    this.success = true;
    this.log('Test completed');
  }

  reportFailure(error) {
    this.finished = true;
    this.success = false;
    this.message = error.toString();
    this.log(this.message);
  }

  assert(condition, msg) {
    if (!condition)
      this.reportFailure("Assertion failed: " + msg);
  }

  assert_eq(val1, val2, msg) {
    if (val1 != val2) {
      this.reportFailure(`Assertion failed: ${msg}. ${val1} != ${val2}.`);
    }
  }

  summary() {
    return this.message + "\n\n" + this.logs.join("\n");
  }

  log(msg) {
    this.logs.push(msg);
    console.log(msg);
    if (this.logWindow === null)
      this.logWindow = document.querySelector('textarea');
    if (this.logWindow)
      this.logWindow.value += msg + '\n';
  }

  run(arg) {
    main(arg).then(
        _ => {
          if (!this.finished)
            this.reportSuccess();
        },
        error => {
          if (!this.finished)
            this.reportFailure(error);
        });
  }
};
var TEST = new TestHarness();

function waitForNextFrame() {
  return new Promise((resolve, _) => {
    window.requestAnimationFrame(resolve);
  });
}

function fourColorsFrame(ctx, width, height, text) {
  const kYellow = "#FFFF00";
  const kRed = "#FF0000";
  const kBlue = "#0000FF";
  const kGreen = "#00FF00";

  ctx.fillStyle = kYellow;
  ctx.fillRect(0, 0, width / 2, height / 2);

  ctx.fillStyle = kRed;
  ctx.fillRect(width / 2, 0, width / 2, height / 2);

  ctx.fillStyle = kBlue;
  ctx.fillRect(0, height / 2, width / 2, height / 2);

  ctx.fillStyle = kGreen;
  ctx.fillRect(width / 2, height / 2, width / 2, height / 2);

  ctx.fillStyle = 'white';
  ctx.font = (height / 10) + 'px sans-serif';
  ctx.fillText(text, width / 2, height / 2);
}

function peekPixel(ctx, x, y) {
  if (ctx.readPixels) {
    let pixels = new Uint8Array(4);
    ctx.readPixels(x, ctx.drawingBufferHeight - y, 1, 1,
                   ctx.RGBA, ctx.UNSIGNED_BYTE, pixels);
    return Array.from(pixels);
  }
  if (ctx.getImageData) {
    let settings = {colorSpaceConversion: 'none'};
    return ctx.getImageData(x, y, 1, 1, settings).data;
  }
}

function compareColors(actual, expected, tolerance, msg) {
  let channel = ['R', 'G', 'B', 'A'];
  for (let i = 0; i < 4; i++) {
    if (Math.abs(actual[i] - expected[i]) > tolerance) {
      TEST.reportFailure(msg +
       ` channel: ${channel[i]} actual: ${actual[i]} expected: ${expected[i]}`);
    }
  }
}

function checkFourColorsFrame(ctx, width, height, tolerance) {
  const kYellow = [0xFF, 0xFF, 0x00, 0xFF];
  const kRed = [0xFF, 0x00, 0x00, 0xFF];
  const kBlue = [0x00, 0x00, 0xFF, 0xFF];
  const kGreen = [0x00, 0xFF, 0x00, 0xFF];

  let m = 10; // margin from the frame's edge
  compareColors(peekPixel(ctx, m, m), kYellow,
                      tolerance, 'top left corner is yellow');
  compareColors(peekPixel(ctx, width - m, m), kRed,
                      tolerance, 'top right corner is red');
  compareColors(peekPixel(ctx, m, height - m), kBlue,
                      tolerance, 'bottom left corner is blue');
  compareColors(peekPixel(ctx, width - m, height - m), kGreen,
                      tolerance, 'bottom right corner is green');
}

// Paints |count| black dots on the |ctx|, so their presence can be validated
// later. This is an analog of the most basic bar code.
function putBlackDots(ctx, width, height, count) {
  ctx.fillStyle = 'black';
  const dot_size = 10;
  const step = dot_size * 3;

  for (let i = 1; i <= count; i++) {
    let x = i * step;
    let y = step * (x / width + 1);
    x %= width;
    ctx.fillRect(x, y, dot_size, dot_size);
  }
}

// Validates that frame has |count| black dots in predefined places.
function validateBlackDots(frame, count) {
  const width = frame.displayWidth;
  const height = frame.displayHeight;
  let cnv = new OffscreenCanvas(width, height);
  var ctx = cnv.getContext('2d', { willReadFrequently : true });
  ctx.drawImage(frame, 0, 0);
  const dot_size = 10;
  const step = dot_size * 3;

  for (let i = 1; i <= count; i++) {
    let x = i * step + dot_size / 2;
    let y = step * (x / width + 1) + dot_size / 2;
    x %= width;
    let rgba = ctx.getImageData(x, y, 1, 1).data;
    const tolerance = 40;
    if (rgba[0] > tolerance || rgba[1] > tolerance || rgba[2] > tolerance) {
      // The dot is too bright to be a black dot.
      return false;
    }
  }
  return true;
}


// Base class for video frame sources.
class FrameSource {
  constructor() {}

  async getNextFrame() {
    return null;
  }

  close() {}
}

// Source of video frames coming from taking snapshots of a canvas.
class CanvasSource extends FrameSource {
  constructor(width, height) {
    super();
    this.width = width;
    this.height = height;
    this.canvas = new OffscreenCanvas(width, height);
    this.ctx = this.canvas.getContext('2d', {colorSpace: 'srgb'});
    this.timestamp = 0;
    this.duration = 16666;  // 1/60 s
    this.frame_index = 0;
  }

  async getNextFrame() {
    fourColorsFrame(this.ctx, this.width, this.height,
                    this.timestamp.toString());
    putBlackDots(this.ctx, this.width, this.height, this.frame_index);
    let result = new VideoFrame(this.canvas, {timestamp: this.timestamp});
    this.timestamp += this.duration;
    this.frame_index++;
    return result;
  }
}

// Source of video frames coming from MediaStreamTrack.
class StreamSource extends FrameSource {
  constructor(track) {
    super();
    this.media_processor = new MediaStreamTrackProcessor(track);
    this.reader = this.media_processor.readable.getReader();
  }

  async getNextFrame() {
    const result = await this.reader.read();
    const frame = result.value;
    return frame;
  }

  close() {
    if (this.reader)
      this.reader.cancel();
  }
}

class ArrayBufferSource extends FrameSource {
  constructor(width, height) {
    super();
    this.width = width;
    this.height = height;
    this.canvas = new OffscreenCanvas(width, height);
    this.ctx = this.canvas.getContext(
        '2d', {willReadFrequently: true, colorSpace: 'srgb'});
    this.timestamp = 0;
    this.duration = 16666;  // 1/60 s
    this.frame_index = 0;
  }

  async getNextFrame() {
    fourColorsFrame(
        this.ctx, this.width, this.height, this.timestamp.toString());
    putBlackDots(this.ctx, this.width, this.height, this.frame_index);
    const imageData = this.ctx.getImageData(0, 0, this.width, this.height);
    const buffer = imageData.data;
    let init = {
      format: 'RGBA',
      timestamp: this.timestamp,
      codedWidth: this.width,
      codedHeight: this.height,
      // This describes sRGB color-space used by the canvas
      colorSpace: {
        fullRange: true,
        matrix: 'rgb',
        primaries: 'bt709',
        transfer: 'iec61966-2-1'
      },
      transfer: [buffer.buffer]
    };
    this.timestamp += this.duration;
    this.frame_index++;
    return new VideoFrame(buffer, init);
  }
}

class HBDArrayBufferSource extends FrameSource {
  constructor(width, height) {
    super();
    this.width = width;
    this.height = height;
    this.timestamp = 0;
    this.duration = 16666;  // 1/60 s
    this.frame_index = 0;
  }

  async getNextFrame() {
    const kDepth = 10;
    const kShift = kDepth - 8;
    // 8-bit YUV colors assuming BT.709 matrix and sRGB primaries.
    const kYellow = [219, 16, 138];
    const kRed = [63, 102, 240];
    const kBlue = [32, 240, 118];
    const kGreen = [173, 42, 26];

    const width = this.width;
    const height = this.height;
    const halfW = Math.ceil(width / 2);
    const halfH = Math.ceil(height / 2);
    const qtrW = Math.ceil(width / 4);
    const qtrH = Math.ceil(height / 4);
    const data = new Uint8Array((width * height + 2 * halfW * halfH) * 2);
    const view = new DataView(data.buffer)

    let i = 0;

    // Y plane.
    for (let y = 0; y < height; ++y) {
      const colors = y < halfH ? [kYellow, kRed] : [kBlue, kGreen];
      for (let x = 0; x < width; ++x) {
        const color = x < halfW ? colors[0] : colors[1];
        // Note: Rounding is not quite accurate due to shifting rather than
        // scaling, in addition to using already rounded YUV values.
        view.setUint16(i, color[0] << kShift, true);
        i += 2;
      }
    }

    // U plane.
    for (let y = 0; y < halfH; ++y) {
      const colors = y < qtrH ? [kYellow, kRed] : [kBlue, kGreen];
      for (let x = 0; x < halfW; ++x) {
        const color = x < qtrW ? colors[0] : colors[1];
        view.setUint16(i, color[1] << kShift, true);
        i += 2;
      }
    }

    // V plane.
    for (let y = 0; y < halfH; ++y) {
      const colors = y < qtrH ? [kYellow, kRed] : [kBlue, kGreen];
      for (let x = 0; x < halfW; ++x) {
        const color = x < qtrW ? colors[0] : colors[1];
        view.setUint16(i, color[2] << kShift, true);
        i += 2;
      }
    }

    const init = {
      format: `I420P${kDepth}`,
      timestamp: this.timestamp,
      codedWidth: width,
      codedHeight: height,
      // sRGB with BT.709 YUV matrix.
      colorSpace: {
        matrix: 'bt709',
        primaries: 'bt709',
        transfer: 'iec61966-2-1',
        fullRange: false,
      },
      transfer: [data.buffer]
    };
    this.timestamp += this.duration;
    this.frame_index++;
    return new VideoFrame(data, init);
  }
}

// Source of video frames coming from either hardware of software decoder.
class DecoderSource extends FrameSource {
  constructor(decoderConfig, chunks) {
    super();
    this.decoderConfig = decoderConfig;
    this.frames = [];
    this.error = null;
    this.decoder = new VideoDecoder(
        {error: this.onError.bind(this), output: this.onFrame.bind(this)});
    this.decoder.configure(this.decoderConfig);
    while (chunks.length != 0)
      this.decoder.decode(chunks.shift());
    this.decoder.flush();
  }

  onError(error) {
    TEST.log(error);
    this.error = error;
    if (this.next) {
      this.next.reject(error);
      this.next = null;
    }
  }

  onFrame(frame) {
    if (this.next) {
      this.next.resolve(frame);
      this.next = null;
    } else {
      this.frames.push(frame);
    }
  }

  async getNextFrame() {
    if (this.next)
      return this.next.promise;

    if (this.frames.length > 0)
      return this.frames.shift();

    if (this.error)
      throw this.error;

    let next = {};
    this.next = next;
    this.next.promise = new Promise((resolve, reject) => {
      next.resolve = resolve;
      next.reject = reject;
    });

    return next.promise;
  }

  close() {
    if (this.decoder)
      this.decoder.close();
  }
}

function createCanvasCaptureSource(width, height) {
  let canvas = document.createElement('canvas');
  canvas.id = 'canvas-for-capture';
  canvas.width = width;
  canvas.height = height;
  document.body.appendChild(canvas);

  let ctx = canvas.getContext('2d');
  let drawOneFrame = function(time) {
    fourColorsFrame(ctx, width, height, time.toString());
    window.requestAnimationFrame(drawOneFrame);
  };
  window.requestAnimationFrame(drawOneFrame);

  const stream = canvas.captureStream(60);
  const track = stream.getVideoTracks()[0];
  return new StreamSource(track);
}

async function prepareDecoderSource(
    frames_to_encode, width, height, codec, acceleration) {
  if (!acceleration)
    acceleration = 'no-preference';
  const encoder_config = {
    codec: codec,
    width: width,
    height: height,
    bitrate: 10000000,
    framerate: 24
  };

  if (codec.startsWith('avc1')) {
    encoder_config.avc = {format: 'annexb'};
  } else if (codec.startsWith('hvc1')) {
    encoder_config.hevc = {format: 'annexb'};
  }

  let decoder_config = {
    codec: codec,
    codedWidth: width,
    codedHeight: height,
    visibleRegion: {left: 0, top: 0, width: width, height: height},
    hardwareAcceleration: acceleration
  };

  try {
    let support = await VideoDecoder.isConfigSupported(decoder_config);
    if (!support.supported)
      return null;
  } catch (e) {
    return null;
  }

  let chunks = [];
  let errors = 0;
  const init = {
    output(chunk, metadata) {
      let config = metadata.decoderConfig;
      if (config) {
        decoder_config = config;
        decoder_config.hardwareAcceleration = acceleration;
      }
      chunks.push(chunk);
    },
    error(e) {
      errors++;
      TEST.log(e);
    }
  };

  let encoder = new VideoEncoder(init);
  encoder.configure(encoder_config);
  let innerSource = new ArrayBufferSource(width, height);

  for (let i = 0; i < frames_to_encode; i++) {
    let frame = await innerSource.getNextFrame();
    encoder.encode(frame, {keyFrame: false});
    frame.close();
  }
  try {
    await encoder.flush();
    encoder.close();
    innerSource.close();
  } catch (e) {
    errors++;
    TEST.log(e);
  }

  if (errors > 0)
    return null;

  return new DecoderSource(decoder_config, chunks);
}

async function createFrameSource(type, width, height) {
  switch (type) {
    case 'camera': {
      let constraints = {audio: false, video: {width: width, height: height}};
      let stream =
          await window.navigator.mediaDevices.getUserMedia(constraints);
      var track = stream.getTracks()[0];
      return new StreamSource(track);
    }
    case 'capture': {
      return createCanvasCaptureSource(width, height);
    }
    case 'offscreen': {
      return new CanvasSource(width, height);
    }
    case 'hw_decoder': {
      // Trying to find any hardware decoder supported by the platform.
      let src = await prepareDecoderSource(
          40, width, height, 'avc1.42001E', 'prefer-hardware');
      if (!src)
        src = await prepareDecoderSource(
            40, width, height, 'hvc1.1.6.L123.00', 'prefer-hardware');
      if (!src)
        src = await prepareDecoderSource(
            40, width, height, 'vp8', 'prefer-hardware');
      if (!src) {
        src = await prepareDecoderSource(
            40, width, height, 'vp09.00.10.08', 'prefer-hardware');
      }
      if (!src) {
        TEST.log('Can\'t find a supported hardware decoder.');
      }
      return src;
    }
    case 'sw_decoder': {
      return await prepareDecoderSource(
          40, width, height, 'vp8', 'prefer-software');
    }
    case 'arraybuffer': {
      return new ArrayBufferSource(width, height);
    }
    case 'hbd_arraybuffer': {
      return new HBDArrayBufferSource(width, height);
    }
  }
}

function addManualTestButton(configs) {
  document.addEventListener('DOMContentLoaded', _ => {
    configs.forEach(config => {
      const btn = document.createElement('button');
      const label = document.createTextNode(
          'Run test with config: ' + JSON.stringify(config));
      btn.onclick = function() {
        main(config);
      };
      btn.appendChild(label);
      btn.style.margin = '5px';
      document.body.appendChild(btn);
    });
  }, true);
}

function readProfileFromAvcExtraData(view) {
  if (view.byteLength < 6) {
    // Too short to be a proper AVCDecoderConfigurationRecord
    return null;
  }
  const version = view.getUint8(0);
  if (version != 1) {
    return null;
  }

  const profile = view.getUint8(1);
  return profile;
}
