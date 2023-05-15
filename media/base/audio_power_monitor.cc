// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/audio_power_monitor.h"

#include <algorithm>
#include <cmath>

#include "base/check_op.h"
#include "base/time/time.h"
#include "media/base/audio_bus.h"
#include "media/base/vector_math.h"

namespace media {

AudioPowerMonitor::AudioPowerMonitor(int sample_rate,
                                     base::TimeDelta time_constant)
    : sample_weight_(1.0f -
                     expf(-1.0f / (sample_rate * time_constant.InSecondsF()))) {
  Reset();
}

AudioPowerMonitor::~AudioPowerMonitor() = default;

void AudioPowerMonitor::Reset() {
  // These are only read/written by Scan(), but Scan() should not be running
  // when Reset() is called.
  average_power_ = 0.0f;
  has_clipped_ = false;

  // These are the copies read by ReadCurrentPowerAndClip().  The lock here is
  // not necessary, as racey writes/reads are acceptable, but this prevents
  // quality-enhancement tools like TSAN from complaining.
  base::AutoLock for_reset(reading_lock_);
  power_reading_ = 0.0f;
  clipped_reading_ = false;
}

void AudioPowerMonitor::Scan(const AudioBus& buffer, int num_frames) {
  DCHECK_LE(num_frames, buffer.frames());
  const int num_channels = buffer.channels();
  if (num_frames <= 0 || num_channels <= 0)
    return;

  // Calculate a new average power by applying a first-order low-pass filter
  // (a.k.a. an exponentially-weighted moving average) over the audio samples in
  // each channel in |buffer|.
  float sum_power = 0.0f;
  for (int i = 0; i < num_channels; ++i) {
    const std::pair<float, float> ewma_and_max = vector_math::EWMAAndMaxPower(
        average_power_, buffer.channel(i), num_frames, sample_weight_);
    // If data in audio buffer is garbage, ignore its effect on the result.
    if (!std::isfinite(ewma_and_max.first)) {
      sum_power += average_power_;
    } else {
      sum_power += ewma_and_max.first;
      has_clipped_ |= (ewma_and_max.second > 1.0f);
    }
  }

  // Update accumulated results, with clamping for sanity.
  average_power_ = std::clamp(sum_power / num_channels, 0.0f, 1.0f);

  // Push results for reading by other threads, non-blocking.
  if (reading_lock_.Try()) {
    power_reading_ = average_power_;
    if (has_clipped_) {
      clipped_reading_ = true;
      has_clipped_ = false;
    }
    reading_lock_.Release();
  }
}

std::pair<float, bool> AudioPowerMonitor::ReadCurrentPowerAndClip() {
  base::AutoLock for_reading(reading_lock_);

  // Convert power level to dBFS units, and pin it down to zero if it is
  // insignificantly small.
  const float kInsignificantPower = 1.0e-10f;  // -100 dBFS
  const float power_dbfs = power_reading_ < kInsignificantPower
                               ? zero_power()
                               : 10.0f * log10f(power_reading_);

  const bool clipped = clipped_reading_;
  clipped_reading_ = false;

  return std::make_pair(power_dbfs, clipped);
}

}  // namespace media
