// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAST_TEST_UTILITY_AUDIO_UTILITY_H_
#define MEDIA_CAST_TEST_UTILITY_AUDIO_UTILITY_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>

#include "media/audio/simple_sources.h"

namespace base {
class TimeDelta;
}

namespace media {
class AudioBus;
}

namespace media::cast {

// Produces AudioBuses of varying duration where each successive output contains
// the continuation of a single sine wave.
class TestAudioBusFactory {
 public:
  TestAudioBusFactory(int num_channels,
                      int sample_rate,
                      float sine_wave_frequency,
                      float volume);

  TestAudioBusFactory(const TestAudioBusFactory&) = delete;
  TestAudioBusFactory& operator=(const TestAudioBusFactory&) = delete;

  ~TestAudioBusFactory();

  // Creates a new AudioBus of the given |duration|, filled with the next batch
  // of sine wave samples.
  std::unique_ptr<AudioBus> NextAudioBus(const base::TimeDelta& duration);

  // A reasonable test tone.
  static constexpr int kMiddleANoteFreq = 440;

 private:
  const int num_channels_ = 0;
  const int sample_rate_ = 0;
  const float volume_ = 0.0f;
  SineWaveAudioSource source_;
};

}  // namespace media::cast

#endif  // MEDIA_CAST_TEST_UTILITY_AUDIO_UTILITY_H_
