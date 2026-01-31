// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/base/mac/channel_layout_util_mac.h"

#include <utility>

#include "media/base/channel_layout.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

TEST(ChannelLayoutUtilMac, AudioChannelLabelToChannel) {
  Channels output_channel;
  EXPECT_EQ(
      AudioChannelLabelToChannel(kAudioChannelLabel_Left, &output_channel),
      true);
  EXPECT_EQ(output_channel, Channels::LEFT);
  EXPECT_EQ(
      AudioChannelLabelToChannel(kAudioChannelLabel_Right, &output_channel),
      true);
  EXPECT_EQ(output_channel, Channels::RIGHT);
  EXPECT_EQ(
      AudioChannelLabelToChannel(kAudioChannelLabel_Center, &output_channel),
      true);
  EXPECT_EQ(output_channel, Channels::CENTER);
  EXPECT_EQ(
      AudioChannelLabelToChannel(kAudioChannelLabel_Mono, &output_channel),
      true);
  EXPECT_EQ(output_channel, Channels::CENTER);

  EXPECT_EQ(
      AudioChannelLabelToChannel(kAudioChannelLabel_LFEScreen, &output_channel),
      true);
  EXPECT_EQ(output_channel, Channels::LFE);
  EXPECT_EQ(AudioChannelLabelToChannel(kAudioChannelLabel_RearSurroundLeft,
                                       &output_channel),
            true);
  EXPECT_EQ(output_channel, Channels::BACK_LEFT);
  EXPECT_EQ(AudioChannelLabelToChannel(kAudioChannelLabel_RearSurroundRight,
                                       &output_channel),
            true);
  EXPECT_EQ(output_channel, Channels::BACK_RIGHT);
  EXPECT_EQ(AudioChannelLabelToChannel(kAudioChannelLabel_LeftCenter,
                                       &output_channel),
            true);
  EXPECT_EQ(output_channel, Channels::LEFT_OF_CENTER);
  EXPECT_EQ(AudioChannelLabelToChannel(kAudioChannelLabel_RightCenter,
                                       &output_channel),
            true);
  EXPECT_EQ(output_channel, Channels::RIGHT_OF_CENTER);
  EXPECT_EQ(AudioChannelLabelToChannel(kAudioChannelLabel_CenterSurround,
                                       &output_channel),
            true);
  EXPECT_EQ(output_channel, Channels::BACK_CENTER);
  EXPECT_EQ(AudioChannelLabelToChannel(kAudioChannelLabel_LeftSurround,
                                       &output_channel),
            true);
  EXPECT_EQ(output_channel, Channels::SIDE_LEFT);
  EXPECT_EQ(AudioChannelLabelToChannel(kAudioChannelLabel_RightSurround,
                                       &output_channel),
            true);
  EXPECT_EQ(output_channel, Channels::SIDE_RIGHT);
  EXPECT_EQ(
      AudioChannelLabelToChannel(kAudioChannelLabel_LeftWide, &output_channel),
      false);
}

TEST(ChannelLayoutUtilMac, ChannelToAudioChannelLabel) {
  EXPECT_EQ(ChannelToAudioChannelLabel(Channels::LEFT),
            kAudioChannelLabel_Left);
  EXPECT_EQ(ChannelToAudioChannelLabel(Channels::RIGHT),
            kAudioChannelLabel_Right);
  EXPECT_EQ(ChannelToAudioChannelLabel(Channels::CENTER),
            kAudioChannelLabel_Center);
  EXPECT_EQ(ChannelToAudioChannelLabel(Channels::LFE),
            kAudioChannelLabel_LFEScreen);
  EXPECT_EQ(ChannelToAudioChannelLabel(Channels::BACK_LEFT),
            kAudioChannelLabel_RearSurroundLeft);
  EXPECT_EQ(ChannelToAudioChannelLabel(Channels::BACK_RIGHT),
            kAudioChannelLabel_RearSurroundRight);
  EXPECT_EQ(ChannelToAudioChannelLabel(Channels::LEFT_OF_CENTER),
            kAudioChannelLabel_LeftCenter);
  EXPECT_EQ(ChannelToAudioChannelLabel(Channels::RIGHT_OF_CENTER),
            kAudioChannelLabel_RightCenter);
  EXPECT_EQ(ChannelToAudioChannelLabel(Channels::BACK_CENTER),
            kAudioChannelLabel_CenterSurround);
  EXPECT_EQ(ChannelToAudioChannelLabel(Channels::SIDE_LEFT),
            kAudioChannelLabel_LeftSurround);
  EXPECT_EQ(ChannelToAudioChannelLabel(Channels::SIDE_RIGHT),
            kAudioChannelLabel_RightSurround);
}

TEST(ChannelLayoutUtilMac, ChannelLayoutMonoToAudioChannelLayout) {
  int channels = 1;
  auto output_layout = ChannelLayoutToAudioChannelLayout(
      ChannelLayout::CHANNEL_LAYOUT_MONO, channels);

  EXPECT_GT(output_layout->layout_size(), 0u);
  EXPECT_EQ(output_layout->layout()->mChannelLayoutTag,
            kAudioChannelLayoutTag_UseChannelDescriptions);
  EXPECT_EQ(output_layout->layout()->mNumberChannelDescriptions,
            static_cast<UInt32>(channels));
  EXPECT_EQ(output_layout->layout()->mChannelDescriptions[0].mChannelLabel,
            kAudioChannelLabel_Mono);
  EXPECT_EQ(output_layout->layout()->mChannelDescriptions[0].mChannelFlags,
            kAudioChannelFlags_AllOff);
}

TEST(ChannelLayoutUtilMac, ChannelLayoutDiscreteToAudioChannelLayout) {
  int channels = 12;
  auto output_layout = ChannelLayoutToAudioChannelLayout(
      ChannelLayout::CHANNEL_LAYOUT_DISCRETE, channels);

  EXPECT_GT(output_layout->layout_size(), 0u);
  EXPECT_EQ(output_layout->layout()->mChannelLayoutTag,
            kAudioChannelLayoutTag_UseChannelDescriptions);
  EXPECT_EQ(output_layout->layout()->mNumberChannelDescriptions,
            static_cast<UInt32>(channels));
  for (int i = 0; i < channels; i++) {
    EXPECT_EQ(output_layout->layout()->mChannelDescriptions[i].mChannelLabel,
              kAudioChannelLabel_Unknown);
    EXPECT_EQ(output_layout->layout()->mChannelDescriptions[i].mChannelFlags,
              kAudioChannelFlags_AllOff);
  }
}

TEST(ChannelLayoutUtilMac, ChannelLayout7Point1ToAudioChannelLayout) {
  int channels = 8;
  auto output_layout = ChannelLayoutToAudioChannelLayout(
      ChannelLayout::CHANNEL_LAYOUT_7_1, channels);

  EXPECT_GT(output_layout->layout_size(), 0u);
  EXPECT_EQ(output_layout->layout()->mChannelLayoutTag,
            kAudioChannelLayoutTag_UseChannelDescriptions);
  EXPECT_EQ(output_layout->layout()->mNumberChannelDescriptions,
            static_cast<UInt32>(channels));
  EXPECT_EQ(output_layout->layout()->mChannelDescriptions[0].mChannelLabel,
            kAudioChannelLabel_Left);
  EXPECT_EQ(output_layout->layout()->mChannelDescriptions[0].mChannelFlags,
            kAudioChannelFlags_AllOff);
  EXPECT_EQ(output_layout->layout()->mChannelDescriptions[1].mChannelLabel,
            kAudioChannelLabel_Right);
  EXPECT_EQ(output_layout->layout()->mChannelDescriptions[1].mChannelFlags,
            kAudioChannelFlags_AllOff);
  EXPECT_EQ(output_layout->layout()->mChannelDescriptions[2].mChannelLabel,
            kAudioChannelLabel_Center);
  EXPECT_EQ(output_layout->layout()->mChannelDescriptions[2].mChannelFlags,
            kAudioChannelFlags_AllOff);
  EXPECT_EQ(output_layout->layout()->mChannelDescriptions[3].mChannelLabel,
            kAudioChannelLabel_LFEScreen);
  EXPECT_EQ(output_layout->layout()->mChannelDescriptions[3].mChannelFlags,
            kAudioChannelFlags_AllOff);
  EXPECT_EQ(output_layout->layout()->mChannelDescriptions[4].mChannelLabel,
            kAudioChannelLabel_RearSurroundLeft);
  EXPECT_EQ(output_layout->layout()->mChannelDescriptions[4].mChannelFlags,
            kAudioChannelFlags_AllOff);
  EXPECT_EQ(output_layout->layout()->mChannelDescriptions[5].mChannelLabel,
            kAudioChannelLabel_RearSurroundRight);
  EXPECT_EQ(output_layout->layout()->mChannelDescriptions[5].mChannelFlags,
            kAudioChannelFlags_AllOff);
  EXPECT_EQ(output_layout->layout()->mChannelDescriptions[6].mChannelLabel,
            kAudioChannelLabel_LeftSurround);
  EXPECT_EQ(output_layout->layout()->mChannelDescriptions[6].mChannelFlags,
            kAudioChannelFlags_AllOff);
  EXPECT_EQ(output_layout->layout()->mChannelDescriptions[7].mChannelLabel,
            kAudioChannelLabel_RightSurround);
  EXPECT_EQ(output_layout->layout()->mChannelDescriptions[7].mChannelFlags,
            kAudioChannelFlags_AllOff);
}

TEST(ChannelLayoutUtilMac, AudioChannelLayoutWithDescriptionsToChannelLayout) {
  int channels = 6;
  int layout_size =
      offsetof(AudioChannelLayout, mChannelDescriptions[channels]);
  ScopedAudioChannelLayout input_layout(layout_size);

  input_layout.layout()->mNumberChannelDescriptions = channels;
  input_layout.layout()->mChannelLayoutTag =
      kAudioChannelLayoutTag_UseChannelDescriptions;

  input_layout.layout()->mChannelDescriptions[0].mChannelLabel =
      kAudioChannelLabel_Left;
  input_layout.layout()->mChannelDescriptions[0].mChannelFlags =
      kAudioChannelFlags_AllOff;
  input_layout.layout()->mChannelDescriptions[1].mChannelLabel =
      kAudioChannelLabel_Right;
  input_layout.layout()->mChannelDescriptions[1].mChannelFlags =
      kAudioChannelFlags_AllOff;
  input_layout.layout()->mChannelDescriptions[2].mChannelLabel =
      kAudioChannelLabel_Center;
  input_layout.layout()->mChannelDescriptions[2].mChannelFlags =
      kAudioChannelFlags_AllOff;
  input_layout.layout()->mChannelDescriptions[3].mChannelLabel =
      kAudioChannelLabel_LFEScreen;
  input_layout.layout()->mChannelDescriptions[3].mChannelFlags =
      kAudioChannelFlags_AllOff;
  input_layout.layout()->mChannelDescriptions[4].mChannelLabel =
      kAudioChannelLabel_LeftSurround;
  input_layout.layout()->mChannelDescriptions[4].mChannelFlags =
      kAudioChannelFlags_AllOff;
  input_layout.layout()->mChannelDescriptions[5].mChannelLabel =
      kAudioChannelLabel_RightSurround;
  input_layout.layout()->mChannelDescriptions[5].mChannelFlags =
      kAudioChannelFlags_AllOff;

  ChannelLayout output_layout;
  EXPECT_EQ(
      AudioChannelLayoutToChannelLayout(*input_layout.layout(), &output_layout),
      true);
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
  EXPECT_EQ(
      AudioChannelLayoutToChannelLayout(*input_layout.layout(), &output_layout),
      true);
  EXPECT_EQ(output_layout, CHANNEL_LAYOUT_5_1);
}

TEST(ChannelLayoutUtilMac, AudioChannelLayoutWithMonoTagToChannelLayout) {
  int layout_size = offsetof(AudioChannelLayout, mChannelDescriptions[0]);
  ScopedAudioChannelLayout input_layout(layout_size);

  input_layout.layout()->mNumberChannelDescriptions = 0;
  // C.
  input_layout.layout()->mChannelLayoutTag = kAudioChannelLayoutTag_Mono;

  ChannelLayout output_layout;
  EXPECT_EQ(
      AudioChannelLayoutToChannelLayout(*input_layout.layout(), &output_layout),
      true);
  EXPECT_EQ(output_layout, ChannelLayout::CHANNEL_LAYOUT_MONO);
}

TEST(ChannelLayoutUtilMac, AudioChannelLayoutWithStereoTagToChannelLayout) {
  int layout_size = offsetof(AudioChannelLayout, mChannelDescriptions[0]);
  ScopedAudioChannelLayout input_layout(layout_size);

  input_layout.layout()->mNumberChannelDescriptions = 0;
  // L R.
  input_layout.layout()->mChannelLayoutTag = kAudioChannelLayoutTag_Stereo;

  ChannelLayout output_layout;
  EXPECT_EQ(
      AudioChannelLayoutToChannelLayout(*input_layout.layout(), &output_layout),
      true);
  EXPECT_EQ(output_layout, ChannelLayout::CHANNEL_LAYOUT_STEREO);
}

TEST(ChannelLayoutUtilMac,
     AudioChannelLayoutWithQuadraphonicTagToChannelLayout) {
  int layout_size = offsetof(AudioChannelLayout, mChannelDescriptions[0]);
  ScopedAudioChannelLayout input_layout(layout_size);

  input_layout.layout()->mNumberChannelDescriptions = 0;
  // L R Ls Rs.
  input_layout.layout()->mChannelLayoutTag =
      kAudioChannelLayoutTag_Quadraphonic;

  ChannelLayout output_layout;
  EXPECT_EQ(
      AudioChannelLayoutToChannelLayout(*input_layout.layout(), &output_layout),
      true);
  EXPECT_EQ(output_layout, ChannelLayout::CHANNEL_LAYOUT_2_2);
}

TEST(ChannelLayoutUtilMac, AudioChannelLayoutWith6Point1TagToChannelLayout) {
  int layout_size = offsetof(AudioChannelLayout, mChannelDescriptions[0]);
  ScopedAudioChannelLayout input_layout(layout_size);

  input_layout.layout()->mNumberChannelDescriptions = 0;
  // L R C LFE Ls Rs Cs.
  input_layout.layout()->mChannelLayoutTag = kAudioChannelLayoutTag_Logic_6_1_C;

  ChannelLayout output_layout;
  EXPECT_EQ(
      AudioChannelLayoutToChannelLayout(*input_layout.layout(), &output_layout),
      true);
  EXPECT_EQ(output_layout, ChannelLayout::CHANNEL_LAYOUT_6_1);
}

TEST(ChannelLayoutUtilMac, AudioChannelLayoutWithAAC5Point1TagToChannelLayout) {
  int layout_size = offsetof(AudioChannelLayout, mChannelDescriptions[0]);
  ScopedAudioChannelLayout input_layout(layout_size);

  input_layout.layout()->mNumberChannelDescriptions = 0;
  // C L R Ls Rs Lfe.
  input_layout.layout()->mChannelLayoutTag = kAudioChannelLayoutTag_AAC_5_1;

  ChannelLayout output_layout;
  EXPECT_EQ(
      AudioChannelLayoutToChannelLayout(*input_layout.layout(), &output_layout),
      true);
  EXPECT_EQ(output_layout, ChannelLayout::CHANNEL_LAYOUT_5_1);
}

TEST(ChannelLayoutUtilMac, AudioChannelLayoutWithAAC7Point1TagToChannelLayout) {
  int layout_size = offsetof(AudioChannelLayout, mChannelDescriptions[0]);
  ScopedAudioChannelLayout input_layout(layout_size);

  input_layout.layout()->mNumberChannelDescriptions = 0;
  // C Lc Rc L R Ls Rs Lfe.
  input_layout.layout()->mChannelLayoutTag = kAudioChannelLayoutTag_AAC_7_1;

  ChannelLayout output_layout;
  EXPECT_EQ(
      AudioChannelLayoutToChannelLayout(*input_layout.layout(), &output_layout),
      true);
  EXPECT_EQ(output_layout, ChannelLayout::CHANNEL_LAYOUT_7_1_WIDE);
}

TEST(ChannelLayoutUtilMac,
     AudioChannelLayoutWithEAC37Point1ATagToChannelLayout) {
  int layout_size = offsetof(AudioChannelLayout, mChannelDescriptions[0]);
  ScopedAudioChannelLayout input_layout(layout_size);

  input_layout.layout()->mNumberChannelDescriptions = 0;
  // L C R Ls Rs LFE Rls Rrs.
  input_layout.layout()->mChannelLayoutTag = kAudioChannelLayoutTag_EAC3_7_1_A;

  ChannelLayout output_layout;
  EXPECT_EQ(
      AudioChannelLayoutToChannelLayout(*input_layout.layout(), &output_layout),
      true);
  EXPECT_EQ(output_layout, ChannelLayout::CHANNEL_LAYOUT_7_1);
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
  EXPECT_EQ(
      AudioChannelLayoutToChannelLayout(*input_layout.layout(), &output_layout),
      false);
}

TEST(ChannelLayoutUtilMac, ChannelLayoutConvertBackToChannelLayout) {
  for (int i = 0; i <= CHANNEL_LAYOUT_MAX; i++) {
    ChannelLayout input_layout = static_cast<ChannelLayout>(i);
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
    auto intermediate_layout =
        ChannelLayoutToAudioChannelLayout(input_layout, input_channels);
    EXPECT_NE(intermediate_layout, nullptr);
    EXPECT_GT(intermediate_layout->layout_size(), 0u);
    ChannelLayout output_layout;
    EXPECT_EQ(AudioChannelLayoutToChannelLayout(*intermediate_layout->layout(),
                                                &output_layout),
              true);
    EXPECT_EQ(input_layout, output_layout);
  }
}

}  // namespace media
