// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/audio_volume_filter.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {

namespace {

class FakeAudioVolumeFilter : public AudioVolumeFilter {
 public:
  FakeAudioVolumeFilter(int silence_threshold)
      : AudioVolumeFilter(silence_threshold) {}
  ~FakeAudioVolumeFilter() override = default;

  void set_audio_level(float level) { level_ = level; }

 protected:
  float GetAudioLevel() override { return level_; }

 private:
  float level_ = 0;
};

}  // namespace

TEST(AudioVolumeFilterTest, TwoChannels) {
  int16_t samples[] = {1, 1, 2, 2, 3, 3, 4, 4, 5,  5,
                       6, 6, 7, 7, 8, 8, 9, 9, 10, 10};
  FakeAudioVolumeFilter filter(0);
  filter.set_audio_level(0.5f);
  filter.Initialize(9, 2);
  // After applying the audio volume, the |samples| should still pass the
  // AudioSilenceDetector, AudioVolumeFilter::Apply() returns true under this
  // condition. Ditto.
  ASSERT_TRUE(filter.Apply(samples, std::size(samples) / 2));
}

TEST(AudioVolumeFilterTest, ThreeChannels) {
  int16_t samples[] = {1, 1, 2, 2, 3, 3, 4, 4,  5,  5, 6,
                       6, 7, 7, 8, 8, 9, 9, 10, 10, 11};
  FakeAudioVolumeFilter filter(0);
  filter.set_audio_level(0.5f);
  filter.Initialize(6, 3);
  ASSERT_TRUE(filter.Apply(samples, std::size(samples) / 3));
}

TEST(AudioVolumeFilterTest, SilentSamples) {
  int16_t samples[] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                       0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
  FakeAudioVolumeFilter filter(0);
  filter.set_audio_level(0.5f);
  filter.Initialize(9, 2);
  ASSERT_FALSE(filter.Apply(samples, std::size(samples) / 2));
}

TEST(AudioVolumeFilterTest, AudioLevel0) {
  int16_t samples[] = {1, 1, 2, 2, 3, 3, 4, 4, 5,  5,
                       6, 6, 7, 7, 8, 8, 9, 9, 10, 10};
  FakeAudioVolumeFilter filter(0);
  filter.set_audio_level(0);
  filter.Initialize(9, 2);
  ASSERT_FALSE(filter.Apply(samples, std::size(samples) / 2));
}

TEST(AudioVolumeFilterTest, SilentAfterApplying) {
  int16_t samples[] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
                       1, 1, 1, 1, 1, 1, 1, 1, 1, 1};
  FakeAudioVolumeFilter filter(0);
  filter.set_audio_level(0.9f);
  filter.Initialize(9, 2);
  ASSERT_TRUE(filter.Apply(samples, std::size(samples) / 2));
}

}  // namespace remoting
