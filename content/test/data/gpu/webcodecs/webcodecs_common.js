// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

"use strict";

class TestHarness {
  finished = false;
  success = false;
  message = 'ok';
  logs = [];

  constructor() {}

  reportSuccess() {
    this.finished = true;
    this.success = true;
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

  summary() {
    return this.message + "\n\n" + this.logs.join("\n");
  }

  log(msg) {
    this.logs.push(msg);
    console.log(msg);
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

function drawRainbow(ctx, width, height, text) {
  let gradient = ctx.createLinearGradient(0, 0, width, height);
  gradient.addColorStop(0, 'magenta');
  gradient.addColorStop(0.15, 'blue');
  gradient.addColorStop(0.30, 'green');
  gradient.addColorStop(0.50, 'yellow');
  gradient.addColorStop(0.85, 'orange');
  gradient.addColorStop(1.0, 'red');
  ctx.fillStyle = gradient;
  ctx.fillRect(0, 0, width, height);

  ctx.fillStyle = 'black';
  ctx.font = (height / 4) + 'px fantasy';
  ctx.fillText(text, width / 3, height / 2);

  ctx.lineWidth = 20;
  ctx.strokeStyle = 'turquoise';
  ctx.rect(0, 0, width, height);
  ctx.stroke();
}


// Base class for video frame sources.
class FrameSource {
  constructor() {}

  async getNextFrame() {
    return null;
  }
}

// Source of video frames coming from taking snapshots of a canvas.
class CanvasSource extends FrameSource {
  constructor(width, height) {
    super();
    this.width = width;
    this.height = height;
    this.canvas = new OffscreenCanvas(width, height);
    this.ctx = this.canvas.getContext('2d');
    this.timestamp = 0;
    this.duration = 16666;  // 1/60 s
  }

  async getNextFrame() {
    drawRainbow(this.ctx, this.width, this.height, this.timestamp.toString());
    let result = new VideoFrame(this.canvas, {timestamp: this.timestamp});
    this.timestamp += this.duration;
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
}

function createCanvasCaptureSource(width, height) {
  let canvas = document.createElement('canvas');
  canvas.id = 'canvas-for-capture';
  canvas.width = width;
  canvas.height = height;
  document.body.appendChild(canvas);

  let ctx = canvas.getContext('2d');
  let drawOneFrame = function(time) {
    drawRainbow(ctx, width, height, time.toString());
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
    acceleration = 'allow';
  const encoder_config = {
    codec: codec,
    width: width,
    height: height,
    bitrate: 1000000,
    framerate: 24
  };

  if (codec.startsWith('avc1'))
    encoder_config.avc = {format: 'annexb'};

  let decoder_config = {
    codec: codec,
    codedWidth: width,
    codedHeight: height,
    visibleRegion: {left: 0, top: 0, width: width, height: height},
    hardwareAcceleration: acceleration
  };

  let support = await VideoDecoder.isConfigSupported(decoder_config);
  if (!support.supported)
    return null;

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
  let canvasSource = new CanvasSource(width, height);

  for (let i = 0; i < frames_to_encode; i++) {
    let frame = await canvasSource.getNextFrame();
    encoder.encode(frame, {keyFrame: false});
    frame.close();
  }
  await encoder.flush();
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
      let src =
          prepareDecoderSource(40, width, height, 'avc1.42001E', 'require');
      if (!src)
        src = prepareDecoderSource(40, width, height, 'vp8', 'require');
      if (!src) {
        src =
            prepareDecoderSource(40, width, height, 'vp09.00.10.08', 'require');
      }
      if (!src) {
        TEST.log('Can\'t find a supported hardware decoder.');
      }
      return src;
    }
    case 'sw_decoder': {
      return prepareDecoderSource(40, width, height, 'vp8', 'deny');
    }
  }
}