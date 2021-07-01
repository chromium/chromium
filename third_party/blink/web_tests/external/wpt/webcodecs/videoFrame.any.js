// META: global=window,dedicatedworker
// META: script=/webcodecs/utils.js

test(t => {
  let image = makeImageBitmap(32, 16);
  let frame = new VideoFrame(image, {timestamp: 10});

  assert_equals(frame.timestamp, 10, 'timestamp');
  assert_equals(frame.duration, null, 'duration');
  assert_equals(frame.visibleRect.width, 32, 'visibleRect.width');
  assert_equals(frame.visibleRect.height, 16, 'visibleRect.height');
  assert_equals(frame.displayWidth, 32, 'displayWidth');
  assert_equals(frame.displayHeight, 16, 'displayHeight');

  frame.close();
}, 'Test we can construct a VideoFrame.');

test(t => {
  let image = makeImageBitmap(32, 16);
  let frame = new VideoFrame(image, {timestamp: -10});
  assert_equals(frame.timestamp, -10, 'timestamp');
  frame.close();
}, 'Test we can construct a VideoFrame with a negative timestamp.');

test(t => {
  let image = makeImageBitmap(1, 1);
  let frame = new VideoFrame(image, {timestamp: 10});

  assert_equals(frame.visibleRect.width, 1, 'visibleRect.width');
  assert_equals(frame.visibleRect.height, 1, 'visibleRect.height');
  assert_equals(frame.displayWidth, 1, 'displayWidth');
  assert_equals(frame.displayHeight, 1, 'displayHeight');

  frame.close();
}, 'Test we can construct an odd-sized VideoFrame.');

test(t => {
  let image = makeImageBitmap(32, 16);
  let frame = new VideoFrame(image, {timestamp: 0});

  // TODO(sandersd): This would be more clear as RGBA, but conversion has
  // not be specified (or implemented) yet.
  if (frame.format !== 'I420') {
    return;
  }
  assert_equals(frame.planes.length, 3, 'number of planes');

  // Validate Y plane metadata.
  let yPlane = frame.planes[0];
  let yStride = yPlane.stride;
  let yRows = yPlane.rows;
  let yLength = yPlane.length;

  // Required minimums to contain the visible data.
  assert_greater_than_equal(yRows, 16, 'Y plane rows');
  assert_greater_than_equal(yStride, 32, 'Y plane stride');
  assert_greater_than_equal(yLength, 32 * 16, 'Y plane length');

  // Not required by spec, but sets limit at 50% padding per dimension.
  assert_less_than_equal(yRows, 32, 'Y plane rows');
  assert_less_than_equal(yStride, 64, 'Y plane stride');
  assert_less_than_equal(yLength, 32 * 64, 'Y plane length');

  // Validate Y plane data.
  let buffer = new ArrayBuffer(yLength);
  let view = new Uint8Array(buffer);
  frame.planes[0].readInto(view);

  // TODO(sandersd): This probably needs to be fuzzy unless we can make
  // guarantees about the color space.
  assert_equals(view[0], 94, 'Y value at (0, 0)');

  frame.close();
}, 'Test we can read planar data from a VideoFrame.');

test(t => {
  let image = makeImageBitmap(32, 16);
  let frame = new VideoFrame(image, {timestamp: 0});

  // TODO(sandersd): This would be more clear as RGBA, but conversion has
  // not be specified (or implemented) yet.
  if (frame.format !== 'I420') {
    return;
  }

  assert_equals(frame.planes.length, 3, 'number of planes');

  // Attempt to read Y plane data, but close the frame first.
  let yPlane = frame.planes[0];
  let yLength = yPlane.length;
  frame.close();

  let buffer = new ArrayBuffer(yLength);
  let view = new Uint8Array(buffer);
  assert_throws_dom('InvalidStateError', () => yPlane.readInto(view));
}, 'Test we cannot read planar data from a closed VideoFrame.');

test(t => {
  let image = makeImageBitmap(32, 16);

  image.close();

  assert_throws_dom('InvalidStateError', () => {
    let frame = new VideoFrame(image, {timestamp: 10});
  })
}, 'Test constructing VideoFrames from closed ImageBitmap throws.');

test(t => {
  assert_throws_js(
      TypeError,
      () => new VideoFrame(
          new Uint8Array(1),
          {format: 'ABCD', timestamp: 1234, codedWidth: 4, codedHeight: 2}),
      'invalid pixel format');

  assert_throws_js(
      TypeError,
      () =>
          new VideoFrame(new Uint32Array(1), {format: 'RGBA', timestamp: 1234}),
      'missing coded size');

  function constructFrame(init) {
    let data = new Uint8Array([
      1, 2, 3, 4, 5, 6, 7, 8,  // y
      1, 2,                    // u
      1, 2,                    // v
    ]);
    return new VideoFrame(data, {...init, format: 'I420'});
  }

  assert_throws_js(
      TypeError, () => constructFrame({
                   timestamp: 1234,
                   codedWidth: Math.pow(2, 32) - 1,
                   codedHeight: Math.pow(2, 32) - 1,
                 }),
      'invalid coded size');
  assert_throws_js(
      TypeError,
      () => constructFrame({timestamp: 1234, codedWidth: 4, codedHeight: 0}),
      'invalid coded height');
  assert_throws_js(
      TypeError,
      () => constructFrame({timestamp: 1234, codedWidth: 0, codedHeight: 4}),
      'invalid coded width');
  assert_throws_js(
      TypeError, () => constructFrame({
                   timestamp: 1234,
                   codedWidth: 4,
                   codedHeight: 2,
                   visibleRect: {x: 100, y: 100, width: 1, height: 1}
                 }),
      'invalid visible left/right');
  assert_throws_js(
      TypeError, () => constructFrame({
                   timestamp: 1234,
                   codedWidth: 4,
                   codedHeight: 2,
                   visibleRect: {x: 0, y: 0, width: 0, height: 2}
                 }),
      'invalid visible width');
  assert_throws_js(
      TypeError, () => constructFrame({
                   timestamp: 1234,
                   codedWidth: 4,
                   codedHeight: 2,
                   visibleRect: {x: 0, y: 0, width: 2, height: 0}
                 }),
      'invalid visible height');
  assert_throws_js(
      TypeError, () => constructFrame({
                   timestamp: 1234,
                   codedWidth: 4,
                   codedHeight: 2,
                   visibleRect: {x: 0, y: 0, width: -100, height: -100}
                 }),
      'invalid negative visible size');
  assert_throws_js(
      TypeError, () => constructFrame({
                   timestamp: 1234,
                   codedWidth: 4,
                   codedHeight: 2,
                   displayWidth: Math.pow(2, 32),
                 }),
      'invalid display width');
  assert_throws_js(
      TypeError, () => constructFrame({
                   timestamp: 1234,
                   codedWidth: 4,
                   codedHeight: 2,
                   displayWidth: Math.pow(2, 32) - 1,
                   displayHeight: Math.pow(2, 32)
                 }),
      'invalid display height');
}, 'Test invalid planar constructed VideoFrames');

test(t => {
  let fmt = 'I420';
  let vfInit = {format: fmt, timestamp: 1234, codedWidth: 4, codedHeight: 2};
  let data = new Uint8Array([
    1, 2, 3, 4, 5, 6, 7, 8,  // y
    1, 2,                    // u
    1, 2,                    // v
  ]);
  let frame = new VideoFrame(data, vfInit);
  assert_equals(frame.planes.length, 3, 'plane count');
  assert_equals(frame.format, fmt, 'plane format');
  verifyPlane({stride: 4, rows: 2, src: data.slice(0, 8)}, frame.planes[0]);
  verifyPlane({stride: 2, rows: 1, src: data.slice(8, 10)}, frame.planes[1]);
  verifyPlane({stride: 2, rows: 1, src: data.slice(10, 12)}, frame.planes[2]);
  frame.close();

  let y = {offset: 0, stride: 4};
  let u = {offset: 8, stride: 2};
  let v = {offset: 10, stride: 2};

  assert_throws_js(TypeError, () => {
    let y = {offset: 0, stride: 1};
    let frame = new VideoFrame(data, {...vfInit, layout: [y, u, v]});
  }, 'y stride too small');
  assert_throws_js(TypeError, () => {
    let u = {offset: 8, stride: 1};
    let frame = new VideoFrame(data, {...vfInit, layout: [y, u, v]});
  }, 'u stride too small');
  assert_throws_js(TypeError, () => {
    let v = {offset: 10, stride: 1};
    let frame = new VideoFrame(data, {...vfInit, layout: [y, u, v]});
  }, 'v stride too small');
  assert_throws_js(TypeError, () => {
    let frame = new VideoFrame(data.slice(0, 8), vfInit);
  }, 'data too small');
}, 'Test planar constructed I420 VideoFrame');

test(t => {
  let fmt = 'I420A';
  let vfInit = {format: fmt, timestamp: 1234, codedWidth: 4, codedHeight: 2};
  let data = new Uint8Array([
    1, 2, 3, 4, 5, 6, 7, 8,  // y
    1, 2,                    // u
    1, 2,                    // v
    8, 7, 6, 5, 4, 3, 2, 1,  // a
  ]);
  let frame = new VideoFrame(data, vfInit);
  assert_equals(frame.planes.length, 4, 'plane count');
  assert_equals(frame.format, fmt, 'plane format');
  verifyPlane({stride: 4, rows: 2, src: data.slice(0, 8)}, frame.planes[0]);
  verifyPlane({stride: 2, rows: 1, src: data.slice(8, 10)}, frame.planes[1]);
  verifyPlane({stride: 2, rows: 1, src: data.slice(10, 12)}, frame.planes[2]);
  verifyPlane({stride: 4, rows: 2, src: data.slice(12, 20)}, frame.planes[3]);
  frame.close();

  // Most constraints are tested as part of I420 above.

  let y = {offset: 0, stride: 4};
  let u = {offset: 8, stride: 2};
  let v = {offset: 10, stride: 2};
  let a = {offset: 12, stride: 4};

  assert_throws_js(TypeError, () => {
    let a = {offset: 12, stride: 1};
    let frame = new VideoFrame(data, {...vfInit, layout: [y, u, v, a]});
  }, 'a stride too small');
  assert_throws_js(TypeError, () => {
    let frame = new VideoFrame(data.slice(0, 12), vfInit);
  }, 'data too small');
}, 'Test planar constructed I420+Alpha VideoFrame');

test(t => {
  let fmt = 'NV12';
  let vfInit = {format: fmt, timestamp: 1234, codedWidth: 4, codedHeight: 2};
  let data = new Uint8Array([
    1, 2, 3, 4, 5, 6, 7, 8,  // y
    1, 2, 3, 4,              // uv
  ]);
  let frame = new VideoFrame(data, vfInit);
  assert_equals(frame.planes.length, 2, 'plane count');
  assert_equals(frame.format, fmt, 'plane format');
  verifyPlane({stride: 4, rows: 2, src: data.slice(0, 8)}, frame.planes[0]);
  verifyPlane({stride: 4, rows: 1, src: data.slice(8, 12)}, frame.planes[1]);
  frame.close();

  let y = {offset: 0, stride: 4};
  let uv = {offset: 8, stride: 4};

  assert_throws_js(TypeError, () => {
    let y = {offset: 0, stride: 1};
    let frame = new VideoFrame(data, {...vfInit, layout: [y, uv]});
  }, 'y stride too small');
  assert_throws_js(TypeError, () => {
    let uv = {offset: 8, stride: 1};
    let frame = new VideoFrame(data, {...vfInit, layout: [y, uv]});
  }, 'uv stride too small');
  assert_throws_js(TypeError, () => {
    let frame = new VideoFrame(data.slice(0, 8), vfInit);
  }, 'data too small');
}, 'Test planar constructed NV12 VideoFrame');

test(t => {
  let vfInit = {timestamp: 1234, codedWidth: 4, codedHeight: 2};
  let data = new Uint8Array([
    1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12, 13, 14, 15, 16,
    17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32,
  ]);
  let frame = new VideoFrame(data, {...vfInit, format: 'RGBA'});
  assert_equals(frame.planes.length, 1, 'plane count');
  assert_equals(frame.format, 'RGBA', 'plane format');
  verifyPlane({stride: 16, rows: 2, src: data}, frame.planes[0]);
  frame.close();

  // TODO(sandersd): verifyPlane() should not check alpha bytes.
  frame = new VideoFrame(data, {...vfInit, format: 'RGBX'});
  assert_equals(frame.planes.length, 1, 'plane count');
  assert_equals(frame.format, 'RGBX', 'plane format');
  verifyPlane({stride: 16, rows: 2, src: data}, frame.planes[0]);
  frame.close();

  frame = new VideoFrame(data, {...vfInit, format: 'BGRA'});
  assert_equals(frame.planes.length, 1, 'plane count');
  assert_equals(frame.format, 'BGRA', 'plane format');
  verifyPlane({stride: 16, rows: 2, src: data}, frame.planes[0]);
  frame.close();

  // TODO(sandersd): verifyPlane() should not check alpha bytes.
  frame = new VideoFrame(data, {...vfInit, format: 'BGRX'});
  assert_equals(frame.planes.length, 1, 'plane count');
  assert_equals(frame.format, 'BGRX', 'plane format');
  verifyPlane({stride: 16, rows: 2, src: data}, frame.planes[0]);
  frame.close();
}, 'Test planar constructed RGB VideoFrames');

test(t => {
  let image = makeImageBitmap(32, 16);
  let frame = new VideoFrame(image, {timestamp: 0});
  assert_true(!!frame);

  frame_copy = new VideoFrame(frame);
  assert_equals(frame.format, frame_copy.format);
  assert_equals(frame.timestamp, frame_copy.timestamp);
  assert_equals(frame.codedWidth, frame_copy.codedWidth);
  assert_equals(frame.codedHeight, frame_copy.codedHeight);
  assert_equals(frame.displayWidth, frame_copy.displayWidth);
  assert_equals(frame.displayHeight, frame_copy.displayHeight);
  assert_equals(frame.duration, frame_copy.duration);
  frame_copy.close();

  frame_copy = new VideoFrame(frame, {duration: 1234});
  assert_equals(frame.timestamp, frame_copy.timestamp);
  assert_equals(frame_copy.duration, 1234);
  frame_copy.close();

  frame_copy = new VideoFrame(frame, {timestamp: 1234, duration: 456});
  assert_equals(frame_copy.timestamp, 1234);
  assert_equals(frame_copy.duration, 456);
  frame_copy.close();

  frame.close();
}, 'Test VideoFrame constructed VideoFrame');

test(t => {
  let canvas = makeOffscreenCanvas(16, 16);
  let frame = new VideoFrame(canvas);
  assert_equals(frame.displayWidth, 16);
  assert_equals(frame.displayHeight, 16);
  frame.close();
}, 'Test we can construct a VideoFrame from an offscreen canvas.');

test(t => {
  let fmt = 'I420A';
  let vfInit = {format: fmt, timestamp: 1234, codedWidth: 4, codedHeight: 2};
  let data = new Uint8Array([
    1, 2, 3, 4, 5, 6, 7, 8,  // y
    1, 2,                    // u
    1, 2,                    // v
    8, 7, 6, 5, 4, 3, 2, 1,  // a
  ]);
  let frame = new VideoFrame(data, vfInit);
  assert_equals(frame.planes.length, 4, 'plane count');
  assert_equals(frame.format, fmt, 'plane format');

  let alpha_frame_copy = new VideoFrame(frame, {alpha: 'keep'});
  assert_equals(alpha_frame_copy.format, 'I420A', 'plane format');
  assert_equals(alpha_frame_copy.planes.length, 4, 'plane count');

  let opaque_frame_copy = new VideoFrame(frame, {alpha: 'discard'});
  assert_equals(opaque_frame_copy.format, 'I420', 'plane format');
  assert_equals(opaque_frame_copy.planes.length, 3, 'plane count');

  frame.close();
  alpha_frame_copy.close();
  opaque_frame_copy.close();
}, 'Test I420A VideoFrame and alpha={keep,discard}');

test(t => {
  let vfInit = {timestamp: 1234, codedWidth: 4, codedHeight: 2};
  let data = new Uint8Array([
    1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12, 13, 14, 15, 16,
    17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32,
  ]);
  let frame = new VideoFrame(data, {...vfInit, format: 'RGBA'});
  assert_equals(frame.planes.length, 1, 'plane count');
  assert_equals(frame.format, 'RGBA', 'plane format');

  let alpha_frame_copy = new VideoFrame(frame, {alpha: 'keep'});
  assert_equals(alpha_frame_copy.format, 'RGBA', 'plane format');

  let opaque_frame_copy = new VideoFrame(frame, {alpha: 'discard'});
  assert_equals(opaque_frame_copy.format, 'RGBX', 'plane format');

  alpha_frame_copy.close();
  opaque_frame_copy.close();
  frame.close();

  frame = new VideoFrame(data, {...vfInit, format: 'BGRA'});
  assert_equals(frame.planes.length, 1, 'plane count');
  assert_equals(frame.format, 'BGRA', 'plane format');

  alpha_frame_copy = new VideoFrame(frame, {alpha: 'keep'});
  assert_equals(alpha_frame_copy.format, 'BGRA', 'plane format');

  opaque_frame_copy = new VideoFrame(frame, {alpha: 'discard'});
  assert_equals(opaque_frame_copy.format, 'BGRX', 'plane format');

  alpha_frame_copy.close();
  opaque_frame_copy.close();
  frame.close();
}, 'Test RGBA, BGRA VideoFrames with alpha={keep,discard}');

test(t => {
  let canvas = makeOffscreenCanvas(16, 16, {alpha: true});
  let frame = new VideoFrame(canvas);
  assert_true(
      frame.format == 'RGBA' || frame.format == 'BGRA' ||
          frame.format == 'I420A',
      'plane format should have alpha: ' + frame.format);
  frame.close();

  frame = new VideoFrame(canvas, {alpha: 'discard'});
  assert_true(
      frame.format == 'RGBX' || frame.format == 'BGRX' ||
          frame.format == 'I420',
      'plane format should not have alpha: ' + frame.format);
  frame.close();
}, 'Test a VideoFrame constructed from canvas can drop the alpha channel.');
