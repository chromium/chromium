// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/mediastream/media_stream_audio_level_calculator.h"

#include <cmath>
#include <limits>

#include "base/check.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_refptr.h"
#include "base/types/pass_key.h"
#include "media/base/audio_bus.h"

namespace blink {

namespace {

// Calculates the maximum absolute amplitude of the audio data.
float MaxAbsoluteAmplitude(base::span<const float> audio_data) {
  float max = 0.0f;
  for (auto sample : audio_data) {
    max = std::max(max, fabsf(sample));
  }
  DCHECK(std::isfinite(max));
  return max;
}

}  // namespace

MediaStreamAudioLevelCalculator::Level::Level(
    base::PassKey<MediaStreamAudioLevelCalculator>) {}

MediaStreamAudioLevelCalculator::Level::~Level() = default;

float MediaStreamAudioLevelCalculator::Level::GetCurrent() const {
  base::AutoLock auto_lock(lock_);
  return level_;
}

void MediaStreamAudioLevelCalculator::Level::Set(float level) {
  base::AutoLock auto_lock(lock_);
  level_ = level;
}

MediaStreamAudioLevelCalculator::MediaStreamAudioLevelCalculator()
    : level_(base::MakeRefCounted<Level>(
          base::PassKey<MediaStreamAudioLevelCalculator>())) {}

MediaStreamAudioLevelCalculator::~MediaStreamAudioLevelCalculator() {
  level_->Set(0.0f);
}

void MediaStreamAudioLevelCalculator::Calculate(
    const media::AudioBus& audio_bus,
    bool assume_nonzero_energy) {
  // |level_| is updated every 10 callbacks. For the case where callback comes
  // every 10ms, |level_| will be updated approximately every 100ms.
  static const int kUpdateFrequency = 10;

  float max_for_bus =
      assume_nonzero_energy ? 1.0f / std::numeric_limits<int16_t>::max() : 0.0f;

  for (auto channel : audio_bus.AllChannels()) {
    max_for_bus = std::max(max_for_bus, MaxAbsoluteAmplitude(channel));
  }

  max_amplitude_ = std::max(max_amplitude_, max_for_bus);

  if (++counter_ == kUpdateFrequency) {
    // Clip the exposed signal level to make sure it is in the range [0.0,1.0].
    level_->Set(std::min(1.0f, max_amplitude_));

    // Decay the absolute maximum amplitude by 1/4.
    max_amplitude_ /= 4.0f;

    // Reset the counter.
    counter_ = 0;
  }
}

}  // namespace blink
