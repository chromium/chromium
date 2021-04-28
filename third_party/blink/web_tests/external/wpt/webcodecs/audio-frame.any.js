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
createDefaultAudioFrame() {
  return make_audio_frame(
      defaultInit.timestamp, defaultInit.channels, defaultInit.sampleRate,
      defaultInit.frames);
}

test(t => {
  let localBuffer = new AudioBuffer({
    length: defaultInit.frames,
    numberOfChannels: defaultInit.channels,
    sampleRate: defaultInit.sampleRate
  });

  let audioFrameInit = {timestamp: defaultInit.timestamp, buffer: localBuffer}

  let frame = new AudioFrame(audioFrameInit);

  assert_equals(frame.timestamp, defaultInit.timestamp, 'timestamp');
  assert_equals(frame.buffer.length, defaultInit.frames, 'frames');
  assert_equals(
      frame.buffer.numberOfChannels, defaultInit.channels, 'channels');
  assert_equals(frame.buffer.sampleRate, defaultInit.sampleRate, 'sampleRate');

  assert_throws_js(
      TypeError, () => {let frame = new AudioFrame({buffer: localBuffer})},
      'AudioFrames require \'timestamp\'')

  assert_throws_js(
      TypeError,
      () => {let frame = new AudioFrame({timestamp: defaultInit.timestamp})},
      'AudioFrames require \'buffer\'')
}, 'Verify AudioFrame constructors');

test(t => {
  let frame = createDefaultAudioFrame();

  let clone = frame.clone();

  // Verify the parameters match.
  assert_equals(frame.timestamp, clone.timestamp, 'timestamp');
  assert_equals(frame.buffer.length, clone.buffer.length, 'frames');
  assert_equals(
      frame.buffer.numberOfChannels, clone.buffer.numberOfChannels, 'channels');
  assert_equals(frame.buffer.sampleRate, clone.buffer.sampleRate, 'sampleRate');

  // Verify the data matches.
  for (var channel = 0; channel < frame.buffer.numberOfChannels; channel++) {
    var orig_ch = frame.buffer.getChannelData(channel);
    var cloned_ch = clone.buffer.getChannelData(channel);

    assert_array_equals(orig_ch, cloned_ch, 'Cloned data ch=' + channel);
  }

  // Verify closing the original frame doesn't close the clone.
  frame.close();
  assert_equals(frame.buffer, null, 'frame.buffer (closed)');
  assert_not_equals(clone.buffer, null, 'clone.buffer (not closed)');

  clone.close();
  assert_equals(clone.buffer, null, 'clone.buffer (closed)');

  // Verify closing a closed frame does not throw.
  frame.close();
}, 'Verify closing and cloning AudioFrames');

test(t => {
  let frame = createDefaultAudioFrame();

  // Get a copy of the original data.
  let pre_modification_clone = frame.clone();

  for (var channel = 0; channel < frame.buffer.numberOfChannels; channel++) {
    var orig_ch = frame.buffer.getChannelData(channel);

    // Flip the polarity of the original frame's buffer.
    for (let i = 0; i < orig_ch.length; ++i) {
      orig_ch.buffer[i] = -orig_ch.buffer[i];
    }
  }

  // The data in 'frame' should have been snapshotted internally, and
  // despite changes to frame.buffer, post_modification_clone should not contain
  // modified data.
  let post_modification_clone = frame.clone();

  // Verify the data matches.
  for (var channel = 0; channel < frame.buffer.numberOfChannels; channel++) {
    var pre_ch = pre_modification_clone.buffer.getChannelData(channel);
    var post_ch = post_modification_clone.buffer.getChannelData(channel);

    assert_array_equals(pre_ch, post_ch, 'Cloned data ch=' + channel);
  }
}, 'Verify frame data is snapshotted and internally immutable');

test(t => {
  let frame = make_audio_frame(
      -10, defaultInit.channels, defaultInit.sampleRate, defaultInit.frames);
  assert_equals(frame.timestamp, -10, 'timestamp');
  frame.close();
}, 'Test we can construct a AudioFrame with a negative timestamp.');
