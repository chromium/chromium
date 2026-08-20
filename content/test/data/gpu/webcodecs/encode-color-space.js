// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

function isRec709(colorSpace, fullRange) {
  return colorSpace.primaries === 'bt709' &&
      (colorSpace.transfer === 'bt709' ||
       colorSpace.transfer === 'iec61966-2-1') &&
      colorSpace.matrix === 'bt709' && colorSpace.fullRange === fullRange;
}

function isSRGB(colorSpace) {
  return colorSpace.primaries === 'bt709' &&
      colorSpace.transfer === 'iec61966-2-1' && colorSpace.matrix === 'rgb' &&
      colorSpace.fullRange === true;
}

function isRec601(colorSpace, fullRange) {
  return colorSpace.primaries === 'smpte170m' &&
      (colorSpace.transfer === 'smpte170m' ||
       colorSpace.transfer === 'bt709') &&
      colorSpace.matrix === 'smpte170m' && colorSpace.fullRange === fullRange;
}

function makePixelArray(byteLength) {
  let data = new Uint8Array(byteLength);
  for (let i = 0; i < byteLength; i++) {
    data[i] = i;
  }
  return data;
}

function arrayBufferToBase64(buffer) {
  const bytes = new Uint8Array(buffer);
  const binString = Array.from(bytes, (byte) =>
    String.fromCharCode(byte),
  ).join("");
  return window.btoa(binString);
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
      init.colorSpace = {
        matrix: 'smpte170m',
        primaries: 'smpte170m',
        transfer: 'smpte170m',
        fullRange: false
      };
      return new VideoFrame(data, {...init, format: 'I420'});
    }
    case 'RGBA': {
      const rgbaByteLength = 4 * FRAME_WIDTH * FRAME_HEIGHT;
      let data = makePixelArray(rgbaByteLength);
      init.colorSpace = {
        matrix: 'rgb',
        primaries: 'bt709',
        transfer: 'iec61966-2-1',
        fullRange: true
      };
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
    makeFrame('I420', 0 * frameDuration),
    makeFrame('I420', 1 * frameDuration),
    makeFrame('RGBA', 2 * frameDuration),
    makeFrame('RGBA', 3 * frameDuration),
  ];
  let outputChunks = [];
  let outputMetadata = [];
  let errors = 0;

  const origReportFailure = TEST.reportFailure.bind(TEST);
  TEST.reportFailure = function(error) {
    for (let i = 0; i < outputMetadata.length; i++) {
      TEST.log(
          `outputMetadata[${i}].decoderConfig.colorSpace: ` +
          JSON.stringify(outputMetadata[i]?.decoderConfig?.colorSpace));
    }
    origReportFailure(error);
  };

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
  }
  await encoder.flush();
  encoder.close();

  TEST.assert_eq(errors, 0, 'Encoding errors occurred during the test');
  TEST.assert_eq(outputChunks.length, 4, 'Unexpected number of outputs');
  TEST.assert_eq(
      outputMetadata.length, 4, 'Unexpected number of output metadata');

  // I420 input should preserve rec601 limited range.
  TEST.assert_eq(inputFrames[0].format, 'I420', 'inputs[0] is I420');
  TEST.assert(
      isRec601(inputFrames[0].colorSpace, /*fullRange=*/ false),
      'inputs[0] is rec601 limited');
  TEST.assert_eq(outputChunks[0].type, 'key', 'outputs[0] is key');
  TEST.assert(
      'decoderConfig' in outputMetadata[0], 'metadata[0] has decoderConfig');
  TEST.assert(
      isRec601(
          outputMetadata[0].decoderConfig.colorSpace, /*fullRange=*/ false),
      'metadata[0] is rec601 limited');

  // Next output may or may not be a key frame w/ metadata (up to
  // encoder). Corresponding input is still I420 rec601, so if metadata is
  // given, we expect same colorSpace as for the previous frame.
  TEST.assert_eq(inputFrames[1].format, 'I420', 'inputs[1] is I420');
  TEST.assert(
      isRec601(inputFrames[1].colorSpace, /*fullRange=*/ false),
      'inputs[1] is rec601 limited');
  if ('decoderConfig' in outputMetadata[1]) {
    TEST.assert(
        isRec601(
            outputMetadata[1].decoderConfig.colorSpace, /*fullRange=*/ false),
        'metadata[1] is rec601 limited');
  }

  // Next output should be a key frame and have accompanying metadata
  // because the corresponding input format changed to RGBA (sRGB), which
  // converts to YUV w/ rec709 full range during encoding.
  TEST.assert_eq(inputFrames[2].format, 'RGBA', 'inputs[2] is RGBA');
  TEST.assert(isSRGB(inputFrames[2].colorSpace), 'inputs[2] is sRGB');

  TEST.assert(outputChunks[2].type == 'key', 'outputs[2] is key');
  TEST.assert(
      'decoderConfig' in outputMetadata[2], 'metadata[2] has decoderConfig');
  TEST.assert(
      isRec709(outputMetadata[2].decoderConfig.colorSpace, /*fullRange=*/ true),
      'metadata[2] is rec709 full');

  // Next output may or may not be a key frame w/ metadata (up to
  // encoder). Corresponding input is still RGBA sRGB, so if metadata is
  // given, we expect same colorSpace as for the previous frame.
  TEST.assert_eq(inputFrames[3].format, 'RGBA', 'inputs[3] is RGBA');
  TEST.assert(isSRGB(inputFrames[3].colorSpace), 'inputs[3] is sRGB');
  if ('decoderConfig' in outputMetadata[3]) {
    TEST.assert(
        isRec709(
            outputMetadata[3].decoderConfig.colorSpace, /*fullRange=*/ true),
        'metadata[3] is rec709 full');
  }

  for (let frame of inputFrames) {
    frame.close();
  }

  if (TEST.success) {
    TEST.reportSuccess();
  }
}
