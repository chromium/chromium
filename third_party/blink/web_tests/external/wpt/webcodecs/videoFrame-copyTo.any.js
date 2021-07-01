// META: global=window,dedicatedworker

function makeI420_4x2() {
  const data = new Uint8Array([
      1, 2, 3, 4,  // y
      5, 6, 7, 8,
      9, 10,       // u
      11, 12,      // v
  ]);
  const init = {
      format: 'I420',
      timestamp: 0,
      codedWidth: 4,
      codedHeight: 2,
  };
  return new VideoFrame(data, init);
}

function makeRGBA_2x2() {
  const data = new Uint8Array([
      1,2,3,4,    5,6,7,8,
      9,10,11,12, 13,14,15,16,
  ]);
  const init = {
      format: 'RGBA',
      timestamp: 0,
      codedWidth: 2,
      codedHeight: 2,
  };
  return new VideoFrame(data, init);
}

function assert_buffer_equals(actual, expected) {
  assert_true(actual instanceof Uint8Array, 'actual instanceof Uint8Array');
  assert_true(expected instanceof Uint8Array, 'buffer instanceof Uint8Array');
  assert_equals(actual.length, expected.length, 'buffer length');
  for (let i = 0; i < actual.length; i++) {
    if (actual[i] != expected[i]) {
      assert_equals(actual[i], expected[i], 'buffer contents at index ' + i);
    }
  }
}

function assert_layout_equals(actual, expected) {
  assert_equals(actual.length, expected.length, 'layout planes');
  for (let i = 0; i < actual.length; i++) {
    assert_object_equals(actual[i], expected[i], 'plane ' + i + ' layout');
  }
}

promise_test(async t => {
  const frame = makeI420_4x2();
  const expectedLayout = [
      {offset: 0, stride: 4},
      {offset: 8, stride: 2},
      {offset: 10, stride: 2},
  ];
  const expectedData = new Uint8Array([
      1, 2, 3, 4,  // y
      5, 6, 7, 8,
      9, 10,       // u
      11, 12       // v
  ]);
  assert_equals(frame.allocationSize(), expectedData.length, 'allocationSize()');
  const data = new Uint8Array(expectedData.length);
  const layout = await frame.copyTo(data);
  assert_layout_equals(layout, expectedLayout);
  assert_buffer_equals(data, expectedData);
}, 'Test I420 frame.');

promise_test(async t => {
  const frame = makeRGBA_2x2();
  const expectedLayout = [
      {offset: 0, stride: 8},
  ];
  const expectedData = new Uint8Array([
      1,2,3,4,    5,6,7,8,
      9,10,11,12, 13,14,15,16,
  ]);
  assert_equals(frame.allocationSize(), expectedData.length, 'allocationSize()');
  const data = new Uint8Array(expectedData.length);
  const layout = await frame.copyTo(data);
  assert_layout_equals(layout, expectedLayout);
  assert_buffer_equals(data, expectedData);
}, 'Test RGBA frame.');

promise_test(async t => {
  const frame = makeI420_4x2();
  const data = new Uint8Array(11);
  await promise_rejects_js(t, TypeError, frame.copyTo(data));
}, 'Test undersized buffer.');

promise_test(async t => {
  const frame = makeI420_4x2();
  const options = {
    layout: [{offset: 0, stride: 4}],
  };
  assert_throws_js(TypeError, () => frame.allocationSize(options));
  const data = new Uint8Array(12);
  await promise_rejects_js(t, TypeError, frame.copyTo(data, options));
}, 'Test incorrect plane count.');

promise_test(async t => {
  const frame = makeI420_4x2();
  const options = {
      layout: [
          {offset: 4, stride: 4},
          {offset: 0, stride: 2},
          {offset: 2, stride: 2},
      ],
  };
  const expectedData = new Uint8Array([
      9, 10,       // u
      11, 12,      // v
      1, 2, 3, 4,  // y
      5, 6, 7, 8,
  ]);
  assert_equals(frame.allocationSize(options), expectedData.length, 'allocationSize()');
  const data = new Uint8Array(expectedData.length);
  const layout = await frame.copyTo(data, options);
  assert_layout_equals(layout, options.layout);
  assert_buffer_equals(data, expectedData);
}, 'Test stride and offset work.');

promise_test(async t => {
  const frame = makeI420_4x2();
  const options = {
      layout: [
          {offset: 9, stride: 5},
          {offset: 1, stride: 3},
          {offset: 5, stride: 3},
      ],
  };
  const expectedData = new Uint8Array([
      0,
      9, 10, 0,       // u
      0,
      11, 12, 0,      // v
      0,
      1, 2, 3, 4, 0,  // y
      5, 6, 7, 8, 0,
  ]);
  assert_equals(frame.allocationSize(options), expectedData.length, 'allocationSize()');
  const data = new Uint8Array(expectedData.length);
  const layout = await frame.copyTo(data, options);
  assert_layout_equals(layout, options.layout);
  assert_buffer_equals(data, expectedData);
}, 'Test stride and offset with padding.');

promise_test(async t => {
  const frame = makeI420_4x2();
  const options = {
      layout: [
          {offset: 0, stride: 1},
          {offset: 8, stride: 2},
          {offset: 10, stride: 2},
      ],
  };
  assert_throws_js(TypeError, () => frame.allocationSize(options));
  const data = new Uint8Array(12);
  await promise_rejects_js(t, TypeError, frame.copyTo(data, options));
}, 'Test invalid stride.');

promise_test(async t => {
  const frame = makeI420_4x2();
  const options = {
      layout: [
          {offset: 0, stride: 4},
          {offset: 8, stride: 2},
          {offset: 2 ** 32 - 2, stride: 2},
      ],
  };
  assert_throws_js(TypeError, () => frame.allocationSize(options));
  const data = new Uint8Array(12);
  await promise_rejects_js(t, TypeError, frame.copyTo(data, options));
}, 'Test address overflow.');

promise_test(async t => {
  const frame = makeI420_4x2();
  const options = {
      rect: frame.codedRect,
  };
  const expectedLayout = [
      {offset: 0, stride: 4},
      {offset: 8, stride: 2},
      {offset: 10, stride: 2},
  ];
  const expectedData = new Uint8Array([
      1, 2, 3, 4, 5, 6, 7, 8,  // y
      9, 10,                   // u
      11, 12                   // v
  ]);
  assert_equals(frame.allocationSize(options), expectedData.length, 'allocationSize()');
  const data = new Uint8Array(expectedData.length);
  const layout = await frame.copyTo(data, options);
  assert_layout_equals(layout, expectedLayout);
  assert_buffer_equals(data, expectedData);
}, 'Test codedRect.');

promise_test(async t => {
  const frame = makeI420_4x2();
  const options = {
      rect: {x: 0, y: 0, width: 4, height: 0},
  };
  assert_throws_js(TypeError, () => frame.allocationSize(options));
  const data = new Uint8Array(12);
  await promise_rejects_js(t, TypeError, frame.copyTo(data, options));
}, 'Test empty rect.');

promise_test(async t => {
  const frame = makeI420_4x2();
  const options = {
      rect: {x: 0, y: 0, width: 4, height: 1},
  };
  assert_throws_js(TypeError, () => frame.allocationSize(options));
  const data = new Uint8Array(12);
  await promise_rejects_js(t, TypeError, frame.copyTo(data, options));
}, 'Test unaligned rect.');

promise_test(async t => {
  const frame = makeI420_4x2();
  const options = {
      rect: {x: 2, y: 0, width: 2, height: 2},
  };
  const expectedLayout = [
      {offset: 0, stride: 2},
      {offset: 4, stride: 1},
      {offset: 5, stride: 1},
  ];
  const expectedData = new Uint8Array([
      3, 4,  // y
      7, 8,
      10,    // u
      12     // v
  ]);
  assert_equals(frame.allocationSize(options), expectedData.length, 'allocationSize()');
  const data = new Uint8Array(expectedData.length);
  const layout = await frame.copyTo(data, options);
  assert_layout_equals(layout, expectedLayout);
  assert_buffer_equals(data, expectedData);
}, 'Test left crop.');

promise_test(async t => {
  const frame = makeI420_4x2();
  const options = {
      rect: {x: 0, y: 0, width: 4, height: 4},
  };
  assert_throws_js(TypeError, () => frame.allocationSize(options));
  const data = new Uint8Array(12);
  await promise_rejects_js(t, TypeError, frame.copyTo(data, options));
}, 'Test invalid rect.');
