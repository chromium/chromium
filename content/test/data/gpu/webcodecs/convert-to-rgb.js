// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

async function validateFourColorsBytes(frame) {
  const tolerance = 8;
  const m = 4;
  let expected_xy_color = [
    // Left-top yellow
    {x: m, y: m, r: 255, g: 255, b: 0},
    // Right-top red
    {x: frame.displayWidth - m, y: m, r: 255, g: 0, b: 0},
    // Left-bottom blue
    {x: m, y: frame.displayHeight - m, r: 0, g: 0, b: 255},
    // Right-bottom green
    {x: frame.displayWidth - m, y: frame.displayHeight - m, r: 0, g: 255, b: 0},
  ];

  for (let test of expected_xy_color) {
    let options = {
      rect: {x: test.x, y: test.y, width: 1, height: 1},
      format: 'RGBA'
    };
    let size = frame.allocationSize(options);
    let buffer = new ArrayBuffer(size);
    let layout = await frame.copyTo(buffer, options);
    let view = new DataView(buffer);

    let rgb = {
      r: view.getUint8(layout[0].offset),
      g: view.getUint8(layout[0].offset + 1),
      b: view.getUint8(layout[0].offset + 2)
    };

    let message = `Test x:${test.x} y:${test.y}` +
        ` expected: ${JSON.stringify({r: test.r, g: test.g, b: test.b})}` +
        ` actual: ${JSON.stringify(rgb)}` +
        ` original format: ${frame.format}`;
    TEST.log(message);
    TEST.assert(Math.abs(rgb.r - test.r) < tolerance, 'RED mismatch');
    TEST.assert(Math.abs(rgb.g - test.g) < tolerance, 'GREEN mismatch');
    TEST.assert(Math.abs(rgb.b - test.b) < tolerance, 'BLUE mismatch');
  }
}

async function main(arg) {
  let source_type = arg.source_type;
  TEST.log('Starting test with arguments: ' + JSON.stringify(arg));
  let source = await createFrameSource(source_type, 320, 240);
  if (!source) {
    TEST.skip('Unsupported source: ' + source_type);
    return;
  }

  let frame = await source.getNextFrame();

  // Read the frame as RGB into ImageData to put it on the canvas
  const cnv = document.getElementById('cnv');
  cnv.width = frame.codedWidth;
  cnv.height = frame.codedHeight;
  const ctx = cnv.getContext('2d');

  const imageData = ctx.createImageData(cnv.width, cnv.height);
  const buffer = imageData.data;
  let layout = null;
  try {
    layout = await frame.copyTo(buffer, {format: 'RGBA', colorSpace: 'srgb'});
  } catch (e) {
    TEST.reportFailure(`copyTo() failure: ${e}`);
    return;
  }
  if (layout.length != 1) {
    TEST.skip('Conversion to RGB is not supported by the browser');
    return;
  }
  ctx.putImageData(imageData, 0, 0);

  // Validate pixels
  if (!arg.validate_camera_frames && source_type == 'camera') {
    TEST.log('Skip copyTo result validation');
  } else {
    await validateFourColorsBytes(frame);
  }

  frame.close();
  source.close();
  TEST.log('Test completed');
}
