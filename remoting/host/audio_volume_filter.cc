// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "remoting/host/audio_volume_filter.h"

namespace remoting {

AudioVolumeFilter::AudioVolumeFilter(int silence_threshold)
    : silence_detector_(silence_threshold) {}
AudioVolumeFilter::~AudioVolumeFilter() = default;

bool AudioVolumeFilter::Apply(int16_t* data, size_t frames) {
  if (frames == 0) {
    return false;
  }

  if (silence_detector_.IsSilence(data, frames)) {
    return false;
  }

  float level = GetAudioLevel();
  if (level == 0) {
    return false;
  }

  if (level == 1) {
    return true;
  }

  const int sample_count = frames * silence_detector_.channels();
  const int32_t level_int = static_cast<int32_t>(level * 65536);
  for (int i = 0; i < sample_count; i++) {
    data[i] = (static_cast<int32_t>(data[i]) * level_int) >> 16;
  }

  return true;
}

void AudioVolumeFilter::Initialize(int sampling_rate, int channels) {
  silence_detector_.Reset(sampling_rate, channels);
}

}  // namespace remoting
