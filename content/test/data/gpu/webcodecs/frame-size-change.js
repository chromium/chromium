// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

async function main(arg) {
  // Reduce frame size first, then increase frame size. The last item is the
  // largest frame size, which will be used for creating source.
  const frame_size_params = [
    {
      width: 320,
      height: 240,
      bitrate: 500_000,
    },
    {
      width: 160,
      height: 120,
      bitrate: 300_000,
    },
    {
      width: FRAME_WIDTH,
      height: FRAME_HEIGHT,
      bitrate: 800_000,
    }
  ];
  const frames_in_one_pass = 10;
  let errors = 0;
  let decoder_param_index = 0;
  let output_trunks = 0;

  let source = await createFrameSource(
      arg.source_type, frame_size_params[frame_size_params.length - 1].width,
      frame_size_params[frame_size_params.length - 1].height);
  if (!source) {
    TEST.skip('Unsupported source: ' + arg.source_type);
    return;
  }

  const init = {
    output(chunk, metadata) {
      TEST.assert(decoder_param_index < frame_size_params.length);
      if (metadata.decoderConfig) {
        TEST.assert(
            output_trunks == 0,
            'metadata.decoderConfig is only available for the first frame ' +
                'after configuration change.');
        // Some platform requires 16x16 alignment.
        TEST.assert(
            metadata.decoderConfig.codedWidth >=
                    frame_size_params[decoder_param_index].width - 16 &&
                metadata.decoderConfig.codedWidth <=
                    frame_size_params[decoder_param_index].width + 16,
            'Unexpected codedWidth.');
        TEST.assert(
            metadata.decoderConfig.codedHeight >=
                    frame_size_params[decoder_param_index].height - 16 &&
                metadata.decoderConfig.codedHeight <=
                    frame_size_params[decoder_param_index].height + 16,
            'Unexpected codedHeight.');
      }
      output_trunks++;
      if (output_trunks == frames_in_one_pass) {
        decoder_param_index++;
        output_trunks = 0;
      }
    },
    error(e) {
      errors++;
      TEST.log(e);
    }
  };

  let encoder = new VideoEncoder(init);
  for (const param of frame_size_params) {
    const encoder_config = {
      ...param,
      codec: arg.codec,
      acceleration: 'prefer-hardware',
    };

    let supported = false;
    try {
      supported =
          (await VideoEncoder.isConfigSupported(encoder_config)).supported;
    } catch (e) {
      TEST.assert(
          false, 'Failed to check if the given configuration is supported.');
    }
    if (!supported) {
      TEST.skip('Unsupported configuration: ' + JSON.stringify(encoder_config));
      return;
    }
    encoder.configure(encoder_config);
    for (let i = 0; i < frames_in_one_pass; i++) {
      let frame = await source.getNextFrame();
      encoder.encode(frame);
      frame.close();
      await waitForNextFrame();
    }
  }

  await encoder.flush();
  encoder.close();
  source.close();

  TEST.assert(
      decoder_param_index == frame_size_params.length,
      `Decoder config should be changed ${frame_size_params.length} times.`);
  TEST.assert(
      output_trunks == 0,
      `Decoder should output ${
          frames_in_one_pass} frames for the last config.`);
  TEST.assert(errors == 0, 'Encoding errors occurred during the test');
  TEST.log('Test completed');
}
