// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/audio_pcm_encoder.h"

#include <utility>

namespace media {

AudioPcmEncoder::AudioPcmEncoder(const AudioParameters& input_params,
                                 EncodeCB encode_callback,
                                 StatusCB status_callback)
    : AudioEncoder(input_params,
                   std::move(encode_callback),
                   std::move(status_callback)) {}

void AudioPcmEncoder::EncodeAudioImpl(const AudioBus& audio_bus,
                                      base::TimeTicks capture_time) {
  const size_t size = audio_bus.frames() * audio_bus.channels() * sizeof(float);
  std::unique_ptr<uint8_t[]> encoded_data(new uint8_t[size]);
  audio_bus.ToInterleaved<Float32SampleTypeTraits>(
      audio_bus.frames(), reinterpret_cast<float*>(encoded_data.get()));

  encode_callback().Run(
      EncodedAudioBuffer(audio_input_params(), std::move(encoded_data), size,
                         ComputeTimestamp(audio_bus.frames(), capture_time)));
}

}  // namespace media
