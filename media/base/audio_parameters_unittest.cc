// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/base/audio_parameters.h"

#include <stddef.h>

#include "base/strings/string_number_conversions.h"
#include "media/base/channel_layout.h"
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
  constexpr ChannelLayout expected_channel_layout = CHANNEL_LAYOUT_5_1;
  int expected_rate = 44100;
  int expected_samples = 880;

  AudioParameters params(
      expected_format,
      ChannelLayoutConfig::FromLayout<expected_channel_layout>(), expected_rate,
      expected_samples);

  EXPECT_EQ(expected_format, params.format());
  EXPECT_EQ(expected_channels, params.channels());
  EXPECT_EQ(expected_channel_layout, params.channel_layout());
  EXPECT_EQ(expected_rate, params.sample_rate());
  EXPECT_EQ(expected_samples, params.frames_per_buffer());
  EXPECT_FALSE(params.RequireEncapsulation());
}

TEST(AudioParameters, Constructor_ParameterValuesPlusHardwareCapabilities) {
  AudioParameters::Format expected_format =
      AudioParameters::AUDIO_PCM_LOW_LATENCY;
  int expected_channels = 6;
  constexpr ChannelLayout expected_channel_layout = CHANNEL_LAYOUT_5_1;
  int expected_rate = 44100;
  int expected_samples = 880;

  AudioParameters::HardwareCapabilities hardware_capabilities(0, true);
  hardware_capabilities.require_audio_offload = true;
  AudioParameters params(
      expected_format,
      ChannelLayoutConfig::FromLayout<expected_channel_layout>(), expected_rate,
      expected_samples, hardware_capabilities);

  EXPECT_EQ(expected_format, params.format());
  EXPECT_EQ(expected_channels, params.channels());
  EXPECT_EQ(expected_channel_layout, params.channel_layout());
  EXPECT_EQ(expected_rate, params.sample_rate());
  EXPECT_EQ(expected_samples, params.frames_per_buffer());
  EXPECT_TRUE(params.RequireEncapsulation());
  EXPECT_TRUE(params.RequireOffload());
}

TEST(AudioParameters, GetBytesPerBuffer) {
  EXPECT_EQ(100, AudioParameters(AudioParameters::AUDIO_PCM_LINEAR,
                                 ChannelLayoutConfig::Mono(), 1000, 100)
                     .GetBytesPerBuffer(kSampleFormatU8));
  EXPECT_EQ(200, AudioParameters(AudioParameters::AUDIO_PCM_LINEAR,
                                 ChannelLayoutConfig::Mono(), 1000, 100)
                     .GetBytesPerBuffer(kSampleFormatS16));
  EXPECT_EQ(200, AudioParameters(AudioParameters::AUDIO_PCM_LINEAR,
                                 ChannelLayoutConfig::Stereo(), 1000, 100)
                     .GetBytesPerBuffer(kSampleFormatU8));
  EXPECT_EQ(200, AudioParameters(AudioParameters::AUDIO_PCM_LINEAR,
                                 ChannelLayoutConfig::Mono(), 1000, 200)
                     .GetBytesPerBuffer(kSampleFormatU8));
  EXPECT_EQ(800, AudioParameters(AudioParameters::AUDIO_PCM_LINEAR,
                                 ChannelLayoutConfig::Stereo(), 1000, 200)
                     .GetBytesPerBuffer(kSampleFormatS16));
}

TEST(AudioParameters, Compare) {
  AudioParameters values[] = {
      AudioParameters(AudioParameters::AUDIO_PCM_LINEAR,
                      ChannelLayoutConfig::Mono(), 1000, 100),
      AudioParameters(AudioParameters::AUDIO_PCM_LINEAR,
                      ChannelLayoutConfig::Mono(), 1000, 200),
      AudioParameters(AudioParameters::AUDIO_PCM_LINEAR,
                      ChannelLayoutConfig::Mono(), 2000, 100),
      AudioParameters(AudioParameters::AUDIO_PCM_LINEAR,
                      ChannelLayoutConfig::Mono(), 2000, 200),

      AudioParameters(AudioParameters::AUDIO_PCM_LINEAR,
                      ChannelLayoutConfig::Stereo(), 1000, 100),
      AudioParameters(AudioParameters::AUDIO_PCM_LINEAR,
                      ChannelLayoutConfig::Stereo(), 1000, 200),
      AudioParameters(AudioParameters::AUDIO_PCM_LINEAR,
                      ChannelLayoutConfig::Stereo(), 2000, 100),
      AudioParameters(AudioParameters::AUDIO_PCM_LINEAR,
                      ChannelLayoutConfig::Stereo(), 2000, 200),

      AudioParameters(AudioParameters::AUDIO_PCM_LOW_LATENCY,
                      ChannelLayoutConfig::Mono(), 1000, 100),
      AudioParameters(AudioParameters::AUDIO_PCM_LOW_LATENCY,
                      ChannelLayoutConfig::Mono(), 1000, 200),
      AudioParameters(AudioParameters::AUDIO_PCM_LOW_LATENCY,
                      ChannelLayoutConfig::Mono(), 2000, 100),
      AudioParameters(AudioParameters::AUDIO_PCM_LOW_LATENCY,
                      ChannelLayoutConfig::Mono(), 2000, 200),

      AudioParameters(AudioParameters::AUDIO_PCM_LOW_LATENCY,
                      ChannelLayoutConfig::Stereo(), 1000, 100),
      AudioParameters(AudioParameters::AUDIO_PCM_LOW_LATENCY,
                      ChannelLayoutConfig::Stereo(), 1000, 200),
      AudioParameters(AudioParameters::AUDIO_PCM_LOW_LATENCY,
                      ChannelLayoutConfig::Stereo(), 2000, 100),
      AudioParameters(AudioParameters::AUDIO_PCM_LOW_LATENCY,
                      ChannelLayoutConfig::Stereo(), 2000, 200),
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
  ChannelLayoutConfig channel_layout_config(CHANNEL_LAYOUT_DISCRETE,
                                            expected_channels);

  AudioParameters params(AudioParameters::AUDIO_PCM_LOW_LATENCY,
                         channel_layout_config, 44100, 880);
  EXPECT_EQ(expected_channels, params.channels());
  EXPECT_EQ(expected_layout, params.channel_layout());
  EXPECT_TRUE(params.IsValid());
}

TEST(AudioParameters, Constructor_ValidChannelCountsFor514Downmix) {
  int expected_channels = 7;
  constexpr ChannelLayout expected_layout = CHANNEL_LAYOUT_5_1_4_DOWNMIX;
  ChannelLayoutConfig channel_layout_config(expected_layout, expected_channels);

  AudioParameters params(AudioParameters::AUDIO_PCM_LOW_LATENCY,
                         channel_layout_config, 44100, 880);
  EXPECT_EQ(expected_channels, params.channels());
  EXPECT_EQ(expected_layout, params.channel_layout());
  EXPECT_TRUE(params.IsValid());

  // We do not have to explicitly set the channels for this layout.
  params.Reset(AudioParameters::AUDIO_PCM_LOW_LATENCY,
               ChannelLayoutConfig::FromLayout<expected_layout>(), 44100, 880);
  EXPECT_EQ(6, params.channels());
  EXPECT_EQ(expected_layout, params.channel_layout());
  EXPECT_TRUE(params.IsValid());
}

TEST(AudioParameters, Constructor_CopyChannelLayoutConfig) {
  int expected_channels = 8;
  ChannelLayout expected_layout = CHANNEL_LAYOUT_DISCRETE;
  ChannelLayoutConfig channel_layout_config(CHANNEL_LAYOUT_DISCRETE,
                                            expected_channels);

  AudioParameters params1(AudioParameters::AUDIO_PCM_LOW_LATENCY,
                          channel_layout_config, 44100, 880);
  AudioParameters params2(AudioParameters::AUDIO_PCM_LOW_LATENCY,
                          params1.channel_layout_config(), 44100, 880);

  EXPECT_EQ(expected_channels, params2.channels());
  EXPECT_EQ(expected_layout, params2.channel_layout());
  EXPECT_TRUE(params2.IsValid());
}

TEST(AudioParameters, ShouldCheckDiscreteWithNoChannels) {
  ASSERT_DEATH_IF_SUPPORTED(
      {
        ChannelLayoutConfig channel_layout_config(CHANNEL_LAYOUT_DISCRETE, 0);
      },
      "");
}

TEST(AudioParameters, ChannelLayoutConfig_Guess) {
  ChannelLayoutConfig channel_layout_config = ChannelLayoutConfig::Guess(2);
  EXPECT_EQ(CHANNEL_LAYOUT_STEREO, channel_layout_config.channel_layout());
  EXPECT_EQ(2, channel_layout_config.channels());
}

TEST(AudioParameters, ChannelLayoutConfig_GuessUnsupported) {
  ChannelLayoutConfig channel_layout_config = ChannelLayoutConfig::Guess(100);
  EXPECT_EQ(CHANNEL_LAYOUT_UNSUPPORTED, channel_layout_config.channel_layout());
  EXPECT_EQ(0, channel_layout_config.channels());
}

}  // namespace media
