// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

function isRec709(colorSpace) {
  return colorSpace.primaries === 'bt709' && colorSpace.transfer === 'bt709' &&
      colorSpace.matrix === 'bt709' && colorSpace.fullRange === false;
}

function isSRGB(colorSpace) {
  return colorSpace.primaries === 'bt709' &&
      colorSpace.transfer === 'iec61966-2-1' && colorSpace.matrix === 'rgb' &&
      colorSpace.fullRange === true;
}

function isRec601(colorSpace) {
  return colorSpace.primaries === 'smpte170m' &&
      (colorSpace.transfer === 'smpte170m' ||
       colorSpace.transfer === 'bt709') &&
      colorSpace.matrix === 'smpte170m' && colorSpace.fullRange === false;
}

function makePixelArray(byteLength) {
  let data = new Uint8Array(byteLength);
  for (let i = 0; i < byteLength; i++) {
    data[i] = i;
  }
  return data;
}

function makeFrame(type, timestamp) {
  let init = {
    format: 'RGBA',
    timestamp: timestamp,
    codedWidth: FRAME_WIDTH,
    codedHeight: FRAME_HEIGHT
  };
  switch (type) {
    case 'I420': {
      const yuvByteLength = 1.5 * FRAME_WIDTH * FRAME_HEIGHT;
      let data = makePixelArray(yuvByteLength);
      return new VideoFrame(data, {...init, format: 'I420'});
    }
    case 'RGBA': {
      const rgbaByteLength = 4 * FRAME_WIDTH * FRAME_HEIGHT;
      let data = makePixelArray(rgbaByteLength);
      return new VideoFrame(data, {...init, format: 'RGBA'});
    }
  }
}

async function main(arg) {
  const encoderConfig = {
    codec: arg.codec,
    hardwareAcceleration: arg.acceleration,
    width: FRAME_WIDTH,
    height: FRAME_HEIGHT,
  };

  TEST.log('Starting test with arguments: ' + JSON.stringify(arg));
  let supported = false;
  try {
    supported = (await VideoEncoder.isConfigSupported(encoderConfig)).supported;
  } catch (e) {
  }
  if (!supported) {
    TEST.skip('Unsupported codec: ' + arg.codec);
    return;
  }

  const frameDuration = 16666;
  let inputFrames = [
    // Use I420/BT.709 first since default macOS colorspace is sRGB.
    makeFrame('I420', 0 * frameDuration),
    makeFrame('I420', 1 * frameDuration),
    makeFrame('RGBA', 2 * frameDuration),
    makeFrame('RGBA', 3 * frameDuration),
  ];
  let outputChunks = [];
  let outputMetadata = [];
  let errors = 0;

  const init = {
    output(chunk, metadata) {
      outputChunks.push(chunk);
      outputMetadata.push(metadata);
    },
    error(e) {
      errors++;
      TEST.log(e);
    }
  };

  let encoder = new VideoEncoder(init);
  encoder.configure(encoderConfig);

  for (let frame of inputFrames) {
    encoder.encode(frame);
    await waitForNextFrame();
  }
  await encoder.flush();
  encoder.close();

  TEST.assert_eq(errors, 0, 'Encoding errors occurred during the test');
  TEST.assert_eq(outputChunks.length, 4, 'Unexpected number of outputs');
  TEST.assert_eq(
      outputMetadata.length, 4, 'Unexpected number of output metadata');

  // I420 passthrough should preserve default rec709 color space.
  TEST.assert_eq(inputFrames[0].format, 'I420', 'inputs[0] is I420');
  TEST.assert(isRec709(inputFrames[0].colorSpace), 'inputs[0] is rec709');
  TEST.assert_eq(outputChunks[0].type, 'key', 'outputs[0] is key');
  TEST.assert(
      'decoderConfig' in outputMetadata[0], 'metadata[0] has decoderConfig');
  TEST.assert(
      isRec709(outputMetadata[0].decoderConfig.colorSpace),
      'metadata[0] is rec709');

  // Next output may or may not be a key frame w/ metadata (up to
  // encoder). Corresponding input is still I420 rec709, so if metadata is
  // given, we expect same colorSpace as for the previous frame.
  TEST.assert_eq(inputFrames[1].format, 'I420', 'inputs[1] is I420');
  TEST.assert(isRec709(inputFrames[1].colorSpace, 'inputs[1] is rec709'));
  if ('decoderConfig' in outputMetadata[1]) {
    TEST.assert(
        isRec709(outputMetadata[1].decoderConfig.colorSpace),
        'metadata[1] is rec709');
  }

  // Next output should be a key frame and have accompanying metadata
  // because the corresponding input format changed to RGBA, which means
  // we libyuv will convert to I420 w/ rec601 during encoding.
  TEST.assert_eq(inputFrames[2].format, 'RGBA', 'inputs[2] is RGBA');
  TEST.assert(isSRGB(inputFrames[2].colorSpace), 'inputs[2] is sRGB');

  TEST.assert(outputChunks[2].type == 'key', 'outputs[2] is key');
  TEST.assert(
      'decoderConfig' in outputMetadata[2], 'metadata[2] has decoderConfig');
  TEST.assert(
      isRec601(outputMetadata[2].decoderConfig.colorSpace),
      'metadata[2] is rec601');

  // Next output may or may not be a key frame w/ metadata (up to
  // encoder). Corresponding input is still RGBA sRGB, so if metadata is
  // given, we expect same colorSpace as for the previous frame.
  TEST.assert_eq(inputFrames[3].format, 'RGBA', 'inputs[3] is RGBA');
  TEST.assert(isSRGB(inputFrames[3].colorSpace), 'inputs[3] is sRGB');
  if ('decoderConfig' in outputMetadata[3]) {
    TEST.assert(
        isRec601(outputMetadata[3].decoderConfig.colorSpace),
        'metadata[3] is rec601');
  }

  for (let frame of inputFrames) {
    frame.close();
  }

  // Now decode the frames and ensure the encoder embedded the right color
  // space information in the bitstream.

  // VP8 doesn't have embedded color space information in the bitstream.
  if (arg.codec == 'vp8') {
    TEST.reportSuccess();
    return;
  }

  let decodedFrames = [];
  const decoderInit = {
    output(frame) {
      decodedFrames.push(frame);
    },
    error(e) {
      errors++;
      TEST.log(e);
    }
  };

  let decoder = new VideoDecoder(decoderInit);
  for (var i = 0; i < outputChunks.length; ++i) {
    if ('decoderConfig' in outputMetadata[i]) {
      let config = {...outputMetadata[i].decoderConfig};

      // Removes the color space provided by the encoder so that color space
      // information in the underlying bitstream is exposed during decode.
      config.colorSpace = {};

      config.hardwareAcceleration = arg.acceleration;
      let support = await VideoDecoder.isConfigSupported(config);
      if (!support.supported)
        config.hardwareAcceleration = 'no-preference';

      decoder.configure(config);
    }
    decoder.decode(outputChunks[i]);
    await waitForNextFrame();
  }
  await decoder.flush();
  decoder.close();

  TEST.assert_eq(
      errors, 0, 'Encoding errors occurred during the decoding test');
  TEST.assert_eq(
      decodedFrames.length, outputChunks.length,
      'Unexpected number of decoded outputs');

  let colorSpace = {};
  for (var i = 0; i < decodedFrames.length; ++i) {
    if ('decoderConfig' in outputMetadata[i]) {
      colorSpace = outputMetadata[i].decoderConfig.colorSpace;
    }

    // It's acceptable to have no bitstream color space information.
    if (decodedFrames[i].colorSpace.primaries != null) {
      TEST.assert_eq(
          decodedFrames[i].colorSpace.primaries, colorSpace.primaries,
          `Frame ${i} color primaries mismatch`);
    }

    if (decodedFrames[i].colorSpace.matrix != null) {
      if (decodedFrames[i].colorSpace.matrix != colorSpace.matrix) {
        // Allow functionally equivalent matches.
        TEST.assert(
            colorSpace.matrix == 'smpte170m' &&
                decodedFrames[i].colorSpace.matrix == 'bt470bg',
            `Frame ${i} color matrix mismatch`);
      } else {
        TEST.assert_eq(
            decodedFrames[i].colorSpace.matrix, colorSpace.matrix,
            `Frame ${i} color matrix mismatch`);
      }
    }

    if (decodedFrames[i].colorSpace.transfer != null) {
      if (decodedFrames[i].colorSpace.transfer != colorSpace.transfer) {
        // Allow functionally equivalent matches.
        TEST.assert(
            (colorSpace.transfer == 'smpte170m' &&
             decodedFrames[i].colorSpace.transfer == 'bt709') ||
                (colorSpace.transfer == 'bt709' &&
                 decodedFrames[i].colorSpace.transfer == 'smpte170m'),
            `Frame ${i} color transfer mismatch`)
      } else {
        TEST.assert_eq(
            decodedFrames[i].colorSpace.transfer, colorSpace.transfer,
            `Frame ${i} color transfer mismatch`);
      }
    }

    if (decodedFrames[i].colorSpace.fullRange != null) {
      TEST.assert_eq(
          decodedFrames[i].colorSpace.fullRange, colorSpace.fullRange,
          `Frame ${i} color fullRange mismatch`);
    }
    decodedFrames[i].close();
  }
  TEST.reportSuccess();
}
