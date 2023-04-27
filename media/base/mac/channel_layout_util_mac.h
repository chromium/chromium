// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_MAC_CHANNEL_LAYOUT_UTIL_MAC_H_
#define MEDIA_BASE_MAC_CHANNEL_LAYOUT_UTIL_MAC_H_

#include <AudioToolbox/AudioToolbox.h>

#include <memory>
#include <vector>

#include "media/base/channel_layout.h"
#include "media/base/media_export.h"

namespace media {

// Used to store audio channel layout and layout size.
class MEDIA_EXPORT ScopedAudioChannelLayout {
 public:
  explicit ScopedAudioChannelLayout(size_t layout_size);
  ~ScopedAudioChannelLayout();

  ScopedAudioChannelLayout(const ScopedAudioChannelLayout&) = delete;
  ScopedAudioChannelLayout& operator=(const ScopedAudioChannelLayout&) = delete;

  size_t layout_size() const { return layout_.size(); }

  AudioChannelLayout* layout() {
    return reinterpret_cast<AudioChannelLayout*>(layout_.data());
  }

 private:
  std::vector<uint8_t> layout_;
};

// Mapping from Chrome's channel to CoreAudio's channel.
MEDIA_EXPORT AudioChannelLabel
ChannelToAudioChannelLabel(Channels input_channel);

// Mapping from CoreAudio's channel to Chrome's channel.
// Return false if couldn't find a matched channel.
MEDIA_EXPORT bool AudioChannelLabelToChannel(AudioChannelLabel input_channel,
                                             Channels* output_channel);

// Mapping from Chrome's layout to CoreAudio's layout.
MEDIA_EXPORT std::unique_ptr<ScopedAudioChannelLayout>
ChannelLayoutToAudioChannelLayout(ChannelLayout input_layout,
                                  int input_channels);

// Mapping from CoreAudio's layout to Chrome's layout.
// Return false if couldn't find a matched layout.
MEDIA_EXPORT bool AudioChannelLayoutToChannelLayout(
    const AudioChannelLayout& input_layout,
    ChannelLayout* output_layout);

}  // namespace media

#endif  // MEDIA_BASE_MAC_CHANNEL_LAYOUT_UTIL_MAC_H_
