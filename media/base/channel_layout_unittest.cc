// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/channel_layout.h"

#include "media/base/limits.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

TEST(ChannelLayoutTest, ChannelMaskToLayout_StandardLayouts) {
  // Test standard layouts.
  EXPECT_EQ(CHANNEL_LAYOUT_MONO, ChannelMaskToLayout(1 << CENTER));
  EXPECT_EQ(CHANNEL_LAYOUT_STEREO,
            ChannelMaskToLayout((1 << LEFT) | (1 << RIGHT)));
  EXPECT_EQ(
      CHANNEL_LAYOUT_5_1,
      ChannelMaskToLayout((1 << LEFT) | (1 << RIGHT) | (1 << CENTER) |
                          (1 << LFE) | (1 << SIDE_LEFT) | (1 << SIDE_RIGHT)));
}

TEST(ChannelLayoutTest, ChannelMaskToLayout_DuplicateLayouts) {
  // Layouts with a lower index should win.

  // CHANNEL_LAYOUT_STEREO (3) vs CHANNEL_LAYOUT_STEREO_DOWNMIX (16)
  EXPECT_EQ(CHANNEL_LAYOUT_STEREO,
            ChannelMaskToLayout((1 << LEFT) | (1 << RIGHT)));

  // CHANNEL_LAYOUT_5_1 (10) vs CHANNEL_LAYOUT_5_1_4_DOWNMIX (33)
  EXPECT_EQ(CHANNEL_LAYOUT_5_1,
            ChannelMaskToLayout(1 << LEFT | 1 << RIGHT | 1 << CENTER |
                                1 << LFE | 1 << SIDE_LEFT | 1 << SIDE_RIGHT));
  // CHANNEL_LAYOUT_SURROUND (5) vs CHANNEL_LAYOUT_STEREO_AND_KEYBOARD_MIC (30)
  EXPECT_EQ(CHANNEL_LAYOUT_SURROUND,
            ChannelMaskToLayout((1 << LEFT) | (1 << RIGHT) | (1 << CENTER)));
}

TEST(ChannelLayoutTest, ChannelMaskToLayout_NonstandardLayouts) {
  EXPECT_EQ(CHANNEL_LAYOUT_DISCRETE, ChannelMaskToLayout(1UL << 31));
  EXPECT_EQ(CHANNEL_LAYOUT_NONE, ChannelMaskToLayout(0));
}

TEST(ChannelLayoutTest, ChannelMaskToLayout_UnknownChannelsReturnDiscrete) {
  constexpr uint32_t kUnknownSpeaker = 31;
  // Ensure that the speaker type is not currently supported.
  static_assert(kUnknownSpeaker > CHANNELS_MAX);

  EXPECT_EQ(
      CHANNEL_LAYOUT_DISCRETE,
      ChannelMaskToLayout((1 << LEFT) | (1 << RIGHT) | (1 << kUnknownSpeaker)));

  EXPECT_EQ(CHANNEL_LAYOUT_DISCRETE,
            ChannelMaskToLayout((1 << LEFT) | (1 << RIGHT) | (1 << CENTER) |
                                (1 << LFE) | (1 << SIDE_LEFT) |
                                (1 << SIDE_RIGHT) | 1 << kUnknownSpeaker));
}

// Ensure the special case monst common constexpr value are valid
TEST(ChannelLayoutTest, ChannelLayoutConfig_constexpr_constructors) {
  constexpr ChannelLayoutConfig empty = ChannelLayoutConfig();
  EXPECT_EQ(CHANNEL_LAYOUT_NONE, empty.channel_layout());
  EXPECT_EQ(0, empty.channels());

  constexpr ChannelLayoutConfig mono = ChannelLayoutConfig::Mono();
  EXPECT_EQ(CHANNEL_LAYOUT_MONO, mono.channel_layout());
  EXPECT_EQ(1, mono.channels());

  constexpr ChannelLayoutConfig stereo = ChannelLayoutConfig::Stereo();
  EXPECT_EQ(CHANNEL_LAYOUT_STEREO, stereo.channel_layout());
  EXPECT_EQ(2, stereo.channels());
}

TEST(ChannelLayoutTest, ChannelLayoutConfig_Guess) {
  EXPECT_EQ(CHANNEL_LAYOUT_UNSUPPORTED,
            ChannelLayoutConfig::Guess(0).channel_layout());

  EXPECT_EQ(ChannelLayoutConfig::Mono(), ChannelLayoutConfig::Guess(1));

  EXPECT_EQ(ChannelLayoutConfig::Stereo(), ChannelLayoutConfig::Guess(2));

  auto six_channels = ChannelLayoutConfig::Guess(6);
  EXPECT_EQ(CHANNEL_LAYOUT_5_1, six_channels.channel_layout());
  EXPECT_EQ(6, six_channels.channels());

  constexpr int kLargeChannelCount = kMaxConcurrentChannels + 1;
  auto large_layout = ChannelLayoutConfig::Guess(kLargeChannelCount);
  EXPECT_EQ(CHANNEL_LAYOUT_DISCRETE, large_layout.channel_layout());
  EXPECT_EQ(kLargeChannelCount, large_layout.channels());

  auto max_layout = ChannelLayoutConfig::Guess(limits::kMaxChannels);
  EXPECT_EQ(CHANNEL_LAYOUT_DISCRETE, max_layout.channel_layout());
  EXPECT_EQ(limits::kMaxChannels, max_layout.channels());

  constexpr int kHugeChannelCount = limits::kMaxChannels + 1;
  auto huge_layout = ChannelLayoutConfig::Guess(kHugeChannelCount);
  EXPECT_EQ(CHANNEL_LAYOUT_UNSUPPORTED, huge_layout.channel_layout());
  EXPECT_EQ(0, huge_layout.channels());

  auto invalid_layout = ChannelLayoutConfig::Guess(-1);
  EXPECT_EQ(CHANNEL_LAYOUT_UNSUPPORTED, invalid_layout.channel_layout());
  EXPECT_EQ(0, invalid_layout.channels());
}

TEST(ChannelLayoutTest, ChannelLayoutConfig_basic_constructor) {
  EXPECT_EQ(ChannelLayoutConfig(), ChannelLayoutConfig(CHANNEL_LAYOUT_NONE, 0));

  EXPECT_EQ(ChannelLayoutConfig::Mono(),
            ChannelLayoutConfig(CHANNEL_LAYOUT_MONO, 1));

  EXPECT_EQ(ChannelLayoutConfig::Stereo(),
            ChannelLayoutConfig(CHANNEL_LAYOUT_STEREO, 2));

  auto wide_layout = ChannelLayoutConfig(CHANNEL_LAYOUT_7_1_WIDE, 8);
  EXPECT_EQ(CHANNEL_LAYOUT_7_1_WIDE, wide_layout.channel_layout());
  EXPECT_EQ(8, wide_layout.channels());

  auto discrete_one = ChannelLayoutConfig(CHANNEL_LAYOUT_DISCRETE, 1);
  EXPECT_EQ(1, discrete_one.channels());

  auto discrete_fifteen = ChannelLayoutConfig(CHANNEL_LAYOUT_DISCRETE, 15);
  EXPECT_EQ(15, discrete_fifteen.channels());

  auto discrete_max =
      ChannelLayoutConfig(CHANNEL_LAYOUT_DISCRETE, limits::kMaxChannels);
  EXPECT_EQ(limits::kMaxChannels, discrete_max.channels());

  auto bitstream_layout = ChannelLayoutConfig(CHANNEL_LAYOUT_BITSTREAM, 0);
  EXPECT_EQ(bitstream_layout.channel_layout(), CHANNEL_LAYOUT_BITSTREAM);
  EXPECT_EQ(0, bitstream_layout.channels());

  auto downmix_layout = ChannelLayoutConfig(CHANNEL_LAYOUT_5_1_4_DOWNMIX, 6);
  EXPECT_EQ(downmix_layout.channel_layout(), CHANNEL_LAYOUT_5_1_4_DOWNMIX);
  EXPECT_EQ(6, downmix_layout.channels());
}

TEST(ChannelLayoutTest, ChannelLayoutConfig_FromLayout) {
  constexpr auto mono = ChannelLayoutConfig::FromLayout<CHANNEL_LAYOUT_MONO>();
  EXPECT_EQ(ChannelLayoutConfig::Mono(), mono);

  constexpr auto stereo =
      ChannelLayoutConfig::FromLayout<CHANNEL_LAYOUT_STEREO>();
  EXPECT_EQ(ChannelLayoutConfig::Stereo(), stereo);

  auto quad_layout = ChannelLayoutConfig::FromLayout<CHANNEL_LAYOUT_QUAD>();
  EXPECT_EQ(CHANNEL_LAYOUT_QUAD, quad_layout.channel_layout());
  EXPECT_EQ(4, quad_layout.channels());

  auto none_layout = ChannelLayoutConfig::FromLayout<CHANNEL_LAYOUT_NONE>();
  EXPECT_EQ(ChannelLayoutConfig(), none_layout);

  auto unsupported_layout =
      ChannelLayoutConfig::FromLayout<CHANNEL_LAYOUT_UNSUPPORTED>();
  EXPECT_EQ(CHANNEL_LAYOUT_UNSUPPORTED, unsupported_layout.channel_layout());
  EXPECT_EQ(0, unsupported_layout.channels());

  auto bitstream_layout =
      ChannelLayoutConfig::FromLayout<CHANNEL_LAYOUT_BITSTREAM>();
  EXPECT_EQ(CHANNEL_LAYOUT_BITSTREAM, bitstream_layout.channel_layout());
  EXPECT_EQ(0, bitstream_layout.channels());

  auto downmix_layout =
      ChannelLayoutConfig::FromLayout<CHANNEL_LAYOUT_5_1_4_DOWNMIX>();
  EXPECT_EQ(CHANNEL_LAYOUT_5_1_4_DOWNMIX, downmix_layout.channel_layout());
  EXPECT_EQ(6, downmix_layout.channels());
}

TEST(ChannelLayoutTest, ChannelLayoutConfig_FromLayoutRuntime) {
  EXPECT_EQ(ChannelLayoutConfig::Mono(),
            ChannelLayoutConfig::FromLayout(CHANNEL_LAYOUT_MONO));

  EXPECT_EQ(ChannelLayoutConfig::Stereo(),
            ChannelLayoutConfig::FromLayout(CHANNEL_LAYOUT_STEREO));

  const auto quad_layout = ChannelLayoutConfig::FromLayout(CHANNEL_LAYOUT_QUAD);
  EXPECT_EQ(CHANNEL_LAYOUT_QUAD, quad_layout.channel_layout());
  EXPECT_EQ(4, quad_layout.channels());

  EXPECT_EQ(ChannelLayoutConfig(),
            ChannelLayoutConfig::FromLayout(CHANNEL_LAYOUT_NONE));

  const auto unsupported_layout =
      ChannelLayoutConfig::FromLayout(CHANNEL_LAYOUT_UNSUPPORTED);
  EXPECT_EQ(CHANNEL_LAYOUT_UNSUPPORTED, unsupported_layout.channel_layout());
  EXPECT_EQ(0, unsupported_layout.channels());

  const auto bitstream_layout =
      ChannelLayoutConfig::FromLayout(CHANNEL_LAYOUT_BITSTREAM);
  EXPECT_EQ(CHANNEL_LAYOUT_BITSTREAM, bitstream_layout.channel_layout());
  EXPECT_EQ(0, bitstream_layout.channels());
}

#if GTEST_HAS_DEATH_TEST

TEST(ChannelLayoutTest, ChannelLayoutConfig_death_tests) {
  EXPECT_DEATH(ChannelLayoutConfig(CHANNEL_LAYOUT_DISCRETE, 0), "");
  EXPECT_DEATH(ChannelLayoutConfig(CHANNEL_LAYOUT_DISCRETE, -1), "");
  EXPECT_DEATH(ChannelLayoutConfig(CHANNEL_LAYOUT_DISCRETE,
                                   media::limits::kMaxChannels + 1),
               "");

  EXPECT_DEATH(ChannelLayoutConfig(CHANNEL_LAYOUT_MONO, 2), "");

  EXPECT_DEATH(ChannelLayoutConfig(CHANNEL_LAYOUT_STEREO, 1), "");

  EXPECT_DEATH(ChannelLayoutConfig(CHANNEL_LAYOUT_BITSTREAM, 1), "");

  EXPECT_DEATH(ChannelLayoutConfig(CHANNEL_LAYOUT_5_1_4_DOWNMIX, 3), "");
}

#endif  // GTEST_HAS_DEATH_TEST

}  // namespace media
