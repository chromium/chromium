// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SPEECH_AUDIO_ENCODER_H_
#define CONTENT_BROWSER_SPEECH_AUDIO_ENCODER_H_

#include <string>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "content/browser/speech/audio_buffer.h"
#include "third_party/flac/include/FLAC/stream_encoder.h"

namespace content{
class AudioChunk;

// Provides a simple interface to encode raw audio using FLAC codec.
class AudioEncoder {
 public:
  AudioEncoder(int sampling_rate, int bits_per_sample);
  ~AudioEncoder();

  // Encodes |raw audio| to the internal buffer. Use
  // |GetEncodedDataAndClear| to read the result after this call or when
  // audio capture completes.
  void Encode(const AudioChunk& raw_audio);

  // Finish encoding and flush any pending encoded bits out.
  void Flush();

  // Merges, retrieves and clears all the accumulated encoded audio chunks.
  scoped_refptr<AudioChunk> GetEncodedDataAndClear();

  std::string GetMimeType();
  int GetBitsPerSample();

 private:
  AudioBuffer encoded_audio_buffer_;

  FLAC__StreamEncoder* encoder_;
  bool is_encoder_initialized_;

  DISALLOW_COPY_AND_ASSIGN(AudioEncoder);
};

}  // namespace content

#endif  // CONTENT_BROWSER_SPEECH_AUDIO_ENCODER_H_
