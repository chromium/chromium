// META: global=window,dedicatedworker

function makeI420_4x2() {
  const yData = new Uint8Array([
      1, 2, 3, 4,
      5, 6, 7, 8,
  ]);
  const uData = new Uint8Array([
      9, 10,
  ]);
  const vData = new Uint8Array([
      11, 12,
  ]);
  const planes = [
      {data: yData, stride: 4},
      {data: uData, stride: 2},
      {data: vData, stride: 2},
  ];
  const init = {
      timestamp: 0,
      codedWidth: 4,
      codedHeight: 2,
  };
  return new VideoFrame('I420', planes, init);
}

function makeRGBA_2x2() {
  const rgbaData = new Uint8Array([
      1, 2, 3, 4,     5, 6, 7, 8,
      9, 10, 11, 12,  13, 14, 15, 16,
  ]);
  const planes = [
      {data: rgbaData, stride: 8},
  ];
  const init = {
      timestamp: 0,
      codedWidth: 2,
      codedHeight: 2,
  };
  // TODO(sandersd): Should be RGBA but the IDL is reversed right now.
  return new VideoFrame('ABGR', planes, init);
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
  const layout = await frame.readInto(data);
  assert_layout_equals(layout, expectedLayout);
  assert_buffer_equals(data, expectedData);
}, 'Test I420 frame.');

promise_test(async t => {
  const frame = makeRGBA_2x2();
  const expectedLayout = [
      {offset: 0, stride: 8},
  ];
  const expectedData = new Uint8Array([
      1, 2, 3, 4,     5, 6, 7, 8,
      9, 10, 11, 12,  13, 14, 15, 16
  ]);
  assert_equals(frame.allocationSize(), expectedData.length, 'allocationSize()');
  const data = new Uint8Array(expectedData.length);
  const layout = await frame.readInto(data);
  assert_layout_equals(layout, expectedLayout);
  assert_buffer_equals(data, expectedData);
}, 'Test RGBA frame.');

promise_test(async t => {
  const frame = makeI420_4x2();
  const data = new Uint8Array(11);
  await promise_rejects_dom(t, 'ConstraintError', frame.readInto(data));
}, 'Test undersized buffer.');

promise_test(async t => {
  const frame = makeI420_4x2();
  const options = {
      layout: [{}, {}, {}],
  };
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
  assert_equals(frame.allocationSize(options), expectedData.length, 'allocationSize()');
  const data = new Uint8Array(expectedData.length);
  const layout = await frame.readInto(data, options);
  assert_layout_equals(layout, expectedLayout);
  assert_buffer_equals(data, expectedData);
}, 'Test layout can be empty.');

promise_test(async t => {
  const frame = makeI420_4x2();
  const options = {
      layout: [{}],
  };
  assert_throws_dom('ConstraintError', () => frame.allocationSize(options));
  const data = new Uint8Array(12);
  await promise_rejects_dom(t, 'ConstraintError', frame.readInto(data, options));
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
  const layout = await frame.readInto(data, options);
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
  const layout = await frame.readInto(data, options);
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
  assert_throws_dom('ConstraintError', () => frame.allocationSize(options));
  const data = new Uint8Array(12);
  await promise_rejects_dom(t, 'ConstraintError', frame.readInto(data, options));
}, 'Test invalid stride.');

promise_test(async t => {
  const frame = makeI420_4x2();
  const options = {
      layout: [
          {offset: 0, stride: 4},
          {offset: 8, stride: 2},
          {offset: 10},
      ],
  };
  assert_throws_dom('ConstraintError', () => frame.allocationSize(options));
  const data = new Uint8Array(12);
  await promise_rejects_dom(t, 'ConstraintError', frame.readInto(data, options));
}, 'Test missing stride.');


promise_test(async t => {
  const frame = makeI420_4x2();
  const options = {
      layout: [
          {offset: 0, stride: 4},
          {offset: 8, stride: 2},
          {stride: 2},
      ],
  };
  assert_throws_dom('ConstraintError', () => frame.allocationSize(options));
  const data = new Uint8Array(12);
  await promise_rejects_dom(t, 'ConstraintError', frame.readInto(data, options));
}, 'Test missing offset.');

promise_test(async t => {
  const frame = makeI420_4x2();
  const options = {
      layout: [
          {offset: 0, stride: 4},
          {offset: 8, stride: 2},
          {offset: 2 ** 32 - 2, stride: 2},
      ],
  };
  assert_throws_dom('ConstraintError', () => frame.allocationSize(options));
  const data = new Uint8Array(12);
  await promise_rejects_dom(t, 'ConstraintError', frame.readInto(data, options));
}, 'Test address overflow.');

promise_test(async t => {
  const frame = makeI420_4x2();
  const options = {
      region: frame.codedRegion,
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
  const layout = await frame.readInto(data, options);
  assert_layout_equals(layout, expectedLayout);
  assert_buffer_equals(data, expectedData);
}, 'Test codedRegion.');

promise_test(async t => {
  const frame = makeI420_4x2();
  const options = {
      region: {left: 0, top: 0, width: 4, height: 0},
  };
  assert_throws_dom('ConstraintError', () => frame.allocationSize(options));
  const data = new Uint8Array(12);
  await promise_rejects_dom(t, 'ConstraintError', frame.readInto(data, options));
}, 'Test empty region.');

promise_test(async t => {
  const frame = makeI420_4x2();
  const options = {
      region: {left: 0, top: 0, width: 4, height: 1},
  };
  assert_throws_dom('ConstraintError', () => frame.allocationSize(options));
  const data = new Uint8Array(12);
  await promise_rejects_dom(t, 'ConstraintError', frame.readInto(data, options));
}, 'Test unaligned region.');

promise_test(async t => {
  const frame = makeI420_4x2();
  const options = {
      region: {left: 2, top: 0, width: 2, height: 2},
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
  const layout = await frame.readInto(data, options);
  assert_layout_equals(layout, expectedLayout);
  assert_buffer_equals(data, expectedData);
}, 'Test left crop.');

promise_test(async t => {
  const frame = makeI420_4x2();
  const options = {
      region: {left: 0, top: 0, width: 4, height: 4},
  };
  assert_throws_dom('ConstraintError', () => frame.allocationSize(options));
  const data = new Uint8Array(12);
  await promise_rejects_dom(t, 'ConstraintError', frame.readInto(data, options));
}, 'Test invalid region.');
