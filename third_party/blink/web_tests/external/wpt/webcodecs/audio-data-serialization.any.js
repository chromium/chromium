// META: global=window
// META: script=/common/media.js
// META: script=/webcodecs/utils.js

var defaultInit = {
  timestamp: 1234,
  channels: 2,
  sampleRate: 8000,
  frames: 100,
}

function createDefaultAudioData() {
  return make_audio_data(defaultInit.timestamp,
                          defaultInit.channels,
                          defaultInit.sampleRate,
                          defaultInit.frames);
}

async_test(t => {
  let localData = createDefaultAudioData();

  let channel = new MessageChannel();
  let localPort = channel.port1;
  let externalPort = channel.port2;

  externalPort.onmessage = t.step_func((e) => {
    let externalData = e.data;
    let buffer = externalData.buffer;
    // We should have a valid deserialized buffer.
    assert_true(buffer != undefined || buffer != null);
    assert_equals(buffer.numberOfChannels,
                  localData.buffer.numberOfChannels, "numberOfChannels");

    for (var channel = 0; channel < buffer.numberOfChannels; channel++) {
      // This gives us the actual array that contains the data
      var dest_array = buffer.getChannelData(channel);
      var source_array = localData.buffer.getChannelData(channel);
      for (var i = 0; i < dest_array.length; i+=10) {
        assert_equals(dest_array[i], source_array[i],
          "data (ch=" + channel + ", i=" + i + ")");
      }
    }

    externalData.close();
    externalPort.postMessage("Done");
  })

  localPort.onmessage = t.step_func_done((e) => {
    assert_true(localData.buffer != null);
    localData.close();
  })

  localPort.postMessage(localData);

}, 'Verify closing AudioData does not propagate accross contexts.');

async_test(t => {
  let localData = createDefaultAudioData();

  let channel = new MessageChannel();
  let localPort = channel.port1;

  localPort.onmessage = t.unreached_func();

  localData.close();

  assert_throws_dom("DataCloneError", () => {
    localPort.postMessage(localData);
  });

  t.done();
}, 'Verify posting closed AudioData throws.');
