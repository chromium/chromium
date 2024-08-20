// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "remoting/host/audio_silence_detector.h"

#include <stdlib.h>

#include "base/check_op.h"

namespace remoting {

namespace {

// Silence period threshold in seconds. Silence intervals shorter than this
// value are still encoded and sent to the client, so that we don't disrupt
// playback by dropping them.
int kSilencePeriodThresholdSeconds = 1;

}  // namespace

AudioSilenceDetector::AudioSilenceDetector(int threshold)
    : threshold_(threshold),
      silence_length_max_(0),
      silence_length_(0),
      channels_(0) {
  DCHECK_GE(threshold_, 0);
}

AudioSilenceDetector::~AudioSilenceDetector() = default;

void AudioSilenceDetector::Reset(int sampling_rate, int channels) {
  DCHECK_GT(sampling_rate, 0);
  DCHECK_GT(channels, 0);
  silence_length_ = 0;
  silence_length_max_ =
      sampling_rate * channels * kSilencePeriodThresholdSeconds;
  channels_ = channels;
}

bool AudioSilenceDetector::IsSilence(const int16_t* samples, size_t frames) {
  const int samples_count = frames * channels();
  bool silent_packet = true;
  // Potentially this loop can be optimized (e.g. using SSE or adding special
  // case for threshold_==0), but it's not worth worrying about because the
  // amount of data it processes is relatively small.
  for (int i = 0; i < samples_count; ++i) {
    if (abs(samples[i]) > threshold_) {
      silent_packet = false;
      break;
    }
  }

  if (!silent_packet) {
    silence_length_ = 0;
    return false;
  }

  silence_length_ += samples_count;
  return silence_length_ > silence_length_max_;
}

int AudioSilenceDetector::channels() const {
  return channels_;
}

}  // namespace remoting
