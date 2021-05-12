// META: global=window,dedicatedworker
// META: variant=?vp9
// META: variant=?h264_avc
// META: variant=?h264_annexb

const VP9_DATA = {
  src: 'vp9.mp4',
  // TODO(sandersd): Verify that the file is actually level 1.
  config: {
    codec: 'vp09.00.10.08',
    codedWidth: 320,
    codedHeight: 240,
    visibleRegion: {left: 0, top: 0, width: 320, height: 240},
    displayWidth: 320,
    displayHeight: 240,
  },
  chunks: [
    {offset: 44, size: 3315}, {offset: 3359, size: 203},
    {offset: 3562, size: 245}, {offset: 3807, size: 172},
    {offset: 3979, size: 312}, {offset: 4291, size: 170},
    {offset: 4461, size: 195}, {offset: 4656, size: 181},
    {offset: 4837, size: 356}, {offset: 5193, size: 159},
  ]
};

const H264_AVC_DATA = {
  src: 'h264.mp4',
  config: {
    codec: 'avc1.64000b',
    description: {offset: 9490, size: 45},
    codedWidth: 320,
    codedHeight: 240,
    visibleRegion: {left: 0, top: 0, width: 320, height: 240},
    displayWidth: 320,
    displayHeight: 240,
  },
  chunks: [
    {offset: 48, size: 4140}, {offset: 4188, size: 604},
    {offset: 4792, size: 475}, {offset: 5267, size: 561},
    {offset: 5828, size: 587}, {offset: 6415, size: 519},
    {offset: 6934, size: 532}, {offset: 7466, size: 523},
    {offset: 7989, size: 454}, {offset: 8443, size: 528},
  ]
};

const H264_ANNEXB_DATA = {
  src: 'h264.annexb',
  config: {
    codec: 'avc1.64000b',
    codedWidth: 320,
    codedHeight: 240,
    visibleRegion: {left: 0, top: 0, width: 320, height: 240},
    displayWidth: 320,
    displayHeight: 240,
  },
  chunks: [
    {offset: 0, size: 4175}, {offset: 4175, size: 602},
    {offset: 4777, size: 473}, {offset: 5250, size: 559},
    {offset: 5809, size: 585}, {offset: 6394, size: 517},
    {offset: 6911, size: 530}, {offset: 7441, size: 521},
    {offset: 7962, size: 452}, {offset: 8414, size: 526},
  ],
};

// Allows mutating `callbacks` after constructing the VideoDecoder, wraps calls
// in t.step().
function createVideoDecoder(t, callbacks) {
  return new VideoDecoder({
    output(frame) {
      if (callbacks && callbacks.output) {
        t.step(() => callbacks.output(frame));
      } else {
        t.unreached_func('unexpected output()');
      }
    },
    error(e) {
      if (callbacks && callbacks.error) {
        t.step(() => callbacks.error(e));
      } else {
        t.unreached_func('unexpected error()');
      }
    }
  });
}

// Create a view of an ArrayBuffer.
function view(buffer, {offset, size}) {
  return new Uint8Array(buffer, offset, size);
}

let CONFIG = null;
let CHUNK_DATA = null;
let CHUNKS = null;
promise_setup(async () => {
  const data = {'?vp9': VP9_DATA,
                '?h264_avc': H264_AVC_DATA,
                '?h264_annexb': H264_ANNEXB_DATA}[location.search];

  // Don't run any tests if the codec is not supported.
  try {
    // TODO(sandersd): To properly support H.264 in AVC format, this should
    // include the `description`. For now this test assumes that H.264 Annex B
    // support is the same as H.264 AVC support.
    await VideoDecoder.isConfigSupported({codec: data.config.codec});
  } catch (e) {
    assert_implements_optional(false, data.config.codec + ' unsupported');
  }

  // Fetch the media data and prepare buffers.
  const response = await fetch(data.src);
  const buf = await response.arrayBuffer();

  CONFIG = {...data.config};
  if (data.config.description) {
    CONFIG.description = view(buf, data.config.description);
  }

  CHUNK_DATA = data.chunks.map((chunk, i) => view(buf, chunk));

  CHUNKS = CHUNK_DATA.map((data, i) => new EncodedVideoChunk({
    type: i == 0 ? 'key' : 'delta',
    timestamp: i,
    duration: 1,
    data,
  }));
});

promise_test(async t => {
  const support = await(VideoDecoder.isConfigSupported(CONFIG));
  assert_true(support.supported, 'supported');
}, 'Test isConfigSupported()');

promise_test(async t => {
  // TODO(sandersd): Create a 1080p `description` for H.264 in AVC format.
  // This version is testing only the H.264 Annex B path.
  const config = {
    codec: CONFIG.codec,
    codedWidth: 1920,
    codedHeight: 1088,
    visibleRegion: {left: 0, top: 0, width: 1920, height: 1080},
    displayWidth: 1920,
    displayHeight: 1080,
  };

  const support = await(VideoDecoder.isConfigSupported(config));
  assert_true(support.supported, 'supported');
}, 'Test isConfigSupported() with 1080p crop');

promise_test(async t => {
  // Define a valid config that includes a hypothetical `futureConfigFeature`,
  // which is not yet recognized by the User Agent.
  const config = {
    ...CONFIG,
    futureConfigFeature: 'foo',
  };

  // The UA will evaluate validConfig as being "valid", ignoring the
  // `futureConfigFeature` it  doesn't recognize.
  const support = await VideoDecoder.isConfigSupported(config);
  assert_true(support.supported, 'supported');
  assert_equals(support.config.codec, config.codec, 'codec');
  assert_equals(support.config.codedWidth, config.codedWidth, 'codedWidth');
  assert_equals(support.config.codedHeight, config.codedHeight, 'codedHeight');
  assert_object_equals(support.config.visibleRegion, config.visibleRegion, 'visibleRegion');
  assert_equals(support.config.displayWidth, config.displayWidth, 'displayWidth');
  assert_equals(support.config.displayHeight, config.displayHeight, 'displayHeight');
  assert_false(support.config.hasOwnProperty('futureConfigFeature'), 'futureConfigFeature');

  if (config.description) {
    // The description must be copied.
    assert_false(support.config.description === config.description, 'description is unique');
    assert_array_equals(new Uint8Array(support.config.description, 0),
                        new Uint8Array(config.description, 0),
                        'description');
  } else {
    assert_false(support.config.hasOwnProperty('description'), 'description');
  }
}, 'Test that isConfigSupported() returns a parsed configuration');

promise_test(async t => {
  async function test(t, config, description) {
    await promise_rejects_js(t, TypeError, VideoDecoder.isConfigSupported(config), description);

    const decoder = createVideoDecoder(t);
    assert_throws_js(TypeError, () => decoder.configure(config), description);
    assert_equals(decoder.state, 'unconfigured', 'state');
  }

  await test(t, {...CONFIG, codedWidth: 0}, 'invalid codedWidth');
  await test(t, {...CONFIG, visibleRegion: {...CONFIG.visibleRegion, left: 1}}, 'out of bounds visibleRegion');
  await test(t, {...CONFIG, displayWidth: 0}, 'invalid displayWidth');
}, 'Test invalid configs');

promise_test(async t => {
  const decoder = createVideoDecoder(t);
  decoder.configure(CONFIG);
  assert_equals(decoder.state, 'configured', 'state');
}, 'Test configure()');

promise_test(async t => {
  const callbacks = {};
  const decoder = createVideoDecoder(t, callbacks);
  decoder.configure(CONFIG);
  decoder.decode(CHUNKS[0]);

  let outputs = 0;
  callbacks.output = frame => {
    outputs++;
    assert_object_equals(frame.visibleRegion, CONFIG.visibleRegion, 'visibleRegion');
    assert_equals(frame.timestamp, CHUNKS[0].timestamp, 'timestamp');
    frame.close();
  };

  await decoder.flush();
  assert_equals(outputs, 1, 'outputs');
}, 'Decode a key frame');

promise_test(async t => {
  const callbacks = {};
  const decoder = createVideoDecoder(t, callbacks);
  decoder.configure(CONFIG);
  for (let i = 0; i < 16; i++) {
    decoder.decode(new EncodedVideoChunk({type: 'key', timestamp: 0, data: CHUNK_DATA[0]}));
  }
  assert_greater_than(decoder.decodeQueueSize, 0);

  // Wait for the first output, then reset the decoder.
  let outputs = 0;
  await new Promise(resolve => {
    callbacks.output = frame => {
      outputs++;
      assert_equals(outputs, 1, 'outputs');
      assert_equals(frame.timestamp, 0, 'timestamp');
      frame.close();
      decoder.reset();
      assert_equals(decoder.decodeQueueSize, 0, 'decodeQueueSize');
      resolve();
    };
  });

  decoder.configure(CONFIG);
  for (let i = 0; i < 4; i++) {
    decoder.decode(new EncodedVideoChunk({type: 'key', timestamp: 1, data: CHUNK_DATA[0]}));
  }

  // Expect future outputs to come from after the reset.
  callbacks.output = frame => {
    outputs++;
    assert_equals(frame.timestamp, 1, 'timestamp');
    frame.close();
  };

  await decoder.flush();
  assert_equals(outputs, 5);
  assert_equals(decoder.decodeQueueSize, 0);
}, 'Verify reset() suppresses outputs');

promise_test(async t => {
  const decoder = createVideoDecoder(t);
  assert_equals(decoder.state, 'unconfigured');

  decoder.reset();
  assert_equals(decoder.state, 'unconfigured');
  assert_throws_dom('InvalidStateError', () => decoder.decode(CHUNKS[0]), 'decode');
  await promise_rejects_dom(t, 'InvalidStateError', decoder.flush(), 'flush');
}, 'Test unconfigured VideoDecoder operations');

promise_test(async t => {
  const decoder = createVideoDecoder(t);
  decoder.close();
  assert_equals(decoder.state, 'closed');
  assert_throws_dom('InvalidStateError', () => decoder.configure(CONFIG), 'configure');
  assert_throws_dom('InvalidStateError', () => decoder.reset(), 'reset');
  assert_throws_dom('InvalidStateError', () => decoder.close(), 'close');
  assert_throws_dom('InvalidStateError', () => decoder.decode(CHUNKS[0]), 'decode');
  await promise_rejects_dom(t, 'InvalidStateError', decoder.flush(), 'flush');
}, 'Test closed VideoDecoder operations');

promise_test(async t => {
  const callbacks = {};
  const decoder = createVideoDecoder(t, callbacks);

  decoder.configure(CONFIG);
  decoder.decode(new EncodedVideoChunk({type: 'key', timestamp: 0, data: new ArrayBuffer(0)}));

  let errors = 0;
  callbacks.error = e => errors++;

  // TODO(sandersd): The promise should be rejected with an exception value.
  await promise_rejects_exactly(t, undefined, decoder.flush());

  assert_equals(errors, 1, 'errors');
  assert_equals(decoder.state, 'closed', 'state');
}, 'Decode empty frame');


promise_test(async t => {
  const callbacks = {};
  const decoder = createVideoDecoder(t, callbacks);

  decoder.configure(CONFIG);
  decoder.decode(new EncodedVideoChunk({type: 'key', timestamp: 0, data: Uint8Array.of(0)}));

  let errors = 0;
  callbacks.error = e => errors++;

  // TODO(sandersd): The promise should be rejected with an exception value.
  await promise_rejects_exactly(t, undefined, decoder.flush());

  assert_equals(errors, 1, 'errors');
  assert_equals(decoder.state, 'closed', 'state');
}, 'Decode corrupt frame');

promise_test(async t => {
  const decoder = createVideoDecoder(t);

  decoder.configure(CONFIG);
  decoder.decode(new EncodedVideoChunk({type: 'key', timestamp: 0, data: Uint8Array.of(0)}));

  let flushDone = decoder.flush();
  decoder.close();

  // Flush should have been synchronously rejected, with no output() or error()
  // callbacks.
  // TODO(sandersd): The promise should be rejected with AbortError.
  await promise_rejects_exactly(t, undefined, flushDone);
}, 'Close while decoding corrupt frame');

promise_test(async t => {
  const callbacks = {};
  const decoder = createVideoDecoder(t, callbacks);

  decoder.configure(CONFIG);
  decoder.decode(CHUNKS[0]);

  let outputs = 0;
  callbacks.output = frame => {
    outputs++;
    frame.close();
  };

  await decoder.flush();
  assert_equals(outputs, 1, 'outputs');

  decoder.decode(CHUNKS[0]);
  await decoder.flush();
  assert_equals(outputs, 2, 'outputs');
}, 'Test decoding after flush');

promise_test(async t => {
  const callbacks = {};
  const decoder = createVideoDecoder(t, callbacks);

  decoder.configure(CONFIG);
  decoder.decode(new EncodedVideoChunk({type: 'key', timestamp: -42, data: CHUNK_DATA[0]}));

  let outputs = 0;
  callbacks.output = frame => {
    outputs++;
    assert_equals(frame.timestamp, -42, 'timestamp');
    frame.close();
  };

  await decoder.flush();
  assert_equals(outputs, 1, 'outputs');
}, 'Test decoding a with negative timestamp');

promise_test(async t => {
  const callbacks = {};
  const decoder = createVideoDecoder(t, callbacks);

  decoder.configure(CONFIG);
  decoder.decode(CHUNKS[0]);
  decoder.decode(CHUNKS[1]);
  const flushDone = decoder.flush();

  // Wait for the first output, then reset.
  let outputs = 0;
  await new Promise(resolve => {
    callbacks.output = frame => {
      outputs++;
      assert_equals(outputs, 1, 'outputs');
      decoder.reset();
      frame.close();
      resolve();
    };
  });

  // Flush should have been synchronously rejected.
  // TODO(sandersd): The promise should be rejected with AbortError.
  await promise_rejects_exactly(t, undefined, flushDone);

  assert_equals(outputs, 1, 'outputs');
}, 'Test reset during flush');

promise_test(async t => {
  const callbacks = {};
  const decoder = createVideoDecoder(t, callbacks);

  decoder.configure({...CONFIG, optimizeForLatency: true});
  decoder.decode(CHUNKS[0]);

  // The frame should be output without flushing.
  await new Promise(resolve => {
    callbacks.output = frame => {
      frame.close();
      resolve();
    };
  });
}, 'Test low-latency decoding');
