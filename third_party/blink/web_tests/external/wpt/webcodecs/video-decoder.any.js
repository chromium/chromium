// META: global=window,dedicatedworker
// META: script=/webcodecs/utils.js

// TODO(sandersd): Move metadata into a helper library.
// TODO(sandersd): Add H.264 decode test once there is an API to query for
// supported codecs.

const h264 = {
  async buffer() {
    return (await fetch('h264.mp4')).arrayBuffer();
  },
  codec: 'avc1.64000c',
  description: {offset: 9490, size: 45},
  frames: [
    {offset: 48, size: 4140}, {offset: 4188, size: 604},
    {offset: 4792, size: 475}, {offset: 5267, size: 561},
    {offset: 5828, size: 587}, {offset: 6415, size: 519},
    {offset: 6934, size: 532}, {offset: 7466, size: 523},
    {offset: 7989, size: 454}, {offset: 8443, size: 528}
  ]
};


const vp9 = {
  async buffer() {
    return (await fetch('vp9.mp4')).arrayBuffer();
  },
  // TODO(sandersd): Verify that the file is actually level 1.
  codec: 'vp09.00.10.08',
  frames: [
    {offset: 44, size: 3315}, {offset: 3359, size: 203},
    {offset: 3562, size: 245}, {offset: 3807, size: 172},
    {offset: 3979, size: 312}, {offset: 4291, size: 170},
    {offset: 4461, size: 195}, {offset: 4656, size: 181},
    {offset: 4837, size: 356}, {offset: 5193, size: 159}
  ]
};

const badCodecsList = [
  '',                         // Empty codec
  'bogus',                    // Non exsitent codec
  'vorbis',                   // Audio codec
  'vp9',                      // Ambiguous codec
  'video/webm; codecs="vp9"'  // Codec with mime type
];

const invalidConfigs = [
  {
    comment: 'Emtpy codec',
    config: {codec: ''},
  },
  {
    comment: 'Unrecognized codec',
    config: {codec: 'bogus'},
  },
  {
    comment: 'Audio codec',
    config: {codec: 'vorbis'},
  },
  {
    comment: 'Ambiguous codec',
    config: {codec: 'vp9'},
  },
  {
    comment: 'Codec with MIME type',
    config: {codec: 'video/webm; codecs="vp8"'},
  },
  {
    comment: 'Zero coded size',
    config: {
      codec: h264.codec,
      codedWidth: 0,
      codedHeight: 0,
    },
  },
  {
    comment: 'Out of bounds visibleRegion',
    config: {
      codec: h264.codec,
      codedWidth: 1920,
      codedHeight: 1088,
      visibleRegion: {left: 10, top: 10, width: 1920, height: 1088},
    },
  },
  {
    comment: 'Way out of bounds visibleRegion',
    config: {
      codec: h264.codec,
      codedWidth: 1920,
      codedHeight: 1088,
      visibleRegion: {left: 0, top: 0, width: 4000, height: 5000},
    },
  },
  {
    comment: 'Invalid display size',
    config: {
      codec: h264.codec,
      displayWidth: 0,
      displayHeight: 0,
    },
  },
];  //  invalidConfigs

function view(buffer, {offset, size}) {
  return new Uint8Array(buffer, offset, size);
}

function getFakeChunk() {
  return new EncodedVideoChunk(
      {type: 'key', timestamp: 0, data: Uint8Array.of(0)});
}

invalidConfigs.forEach(entry => {
  promise_test(
      t => {
        return promise_rejects_js(
            t, TypeError, VideoDecoder.isConfigSupported(entry.config));
      },
      'Test that VideoDecoder.isConfigSupported() rejects invalid config:' +
          entry.comment);
});

invalidConfigs.forEach(entry => {
  async_test(
      t => {
        let codec = new VideoDecoder(getDefaultCodecInit(t));
        assert_throws_js(TypeError, () => {
          codec.configure(entry.config);
        });
        t.done();
      },
      'Test that VideoDecoder.configure() rejects invalid config:' +
          entry.comment);
});

promise_test(t => {
  return VideoDecoder.isConfigSupported({codec: vp9.codec});
}, 'Test VideoDecoder.isConfigSupported() with minimal valid config');

promise_test(t => {
  // This config specifies a slight crop. H264 1080p content always crops
  // because H264 coded dimensions are a multiple of 16 (e.g. 1088).
  return VideoDecoder.isConfigSupported({
    codec: h264.codec,
    codedWidth: 1920,
    codedHeight: 1088,
    visibleRegion: {left: 0, top: 0, width: 1920, height: 1080},
    displayWidth: 1920,
    displayHeight: 1080
  });
}, 'Test VideoDecoder.isConfigSupported() with valid expanded config');

promise_test(t => {
  // Define a valid config that includes a hypothetical 'futureConfigFeature',
  // which is not yet recognized by the User Agent.
  const validConfig = {
    codec: h264.codec,
    codedWidth: 1920,
    codedHeight: 1088,
    visibleRegion: {left: 0, top: 0, width: 1920, height: 1080},
    displayWidth: 1920,
    displayHeight: 1080,
    description: new Uint8Array([1, 2, 3]),
    futureConfigFeature: 'foo',
  };

  // The UA will evaluate validConfig as being "valid", ignoring the
  // `futureConfigFeature` it  doesn't recognize.
  return VideoDecoder.isConfigSupported(validConfig).then((decoderSupport) => {
    // VideoDecoderSupport must contain the following properites.
    assert_true(decoderSupport.hasOwnProperty('supported'));
    assert_true(decoderSupport.hasOwnProperty('config'));

    // VideoDecoderSupport.config must not contain unrecognized properties.
    assert_false(decoderSupport.config.hasOwnProperty('futureConfigFeature'));

    // VideoDecoderSupport.config must contiain the recognized properties.
    assert_equals(decoderSupport.config.codec, validConfig.codec);
    assert_equals(decoderSupport.config.codedWidth, validConfig.codedWidth);
    assert_equals(decoderSupport.config.codedHeight, validConfig.codedHeight);
    assert_equals(decoderSupport.config.visibleRegion.top, 0);
    assert_equals(decoderSupport.config.visibleRegion.left, 0);
    assert_equals(decoderSupport.config.visibleRegion.width, 1920);
    assert_equals(decoderSupport.config.visibleRegion.height, 1080);
    assert_equals(decoderSupport.config.displayWidth, validConfig.displayWidth);
    assert_equals(
        decoderSupport.config.displayHeight, validConfig.displayHeight);

    // The description BufferSource must copy the input config description.
    assert_not_equals(
        decoderSupport.config.description, validConfig.description);
    let parsedDescription = new Uint8Array(decoderSupport.config.description);
    assert_equals(parsedDescription.length, validConfig.description.length);
    for (let i = 0; i < parsedDescription.length; ++i) {
      assert_equals(parsedDescription[i], validConfig.description[i]);
    }
  });
}, 'Test that VideoDecoder.isConfigSupported() returns a parsed configuration');


promise_test(t => {
  // VideoDecoderInit lacks required fields.
  assert_throws_js(TypeError, () => {
    new VideoDecoder({});
  });

  // VideoDecoderInit has required fields.
  let decoder = new VideoDecoder(getDefaultCodecInit(t));

  assert_equals(decoder.state, 'unconfigured');

  decoder.close();

  return endAfterEventLoopTurn();
}, 'Test VideoDecoder construction');

promise_test(t => {
  let decoder = new VideoDecoder(getDefaultCodecInit(t));

  // TODO(chcunningham): Remove badCodecsList testing. It's now covered more
  // extensively by other tests.
  testConfigurations(decoder, {codec: vp9.codec}, badCodecsList);

  return endAfterEventLoopTurn();
}, 'Test VideoDecoder.configure() with various codec strings');

promise_test(async t => {
  let buffer = await vp9.buffer();

  let numOutputs = 0;
  let decoder = new VideoDecoder({
    output(frame) {
      t.step(() => {
        assert_equals(++numOutputs, 1, 'outputs');
        assert_equals(frame.visibleRegion.width, 320, 'visibleRegion.width');
        assert_equals(frame.visibleRegion.height, 240, 'visibleRegion.height');
        assert_equals(frame.timestamp, 0, 'timestamp');
        frame.close();
      });
    },
    error(e) {
      t.step(() => {
        throw e;
      });
    }
  });

  decoder.configure({codec: vp9.codec});

  decoder.decode(new EncodedVideoChunk(
      {type: 'key', timestamp: 0, data: view(buffer, vp9.frames[0])}));

  await decoder.flush();

  assert_equals(numOutputs, 1, 'outputs');
}, 'Decode VP9');

promise_test(async t => {
  let buffer = await vp9.buffer();

  let outputs_before_reset = 0;
  let outputs_after_reset = 0;

  let decoder = new VideoDecoder({
    // Pre-reset() chunks will all have timestamp=0, while post-reset() chunks
    // will all have timestamp=1.
    output(frame) {
      t.step(() => {
        if (frame.timestamp == 0)
          outputs_before_reset++;
        else
          outputs_after_reset++;
      });
    },
    error(e) {
      t.step(() => {
        throw e;
      });
    }
  });

  decoder.configure({codec: vp9.codec});

  for (let i = 0; i < 100; i++) {
    decoder.decode(new EncodedVideoChunk(
        {type: 'key', timestamp: 0, data: view(buffer, vp9.frames[0])}));
  }

  assert_greater_than(decoder.decodeQueueSize, 0);

  // Wait for the first frame to be decoded.
  await t.step_wait(
      () => outputs_before_reset > 0, 'Decoded outputs started coming', 10000,
      1);

  let saved_outputs_before_reset = outputs_before_reset;
  assert_greater_than(saved_outputs_before_reset, 0);
  assert_less_than(saved_outputs_before_reset, 100);

  decoder.reset()
  assert_equals(decoder.decodeQueueSize, 0);

  decoder.configure({codec: vp9.codec});

  for (let i = 0; i < 5; i++) {
    decoder.decode(new EncodedVideoChunk(
        {type: 'key', timestamp: 1, data: view(buffer, vp9.frames[0])}));
  }
  await decoder.flush();
  assert_equals(outputs_after_reset, 5);
  assert_equals(saved_outputs_before_reset, outputs_before_reset);
  assert_equals(decoder.decodeQueueSize, 0);

  endAfterEventLoopTurn();
}, 'Verify reset() suppresses output and rejects flush');

promise_test(t => {
  let decoder = new VideoDecoder(getDefaultCodecInit(t));

  return testClosedCodec(t, decoder, {codec: vp9.codec}, getFakeChunk());
}, 'Verify closed VideoDecoder operations');

promise_test(t => {
  let decoder = new VideoDecoder(getDefaultCodecInit(t));

  return testUnconfiguredCodec(t, decoder, getFakeChunk());
}, 'Verify unconfigured VideoDecoder operations');

promise_test(t => {
  let numErrors = 0;
  let codecInit = getDefaultCodecInit(t);
  codecInit.error = _ => numErrors++;

  let decoder = new VideoDecoder(codecInit);

  decoder.configure({codec: vp9.codec});

  let fakeChunk = getFakeChunk();
  decoder.decode(fakeChunk);

  return promise_rejects_exactly(t, undefined, decoder.flush()).then(() => {
    assert_equals(numErrors, 1, 'errors');
    assert_equals(decoder.state, 'closed');
  });
}, 'Decode corrupt VP9 frame');

promise_test(t => {
  let numErrors = 0;
  let codecInit = getDefaultCodecInit(t);
  codecInit.error = _ => numErrors++;

  let decoder = new VideoDecoder(codecInit);

  decoder.configure({codec: vp9.codec});

  let fakeChunk = getFakeChunk();
  decoder.decode(fakeChunk);

  return promise_rejects_exactly(t, undefined, decoder.flush()).then(() => {
    assert_equals(numErrors, 1, 'errors');
    assert_equals(decoder.state, 'closed');
  });
}, 'Decode empty VP9 frame');

promise_test(t => {
  let decoder = new VideoDecoder(getDefaultCodecInit(t));

  decoder.configure({codec: vp9.codec});

  let fakeChunk = getFakeChunk();
  decoder.decode(fakeChunk);

  // Create the flush promise before closing, as it is invalid to do so later.
  let flushPromise = decoder.flush();

  // This should synchronously reject the flush() promise.
  decoder.close();

  // TODO(sandersd): Wait for a bit in case there is a lingering output
  // or error coming.
  return promise_rejects_exactly(t, undefined, flushPromise);
}, 'Close while decoding corrupt VP9 frame');

promise_test(async t => {
  let buffer = await vp9.buffer();

  let numOutputs = 0;
  let decoder = new VideoDecoder({
    output: t.step_func(frame => {
      frame.close();
      ++numOutputs;
    }),
    error: t.unreached_func()
  });

  decoder.configure({codec: vp9.codec});
  decoder.decode(new EncodedVideoChunk(
      {type: 'key', timestamp: 0, data: view(buffer, vp9.frames[0])}));

  await decoder.flush();
  assert_equals(numOutputs, 1, 'outputs');

  decoder.decode(new EncodedVideoChunk(
      {type: 'key', timestamp: 1, data: view(buffer, vp9.frames[0])}));

  await decoder.flush();
  assert_equals(numOutputs, 2, 'outputs');
}, 'Test decoding after flush.');

promise_test(async t => {
  let buffer = await vp9.buffer();

  let numOutputs = 0;
  let decoder = new VideoDecoder({
    output: t.step_func(frame => {
      frame.close();
      ++numOutputs;
    }),
    error: t.unreached_func()
  });

  decoder.configure({codec: vp9.codec});
  decoder.decode(new EncodedVideoChunk(
      {type: 'key', timestamp: -25, data: view(buffer, vp9.frames[0])}));

  await decoder.flush();
  assert_equals(numOutputs, 1, 'outputs');
}, 'Test decoding a frame with a negative timestamp.');

promise_test(async t => {
  let buffer = await vp9.buffer();

  let numOutputs = 0;
  let decoder = new VideoDecoder({
    output: t.step_func(frame => {
      frame.close();
      ++numOutputs;
    }),
    error: t.unreached_func()
  });

  decoder.configure({codec: vp9.codec});
  decoder.decode(new EncodedVideoChunk(
      {type: 'key', timestamp: 0, data: view(buffer, vp9.frames[0])}));

  // Wait for the first frame to be decoded.
  await t.step_wait(() => numOutputs > 0, 'Decoded first frame', 10000, 1);

  let p = decoder.flush();
  decoder.reset();
  return p;
}, 'Test reset during flush.');

promise_test(async t => {
  let result = await VideoDecoder.isConfigSupported({codec: h264.codec});
  assert_implements_optional(
      result.supported, 'Optional codec ' + h264.codec + ' not supported.');
  let buffer = await h264.buffer();

  let numOutputs = 0;
  let decoder = new VideoDecoder({
    output: t.step_func(frame => {
      frame.close();
      ++numOutputs;
    }),
    error: t.unreached_func()
  });

  decoder.configure({
    codec: h264.codec,
    description: view(buffer, h264.description),
    optimizeForLatency: true
  });
  decoder.decode(new EncodedVideoChunk(
      {type: 'key', timestamp: 0, data: view(buffer, h264.frames[0])}));

  // Wait for the first frame to be decoded.
  await t.step_wait(() => numOutputs > 0, 'Decoded first frame', 10000, 1);
  assert_equals(numOutputs, 1, 'outputs');
}, 'Test low latency decoding.');
