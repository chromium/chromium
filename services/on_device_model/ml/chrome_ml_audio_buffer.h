// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_ON_DEVICE_MODEL_ML_CHROME_ML_AUDIO_BUFFER_H_
#define SERVICES_ON_DEVICE_MODEL_ML_CHROME_ML_AUDIO_BUFFER_H_

#include <stdint.h>

#include <string>
#include <variant>
#include <vector>

namespace ml {

struct AudioBuffer {
  AudioBuffer();
  ~AudioBuffer();
  AudioBuffer(const AudioBuffer&);
  AudioBuffer& operator=(const AudioBuffer& other);
  AudioBuffer(AudioBuffer&&);
  AudioBuffer& operator=(AudioBuffer&& other);

  int32_t sample_rate_hz;
  int32_t num_channels;
  int32_t num_frames;
  std::vector<float> data;
};

}  // namespace ml

#endif  // SERVICES_ON_DEVICE_MODEL_ML_CHROME_ML_AUDIO_BUFFER_H_
