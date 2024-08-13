// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

function rgb2yuv(r, g, b) {
  let y = r * .299000 + g * .587000 + b * .114000
  let u = r * -.168736 + g * -.331264 + b * .500000 + 128
  let v = r * .500000 + g * -.418688 + b * -.081312 + 128

  y = Math.round(y);
  u = Math.round(u);
  v = Math.round(v);
  return {
    y, u, v
  }
}

function makeI420_frames() {
  const kYellow = {r: 0xFF, g: 0xFF, b: 0x00};
  const kRed = {r: 0xFF, g: 0x00, b: 0x00};
  const kBlue = {r: 0x00, g: 0x00, b: 0xFF};
  const kGreen = {r: 0x00, g: 0xFF, b: 0x00};
  const kPink = {r: 0xFF, g: 0x78, b: 0xFF};
  const kMagenta = {r: 0xFF, g: 0x00, b: 0xFF};
  const kBlack = {r: 0x00, g: 0x00, b: 0x00};
  const kWhite = {r: 0xFF, g: 0xFF, b: 0xFF};
  const smpte170m = {
    matrix: 'smpte170m',
    primaries: 'smpte170m',
    transfer: 'smpte170m',
    fullRange: false
  };
  const bt709 = {
    matrix: 'bt709',
    primaries: 'bt709',
    transfer: 'bt709',
    fullRange: false
  };

  const result = [];
  const init = {format: 'I420', timestamp: 0, codedWidth: 4, codedHeight: 4};
  const colors =
      [kYellow, kRed, kBlue, kGreen, kMagenta, kBlack, kWhite, kPink];
  const data = new Uint8Array(24);
  for (let colorSpace of [null, smpte170m, bt709]) {
    init.colorSpace = colorSpace;
    result.push(new VideoFrame(data, init));
    for (let color of colors) {
      color = rgb2yuv(color.r, color.g, color.b);
      data.fill(color.y, 0, 16);
      data.fill(color.u, 16, 20);
      data.fill(color.v, 20, 24);
      result.push(new VideoFrame(data, init));
    }
  }
  return result;
}

async function test_frame(frame, colorSpace) {
  const width = frame.visibleRect.width;
  const height = frame.visibleRect.height;
  const frame_description = JSON.stringify({
    format: frame.format,
    width: width,
    height: height,
    codedHeight: frame.codedHeight,
    codedWidth: frame.codedWidth,
    displayHeight: frame.displayHeight,
    displayWidth: frame.displayWidth,
    matrix: frame.colorSpace?.matrix,
    primaries: frame.colorSpace?.primaries,
    transfer: frame.colorSpace?.transfer
  });
  TEST.log(`Test color: ${colorSpace} frame:${frame_description}`);
  const cnv = new OffscreenCanvas(width, height);
  const ctx =
      cnv.getContext('2d', {colorSpace: colorSpace, willReadFrequently: true});

  // Read VideoFrame pixels via copyTo()
  let imageData = ctx.createImageData(width, height);
  let copy_to_buf = imageData.data.buffer;
  let layout = null;
  try {
    const options = {
      rect: {x: 0, y: 0, width: width, height: height},
      format: 'RGBA',
      colorSpace: colorSpace
    };
    layout = await frame.copyTo(copy_to_buf, options);
  } catch (e) {
    TEST.reportFailure(`copyTo() failure: ${e}`);
    return;
  }
  if (layout.length != 1) {
    TEST.skip('Conversion to RGB is not supported by the browser');
    return;
  }

  // Read VideoFrame pixels via drawImage()
  ctx.drawImage(frame, 0, 0, width, height, 0, 0, width, height);
  imageData = ctx.getImageData(0, 0, width, height, {colorSpace: colorSpace});
  let get_image_buf = imageData.data.buffer;

  // Compare!
  const tolerance = 1;
  for (let i = 0; i < copy_to_buf.byteLength; i += 4) {
    compareColors(
        new Uint8Array(copy_to_buf, i, 4), new Uint8Array(get_image_buf, i, 4),
        tolerance, `Mismatch at offset ${i}`);
    if (TEST.finished) {
      break;
    }
  }
}

async function check_predefined_frames() {
  // Test frames constructed from an array buffer.
  // This should be a part of the WPT tests some day.
  for (let frame of makeI420_frames()) {
    await test_frame(frame, 'srgb');
    await test_frame(frame, 'display-p3');
    frame.close();
  }
}

async function main(arg) {
  let source_type = arg.source_type;
  TEST.log('Starting test with arguments: ' + JSON.stringify(arg));
  let source = await createFrameSource(source_type, FRAME_WIDTH, FRAME_HEIGHT);
  if (!source) {
    TEST.skip('Unsupported source: ' + source_type);
    return;
  }

  let frame = await source.getNextFrame();

  await test_frame(frame, 'srgb');
  await test_frame(frame, 'display-p3');
  frame.close();

  await check_predefined_frames();

  source.close();
  TEST.log('Test completed');
}
