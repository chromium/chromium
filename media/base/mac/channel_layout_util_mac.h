// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_MAC_CHANNEL_LAYOUT_UTIL_MAC_H_
#define MEDIA_BASE_MAC_CHANNEL_LAYOUT_UTIL_MAC_H_

#include <AudioToolbox/AudioToolbox.h>

#include <memory>
#include <optional>
#include <vector>

#include "base/compiler_specific.h"
#include "base/containers/heap_array.h"
#include "base/containers/span.h"
#include "media/base/channel_layout.h"
#include "media/base/media_export.h"

namespace media {

inline base::span<const AudioChannelDescription> GetDescriptions(
    const AudioChannelLayout& layout) {
  // SAFETY: the core audio framework guarantees that the channel descriptions
  // variable array is of size `mNumberChannelDescriptions`.
  // https://developer.apple.com/documentation/coreaudiotypes/audiochannellayout/mchanneldescriptions
  return UNSAFE_BUFFERS(base::span<const AudioChannelDescription>(
      layout.mChannelDescriptions, layout.mNumberChannelDescriptions));
}

inline base::span<AudioChannelDescription> GetDescriptions(
    AudioChannelLayout& layout) {
  // SAFETY: the core audio framework guarantees that the channel descriptions
  // variable array is of size `mNumberChannelDescriptions`.
  // https://developer.apple.com/documentation/coreaudiotypes/audiochannellayout/mchanneldescriptions
  return UNSAFE_BUFFERS(base::span<AudioChannelDescription>(
      layout.mChannelDescriptions, layout.mNumberChannelDescriptions));
}

// Used to store audio channel layout and layout size.
class MEDIA_EXPORT ScopedAudioChannelLayout {
 public:
  explicit ScopedAudioChannelLayout(size_t layout_size);
  ScopedAudioChannelLayout(const ScopedAudioChannelLayout&) = delete;
  ScopedAudioChannelLayout(ScopedAudioChannelLayout&&) = delete;
  ScopedAudioChannelLayout& operator=(const ScopedAudioChannelLayout&) = delete;
  ScopedAudioChannelLayout& operator=(ScopedAudioChannelLayout&&) = delete;
  ~ScopedAudioChannelLayout();

  size_t layout_size() const { return layout_memory_.size(); }

  AudioChannelLayout* layout() {
    return reinterpret_cast<AudioChannelLayout*>(layout_memory_.data());
  }
  const AudioChannelLayout* layout() const {
    return reinterpret_cast<const AudioChannelLayout*>(layout_memory_.data());
  }

 private:
  base::HeapArray<uint8_t> layout_memory_;
};

// Mapping from Chrome's channel to CoreAudio's channel.
MEDIA_EXPORT AudioChannelLabel
ChannelToAudioChannelLabel(Channels input_channel);

// Mapping from CoreAudio's channel to Chrome's channel.
// Return std::nullopt if couldn't find a matched channel.
MEDIA_EXPORT std::optional<Channels> AudioChannelLabelToChannel(
    AudioChannelLabel input_channel);

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
