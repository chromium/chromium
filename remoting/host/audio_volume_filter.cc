// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/audio_volume_filter.h"

#include "base/compiler_specific.h"
#include "base/containers/span.h"

namespace remoting {

AudioVolumeFilter::AudioVolumeFilter(int silence_threshold)
    : silence_detector_(silence_threshold) {}
AudioVolumeFilter::~AudioVolumeFilter() = default;

bool AudioVolumeFilter::Apply(base::span<int16_t> data) {
  if (data.empty()) {
    return false;
  }

  if (silence_detector_.IsSilence(base::as_bytes(data))) {
    return false;
  }

  float level = GetAudioLevel();
  if (level == 0) {
    return false;
  }

  if (level == 1) {
    return true;
  }

  const int32_t level_int = static_cast<int32_t>(level * 65536);
  for (int16_t& sample : data) {
    sample = (static_cast<int32_t>(sample) * level_int) >> 16;
  }

  return true;
}

void AudioVolumeFilter::Initialize(int sampling_rate, int channels) {
  silence_detector_.Reset(sampling_rate, channels);
}

}  // namespace remoting
