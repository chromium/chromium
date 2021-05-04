// META: global=window,dedicatedworker

function makeI420_4x2() {
  let yData = new Uint8Array([1, 2, 3, 4, 5, 6, 7, 8]);
  let uData = new Uint8Array([9, 10]);
  let vData = new Uint8Array([11, 12]);
  let planes = [{src: yData, stride: 4},
                {src: uData, stride: 2},
                {src: vData, stride: 2}];
  let init = {timestamp: 0,
              codedWidth: 4,
              codedHeight: 2};
  return new VideoFrame('I420', planes, init);
}

function makeRGBA_2x2() {
  let planes = [{src: new Uint8Array([1, 2, 3, 4, 5, 6, 7, 8,
                                      9, 10, 11, 12, 13, 14, 15, 16]),
                 stride: 8}];
  let init = {timestamp: 0,
              codedWidth: 2,
              codedHeight: 2};
  // TODO(sandersd): Should be RGBA but that's missing in the IDL right now.
  return new VideoFrame('ABGR', planes, init);
}

promise_test(async t => {
  let frame = makeI420_4x2();
  let buf = new Uint8Array(12);
  let expected = new Uint8Array([
      1, 2, 3, 4, 5, 6, 7, 8,  // y
      9, 10,                   // u
      11, 12                   // v
  ]);
  assert_equals(frame.allocationSize(), 12, 'allocationSize()');
  await frame.readInto(buf);
  assert_array_equals(buf, expected, 'destination buffer contents');
}, 'Test I420 frame.');

promise_test(async t => {
  let frame = makeRGBA_2x2();
  let buf = new Uint8Array(16);
  let expected = new Uint8Array([
      1, 2, 3, 4, 5, 6, 7, 8,
      9, 10, 11, 12, 13, 14, 15, 16
  ]);
  assert_equals(frame.allocationSize(), 16, 'allocationSize()');
  await frame.readInto(buf);
  assert_array_equals(buf, expected, 'destination buffer contents');
}, 'Test RGBA frame.');

promise_test(async t => {
  let frame = makeI420_4x2();
  let buf = new Uint8Array(11);
  let expected = new Uint8Array([
      1, 2, 3, 4, 5, 6, 7, 8,  // y
      9, 10,                   // u
      11, 12                   // v
  ]);
  assert_equals(frame.allocationSize(), 12, 'allocationSize()');
  await promise_rejects_dom(t, 'ConstraintError', frame.readInto(buf));
}, 'Test undersized buffer.');

promise_test(async t => {
  let frame = makeI420_4x2();
  let buf = new Uint8Array(12);
  let options = {layout: [{}, {}, {}]};
  let expected = new Uint8Array([
      1, 2, 3, 4, 5, 6, 7, 8,  // y
      9, 10,                   // u
      11, 12                   // v
  ]);
  assert_equals(frame.allocationSize(options), 12, 'allocationSize()');
  await frame.readInto(buf, options);
  assert_array_equals(buf, expected, 'destination buffer contents');
}, 'Test layout can be empty.');

promise_test(async t => {
  let frame = makeI420_4x2();
  let buf = new Uint8Array(12);
  let options = {layout: [{}]};
  assert_throws_dom('ConstraintError', () => frame.allocationSize(options));
  await promise_rejects_dom(t, 'ConstraintError', frame.readInto(buf, options));
}, 'Test incorrect plane count.');

promise_test(async t => {
  let frame = makeI420_4x2();
  let buf = new Uint8Array(12);
  let options = {layout: [{offset: 4, stride: 4},
                          {offset: 0, stride: 2},
                          {offset: 2, stride: 2}]};
  let expected = new Uint8Array([
      9, 10,       // u
      11, 12,      // v
      1, 2, 3, 4,  // y
      5, 6, 7, 8,
  ]);
  assert_equals(frame.allocationSize(options), 12, 'allocationSize()');
  await frame.readInto(buf, options);
  assert_array_equals(buf, expected, 'destination buffer contents');
}, 'Test stride and offset work.');

promise_test(async t => {
  let frame = makeI420_4x2();
  let buf = new Uint8Array(20);
  let options = {layout: [{offset: 9, stride: 5},
                          {offset: 1, stride: 3},
                          {offset: 5, stride: 3}]};
  let expected = new Uint8Array([
      0,
      9, 10, 0,       // u
      0,
      11, 12, 0,      // v
      0,
      1, 2, 3, 4, 0,  // y
      5, 6, 7, 8, 0,
      0
  ]);
  assert_equals(frame.allocationSize(options), 19, 'allocationSize()');
  await frame.readInto(buf, options);
  assert_array_equals(buf, expected, 'destination buffer contents');
}, 'Test stride and offset with padding.');

promise_test(async t => {
  let frame = makeI420_4x2();
  let buf = new Uint8Array(12);
  let options = {layout: [{offset: 0, stride: 1},
                          {offset: 8, stride: 2},
                          {offset: 10, stride: 2}]};
  assert_throws_dom('ConstraintError', () => frame.allocationSize(options));
  await promise_rejects_dom(t, 'ConstraintError', frame.readInto(buf, options));
}, 'Test invalid stride.');

promise_test(async t => {
  let frame = makeI420_4x2();
  let buf = new Uint8Array(12);
  let options = {layout: [{offset: 0, stride: 4},
                          {offset: 8, stride: 2},
                          {offset: 10}]};
  assert_throws_dom('ConstraintError', () => frame.allocationSize(options));
  await promise_rejects_dom(t, 'ConstraintError', frame.readInto(buf, options));
}, 'Test missing stride.');


promise_test(async t => {
  let frame = makeI420_4x2();
  let buf = new Uint8Array(12);
  let options = {layout: [{offset: 0, stride: 4},
                          {offset: 8, stride: 2},
                          {stride: 2}]};
  assert_throws_dom('ConstraintError', () => frame.allocationSize(options));
  await promise_rejects_dom(t, 'ConstraintError', frame.readInto(buf, options));
}, 'Test missing offset.');

promise_test(async t => {
  let frame = makeI420_4x2();
  let buf = new Uint8Array(12);
  let options = {layout: [{offset: 0, stride: 4},
                          {offset: 8, stride: 2},
                          {offset: 2 ** 32 - 2, stride: 2}]};
  assert_throws_dom('ConstraintError', () => frame.allocationSize(options));
  await promise_rejects_dom(t, 'ConstraintError', frame.readInto(buf, options));
}, 'Test address overflow.');

promise_test(async t => {
  let frame = makeI420_4x2();
  let buf = new Uint8Array(12);
  let options = {region: frame.codedRegion};
  let expected = new Uint8Array([
      1, 2, 3, 4, 5, 6, 7, 8,  // y
      9, 10,                   // u
      11, 12                   // v
  ]);
  assert_equals(frame.allocationSize(options), 12, 'allocationSize()');
  await frame.readInto(buf, options);
  assert_array_equals(buf, expected, 'destination buffer contents');
}, 'Test codedRegion.');

promise_test(async t => {
  let frame = makeI420_4x2();
  let buf = new Uint8Array(12);
  let options = {region: {left: 0, top: 0, width: 4, height: 0}};
  assert_throws_dom('ConstraintError', () => frame.allocationSize(options));
  await promise_rejects_dom(t, 'ConstraintError', frame.readInto(buf, options));
}, 'Test empty region.');

promise_test(async t => {
  let frame = makeI420_4x2();
  let buf = new Uint8Array(12);
  let options = {region: {left: 0, top: 0, width: 4, height: 1}};
  assert_throws_dom('ConstraintError', () => frame.allocationSize(options));
  await promise_rejects_dom(t, 'ConstraintError', frame.readInto(buf, options));
}, 'Test unaligned region.');

promise_test(async t => {
  let frame = makeI420_4x2();
  let buf = new Uint8Array(6);
  let options = {region: {left: 2, top: 0, width: 2, height: 2}};
  let expected = new Uint8Array([
      3, 4, 7, 8,  // y
      10,          // u
      12           // v
  ]);
  assert_equals(frame.allocationSize(options), 6, 'allocationSize()');
  await frame.readInto(buf, options);
  assert_array_equals(buf, expected, 'destination buffer contents');
}, 'Test left crop.');

promise_test(async t => {
  let frame = makeI420_4x2();
  let buf = new Uint8Array(12);
  let options = {region: {left: 0, top: 0, width: 4, height: 4}};
  assert_throws_dom('ConstraintError', () => frame.allocationSize(options));
  await promise_rejects_dom(t, 'ConstraintError', frame.readInto(buf, options));
}, 'Test invalid region.');
