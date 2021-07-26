// META: global=window
// META: script=/common/media.js
// META: script=/webcodecs/utils.js

var defaultInit =
    {
      timestamp: 1234,
      channels: 2,
      sampleRate: 8000,
      frames: 100,
    }

function
createDefaultAudioData() {
  return make_audio_data(
      defaultInit.timestamp, defaultInit.channels, defaultInit.sampleRate,
      defaultInit.frames);
}

test(t => {
  let local_data = new Float32Array(defaultInit.channels * defaultInit.frames);

  let audio_data_init = {
    timestamp: defaultInit.timestamp,
    data: local_data,
    numberOfFrames: defaultInit.frames,
    numberOfChannels: defaultInit.channels,
    sampleRate: defaultInit.sampleRate,
    format: 'f32-planar',
  }

  let data = new AudioData(audio_data_init);

  assert_equals(data.timestamp, defaultInit.timestamp, 'timestamp');
  assert_equals(data.numberOfFrames, defaultInit.frames, 'frames');
  assert_equals(data.numberOfChannels, defaultInit.channels, 'channels');
  assert_equals(data.sampleRate, defaultInit.sampleRate, 'sampleRate');
  assert_equals(
      data.duration, defaultInit.frames / defaultInit.sampleRate * 1_000_000,
      'duration');
  assert_equals(data.format, 'f32-planar', 'format');

  // Create an Int16 array of the right length.
  let small_data = new Int16Array(defaultInit.channels * defaultInit.frames);

  let wrong_format_init = {...audio_data_init};
  wrong_format_init.data = small_data;

  // Creating `f32-planar` AudioData from Int16 from should throw.
  assert_throws_js(TypeError, () => {
    let data = new AudioData(wrong_format_init);
  }, `AudioDataInit.data needs to be big enough`);

  var members = [
    'timestamp',
    'data',
    'numberOfFrames',
    'numberOfChannels',
    'sampleRate',
    'format',
  ];

  for (const member of members) {
    let incomplete_init = {... audio_data_init};
    delete incomplete_init[member];

    assert_throws_js(
      TypeError, () => {let data = new AudioData(incomplete_init)},
      "AudioData requires '" + member + "'");
  }
}, 'Verify AudioData constructors');

test(t => {
  let data = createDefaultAudioData();

  let clone = data.clone();

  // Verify the parameters match.
  assert_equals(data.timestamp, clone.timestamp, 'timestamp');
  assert_equals(data.numberOfFrames, clone.numberOfFrames, 'frames');
  assert_equals(
      data.numberOfChannels, clone.numberOfChannels, 'channels');
  assert_equals(data.sampleRate, clone.sampleRate, 'sampleRate');
  assert_equals(data.format, clone.format, 'format');

  const data_copyDest = new Float32Array(defaultInit.frames);
  const clone_copyDest = new Float32Array(defaultInit.frames);

  // Verify the data matches.
  for (var channel = 0; channel < defaultInit.channels; channel++) {
    data.copyTo(data_copyDest, {planeIndex: channel});
    clone.copyTo(clone_copyDest, {planeIndex: channel});

    assert_array_equals(
        data_copyDest, clone_copyDest, 'Cloned data ch=' + channel);
  }

  // Verify closing the original data doesn't close the clone.
  data.close();
  assert_equals(data.numberOfFrames, 0, 'data.buffer (closed)');
  assert_not_equals(clone.numberOfFrames, 0, 'clone.buffer (not closed)');

  clone.close();
  assert_equals(clone.numberOfFrames, 0, 'clone.buffer (closed)');

  // Verify closing a closed AudioData does not throw.
  data.close();
}, 'Verify closing and cloning AudioData');

test(t => {
  let data = make_audio_data(
      -10, defaultInit.channels, defaultInit.sampleRate, defaultInit.frames);
  assert_equals(data.timestamp, -10, 'timestamp');
  data.close();
}, 'Test we can construct AudioData with a negative timestamp.');
