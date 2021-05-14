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
  let localBuffer = new AudioBuffer({
    length: defaultInit.frames,
    numberOfChannels: defaultInit.channels,
    sampleRate: defaultInit.sampleRate
  });

  let audioDataInit = {timestamp: defaultInit.timestamp, buffer: localBuffer}

  let data = new AudioData(audioDataInit);

  assert_equals(data.timestamp, defaultInit.timestamp, 'timestamp');
  assert_equals(data.buffer.length, defaultInit.frames, 'frames');
  assert_equals(
      data.buffer.numberOfChannels, defaultInit.channels, 'channels');
  assert_equals(data.buffer.sampleRate, defaultInit.sampleRate, 'sampleRate');

  assert_throws_js(
      TypeError, () => {let data = new AudioData({buffer: localBuffer})},
      'AudioData requires \'timestamp\'')

  assert_throws_js(
      TypeError,
      () => {let data = new AudioData({timestamp: defaultInit.timestamp})},
      'AudioData requires \'buffer\'')
}, 'Verify AudioData constructors');

test(t => {
  let data = createDefaultAudioData();

  let clone = data.clone();

  // Verify the parameters match.
  assert_equals(data.timestamp, clone.timestamp, 'timestamp');
  assert_equals(data.buffer.length, clone.buffer.length, 'frames');
  assert_equals(
      data.buffer.numberOfChannels, clone.buffer.numberOfChannels, 'channels');
  assert_equals(data.buffer.sampleRate, clone.buffer.sampleRate, 'sampleRate');

  // Verify the data matches.
  for (var channel = 0; channel < data.buffer.numberOfChannels; channel++) {
    var orig_ch = data.buffer.getChannelData(channel);
    var cloned_ch = clone.buffer.getChannelData(channel);

    assert_array_equals(orig_ch, cloned_ch, 'Cloned data ch=' + channel);
  }

  // Verify closing the original data doesn't close the clone.
  data.close();
  assert_equals(data.buffer, null, 'data.buffer (closed)');
  assert_not_equals(clone.buffer, null, 'clone.buffer (not closed)');

  clone.close();
  assert_equals(clone.buffer, null, 'clone.buffer (closed)');

  // Verify closing a closed AudioData does not throw.
  data.close();
}, 'Verify closing and cloning AudioData');

test(t => {
  let data = createDefaultAudioData();

  // Get a copy of the original data.
  let pre_modification_clone = data.clone();

  for (var channel = 0; channel < data.buffer.numberOfChannels; channel++) {
    var orig_ch = data.buffer.getChannelData(channel);

    // Flip the polarity of the original data's buffer.
    for (let i = 0; i < orig_ch.length; ++i) {
      orig_ch.buffer[i] = -orig_ch.buffer[i];
    }
  }

  // The data in 'data' should have been snapshotted internally, and
  // despite changes to data.buffer, post_modification_clone should not contain
  // modified data.
  let post_modification_clone = data.clone();

  // Verify the data matches.
  for (var channel = 0; channel < data.buffer.numberOfChannels; channel++) {
    var pre_ch = pre_modification_clone.buffer.getChannelData(channel);
    var post_ch = post_modification_clone.buffer.getChannelData(channel);

    assert_array_equals(pre_ch, post_ch, 'Cloned data ch=' + channel);
  }
}, 'Verify AudioData is snapshotted and internally immutable');

test(t => {
  let data = make_audio_data(
      -10, defaultInit.channels, defaultInit.sampleRate, defaultInit.frames);
  assert_equals(data.timestamp, -10, 'timestamp');
  data.close();
}, 'Test we can construct AudioData with a negative timestamp.');
