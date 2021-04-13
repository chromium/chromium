// META: global=window,dedicatedworker
// META: script=/webcodecs/utils.js

test(t => {
  let image = makeImageBitmap(32, 16);
  let frame = new VideoFrame(image, {timestamp: 10});

  assert_equals(frame.timestamp, 10, 'timestamp');
  assert_equals(frame.duration, null, 'duration');
  assert_equals(frame.visibleRegion.width, 32, 'visibleRegion.width');
  assert_equals(frame.visibleRegion.height, 16, 'visibleRegion.height');
  assert_equals(frame.displayWidth, 32, 'displayWidth');
  assert_equals(frame.displayHeight, 16, 'displayHeight');

  frame.close();
}, 'Test we can construct a VideoFrame.');

test(t => {
  let image = makeImageBitmap(1, 1);
  let frame = new VideoFrame(image, {timestamp: 10});

  assert_equals(frame.visibleRegion.width, 1, 'visibleRegion.width');
  assert_equals(frame.visibleRegion.height, 1, 'visibleRegion.height');
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
  let vfInit = {timestamp: 1234, codedWidth: 4, codedHeight: 2};
  assert_throws_js(TypeError, () => {
    let frame = new VideoFrame('ABCD', [], vfInit);
  }, 'invalid pixel format');

  assert_throws_js(TypeError, () => {
    let frame = new VideoFrame('ARGB', [], {timestamp: 1234});
  }, 'missing coded size');

  function constructFrame(init) {
    let yPlaneData = new Uint8Array([1, 2, 3, 4, 5, 6, 7, 8]);  // 4x2
    let uPlaneData = new Uint8Array([1, 2]);                    // 2x1
    let yPlane = {src: yPlaneData, stride: 4};
    let uPlane = vPlane = {src: uPlaneData, stride: 2};
    let frame = new VideoFrame('I420', [yPlane, uPlane, vPlane], init);
  }

  assert_throws_dom(
      'ConstraintError', () => {constructFrame({
                           timestamp: 1234,
                           codedWidth: Math.pow(2, 32) - 1,
                           codedHeight: Math.pow(2, 32) - 1,
                         })},
      'invalid coded size');
  assert_throws_dom(
      'ConstraintError',
      () => {constructFrame({timestamp: 1234, codedWidth: 4, codedHeight: 0})},
      'invalid coded height');
  assert_throws_dom(
      'ConstraintError',
      () => {constructFrame({timestamp: 1234, codedWidth: 0, codedHeight: 4})},
      'invalid coded width');
  assert_throws_dom(
      'ConstraintError', () => {constructFrame({
                           timestamp: 1234,
                           codedWidth: 4,
                           codedHeight: 2,
                           visibleRegion: {left: 100, top: 100, width: 1,
                                           height: 1}
                         })},
      'invalid visible left/right');
  assert_throws_dom(
      'ConstraintError',
      () => {constructFrame(
          {timestamp: 1234, codedWidth: 4, codedHeight: 2,
           visibleRegion: {left: 0, top: 0, width: 0, height: 2}})},
      'invalid visible width');
  assert_throws_dom(
      'ConstraintError',
      () => {constructFrame(
          {timestamp: 1234, codedWidth: 4, codedHeight: 2,
           visibleRegion: {left: 0, top: 0, width: 2, height: 0}})},
      'invalid visible height');
  assert_throws_js(
      TypeError,
      () => constructFrame({timestamp: 1234,
                            codedWidth: 4,
                            codedHeight: 2,
                            visibleRegion: {left: 0,
                                            top: 0,
                                            width: -100,
                                            height: -100}}),
      'invalid negative visible size');
  assert_throws_js(
      TypeError, () => {constructFrame({
                   timestamp: 1234,
                   codedWidth: 4,
                   codedHeight: 2,
                   displayWidth: Math.pow(2, 32),
                 })},
      'invalid display width');
  assert_throws_js(
      TypeError, () => {constructFrame({
                   timestamp: 1234,
                   codedWidth: 4,
                   codedHeight: 2,
                   displayWidth: Math.pow(2, 32) - 1,
                   displayHeight: Math.pow(2, 32)
                 })},
      'invalid display height');
}, 'Test invalid planar constructed VideoFrames');

test(t => {
  let fmt = 'I420';
  let vfInit = {timestamp: 1234, codedWidth: 4, codedHeight: 2};
  let yPlaneData = new Uint8Array([1, 2, 3, 4, 5, 6, 7, 8]);  // 4x2
  let uPlaneData = new Uint8Array([1, 2]);                    // 2x1
  // TODO(sandersd): Remove |rows| if/when VideoFrame.planes[] is removed.
  // The value is used only by verifyPlane().
  let yPlane = {src: yPlaneData, stride: 4, rows: 2};
  let uPlane = vPlane = {src: uPlaneData, stride: 2, rows: 1};
  let frame = new VideoFrame(fmt, [yPlane, uPlane, vPlane], vfInit);
  assert_equals(frame.planes.length, 3, 'plane count');
  assert_equals(frame.format, fmt, 'plane format');
  verifyPlane(yPlane, frame.planes[0]);
  verifyPlane(uPlane, frame.planes[1]);
  verifyPlane(vPlane, frame.planes[2]);
  frame.close();

  assert_throws_dom('ConstraintError', () => {
    let frame = new VideoFrame(fmt, [yPlane, uPlane], vfInit);
  }, 'too few planes');
  assert_throws_dom('ConstraintError', () => {
    let badYPlane = {...yPlane};
    badYPlane.stride = 1;
    let frame = new VideoFrame(fmt, [badYPlane, uPlane, vPlane], vfInit);
  }, 'y stride too small');
  assert_throws_dom('ConstraintError', () => {
    let badUPlane = {...uPlane};
    badUPlane.stride = 1;
    let frame = new VideoFrame(fmt, [yPlane, badUPlane, vPlane], vfInit);
  }, 'u stride too small');
  assert_throws_dom('ConstraintError', () => {
    let badVPlane = {...vPlane};
    badVPlane.stride = 1;
    let frame = new VideoFrame(fmt, [yPlane, uPlane, badVPlane], vfInit);
  }, 'v stride too small');
  assert_throws_dom('ConstraintError', () => {
    let badYPlane = {...yPlane};
    badYPlane.src = yPlaneData.slice(1, 4);
    let frame = new VideoFrame(fmt, [badYPlane, uPlane, vPlane], vfInit);
  }, 'y plane size too small');
  assert_throws_dom('ConstraintError', () => {
    let badUPlane = {...uPlane};
    badUPlane.src = uPlaneData.slice(1, 1);
    let frame = new VideoFrame(fmt, [yPlane, badUPlane, vPlane], vfInit);
  }, 'u plane size too small');
  assert_throws_dom('ConstraintError', () => {
    let badVPlane = {...vPlane};
    badVPlane.src = uPlaneData.slice(1, 1);
    let frame = new VideoFrame(fmt, [yPlane, uPlane, badVPlane], vfInit);
  }, 'v plane size too small');
}, 'Test planar constructed I420 VideoFrame');

test(t => {
  let fmt = 'I420';
  let vfInit = {timestamp: 1234, codedWidth: 4, codedHeight: 2};
  let yPlaneData = new Uint8Array([1, 2, 3, 4, 5, 6, 7, 8]);  // 4x2
  let uPlaneData = new Uint8Array([1, 2]);                    // 2x1
  // TODO(sandersd): Remove |rows| if/when VideoFrame.planes[] is removed.
  // The value is used only by verifyPlane().
  let yPlane = {src: yPlaneData, stride: 4, rows: 2};
  let uPlane = vPlane = {src: uPlaneData, stride: 2, rows: 1};
  let aPlaneData = yPlaneData.reverse();
  let aPlane = {src: aPlaneData, stride: 4, rows: 2};
  let frame = new VideoFrame(fmt, [yPlane, uPlane, vPlane, aPlane], vfInit);
  assert_equals(frame.planes.length, 4, 'plane count');
  assert_equals(frame.format, fmt, 'plane format');
  verifyPlane(yPlane, frame.planes[0]);
  verifyPlane(uPlane, frame.planes[1]);
  verifyPlane(vPlane, frame.planes[2]);
  verifyPlane(aPlane, frame.planes[3]);
  frame.close();

  // Most constraints are tested as part of I420 above.

  assert_throws_dom('ConstraintError', () => {
    let badAPlane = {...aPlane};
    badAPlane.stride = 1;
    let frame =
        new VideoFrame(fmt, [yPlane, uPlane, vPlane, badAPlane], vfInit);
  }, 'a stride too small');
  assert_throws_dom('ConstraintError', () => {
    let badAPlane = {...yPlane};
    badAPlane.src = aPlaneData.slice(1, 4);
    let frame =
        new VideoFrame(fmt, [yPlane, uPlane, vPlane, badAPlane], vfInit);
  }, 'a plane size too small');
}, 'Test planar constructed I420+Alpha VideoFrame');

test(t => {
  let fmt = 'NV12';
  let vfInit = {timestamp: 1234, codedWidth: 4, codedHeight: 2};
  let yPlaneData = new Uint8Array([1, 2, 3, 4, 5, 6, 7, 8]);  // 4x2
  // TODO(sandersd): Remove |rows| if/when VideoFrame.planes[] is removed.
  // The value is used only by verifyPlane().
  let yPlane = {src: yPlaneData, stride: 4, rows: 2};
  let uvPlaneData = new Uint8Array([1, 2, 3, 4]);
  let uvPlane = {src: uvPlaneData, stride: 4, rows: 1};
  let frame = new VideoFrame(fmt, [yPlane, uvPlane], vfInit);
  assert_equals(frame.planes.length, 2, 'plane count');
  assert_equals(frame.format, fmt, 'plane format');
  verifyPlane(yPlane, frame.planes[0]);
  verifyPlane(uvPlane, frame.planes[1]);
  frame.close();

  assert_throws_dom('ConstraintError', () => {
    let frame = new VideoFrame(fmt, [yPlane], vfInit);
  }, 'too few planes');
  assert_throws_dom('ConstraintError', () => {
    let badYPlane = {...yPlane};
    badYPlane.stride = 1;
    let frame = new VideoFrame(fmt, [badYPlane, uvPlane], vfInit);
  }, 'y stride too small');
  assert_throws_dom('ConstraintError', () => {
    let badUVPlane = {...uvPlane};
    badUVPlane.stride = 2;
    let frame = new VideoFrame(fmt, [yPlane, badUVPlane], vfInit);
  }, 'uv stride too small');
  assert_throws_dom('ConstraintError', () => {
    let badYPlane = {...yPlane};
    badYPlane.src = yPlaneData.slice(1, 4);
    let frame = new VideoFrame(fmt, [badYPlane, uvPlane], vfInit);
  }, 'y plane size too small');
  assert_throws_dom('ConstraintError', () => {
    let badUVPlane = {...uvPlane};
    badUVPlane.src = uvPlaneData.slice(1, 1);
    let frame = new VideoFrame(fmt, [yPlane, badUVPlane], vfInit);
  }, 'u plane size too small');
}, 'Test planar constructed NV12 VideoFrame');

test(t => {
  let vfInit = {timestamp: 1234, codedWidth: 4, codedHeight: 2};
  let argbPlaneData =
      new Uint8Array(new Uint32Array([1, 2, 3, 4, 5, 6, 7, 8]).buffer);
  // TODO(sandersd): Remove |rows| if/when VideoFrame.planes[] is removed.
  // The value is used only by verifyPlane().
  let argbPlane = {src: argbPlaneData, stride: 4 * 4, rows: 2};
  let frame = new VideoFrame('ABGR', [argbPlane], vfInit);
  assert_equals(frame.planes.length, 1, 'plane count');
  assert_equals(frame.format, 'ABGR', 'plane format');
  verifyPlane(argbPlane, frame.planes[0]);
  frame.close();

  frame = new VideoFrame('ARGB', [argbPlane], vfInit);
  assert_equals(frame.planes.length, 1, 'plane count');
  assert_equals(frame.format, 'ARGB', 'plane format');
  verifyPlane(argbPlane, frame.planes[0]);
  frame.close();

  frame = new VideoFrame('XBGR', [argbPlane], vfInit);
  assert_equals(frame.planes.length, 1, 'plane count');
  assert_equals(frame.format, 'XBGR', 'plane format');
  verifyPlane(argbPlane, frame.planes[0]);
  frame.close();

  frame = new VideoFrame('XRGB', [argbPlane], vfInit);
  assert_equals(frame.planes.length, 1, 'plane count');
  assert_equals(frame.format, 'XRGB', 'plane format');
  verifyPlane(argbPlane, frame.planes[0]);
  frame.close();

  ['ABGR', 'ARGB', 'XBGR', 'XRGB'].forEach(fmt => {
    assert_throws_dom('ConstraintError', () => {
      let frame = new VideoFrame(fmt, [], vfInit);
    }, fmt + ': too few planes');
    assert_throws_dom('ConstraintError', () => {
      let badARGBPlane = {...argbPlane};
      badARGBPlane.stride = 1;
      let frame = new VideoFrame(fmt, [badARGBPlane], vfInit);
    }, fmt + ': stride too small');
    assert_throws_dom('ConstraintError', () => {
      let badARGBPlane = {...argbPlane};
      badARGBPlane.src = argbPlaneData.slice(1, 4);
      let frame = new VideoFrame(fmt, [badARGBPlane], vfInit);
    }, fmt + ': plane size too small');
  });
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
  let fmt = 'I420';
  let vfInit = {timestamp: 1234, codedWidth: 4, codedHeight: 2};
  let yPlaneData = new Uint8Array([1, 2, 3, 4, 5, 6, 7, 8]);  // 4x2
  let uPlaneData = new Uint8Array([1, 2]);                    // 2x1
  let yPlane = {src: yPlaneData, stride: 4};
  let uPlane = vPlane = {src: uPlaneData, stride: 2};
  let aPlaneData = yPlaneData.reverse();
  let aPlane = {src: aPlaneData, stride: 4};
  let frame = new VideoFrame(fmt, [yPlane, uPlane, vPlane, aPlane], vfInit);
  assert_equals(frame.planes.length, 4, 'plane count');
  assert_equals(frame.format, fmt, 'plane format');

  let alpha_frame_copy = new VideoFrame(frame, {alpha: 'keep'});
  assert_equals(alpha_frame_copy.format, fmt, 'plane format');
  assert_equals(alpha_frame_copy.planes.length, 4, 'plane count');

  let opaque_frame_copy = new VideoFrame(frame, {alpha: 'discard'});
  assert_equals(opaque_frame_copy.format, fmt, 'plane format');
  assert_equals(opaque_frame_copy.planes.length, 3, 'plane count');

  frame.close();
  alpha_frame_copy.close();
  opaque_frame_copy.close();
}, 'Test I420+Alpha VideoFrame and alpha={keep,discard}');

test(t => {
  let vfInit = {timestamp: 1234, codedWidth: 4, codedHeight: 2};
  let argbPlaneData =
      new Uint8Array(new Uint32Array([1, 2, 3, 4, 5, 6, 7, 8]).buffer);
  let argbPlane = {src: argbPlaneData, stride: 4 * 4};
  let frame = new VideoFrame('ABGR', [argbPlane], vfInit);
  assert_equals(frame.planes.length, 1, 'plane count');
  assert_equals(frame.format, 'ABGR', 'plane format');

  let alpha_frame_copy = new VideoFrame(frame, {alpha: 'keep'});
  assert_equals(alpha_frame_copy.format, 'ABGR', 'plane format');

  let opaque_frame_copy = new VideoFrame(frame, {alpha: 'discard'});
  assert_equals(opaque_frame_copy.format, 'XBGR', 'plane format');

  alpha_frame_copy.close();
  opaque_frame_copy.close();
  frame.close();

  frame = new VideoFrame('ARGB', [argbPlane], vfInit);
  assert_equals(frame.planes.length, 1, 'plane count');
  assert_equals(frame.format, 'ARGB', 'plane format');

  alpha_frame_copy = new VideoFrame(frame, {alpha: 'keep'});
  assert_equals(alpha_frame_copy.format, 'ARGB', 'plane format');

  opaque_frame_copy = new VideoFrame(frame, {alpha: 'discard'});
  assert_equals(opaque_frame_copy.format, 'XRGB', 'plane format');

  alpha_frame_copy.close();
  opaque_frame_copy.close();
  frame.close();
}, 'Test ABGR, ARGB VideoFrames with alpha={keep,discard}');

test(t => {
  let canvas = makeOffscreenCanvas(16, 16, {alpha: true});
  let frame = new VideoFrame(canvas);
  assert_true(
      frame.format == 'ABGR' || frame.format == 'ARGB' ||
          (frame.format == 'I420' && frame.planes.length == 4),
      'plane format should have alpha: ' + frame.format);
  frame.close();

  frame = new VideoFrame(canvas, {alpha: 'discard'});
  assert_true(
      frame.format != 'ABGR' || frame.format != 'ARGB',
      'plane format should be opaque');
  frame.close();
}, 'Test a VideoFrame constructed from canvas can drop the alpha channel.');
