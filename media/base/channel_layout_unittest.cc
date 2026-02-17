// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/channel_layout.h"

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
}  // namespace media
