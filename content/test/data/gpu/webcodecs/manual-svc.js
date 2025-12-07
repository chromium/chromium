// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

function showFrameForDebug(frame) {
  const cnv = document.getElementById('debug_cnv');
  var ctx = cnv.getContext('2d');
  ctx.drawImage(frame, 0, 0);
}

async function main(arg) {
  const width = 640;
  const height = 480;
  const frames_to_encode = 32;
  let frames_encoded = 0;
  let frames_decoded = 0;
  let errors = 0;
  const base_layer_decimator = ([1, 1, 2, 4])[arg.layers];
  let expected_dot_count = [];

  const encoder_config = {
    codec: arg.codec,
    hardwareAcceleration: arg.acceleration,
    width: width,
    height: height,
    bitrateMode: 'quantizer',
    scalabilityMode: 'manual',
    latencyMode: 'realtime',
  };

  TEST.log('Starting test with arguments: ' + JSON.stringify(arg));
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

  let decoder = new VideoDecoder({
    output(frame) {
      frames_decoded++;
      let dots = expected_dot_count.shift();
      TEST.log(`Decoded frame ${frame.timestamp} ${frames_decoded} ${dots}`);
      // Check that we have intended number of dots and no more.
      // Completely black frame shouldn't pass the test.
      if (!validateBlackDots(frame, dots) ||
          validateBlackDots(frame, dots + 1)) {
        showFrameForDebug(frame);
        TEST.reportFailure(
            `Unexpected dot count ts:${frame.timestamp} dots:${dots}`);
      }

      frame.close();
    },
    error(e) {
      errors++;
      TEST.log(e);
    }
  });

  const encoder_init = {
    output(chunk, metadata) {
      let config = metadata.decoderConfig;
      if (config) {
        decoder.configure(config);
      }

      if (frames_encoded % base_layer_decimator == 0) {
        decoder.decode(chunk);
      }
      frames_encoded++;
    },
    error(e) {
      errors++;
      TEST.log(e);
    }
  };

  let encoder = new VideoEncoder(encoder_init);
  encoder.configure(encoder_config);
  const buffers = encoder.getAllFrameBuffers();
  const last_base_layer_buffer = buffers[0];
  TEST.assert(buffers.length == 3, 'expect 3 encoder buffers');

  let source = new CanvasSource(width, height);

  for (let i = 0; i < frames_to_encode; i++) {
    let frame = await source.getNextFrame();
    const encode_options = {keyFrame: false, av1: {quantizer: 5}};
    switch (arg.layers) {
      case 0:
        // No layers. Each frame can be decoded on its own, no dependencies.
        encode_options.updateBuffer = last_base_layer_buffer;
        break;
      case 1:
        // Single layer, each frame depends on the previous one.
        encode_options.updateBuffer = last_base_layer_buffer;
        encode_options.referenceBuffers = [last_base_layer_buffer];
        break;
      case 2:
        // Two temporal layers L1T2
        // Layer Index 0: |0| |2| |4| |6|
        // Layer Index 1: | |1| |3| |5| |7
        if (i % 2 == 0) {
          encode_options.updateBuffer = last_base_layer_buffer;
        }
        encode_options.referenceBuffers = [last_base_layer_buffer];
        break;
      case 3:
        // Three temporal layers L1T3
        // Layer Index 0: |0| | | |4| | | |8| |  |  |12|
        // Layer Index 1: | | |2| | | |6| | | |10|  |  |
        // Layer Index 2: | |1| |3| |5| |7| |9|  |11|  |
        const middle_layer_buffer = buffers[1];
        const index_in_cycle = i % 4;
        encode_options.referenceBuffers = [last_base_layer_buffer];
        if (index_in_cycle == 0) {
          // Base layer frame.
          encode_options.updateBuffer = last_base_layer_buffer;
        } else if (index_in_cycle == 1) {
          // First frame of the highest layer.
        } else if (index_in_cycle == 2) {
          // Middle layer.
          encode_options.updateBuffer = middle_layer_buffer;
        } else {
          // Second frame of the highest layer.
          encode_options.referenceBuffers.push(middle_layer_buffer);
        }
        break;
    }
    encoder.encode(frame, encode_options);
    if (i % base_layer_decimator == 0)
      expected_dot_count.push(i);
    frame.close();

    await waitForNextFrame();
  }
  await encoder.flush();
  await decoder.flush();
  encoder.close();
  decoder.close();
  source.close();

  TEST.assert(
      errors == 0, 'Decoding or encoding errors occurred during the test');
  TEST.assert(
      frames_encoded == frames_to_encode,
      'frames_encoded mismatch: ' + frames_encoded);

  let base_layer_frames = frames_encoded / base_layer_decimator;
  TEST.assert(
      frames_decoded == base_layer_frames,
      'frames_decoded mismatch: ' + frames_decoded);
  TEST.log('Test completed');
}