// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

async function main(arg) {
  const width = 640;
  const height = 480;
  const source_names = [
    'camera', 'capture', 'offscreen', 'arraybuffer', 'hw_decoder', 'sw_decoder'
  ];
  let errors = 0;

  const encoder_config = {
    codec: arg.codec,
    hardwareAcceleration: arg.acceleration,
    width: width,
    height: height,
    bitrate: 1000000,
    framerate: 24
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

  let sources = [];
  for (const name of source_names) {
    const source = await createFrameSource(name, width, height);
    if (source) {
      sources.push(source);
    }
  }
  TEST.log('Source count: ' + sources.length);
  const frames_to_encode = 2 * sources.length;

  const chunks = [];
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

  for (let i = 0; i < frames_to_encode; i++) {
    const frame = await sources[i % sources.length].getNextFrame();
    const frame_with_adjusted_ts =
        new VideoFrame(frame, {timestamp: i * 50000});
    encoder.encode(frame_with_adjusted_ts, {keyFrame: false});
    frame.close();
    frame_with_adjusted_ts.close();
    await waitForNextFrame();
  }

  await encoder.flush();
  encoder.close();
  for (const source of sources) {
    source.close();
  }

  TEST.assert(
      chunks.length == frames_to_encode,
      `Encoder should output ${frames_to_encode} chunks, but got ${
          chunks.length}.`);
  TEST.assert(errors == 0, 'Encoding errors occurred during the test');
  TEST.log('Test completed');
}
