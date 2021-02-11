// META: global=window
// META: script=/webcodecs/utils.js

function make_audio_frame(timestamp, channels, sampleRate, length) {
  let buffer = new AudioBuffer({
    length: length,
    numberOfChannels: channels,
    sampleRate: sampleRate
  });

  for (var channel = 0; channel < buffer.numberOfChannels; channel++) {
    // This gives us the actual array that contains the data
    var array = buffer.getChannelData(channel);
    let hz = 100 + channel * 50; // sound frequency
    for (var i = 0; i < array.length; i++) {
      let t = (i / sampleRate) * hz * (Math.PI * 2);
      array[i] = Math.sin(t);
    }
  }

  return new AudioFrame({
    timestamp: timestamp,
    buffer: buffer
  });
}

// Merge all audio buffers into a new big one with all the data.
function join_buffers(buffers) {
  assert_greater_than_equal(buffers.length, 0);
  let total_length = 0;
  let base_buffer = buffers[0];
  for (const buffer of buffers) {
    assert_not_equals(buffer, null);
    assert_equals(buffer.sampleRate, base_buffer.sampleRate);
    assert_equals(buffer.numberOfChannels, base_buffer.numberOfChannels);
    total_length += buffer.length;
  }

  let result = new AudioBuffer({
    length: total_length,
    numberOfChannels: base_buffer.numberOfChannels,
    sampleRate: base_buffer.sampleRate
  });

  for (let i = 0; i < base_buffer.numberOfChannels; i++) {
    let channel = result.getChannelData(i);
    let position = 0;
    for (const buffer of buffers) {
      channel.set(buffer.getChannelData(i), position);
      position += buffer.length;
    }
    assert_equals(position, total_length);
  }

  return result;
}

function clone_frame(frame) {
  return new AudioFrame({
    timestamp: frame.timestamp,
    buffer: join_buffers([frame.buffer])
  });
}

promise_test(async t => {
  let sample_rate = 48000;
  let total_duration_s = 2;
  let frame_count = 20;
  let outputs = [];
  let init = {
    error: e => {
      assert_unreached("error: " + e);
    },
    output: chunk => {
      outputs.push(chunk);
    }
  };

  let encoder = new AudioEncoder(init);

  assert_equals(encoder.state, "unconfigured");
  let config = {
    codec: 'opus',
    sampleRate: sample_rate,
    numberOfChannels: 2,
    bitrate: 256000 //256kbit
  };

  encoder.configure(config);

  let timestamp_us = 0;
  for (let i = 0; i < frame_count; i++) {
    let frame_duration_s = total_duration_s / frame_count;
    let length = frame_duration_s * config.sampleRate;
    let frame = make_audio_frame(timestamp_us, config.numberOfChannels,
      config.sampleRate, length);
    encoder.encode(frame);
    timestamp_us += frame_duration_s * 1_000_000;
  }
  await encoder.flush();
  encoder.close();
  assert_greater_than_equal(outputs.length, frame_count);
  assert_equals(outputs[0].timestamp, 0, "first chunk timestamp");
  for (chunk of outputs) {
    assert_greater_than(chunk.data.byteLength, 0);
    assert_greater_than(timestamp_us, chunk.timestamp);
  }
}, 'Simple audio encoding');


promise_test(async t => {
  let sample_rate = 48000;
  let total_duration_s = 2;
  let frame_count = 20;
  let input_frames = [];
  let output_frames = [];

  let decoder_init = {
    error: t.unreached_func("Decode error"),
    output: frame => {
      output_frames.push(frame);
    }
  };
  let decoder = new AudioDecoder(decoder_init);

  let encoder_init = {
    error: t.unreached_func("Encoder error"),
    output: chunk => {
      decoder.decode(chunk);
    }
  };
  let encoder = new AudioEncoder(encoder_init);

  let config = {
    codec: 'opus',
    sampleRate: sample_rate,
    numberOfChannels: 2,
    bitrate: 256000, //256kbit
    // Opus header extradata.
    // TODO(https://crbug.com/1177021) Get this data from AudioEncoder
    description: new Uint8Array([0x4f, 0x70, 0x75, 0x73, 0x48, 0x65, 0x61, 0x64,
      0x01, 0x02, 0x38, 0x01, 0x80, 0xbb, 0x00, 0x00, 0x00, 0x00, 0x00])
  };

  encoder.configure(config);
  decoder.configure(config);

  let timestamp_us = 0;
  const frame_duration_s = total_duration_s / frame_count;
  const frame_length = frame_duration_s * config.sampleRate;
  for (let i = 0; i < frame_count; i++) {
    let frame = make_audio_frame(timestamp_us, config.numberOfChannels,
      config.sampleRate, frame_length);
    input_frames.push(clone_frame(frame));
    encoder.encode(frame);
    timestamp_us += frame_duration_s * 1_000_000;
  }
  await encoder.flush();
  encoder.close();
  await decoder.flush();
  decoder.close();


  let total_input = join_buffers(input_frames.map(f => f.buffer));
  let total_output = join_buffers(output_frames.map(f => f.buffer));
  assert_equals(total_output.numberOfChannels, 2);
  assert_equals(total_output.sampleRate, sample_rate);

  // Output can be slightly longer that the input due to padding
  assert_greater_than_equal(total_output.length, total_input.length);
  assert_greater_than_equal(total_output.duration, total_duration_s);
  assert_approx_equals(total_output.duration, total_duration_s, 0.1);

  // Compare waveform before and after encoding
  for (let channel = 0; channel < total_input.numberOfChannels; channel++) {
    let input_data = total_input.getChannelData(channel);
    let output_data = total_output.getChannelData(channel);
    for (let i = 0; i < total_input.length; i++) {
      assert_approx_equals(input_data[i], output_data[i], 0.5,
        "Difference between input and output is too large."
        + " index: " + i
        + " input: " + input_data[i]
        + " output: " + output_data[i]);
    }
  }

}, 'Encoding and decoding');