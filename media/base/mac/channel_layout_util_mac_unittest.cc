// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/mac/channel_layout_util_mac.h"

#include <cstddef>
#include <iterator>
#include <memory>
#include <utility>
#include <vector>

#include "base/compiler_specific.h"
#include "base/numerics/safe_conversions.h"
#include "media/base/channel_layout.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

TEST(ChannelLayoutUtilMac, BidirectionalMappings) {
  struct ChannelAndLabel {
    Channels channel;
    AudioChannelLabel label;
  };
  static constexpr auto kChannelLabelMap = std::to_array<ChannelAndLabel>({
      {Channels::LEFT, kAudioChannelLabel_Left},
      {Channels::RIGHT, kAudioChannelLabel_Right},
      {Channels::CENTER, kAudioChannelLabel_Center},
      {Channels::LFE, kAudioChannelLabel_LFEScreen},
      {Channels::BACK_LEFT, kAudioChannelLabel_RearSurroundLeft},
      {Channels::BACK_RIGHT, kAudioChannelLabel_RearSurroundRight},
      {Channels::LEFT_OF_CENTER, kAudioChannelLabel_LeftCenter},
      {Channels::RIGHT_OF_CENTER, kAudioChannelLabel_RightCenter},
      {Channels::BACK_CENTER, kAudioChannelLabel_CenterSurround},
      {Channels::SIDE_LEFT, kAudioChannelLabel_LeftSurround},
      {Channels::SIDE_RIGHT, kAudioChannelLabel_RightSurround},
      {Channels::TOP_CENTER, kAudioChannelLabel_TopCenterSurround},
      {Channels::TOP_FRONT_LEFT, kAudioChannelLabel_LeftTopFront},
      {Channels::TOP_FRONT_CENTER, kAudioChannelLabel_CenterTopFront},
      {Channels::TOP_FRONT_RIGHT, kAudioChannelLabel_RightTopFront},
      {Channels::TOP_BACK_LEFT, kAudioChannelLabel_LeftTopRear},
      {Channels::TOP_BACK_CENTER, kAudioChannelLabel_TopBackCenter},
      {Channels::TOP_BACK_RIGHT, kAudioChannelLabel_RightTopRear},
  });
  static_assert(kChannelLabelMap.size() == CHANNELS_MAX + 1,
                "kChannelLabelMap is likely missing a new channel");

  for (const auto& mapping : kChannelLabelMap) {
    // Test: Channel -> Label
    EXPECT_EQ(ChannelToAudioChannelLabel(mapping.channel), mapping.label)
        << "Failed Channel -> Label mapping for channel: "
        << static_cast<int>(mapping.channel);

    // Test: Label -> Channel
    EXPECT_EQ(AudioChannelLabelToChannel(mapping.label).value(),
              mapping.channel)
        << "Failed Label -> Channel mapping for label: "
        << static_cast<int>(mapping.label);
  }
}

TEST(ChannelLayoutUtilMac, MonoChannelToCenterMapping) {
  // Test the N:1 Mono -> Center mapping
  EXPECT_EQ(AudioChannelLabelToChannel(kAudioChannelLabel_Mono),
            std::make_optional(Channels::CENTER));
}

TEST(ChannelLayoutUtilMac, ChannelMappingInvalidLabel) {
  // Test the default fallback for an invalid/unknown AudioChannelLabel.
  AudioChannelLabel invalid_label = static_cast<AudioChannelLabel>(99999);
  EXPECT_EQ(AudioChannelLabelToChannel(invalid_label), std::nullopt);
}

TEST(ChannelLayoutUtilMac, ChannelLayoutMonoToAudioChannelLayout) {
  auto output_layout =
      ChannelLayoutToAudioChannelLayout(ChannelLayoutConfig::Mono());

  EXPECT_GT(output_layout->layout_size(), 0u);
  EXPECT_EQ(output_layout->layout()->mChannelLayoutTag,
            kAudioChannelLayoutTag_UseChannelDescriptions);
  EXPECT_EQ(output_layout->layout()->mNumberChannelDescriptions, 1u);
  const auto descriptions = GetDescriptions(*output_layout->layout());
  EXPECT_EQ(descriptions[0].mChannelLabel, kAudioChannelLabel_Mono);
  EXPECT_EQ(descriptions[0].mChannelFlags, kAudioChannelFlags_AllOff);
}

TEST(ChannelLayoutUtilMac, ChannelLayoutDiscreteToAudioChannelLayout) {
  int channels = 12;
  auto output_layout = ChannelLayoutToAudioChannelLayout(
      ChannelLayoutConfig(CHANNEL_LAYOUT_DISCRETE, channels));

  EXPECT_GT(output_layout->layout_size(), 0u);
  EXPECT_EQ(output_layout->layout()->mChannelLayoutTag,
            kAudioChannelLayoutTag_UseChannelDescriptions);
  EXPECT_EQ(output_layout->layout()->mNumberChannelDescriptions,
            static_cast<UInt32>(channels));

  const auto descriptions = GetDescriptions(*output_layout->layout());
  for (const auto& description : descriptions) {
    EXPECT_EQ(description.mChannelLabel, kAudioChannelLabel_Unknown);
    EXPECT_EQ(description.mChannelFlags, kAudioChannelFlags_AllOff);
  }
}

TEST(ChannelLayoutUtilMac, ChannelLayout7Point1ToAudioChannelLayout) {
  auto output_layout = ChannelLayoutToAudioChannelLayout(
      ChannelLayoutConfig::FromLayout<CHANNEL_LAYOUT_7_1>());

  EXPECT_GT(output_layout->layout_size(), 0u);
  EXPECT_EQ(output_layout->layout()->mChannelLayoutTag,
            kAudioChannelLayoutTag_UseChannelDescriptions);
  EXPECT_EQ(output_layout->layout()->mNumberChannelDescriptions, 8u);

  static constexpr auto kExpectedLabels = std::to_array<AudioChannelLabel>({
      kAudioChannelLabel_Left,
      kAudioChannelLabel_Right,
      kAudioChannelLabel_Center,
      kAudioChannelLabel_LFEScreen,
      kAudioChannelLabel_RearSurroundLeft,
      kAudioChannelLabel_RearSurroundRight,
      kAudioChannelLabel_LeftSurround,
      kAudioChannelLabel_RightSurround,
  });

  const auto descriptions = GetDescriptions(*output_layout->layout());
  for (size_t i = 0; i < kExpectedLabels.size(); ++i) {
    EXPECT_EQ(descriptions[i].mChannelLabel, kExpectedLabels[i]);
    EXPECT_EQ(descriptions[i].mChannelFlags, kAudioChannelFlags_AllOff);
  }
}

TEST(ChannelLayoutUtilMac, AudioChannelLayoutWithDescriptionsToChannelLayout) {
  static constexpr auto labels = std::to_array<AudioChannelLabel>({
      kAudioChannelLabel_Left,
      kAudioChannelLabel_Right,
      kAudioChannelLabel_Center,
      kAudioChannelLabel_LFEScreen,
      kAudioChannelLabel_LeftSurround,
      kAudioChannelLabel_RightSurround,
  });
  const int channels = std::size(labels);
  int layout_size = sizeof(AudioChannelLayout) +
                    (channels - 1) * sizeof(AudioChannelDescription);
  ScopedAudioChannelLayout input_layout(layout_size);

  input_layout.layout()->mNumberChannelDescriptions = channels;
  input_layout.layout()->mChannelLayoutTag =
      kAudioChannelLayoutTag_UseChannelDescriptions;

  auto descriptions = GetDescriptions(*input_layout.layout());
  for (int i = 0; i < channels; ++i) {
    descriptions[i].mChannelLabel = labels[i];
    descriptions[i].mChannelFlags = kAudioChannelFlags_AllOff;
  }

  ChannelLayout output_layout;
  EXPECT_TRUE(AudioChannelLayoutToChannelLayout(*input_layout.layout(),
                                                &output_layout));
  EXPECT_EQ(output_layout, ChannelLayout::CHANNEL_LAYOUT_5_1);
}

TEST(ChannelLayoutUtilMac, AudioChannelLayoutWithBitmapToChannelLayout) {
  int channels = 6;
  int layout_size = offsetof(AudioChannelLayout, mChannelDescriptions[0]);
  ScopedAudioChannelLayout input_layout(layout_size);

  input_layout.layout()->mNumberChannelDescriptions =
      static_cast<UInt32>(channels);
  input_layout.layout()->mChannelLayoutTag =
      kAudioChannelLayoutTag_UseChannelBitmap;
  input_layout.layout()->mChannelBitmap =
      kAudioChannelBit_Left | kAudioChannelBit_Right | kAudioChannelBit_Center |
      kAudioChannelBit_LFEScreen | kAudioChannelBit_LeftSurround |
      kAudioChannelBit_RightSurround;

  ChannelLayout output_layout;
  EXPECT_TRUE(AudioChannelLayoutToChannelLayout(*input_layout.layout(),
                                                &output_layout));
  EXPECT_EQ(output_layout, CHANNEL_LAYOUT_5_1);
}

namespace {

ChannelLayout GetChannelLayoutFromTag(AudioChannelLayoutTag tag) {
  int layout_size = offsetof(AudioChannelLayout, mChannelDescriptions[0]);
  ScopedAudioChannelLayout input_layout(layout_size);
  input_layout.layout()->mNumberChannelDescriptions = 0;
  input_layout.layout()->mChannelLayoutTag = tag;

  ChannelLayout output_layout;
  EXPECT_TRUE(AudioChannelLayoutToChannelLayout(*input_layout.layout(),
                                                &output_layout));
  return output_layout;
}

// Builds a layout which describes one channel per entry in `labels`.
std::unique_ptr<ScopedAudioChannelLayout> MakeChannelLayout(
    const std::vector<AudioChannelLabel>& labels) {
  const size_t layout_size =
      offsetof(AudioChannelLayout, mChannelDescriptions[0]) +
      labels.size() * sizeof(AudioChannelDescription);
  auto scoped_layout = std::make_unique<ScopedAudioChannelLayout>(layout_size);

  AudioChannelLayout* layout = scoped_layout->layout();
  layout->mChannelLayoutTag = kAudioChannelLayoutTag_UseChannelDescriptions;
  layout->mChannelBitmap = 0;
  layout->mNumberChannelDescriptions =
      base::checked_cast<UInt32>(labels.size());

  auto descriptions = GetDescriptions(*layout);
  for (size_t i = 0; i < labels.size(); ++i) {
    descriptions[i] = AudioChannelDescription{
        .mChannelLabel = labels[i], .mChannelFlags = kAudioChannelFlags_AllOff};
  }
  return scoped_layout;
}

}  // namespace

TEST(ChannelLayoutUtilMac, AudioChannelLayoutTagsToChannelLayout) {
  EXPECT_EQ(GetChannelLayoutFromTag(kAudioChannelLayoutTag_Mono),
            ChannelLayout::CHANNEL_LAYOUT_MONO);
  EXPECT_EQ(GetChannelLayoutFromTag(kAudioChannelLayoutTag_Stereo),
            ChannelLayout::CHANNEL_LAYOUT_STEREO);
  EXPECT_EQ(GetChannelLayoutFromTag(kAudioChannelLayoutTag_Quadraphonic),
            ChannelLayout::CHANNEL_LAYOUT_2_2);
  EXPECT_EQ(GetChannelLayoutFromTag(kAudioChannelLayoutTag_Logic_6_1_C),
            ChannelLayout::CHANNEL_LAYOUT_6_1);
  EXPECT_EQ(GetChannelLayoutFromTag(kAudioChannelLayoutTag_AAC_5_1),
            ChannelLayout::CHANNEL_LAYOUT_5_1);
  EXPECT_EQ(GetChannelLayoutFromTag(kAudioChannelLayoutTag_AAC_7_1),
            ChannelLayout::CHANNEL_LAYOUT_7_1_WIDE);
  EXPECT_EQ(GetChannelLayoutFromTag(kAudioChannelLayoutTag_EAC3_7_1_A),
            ChannelLayout::CHANNEL_LAYOUT_7_1);
}

TEST(ChannelLayoutUtilMac,
     AudioChannelLayoutWithEAC37Point1DTagToChannelLayout) {
  int layout_size = offsetof(AudioChannelLayout, mChannelDescriptions[0]);
  ScopedAudioChannelLayout input_layout(layout_size);

  input_layout.layout()->mNumberChannelDescriptions = 0;
  // L C R Ls Rs LFE Lw Rw.
  input_layout.layout()->mChannelLayoutTag = kAudioChannelLayoutTag_EAC3_7_1_D;

  ChannelLayout output_layout;
  // Lw, Rw is not supported by Chrome for now thus the conversion fails.
  EXPECT_FALSE(AudioChannelLayoutToChannelLayout(*input_layout.layout(),
                                                 &output_layout));
}

TEST(ChannelLayoutUtilMac, ChannelLayoutConvertBackToChannelLayout) {
  for (int i = 0; i <= CHANNEL_LAYOUT_MAX; i++) {
    ChannelLayout input_layout = static_cast<ChannelLayout>(i);
    // TODO(crbug.com/474106765): 5.1.4 and 7.1.4 are not supported yet. Once
    // `kMaxConcurrentChannels` is upgraded to 12, then we can include these
    // test cases.
    if (input_layout == CHANNEL_LAYOUT_5_1_4 ||
        input_layout == CHANNEL_LAYOUT_7_1_4) {
      continue;
    }

    // Skip invalid channel layout.
    int input_channels = ChannelLayoutToChannelCount(input_layout);
    if (input_channels == 0) {
      continue;
    }
    // CHANNEL_LAYOUT_STEREO_DOWNMIX is the alias of CHANNEL_LAYOUT_STEREO,
    // CHANNEL_LAYOUT_STEREO_AND_KEYBOARD_MIC is the alias of
    // CHANNEL_LAYOUT_SURROUND, and CHANNEL_LAYOUT_5_1_4_DOWNMIX is the alias
    // of CHANNEL_LAYOUT_5_1.
    if (input_layout == CHANNEL_LAYOUT_STEREO_DOWNMIX ||
        input_layout == CHANNEL_LAYOUT_STEREO_AND_KEYBOARD_MIC ||
        input_layout == CHANNEL_LAYOUT_5_1_4_DOWNMIX) {
      continue;
    }
    auto intermediate_layout = ChannelLayoutToAudioChannelLayout(
        ChannelLayoutConfig::FromLayout(input_layout));
    EXPECT_NE(intermediate_layout, nullptr);
    EXPECT_GT(intermediate_layout->layout_size(), 0u);
    ChannelLayout output_layout;
    EXPECT_TRUE(AudioChannelLayoutToChannelLayout(
        *intermediate_layout->layout(), &output_layout));
    EXPECT_EQ(input_layout, output_layout);
  }
}

TEST(ChannelLayoutUtilMac, ParseCoreAudioChannelLayoutMono) {
  auto input_layout = MakeChannelLayout({kAudioChannelLabel_Mono});

  uint32_t channels = 0;
  ChannelLayout output_layout = CHANNEL_LAYOUT_NONE;
  ParseCoreAudioChannelLayout(*input_layout->layout(), &channels,
                              &output_layout);

  EXPECT_EQ(channels, 1u);
  EXPECT_EQ(output_layout, CHANNEL_LAYOUT_MONO);
}

TEST(ChannelLayoutUtilMac, ParseCoreAudioChannelLayoutStereo) {
  auto input_layout =
      MakeChannelLayout({kAudioChannelLabel_Left, kAudioChannelLabel_Right});

  uint32_t channels = 0;
  ChannelLayout output_layout = CHANNEL_LAYOUT_NONE;
  ParseCoreAudioChannelLayout(*input_layout->layout(), &channels,
                              &output_layout);

  EXPECT_EQ(channels, 2u);
  EXPECT_EQ(output_layout, CHANNEL_LAYOUT_STEREO);
}

TEST(ChannelLayoutUtilMac, ParseCoreAudioChannelLayoutTwoUnknownLabels) {
  auto input_layout = MakeChannelLayout({
      kAudioChannelLabel_Unknown,
      kAudioChannelLabel_Unknown,
  });

  uint32_t channels = 0;
  ChannelLayout output_layout = CHANNEL_LAYOUT_NONE;
  ParseCoreAudioChannelLayout(*input_layout->layout(), &channels,
                              &output_layout);

  EXPECT_EQ(channels, 2u);
  EXPECT_EQ(output_layout, CHANNEL_LAYOUT_STEREO);
}

TEST(ChannelLayoutUtilMac, ParseCoreAudioChannelLayoutNamedLayout) {
  auto input_layout = MakeChannelLayout({
      kAudioChannelLabel_Left,
      kAudioChannelLabel_Right,
      kAudioChannelLabel_Center,
      kAudioChannelLabel_LFEScreen,
      kAudioChannelLabel_LeftSurround,
      kAudioChannelLabel_RightSurround,
  });

  uint32_t channels = 0;
  ChannelLayout output_layout = CHANNEL_LAYOUT_NONE;
  ParseCoreAudioChannelLayout(*input_layout->layout(), &channels,
                              &output_layout);

  EXPECT_EQ(channels, 6u);
  EXPECT_EQ(output_layout, CHANNEL_LAYOUT_5_1);
}

TEST(ChannelLayoutUtilMac, ParseCoreAudioChannelLayoutDuplicateLabels) {
  auto input_layout = MakeChannelLayout({
      kAudioChannelLabel_Left,
      kAudioChannelLabel_Left,
      kAudioChannelLabel_Right,
      kAudioChannelLabel_Right,
      kAudioChannelLabel_Center,
      kAudioChannelLabel_LFEScreen,
  });

  uint32_t channels = 0;
  ChannelLayout output_layout = CHANNEL_LAYOUT_NONE;
  ParseCoreAudioChannelLayout(*input_layout->layout(), &channels,
                              &output_layout);

  EXPECT_EQ(channels, 6u);
  EXPECT_EQ(output_layout, CHANNEL_LAYOUT_DISCRETE);
}

// Regression test for https://crbug.com/561637136.
TEST(ChannelLayoutUtilMac, ParseCoreAudioChannelLayoutAllUnknownLabels) {
  auto input_layout = MakeChannelLayout({
      kAudioChannelLabel_Unknown,
      kAudioChannelLabel_Unknown,
      kAudioChannelLabel_Unknown,
      kAudioChannelLabel_Unknown,
      kAudioChannelLabel_Unknown,
      kAudioChannelLabel_Unknown,
  });

  uint32_t channels = 0;
  ChannelLayout output_layout = CHANNEL_LAYOUT_NONE;
  ParseCoreAudioChannelLayout(*input_layout->layout(), &channels,
                              &output_layout);

  EXPECT_EQ(channels, 6u);
  EXPECT_EQ(output_layout, CHANNEL_LAYOUT_DISCRETE);
}

TEST(ChannelLayoutUtilMac, ParseCoreAudioChannelLayoutSomeUnknownLabels) {
  auto input_layout = MakeChannelLayout({
      kAudioChannelLabel_Left,
      kAudioChannelLabel_Right,
      kAudioChannelLabel_Center,
      kAudioChannelLabel_LFEScreen,
      kAudioChannelLabel_Unknown,
      kAudioChannelLabel_Unknown,
  });

  uint32_t channels = 0;
  ChannelLayout output_layout = CHANNEL_LAYOUT_NONE;
  ParseCoreAudioChannelLayout(*input_layout->layout(), &channels,
                              &output_layout);

  EXPECT_EQ(channels, 6u);
  EXPECT_EQ(output_layout, CHANNEL_LAYOUT_DISCRETE);
}

TEST(ChannelLayoutUtilMac, ParseCoreAudioChannelLayoutWithoutDescriptions) {
  auto input_layout = MakeChannelLayout({});

  uint32_t channels = 1;
  ChannelLayout output_layout = CHANNEL_LAYOUT_DISCRETE;
  ParseCoreAudioChannelLayout(*input_layout->layout(), &channels,
                              &output_layout);

  EXPECT_EQ(channels, 0u);
  EXPECT_EQ(output_layout, CHANNEL_LAYOUT_NONE);
}

TEST(ChannelLayoutUtilMac, ParseCoreAudioChannelLayoutUnmappableLabel) {
  auto input_layout = MakeChannelLayout({
      kAudioChannelLabel_Left,
      kAudioChannelLabel_Right,
      kAudioChannelLabel_Center,
      kAudioChannelLabel_LFEScreen,
      kAudioChannelLabel_LeftSurround,
      kAudioChannelLabel_Unused,
  });

  uint32_t channels = 0;
  ChannelLayout output_layout = CHANNEL_LAYOUT_NONE;
  ParseCoreAudioChannelLayout(*input_layout->layout(), &channels,
                              &output_layout);

  EXPECT_EQ(channels, 6u);
  EXPECT_EQ(output_layout, CHANNEL_LAYOUT_DISCRETE);
}

TEST(ChannelLayoutUtilMac, ParseCoreAudioChannelLayoutUnmatchedSpeakerRoles) {
  auto input_layout = MakeChannelLayout({
      kAudioChannelLabel_Left,
      kAudioChannelLabel_Right,
      kAudioChannelLabel_TopCenterSurround,
  });

  uint32_t channels = 0;
  ChannelLayout output_layout = CHANNEL_LAYOUT_NONE;
  ParseCoreAudioChannelLayout(*input_layout->layout(), &channels,
                              &output_layout);

  EXPECT_EQ(channels, 3u);
  EXPECT_EQ(output_layout, CHANNEL_LAYOUT_DISCRETE);
}

}  // namespace media
