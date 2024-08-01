// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/base/mac/channel_layout_util_mac.h"

#include <memory>

#include "base/check_op.h"
#include "media/base/channel_layout.h"

namespace media {

ScopedAudioChannelLayout::ScopedAudioChannelLayout(size_t layout_size)
    : layout_(layout_size) {}

ScopedAudioChannelLayout::~ScopedAudioChannelLayout() = default;

bool AudioChannelLabelToChannel(AudioChannelLabel input_channel,
                                Channels* output_channel) {
  switch (input_channel) {
    case kAudioChannelLabel_Left:
      *output_channel = Channels::LEFT;
      break;
    case kAudioChannelLabel_Right:
      *output_channel = Channels::RIGHT;
      break;
    case kAudioChannelLabel_Center:
    case kAudioChannelLabel_Mono:
      *output_channel = Channels::CENTER;
      break;
    case kAudioChannelLabel_LFEScreen:
      *output_channel = Channels::LFE;
      break;
    case kAudioChannelLabel_RearSurroundLeft:
      *output_channel = Channels::BACK_LEFT;
      break;
    case kAudioChannelLabel_RearSurroundRight:
      *output_channel = Channels::BACK_RIGHT;
      break;
    case kAudioChannelLabel_LeftCenter:
      *output_channel = Channels::LEFT_OF_CENTER;
      break;
    case kAudioChannelLabel_RightCenter:
      *output_channel = Channels::RIGHT_OF_CENTER;
      break;
    case kAudioChannelLabel_CenterSurround:
      *output_channel = Channels::BACK_CENTER;
      break;
    case kAudioChannelLabel_LeftSurround:
      *output_channel = Channels::SIDE_LEFT;
      break;
    case kAudioChannelLabel_RightSurround:
      *output_channel = Channels::SIDE_RIGHT;
      break;
    default:
      return false;
  }
  return true;
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
  AudioChannelDescription* descriptions =
      new_layout->layout()->mChannelDescriptions;

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
      if (order == -1) {
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
  AudioChannelFlags tag = input_layout.mChannelLayoutTag;
  if (tag == kAudioChannelLayoutTag_UseChannelBitmap) {
    result = AudioFormatGetPropertyInfo(
        kAudioFormatProperty_ChannelLayoutForBitmap, sizeof(UInt32),
        &input_layout.mChannelBitmap, &size);
  } else if (tag != kAudioChannelLayoutTag_UseChannelDescriptions) {
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
  } else if (tag != kAudioChannelLayoutTag_UseChannelDescriptions) {
    result = AudioFormatGetProperty(kAudioFormatProperty_ChannelLayoutForTag,
                                    sizeof(AudioChannelLayoutTag), &tag, &size,
                                    new_layout.layout());
  }

  if (result != noErr) {
    return false;
  }

  UInt32 channel_count = 0;
  if (tag != kAudioChannelLayoutTag_UseChannelDescriptions) {
    new_layout.layout()->mChannelLayoutTag =
        kAudioChannelLayoutTag_UseChannelDescriptions;
    channel_count = new_layout.layout()->mNumberChannelDescriptions;
  } else {
    channel_count = input_layout.mNumberChannelDescriptions;
  }
  CHECK_GT(static_cast<int>(channel_count), 0);

  std::vector<Channels> channels_to_match;
  for (UInt32 i = 0; i < channel_count; i++) {
    Channels channel;
    auto channelLabel =
        tag == kAudioChannelLayoutTag_UseChannelDescriptions
            ? input_layout.mChannelDescriptions[i].mChannelLabel
            : new_layout.layout()->mChannelDescriptions[i].mChannelLabel;
    if (!AudioChannelLabelToChannel(channelLabel, &channel)) {
      return false;
    }
    channels_to_match.push_back(channel);
  }

  for (int i = 0; i <= ChannelLayout::CHANNEL_LAYOUT_MAX; i++) {
    ChannelLayout layout = static_cast<ChannelLayout>(i);
    if (static_cast<UInt32>(ChannelLayoutToChannelCount(layout)) !=
        channel_count) {
      continue;
    }

    bool matched = true;
    for (const auto& channel : channels_to_match) {
      auto channel_order = ChannelOrder(layout, channel);
      if (channel_order == -1) {
        matched = false;
        break;
      }
    }

    if (matched) {
      *output_layout = layout;
      return true;
    }
  }

  return false;
}

}  // namespace media
