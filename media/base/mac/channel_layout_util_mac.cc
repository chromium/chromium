// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#include "media/base/mac/channel_layout_util_mac.h"

#include <memory>

#include "base/check_op.h"
#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "media/base/channel_layout.h"

namespace media {

ScopedAudioChannelLayout::ScopedAudioChannelLayout(size_t layout_size)
    : layout_memory_(base::HeapArray<uint8_t>::Uninit(layout_size)) {}

ScopedAudioChannelLayout::~ScopedAudioChannelLayout() = default;

std::optional<Channels> AudioChannelLabelToChannel(
    AudioChannelLabel input_channel) {
  switch (input_channel) {
    case kAudioChannelLabel_Left:
      return Channels::LEFT;
    case kAudioChannelLabel_Right:
      return Channels::RIGHT;
    case kAudioChannelLabel_Center:
    case kAudioChannelLabel_Mono:
      return Channels::CENTER;
    case kAudioChannelLabel_LFEScreen:
      return Channels::LFE;
    case kAudioChannelLabel_RearSurroundLeft:
      return Channels::BACK_LEFT;
    case kAudioChannelLabel_RearSurroundRight:
      return Channels::BACK_RIGHT;
    case kAudioChannelLabel_LeftCenter:
      return Channels::LEFT_OF_CENTER;
    case kAudioChannelLabel_RightCenter:
      return Channels::RIGHT_OF_CENTER;
    case kAudioChannelLabel_CenterSurround:
      return Channels::BACK_CENTER;
    case kAudioChannelLabel_LeftSurround:
      return Channels::SIDE_LEFT;
    case kAudioChannelLabel_RightSurround:
      return Channels::SIDE_RIGHT;
    case kAudioChannelLabel_LeftTopFront:
      return Channels::TOP_FRONT_LEFT;
    case kAudioChannelLabel_RightTopFront:
      return Channels::TOP_FRONT_RIGHT;
    case kAudioChannelLabel_LeftTopRear:
      return Channels::TOP_BACK_LEFT;
    case kAudioChannelLabel_RightTopRear:
      return Channels::TOP_BACK_RIGHT;
    case kAudioChannelLabel_TopCenterSurround:
      return Channels::TOP_CENTER;
    case kAudioChannelLabel_CenterTopFront:
      return Channels::TOP_FRONT_CENTER;
    case kAudioChannelLabel_TopBackCenter:
      return Channels::TOP_BACK_CENTER;
    default:
      return std::nullopt;
  }
}

AudioChannelLabel ChannelToAudioChannelLabel(Channels input_channel) {
  switch (input_channel) {
    case Channels::LEFT:
      return kAudioChannelLabel_Left;
    case Channels::RIGHT:
      return kAudioChannelLabel_Right;
    case Channels::CENTER:
      return kAudioChannelLabel_Center;
    case Channels::LFE:
      return kAudioChannelLabel_LFEScreen;
    case Channels::BACK_LEFT:
      return kAudioChannelLabel_RearSurroundLeft;
    case Channels::BACK_RIGHT:
      return kAudioChannelLabel_RearSurroundRight;
    case Channels::LEFT_OF_CENTER:
      return kAudioChannelLabel_LeftCenter;
    case Channels::RIGHT_OF_CENTER:
      return kAudioChannelLabel_RightCenter;
    case Channels::BACK_CENTER:
      return kAudioChannelLabel_CenterSurround;
    case Channels::SIDE_LEFT:
      return kAudioChannelLabel_LeftSurround;
    case Channels::SIDE_RIGHT:
      return kAudioChannelLabel_RightSurround;
    case Channels::TOP_FRONT_LEFT:
      return kAudioChannelLabel_LeftTopFront;
    case Channels::TOP_FRONT_RIGHT:
      return kAudioChannelLabel_RightTopFront;
    case Channels::TOP_BACK_LEFT:
      return kAudioChannelLabel_LeftTopRear;
    case Channels::TOP_BACK_RIGHT:
      return kAudioChannelLabel_RightTopRear;
    case Channels::TOP_CENTER:
      return kAudioChannelLabel_TopCenterSurround;
    case Channels::TOP_FRONT_CENTER:
      return kAudioChannelLabel_CenterTopFront;
    case Channels::TOP_BACK_CENTER:
      return kAudioChannelLabel_TopBackCenter;
  }
}

std::unique_ptr<ScopedAudioChannelLayout> ChannelLayoutToAudioChannelLayout(
    ChannelLayout input_layout,
    int input_channels) {
  CHECK_GT(input_layout, CHANNEL_LAYOUT_UNSUPPORTED);
  CHECK_GT(input_channels, 0);

  // AudioChannelLayout is structure ending in a variable length array, so we
  // can't directly allocate one.
  //
  // Code modeled after example from Apple documentation here:
  // https://developer.apple.com/library/content/qa/qa1627/_index.html
  int output_layout_size =
      offsetof(AudioChannelLayout, mChannelDescriptions[input_channels]);
  auto new_layout =
      std::make_unique<ScopedAudioChannelLayout>(output_layout_size);

  new_layout->layout()->mNumberChannelDescriptions = input_channels;
  new_layout->layout()->mChannelLayoutTag =
      kAudioChannelLayoutTag_UseChannelDescriptions;

  auto descriptions = GetDescriptions(*new_layout->layout());
  if (input_layout == CHANNEL_LAYOUT_DISCRETE) {
    // For the discrete case, mark all channels as unknown.
    for (int ch = 0; ch < input_channels; ++ch) {
      descriptions[ch].mChannelLabel = kAudioChannelLabel_Unknown;
      descriptions[ch].mChannelFlags = kAudioChannelFlags_AllOff;
    }
  } else if (input_layout == CHANNEL_LAYOUT_MONO) {
    // CoreAudio has a special label for mono.
    CHECK_EQ(input_channels, 1);
    descriptions[0].mChannelLabel = kAudioChannelLabel_Mono;
    descriptions[0].mChannelFlags = kAudioChannelFlags_AllOff;
  } else {
    for (int ch = 0; ch <= CHANNELS_MAX; ++ch) {
      const int order = ChannelOrder(input_layout, static_cast<Channels>(ch));
      // We only allocate up to `input_channels`, skip if past what was
      // allocated for.
      if (order == -1 || order >= input_channels) {
        continue;
      }
      descriptions[order].mChannelLabel =
          ChannelToAudioChannelLabel(static_cast<Channels>(ch));
      descriptions[order].mChannelFlags = kAudioChannelFlags_AllOff;
    }
  }

  return new_layout;
}

bool AudioChannelLayoutToChannelLayout(const AudioChannelLayout& input_layout,
                                       ChannelLayout* output_layout) {
  OSStatus result = noErr;
  UInt32 size = 0;
  AudioChannelLayoutTag tag = input_layout.mChannelLayoutTag;
  const bool new_descriptions =
      tag != kAudioChannelLayoutTag_UseChannelDescriptions;
  if (tag == kAudioChannelLayoutTag_UseChannelBitmap) {
    result = AudioFormatGetPropertyInfo(
        kAudioFormatProperty_ChannelLayoutForBitmap, sizeof(UInt32),
        &input_layout.mChannelBitmap, &size);
  } else if (new_descriptions) {
    result =
        AudioFormatGetPropertyInfo(kAudioFormatProperty_ChannelLayoutForTag,
                                   sizeof(AudioChannelLayoutTag), &tag, &size);
  }

  if (result != noErr) {
    return false;
  }

  ScopedAudioChannelLayout new_layout(size);

  if (tag == kAudioChannelLayoutTag_UseChannelBitmap) {
    result = AudioFormatGetProperty(
        kAudioFormatProperty_ChannelLayoutForBitmap, sizeof(UInt32),
        &input_layout.mChannelBitmap, &size, new_layout.layout());
  } else if (new_descriptions) {
    result = AudioFormatGetProperty(kAudioFormatProperty_ChannelLayoutForTag,
                                    sizeof(AudioChannelLayoutTag), &tag, &size,
                                    new_layout.layout());
  }

  if (result != noErr) {
    return false;
  }

  // SAFETY: Length of channel descriptions is provided by the CoreAudio layer
  // above and is guaranteed to match mChannelDescriptions here.
  auto& layout = new_descriptions ? *new_layout.layout() : input_layout;
  const auto descriptions = GetDescriptions(layout);
  CHECK_GT(descriptions.size(), 0u);

  std::vector<Channels> channels_to_match;
  for (const AudioChannelDescription& description : descriptions) {
    const std::optional<Channels> maybe_channel =
        AudioChannelLabelToChannel(description.mChannelLabel);
    if (!maybe_channel) {
      return false;
    }
    channels_to_match.push_back(*maybe_channel);
  }

  for (int i = 0; i <= ChannelLayout::CHANNEL_LAYOUT_MAX; i++) {
    ChannelLayout chrome_layout = static_cast<ChannelLayout>(i);
    if (static_cast<UInt32>(ChannelLayoutToChannelCount(chrome_layout)) !=
        descriptions.size()) {
      continue;
    }

    bool matched = true;
    for (const auto& channel : channels_to_match) {
      auto channel_order = ChannelOrder(chrome_layout, channel);
      if (channel_order == -1) {
        matched = false;
        break;
      }
    }

    if (matched) {
      *output_layout = chrome_layout;
      return true;
    }
  }

  return false;
}

}  // namespace media
