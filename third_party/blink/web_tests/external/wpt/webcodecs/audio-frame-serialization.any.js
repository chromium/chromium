// META: global=window
// META: script=/common/media.js
// META: script=/webcodecs/utils.js

var defaultInit = {
  timestamp: 1234,
  channels: 2,
  sampleRate: 8000,
  frames: 100,
}

function createDefaultAudioFrame() {
  return make_audio_frame(defaultInit.timestamp,
                          defaultInit.channels,
                          defaultInit.sampleRate,
                          defaultInit.frames);
}

async_test(t => {
  let localFrame = createDefaultAudioFrame();

  let channel = new MessageChannel();
  let localPort = channel.port1;
  let externalPort = channel.port2;

  externalPort.onmessage = t.step_func((e) => {
    let externalFrame = e.data;
    let buffer = externalFrame.buffer;
    // We should have a valid deserialized buffer.
    assert_true(buffer != undefined || buffer != null);
    assert_equals(buffer.numberOfChannels,
                  localFrame.buffer.numberOfChannels, "numberOfChannels");

    for (var channel = 0; channel < buffer.numberOfChannels; channel++) {
      // This gives us the actual array that contains the data
      var dest_array = buffer.getChannelData(channel);
      var source_array = localFrame.buffer.getChannelData(channel);
      for (var i = 0; i < dest_array.length; i+=10) {
        assert_equals(dest_array[i], source_array[i],
          "data (ch=" + channel + ", i=" + i + ")");
      }
    }

    externalFrame.close();
    externalPort.postMessage("Done");
  })

  localPort.onmessage = t.step_func_done((e) => {
    assert_true(localFrame.buffer != null);
    localFrame.close();
  })

  localPort.postMessage(localFrame);

}, 'Verify closing frames does not propagate accross contexts.');

async_test(t => {
  let localFrame = createDefaultAudioFrame();

  let channel = new MessageChannel();
  let localPort = channel.port1;

  localPort.onmessage = t.unreached_func();

  localFrame.close();

  assert_throws_dom("DataCloneError", () => {
    localPort.postMessage(localFrame);
  });

  t.done();
}, 'Verify posting closed frames throws.');
