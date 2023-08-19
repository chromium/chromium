// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

function createRandomGenerator(seed) {
  return function(max) {
    seed = ((1 + seed) * 7 + 13) & 0x7fffffff;
    return seed % max;
  }
}

// Draw pseudorandom animation on the canvas
function createDrawingFunction(cnv, changes_per_frame) {
  var ctx = cnv.getContext('2d');
  let width = cnv.width;
  let height = cnv.height;
  let rnd = createRandomGenerator(425533);
  const w = 35;
  const h = 30;
  const white_noise_img = ctx.createImageData(w, h);
  self.crypto.getRandomValues(white_noise_img.data);

  return function() {
    let x = 0, y = 0;
    // Paint gradient filled ellipses
    for (let i = 0; i < changes_per_frame; i++) {
      x = rnd(width);
      y = rnd(height);

      let a =
          'rgba(' + rnd(255) + ',' + rnd(255) + ',' + rnd(255) + ',' + 1 + ')';
      let b =
          'rgba(' + rnd(255) + ',' + rnd(255) + ',' + rnd(255) + ',' + 1 + ')';
      let c =
          'rgba(' + rnd(255) + ',' + rnd(255) + ',' + rnd(255) + ',' + 1 + ')';
      let gradient = ctx.createLinearGradient(x, y, x + w, y + h);
      gradient.addColorStop(0, a);
      gradient.addColorStop(0.5, b);
      gradient.addColorStop(1, c);

      ctx.fillStyle = gradient;
      ctx.beginPath();
      ctx.ellipse(x, y, w / 2, h / 2, 0.0, 0.0, 2 * Math.PI);
      ctx.fill();
    }

    // Add a bit of white noise
    ctx.putImageData(white_noise_img, x, y);
  }
}

async function main(arg) {
  const width = 1280;
  const height = 720;
  const seconds = 5;
  const fps = 30;
  const frames_to_encode = fps * seconds;
  let errors = 0;
  let chunks = [];

  const encoder_config = {
    codec: arg.codec,
    hardwareAcceleration: arg.acceleration,
    width: width,
    height: height,
    bitrate: arg.bitrate,
    bitrateMode: arg.bitrate_mode,
    framerate: fps
  };

  let supported = false;
  try {
    supported =
        (await VideoEncoder.isConfigSupported(encoder_config)).supported;
  } catch (e) {
  }
  if (!supported) {
    TEST.skip('Unsupported codec: ' + arg.codec);
    return;
  }

  // Start drawing canvas animation that will be source of the frames
  // for the test.
  let cnv = document.getElementById('src');
  cnv.width = width;
  cnv.height = height;
  const changes_per_frame = 25;
  const draw = createDrawingFunction(cnv, changes_per_frame);

  const init = {
    output(chunk, metadata) {
      chunks.push(chunk);
    },
    error(e) {
      errors++;
      TEST.log(e);
    }
  };

  let encoder = new VideoEncoder(init);
  encoder.configure(encoder_config);

  // Take frames from the canvas and encoder them with a given bitrate
  for (let i = 0; i < frames_to_encode; i++) {
    const time_us = i / fps * 1_000_000;
    draw();
    let frame = new VideoFrame(cnv, {timestamp: time_us});
    encoder.encode(frame, {keyFrame: false});
    frame.close();
    await waitForNextFrame();
  }

  await encoder.flush();
  TEST.log('Encoding completed');
  encoder.close();

  TEST.assert(errors == 0, 'Encoding errors occurred during the test');
  TEST.assert(
      chunks.length == frames_to_encode,
      'Output count mismatch: ' + chunks.length);

  // Calculate total size of the encoded video
  let total_encoded_size = 0;
  for (let chunk of chunks) {
    total_encoded_size += chunk.byteLength;
  }

  const actual_bitrate = total_encoded_size / seconds * 8 /* bits */;
  const bitrate_mismatch = Math.abs(actual_bitrate - arg.bitrate) / arg.bitrate;

  // Check how far off expected encoded size from the ideal, CBR is
  // supposed to be more strict that VBR.
  let tolerance = (arg.bitrate_mode == 'constant') ? 0.15 : 0.30;
  TEST.assert(
      bitrate_mismatch < tolerance,
      `Bitrate is too far off. Expected ${arg.bitrate}` +
          ` Actual bitrate: ${actual_bitrate}` +
          ` Tolerance: ${tolerance * arg.bitrate}` +
          ` Mismatch: ${bitrate_mismatch}`);

  TEST.log('Test completed');
}
