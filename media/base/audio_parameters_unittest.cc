// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/audio_parameters.h"

#include <stddef.h>

#include "base/strings/string_number_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

TEST(AudioParameters, Constructor_Default) {
  AudioParameters::Format expected_format = AudioParameters::AUDIO_PCM_LINEAR;
  int expected_channels = 0;
  ChannelLayout expected_channel_layout = CHANNEL_LAYOUT_NONE;
  int expected_rate = 0;
  int expected_samples = 0;
  AudioParameters::PlatformEffectsMask expected_effects =
      AudioParameters::NO_EFFECTS;
  std::vector<Point> expected_mic_positions;

  AudioParameters params;

  EXPECT_EQ(expected_format, params.format());
  EXPECT_EQ(expected_channels, params.channels());
  EXPECT_EQ(expected_channel_layout, params.channel_layout());
  EXPECT_EQ(expected_rate, params.sample_rate());
  EXPECT_EQ(expected_samples, params.frames_per_buffer());
  EXPECT_EQ(expected_effects, params.effects());
  EXPECT_EQ(expected_mic_positions, params.mic_positions());
}

TEST(AudioParameters, Constructor_ParameterValues) {
  AudioParameters::Format expected_format =
      AudioParameters::AUDIO_PCM_LOW_LATENCY;
  int expected_channels = 6;
  ChannelLayout expected_channel_layout = CHANNEL_LAYOUT_5_1;
  int expected_rate = 44100;
  int expected_samples = 880;

  AudioParameters params(expected_format, expected_channel_layout,
                         expected_rate, expected_samples);

  EXPECT_EQ(expected_format, params.format());
  EXPECT_EQ(expected_channels, params.channels());
  EXPECT_EQ(expected_channel_layout, params.channel_layout());
  EXPECT_EQ(expected_rate, params.sample_rate());
  EXPECT_EQ(expected_samples, params.frames_per_buffer());
}

TEST(AudioParameters, GetBytesPerBuffer) {
  EXPECT_EQ(100, AudioParameters(AudioParameters::AUDIO_PCM_LINEAR,
                                 CHANNEL_LAYOUT_MONO, 1000, 100)
                     .GetBytesPerBuffer(kSampleFormatU8));
  EXPECT_EQ(200, AudioParameters(AudioParameters::AUDIO_PCM_LINEAR,
                                 CHANNEL_LAYOUT_MONO, 1000, 100)
                     .GetBytesPerBuffer(kSampleFormatS16));
  EXPECT_EQ(200, AudioParameters(AudioParameters::AUDIO_PCM_LINEAR,
                                 CHANNEL_LAYOUT_STEREO, 1000, 100)
                     .GetBytesPerBuffer(kSampleFormatU8));
  EXPECT_EQ(200, AudioParameters(AudioParameters::AUDIO_PCM_LINEAR,
                                 CHANNEL_LAYOUT_MONO, 1000, 200)
                     .GetBytesPerBuffer(kSampleFormatU8));
  EXPECT_EQ(800, AudioParameters(AudioParameters::AUDIO_PCM_LINEAR,
                                 CHANNEL_LAYOUT_STEREO, 1000, 200)
                     .GetBytesPerBuffer(kSampleFormatS16));
}

TEST(AudioParameters, Compare) {
  AudioParameters values[] = {
      AudioParameters(AudioParameters::AUDIO_PCM_LINEAR, CHANNEL_LAYOUT_MONO,
                      1000, 100),
      AudioParameters(AudioParameters::AUDIO_PCM_LINEAR, CHANNEL_LAYOUT_MONO,
                      1000, 200),
      AudioParameters(AudioParameters::AUDIO_PCM_LINEAR, CHANNEL_LAYOUT_MONO,
                      2000, 100),
      AudioParameters(AudioParameters::AUDIO_PCM_LINEAR, CHANNEL_LAYOUT_MONO,
                      2000, 200),

      AudioParameters(AudioParameters::AUDIO_PCM_LINEAR, CHANNEL_LAYOUT_STEREO,
                      1000, 100),
      AudioParameters(AudioParameters::AUDIO_PCM_LINEAR, CHANNEL_LAYOUT_STEREO,
                      1000, 200),
      AudioParameters(AudioParameters::AUDIO_PCM_LINEAR, CHANNEL_LAYOUT_STEREO,
                      2000, 100),
      AudioParameters(AudioParameters::AUDIO_PCM_LINEAR, CHANNEL_LAYOUT_STEREO,
                      2000, 200),

      AudioParameters(AudioParameters::AUDIO_PCM_LOW_LATENCY,
                      CHANNEL_LAYOUT_MONO, 1000, 100),
      AudioParameters(AudioParameters::AUDIO_PCM_LOW_LATENCY,
                      CHANNEL_LAYOUT_MONO, 1000, 200),
      AudioParameters(AudioParameters::AUDIO_PCM_LOW_LATENCY,
                      CHANNEL_LAYOUT_MONO, 2000, 100),
      AudioParameters(AudioParameters::AUDIO_PCM_LOW_LATENCY,
                      CHANNEL_LAYOUT_MONO, 2000, 200),

      AudioParameters(AudioParameters::AUDIO_PCM_LOW_LATENCY,
                      CHANNEL_LAYOUT_STEREO, 1000, 100),
      AudioParameters(AudioParameters::AUDIO_PCM_LOW_LATENCY,
                      CHANNEL_LAYOUT_STEREO, 1000, 200),
      AudioParameters(AudioParameters::AUDIO_PCM_LOW_LATENCY,
                      CHANNEL_LAYOUT_STEREO, 2000, 100),
      AudioParameters(AudioParameters::AUDIO_PCM_LOW_LATENCY,
                      CHANNEL_LAYOUT_STEREO, 2000, 200),
  };

  for (size_t i = 0; i < std::size(values); ++i) {
    for (size_t j = 0; j < std::size(values); ++j) {
      SCOPED_TRACE("i=" + base::NumberToString(i) +
                   " j=" + base::NumberToString(j));
      EXPECT_EQ(i < j, values[i] < values[j]);
    }

    // Verify that a value is never less than itself.
    EXPECT_FALSE(values[i] < values[i]);
  }
}

TEST(AudioParameters, Constructor_ValidChannelCounts) {
  int expected_channels = 8;
  ChannelLayout expected_layout = CHANNEL_LAYOUT_DISCRETE;

  AudioParameters params(AudioParameters::AUDIO_PCM_LOW_LATENCY,
                         expected_layout, 44100, 880);
  params.set_channels_for_discrete(expected_channels);
  EXPECT_EQ(expected_channels, params.channels());
  EXPECT_EQ(expected_layout, params.channel_layout());
  EXPECT_TRUE(params.IsValid());
}

}  // namespace media
