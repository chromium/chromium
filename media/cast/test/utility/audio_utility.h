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

namespace media {
namespace cast {

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
  static const int kMiddleANoteFreq = 440;

 private:
  const int num_channels_;
  const int sample_rate_;
  const float volume_;
  SineWaveAudioSource source_;
};

// Assuming |samples| contains a single-frequency sine wave (and maybe some
// low-amplitude noise), count the number of times the sine wave crosses
// zero.
//
// Example use case: When expecting a 440 Hz tone, this can be checked using the
// following expression:
//
//   abs((CountZeroCrossings(...) / seconds_per_frame / 2) - 440) <= 1
//
// ...where seconds_per_frame is the number of samples divided by the sampling
// rate.  The divide by two accounts for the fact that a sine wave crosses zero
// twice per cycle (first downwards, then upwards).  The absolute maximum
// difference of 1 accounts for the sine wave being out of perfect phase.
int CountZeroCrossings(const float* samples, int length);

// Encode |timestamp| into the samples pointed to by 'samples' in a way
// that should be decodable even after compressing/decompressing the audio.
// Assumes 48Khz sampling rate and needs at least 240 samples. Returns
// false if |length| of |samples| is too small. If more than 240 samples are
// available, then the timestamp will be repeated. |sample_offset| should
// contain how many samples has been encoded so far, so that we can make smooth
// transitions between encoded chunks.
// See audio_utility.cc for details on how the encoding is done.
bool EncodeTimestamp(uint16_t timestamp,
                     size_t sample_offset,
                     size_t length,
                     float* samples);

// Decode a timestamp encoded with EncodeTimestamp. Returns true if a
// timestamp was found in |samples|.
bool DecodeTimestamp(const float* samples, size_t length, uint16_t* timestamp);

}  // namespace cast
}  // namespace media

#endif  // MEDIA_CAST_TEST_UTILITY_AUDIO_UTILITY_H_
