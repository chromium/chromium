// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/test/utility/audio_utility.h"

#include <cmath>
#include <numbers>
#include <vector>

#include "base/check_op.h"
#include "base/time/time.h"
#include "media/base/audio_bus.h"

namespace media::cast {

TestAudioBusFactory::TestAudioBusFactory(int num_channels,
                                         int sample_rate,
                                         float sine_wave_frequency,
                                         float volume)
    : num_channels_(num_channels),
      sample_rate_(sample_rate),
      volume_(volume),
      source_(num_channels, sine_wave_frequency, sample_rate) {
  CHECK_GT(num_channels, 0);
  CHECK_GT(sample_rate, 0);
  CHECK_GE(volume_, 0.0f);
  CHECK_LE(volume_, 1.0f);
}

TestAudioBusFactory::~TestAudioBusFactory() = default;

// static
constexpr int TestAudioBusFactory::kMiddleANoteFreq;

std::unique_ptr<AudioBus> TestAudioBusFactory::NextAudioBus(
    const base::TimeDelta& duration) {
  const int num_samples = (sample_rate_ * duration).InSeconds();
  std::unique_ptr<AudioBus> bus(AudioBus::Create(num_channels_, num_samples));
  source_.OnMoreData(base::TimeDelta(), base::TimeTicks::Now(), {}, bus.get());
  bus->Scale(volume_);
  return bus;
}

}  // namespace media::cast
