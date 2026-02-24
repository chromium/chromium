// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';
async function main(arg) {
  const frame_width = 640;
  const frame_height = 480;
  const encoder_width = 160;
  const encoder_height = 120;
  let errors = 0;

  const cnv = document.getElementById('cnv');
  cnv.width = encoder_width;
  cnv.height = encoder_height;
  var ctx = cnv.getContext('2d');

  const encoder_config = {
    codec: arg.codec,
    hardwareAcceleration: arg.acceleration,
    width: encoder_width,
    height: encoder_height,
    bitrate: 5000000,
    framerate: 24
  };
  if (arg.codec.startsWith('avc1')) {
    encoder_config.avc = {format: 'annexb'};
  } else if (arg.codec.startsWith('hvc1')) {
    encoder_config.hevc = {format: 'annexb'};
  }

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

  let source =
      await createFrameSource(arg.source_type, frame_width, frame_height);
  if (!source) {
    TEST.skip('Unsupported source: ' + arg.source_type);
    return;
  }

  let decoder = new VideoDecoder({
    output(frame) {
      ctx.drawImage(frame, 0, 0);
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
      decoder.decode(chunk);
    },
    error(e) {
      errors++;
      TEST.log(e);
    }
  };

  let encoder = new VideoEncoder(encoder_init);
  encoder.configure(encoder_config);

  let frame = await source.getNextFrame();
  let cropped_frame = new VideoFrame(frame, {
    timestamp: frame.timestamp,
    visibleRect: {
      x: (frame.codedWidth - encoder_width) / 2,
      y: (frame.codedHeight - encoder_height) / 2,
      width: encoder_width,
      height: encoder_height
    }
  });

  {
    // Exercise copyTo of a cropped frame.
    let size = cropped_frame.allocationSize();
    let buf = new ArrayBuffer(size);
    let layout = await cropped_frame.copyTo(buf);
    TEST.assert(layout.length > 0, 'copyTo layout is empty');
  }

  encoder.encode(cropped_frame, {keyFrame: true});
  frame.close();
  cropped_frame.close();

  await encoder.flush();
  await decoder.flush();
  encoder.close();
  decoder.close();
  source.close();


  // TODO(crbug.com/349062462): Enable pixel validation once we sort it out
  // on all platforms.
  /*
  if (arg.source_type == 'camera' && !arg.validate_camera_frames) {
    TEST.log('Can\'t validate camera frames, skipping');
  } else {
    // The inner cropped rect maintains the four color quadrant structure,
    // so we can still check the center cropped frame.
    checkFourColorsFrame(ctx, encoder_width, encoder_height, 15);
  }
  */

  TEST.assert(
      errors == 0, 'Decoding or encoding errors occurred during the test');
  TEST.log('Test completed');
}
