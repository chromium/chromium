// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_CHANNEL_LAYOUT_H_
#define MEDIA_BASE_CHANNEL_LAYOUT_H_

#include <stdint.h>

#include "base/types/pass_key.h"
#include "media/base/media_export.h"

namespace media {

// Enumerates the various representations of the ordering of audio channels.
// Logged to UMA, so never reuse a value, always add new/greater ones!
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.media
// GENERATED_JAVA_PREFIX_TO_STRIP: CHANNEL_
enum ChannelLayout {
  CHANNEL_LAYOUT_NONE = 0,
  CHANNEL_LAYOUT_UNSUPPORTED = 1,

  // Front C
  CHANNEL_LAYOUT_MONO = 2,

  // Front L, Front R
  CHANNEL_LAYOUT_STEREO = 3,

  // Front L, Front R, Back C
  CHANNEL_LAYOUT_2_1 = 4,

  // Front L, Front R, Front C
  CHANNEL_LAYOUT_SURROUND = 5,

  // Front L, Front R, Front C, Back C
  CHANNEL_LAYOUT_4_0 = 6,

  // Front L, Front R, Side L, Side R
  CHANNEL_LAYOUT_2_2 = 7,

  // Front L, Front R, Back L, Back R
  CHANNEL_LAYOUT_QUAD = 8,

  // Front L, Front R, Front C, Side L, Side R
  CHANNEL_LAYOUT_5_0 = 9,

  // Front L, Front R, Front C, LFE, Side L, Side R
  CHANNEL_LAYOUT_5_1 = 10,

  // Front L, Front R, Front C, Back L, Back R
  CHANNEL_LAYOUT_5_0_BACK = 11,

  // Front L, Front R, Front C, LFE, Back L, Back R
  CHANNEL_LAYOUT_5_1_BACK = 12,

  // Front L, Front R, Front C, Back L, Back R, Side L, Side R
  CHANNEL_LAYOUT_7_0 = 13,

  // Front L, Front R, Front C, LFE, Back L, Back R, Side L, Side R
  CHANNEL_LAYOUT_7_1 = 14,

  // Front L, Front R, Front C, LFE, Front LofC, Front RofC, Side L, Side R
  CHANNEL_LAYOUT_7_1_WIDE = 15,

  // Front L, Front R
  CHANNEL_LAYOUT_STEREO_DOWNMIX = 16,

  // Front L, Front R, LFE
  CHANNEL_LAYOUT_2POINT1 = 17,

  // Front L, Front R, Front C, LFE
  CHANNEL_LAYOUT_3_1 = 18,

  // Front L, Front R, Front C, LFE, Back C
  CHANNEL_LAYOUT_4_1 = 19,

  // Front L, Front R, Front C, Back C, Side L, Side R
  CHANNEL_LAYOUT_6_0 = 20,

  // Front L, Front R, Front LofC, Front RofC, Side L, Side R
  CHANNEL_LAYOUT_6_0_FRONT = 21,

  // Front L, Front R, Front C, Back L, Back R, Back C
  CHANNEL_LAYOUT_HEXAGONAL = 22,

  // Front L, Front R, Front C, LFE, Back C, Side L, Side R
  CHANNEL_LAYOUT_6_1 = 23,

  // Front L, Front R, Front C, LFE, Back L, Back R, Back C
  CHANNEL_LAYOUT_6_1_BACK = 24,

  // Front L, Front R, LFE, Front LofC, Front RofC, Side L, Side R
  CHANNEL_LAYOUT_6_1_FRONT = 25,

  // Front L, Front R, Front C, Front LofC, Front RofC, Side L, Side R
  CHANNEL_LAYOUT_7_0_FRONT = 26,

  // Front L, Front R, Front C, LFE, Back L, Back R, Front LofC, Front RofC
  CHANNEL_LAYOUT_7_1_WIDE_BACK = 27,

  // Front L, Front R, Front C, Back L, Back R, Back C, Side L, Side R
  CHANNEL_LAYOUT_OCTAGONAL = 28,

  // Channels are not explicitly mapped to speakers.
  CHANNEL_LAYOUT_DISCRETE = 29,

  // Deprecated, but keeping the enum value for UMA consistency.
  // Front L, Front R, Front C. Front C contains the keyboard mic audio. This
  // layout is only intended for input for WebRTC. The Front C channel
  // is stripped away in the WebRTC audio input pipeline and never seen outside
  // of that.
  CHANNEL_LAYOUT_STEREO_AND_KEYBOARD_MIC = 30,

  // Front L, Front R, LFE, Side L, Side R
  CHANNEL_LAYOUT_4_1_QUAD_SIDE = 31,

  // Actual channel layout is specified in the bitstream and the actual channel
  // count is unknown at Chromium media pipeline level (useful for audio
  // pass-through mode).
  CHANNEL_LAYOUT_BITSTREAM = 32,

  // Front L, Front R, Front C, LFE, Side L, Side R,
  // Front Height L, Front Height R, Rear Height L, Rear Height R
  // Will be represented as six channels (5.1) due to eight channel limit
  // kMaxConcurrentChannels
  CHANNEL_LAYOUT_5_1_4_DOWNMIX = 33,

  // Front C, LFE
  CHANNEL_LAYOUT_1_1 = 34,

  // Front L, Front R, LFE, Back C
  CHANNEL_LAYOUT_3_1_BACK = 35,

  // Max value, must always equal the largest entry ever logged.
  CHANNEL_LAYOUT_MAX = CHANNEL_LAYOUT_3_1_BACK
};

// The channel order matches the order of the bitmask in the Windows
// WAVEFORMATEXTENSIBLE format. The value of the enum corresponds to the bit
// position in the mask (e.g. LEFT is bit 0, RIGHT is bit 1, etc.).
//
// This standard is used by Windows (WASAPI), FFmpeg (legacy layouts), and
// SMPTE.
//
// Note: Do not reorder or reassign these values; other code depends on their
// ordering to operate correctly. E.g., CoreAudio channel layout computations
// and ChannelMaskToLayout().
enum Channels {
  LEFT = 0,
  RIGHT = 1,
  CENTER = 2,
  LFE = 3,
  BACK_LEFT = 4,
  BACK_RIGHT = 5,
  LEFT_OF_CENTER = 6,
  RIGHT_OF_CENTER = 7,
  BACK_CENTER = 8,
  SIDE_LEFT = 9,
  SIDE_RIGHT = 10,
  CHANNELS_MAX =
      SIDE_RIGHT,  // Must always equal the largest value ever logged.
};

// The maximum number of concurrently active channels for all possible layouts.
// ChannelLayoutToChannelCount() will never return a value higher than this.
constexpr int kMaxConcurrentChannels = 8;

// Returns the expected channel position in an interleaved stream.  Values of -1
// mean the channel at that index is not used for that layout.  Values range
// from 0 to ChannelLayoutToChannelCount(layout) - 1.
MEDIA_EXPORT int ChannelOrder(ChannelLayout layout, Channels channel);

// Returns the number of channels in a given ChannelLayout or 0 if the
// channel layout can't be mapped to a valid value. Currently, 0
// is returned for CHANNEL_LAYOUT_NONE, CHANNEL_LAYOUT_UNSUPPORTED,
// CHANNEL_LAYOUT_DISCRETE, and CHANNEL_LAYOUT_BITSTREAM. For these cases,
// additional steps must be taken to manually figure out the corresponding
// number of channels.
MEDIA_EXPORT int ChannelLayoutToChannelCount(ChannelLayout layout);

// Given the number of channels, return the best layout,
// or return CHANNEL_LAYOUT_UNSUPPORTED if there is no good match.
MEDIA_EXPORT ChannelLayout GuessChannelLayout(int channels);

// Returns the channel layout for a given channel mask. This code assumes that
// the mask uses the Channels enum as the position of each channel, e.g.
// a `LEFT` channel would be represented as `1 << Channels::LEFT` or `0b1`.
//
// Returns CHANNEL_LAYOUT_DISCRETE if the bitmask does not match any known
// channel layout.
using ChannelMask = uint32_t;
MEDIA_EXPORT ChannelLayout ChannelMaskToLayout(ChannelMask channel_mask);

// Returns a string representation of the channel layout.
MEDIA_EXPORT const char* ChannelLayoutToString(ChannelLayout layout);

// Channel count and ChannelLayout pair, with helper methods to enforce safe
// construction.
class MEDIA_EXPORT ChannelLayoutConfig {
 public:
  using Passkey = base::PassKey<ChannelLayoutConfig>;

  // Use `Passkey` here to limit cases when we bypass checks. This allows for
  // `Mono()` and `Stereo()` to be constexpr, without forcing all helper methods
  // above to also be constexpr.
  constexpr ChannelLayoutConfig(Passkey passkey,
                                ChannelLayout channel_layout,
                                int channels)
      : channel_layout_(channel_layout), channels_(channels) {}

  constexpr ChannelLayoutConfig()
      : ChannelLayoutConfig(Passkey(), CHANNEL_LAYOUT_NONE, 0u) {}

  // Crashes if `channel_layout` and `channels` are incompatible.
  ChannelLayoutConfig(ChannelLayout channel_layout, int channels);

  ChannelLayoutConfig(const ChannelLayoutConfig& other);
  ChannelLayoutConfig& operator=(const ChannelLayoutConfig& other);

  constexpr ~ChannelLayoutConfig() = default;

  template <ChannelLayout layout>
  static constexpr ChannelLayoutConfig FromLayout() {
    if constexpr (layout == CHANNEL_LAYOUT_MONO) {
      return Mono();
    } else if constexpr (layout == CHANNEL_LAYOUT_STEREO) {
      return Stereo();
    }

    // Other layouts cannot be used in a constexpr context.
    return ChannelLayoutConfig(layout, ChannelLayoutToChannelCount(layout));
  }

  static ChannelLayoutConfig FromLayout(ChannelLayout layout) {
    return ChannelLayoutConfig(layout, ChannelLayoutToChannelCount(layout));
  }

  static constexpr ChannelLayoutConfig Mono() {
    return ChannelLayoutConfig(Passkey(), CHANNEL_LAYOUT_MONO, 1u);
  }

  static constexpr ChannelLayoutConfig Stereo() {
    return ChannelLayoutConfig(Passkey(), CHANNEL_LAYOUT_STEREO, 2u);
  }

  static ChannelLayoutConfig Guess(int channels);

  constexpr ChannelLayout channel_layout() const { return channel_layout_; }

  constexpr int channels() const { return channels_; }

  bool operator==(const ChannelLayoutConfig& other) const = default;

 private:
  ChannelLayout channel_layout_;  // Order of surround sound channels.
  int channels_;                  // Number of channels.
};

// For `CHANNEL_LAYOUT_DISCRETE`, we have to explicitly set the number of
// channels, so we need to use the normal constructor.
template <>
constexpr ChannelLayoutConfig
ChannelLayoutConfig::FromLayout<CHANNEL_LAYOUT_DISCRETE>() = delete;
}  // namespace media

#endif  // MEDIA_BASE_CHANNEL_LAYOUT_H_
