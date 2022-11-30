// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include <fuzzer/FuzzedDataProvider.h>

#include "components/speech/audio_encoder.h"

// Copied from speech_recognition_engine.cc.
const int kDefaultConfigSampleRate = 8000;
const int kDefaultConfigBitsPerSample = 16;
const int kAudioSampleRate = 16000;
const int kAudioPacketIntervalMs = 100;

const int kDefaultConfigBytesPerSample = kDefaultConfigBitsPerSample / 8;

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  FuzzedDataProvider provider(data, size);
  AudioEncoder encoder(kDefaultConfigSampleRate, kDefaultConfigBitsPerSample);

  while (provider.remaining_bytes()) {
    std::string chunk_str =
        provider.ConsumeRandomLengthString(provider.remaining_bytes());
    if (chunk_str.size() % kDefaultConfigBytesPerSample) {
      // `AudioChunk` expects the data size to be divisible by sample size.
      continue;
    }
    scoped_refptr<AudioChunk> chunk =
        new AudioChunk(reinterpret_cast<const uint8_t*>(chunk_str.data()),
                       chunk_str.size(), kDefaultConfigBytesPerSample);
    encoder.Encode(*chunk);
  }

  size_t sample_count = kAudioSampleRate * kAudioPacketIntervalMs / 1000;
  scoped_refptr<AudioChunk> dummy_chunk = new AudioChunk(
      sample_count * sizeof(int16_t), kDefaultConfigBytesPerSample);
  encoder.Encode(*dummy_chunk.get());
  encoder.Flush();
  scoped_refptr<AudioChunk> encoded_data(encoder.GetEncodedDataAndClear());
  encoded_data->AsString();
  encoder.GetMimeType();
  encoder.GetBitsPerSample();
  return 0;
}
