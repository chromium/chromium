// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_CHANNEL_LAYOUT_H_
#define MEDIA_BASE_CHANNEL_LAYOUT_H_

#include "media/base/media_shmem_export.h"

namespace media {

// Enumerates the various representations of the ordering of audio channels.
// Logged to UMA, so never reuse a value, always add new/greater ones!
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

// Note: Do not reorder or reassign these values; other code depends on their
// ordering to operate correctly. E.g., CoreAudio channel layout computations.
enum Channels {
  LEFT = 0,
  RIGHT,
  CENTER,
  LFE,
  BACK_LEFT,
  BACK_RIGHT,
  LEFT_OF_CENTER,
  RIGHT_OF_CENTER,
  BACK_CENTER,
  SIDE_LEFT,
  SIDE_RIGHT,
  CHANNELS_MAX = SIDE_RIGHT, // Must always equal the largest value ever logged.
};

// The maximum number of concurrently active channels for all possible layouts.
// ChannelLayoutToChannelCount() will never return a value higher than this.
constexpr int kMaxConcurrentChannels = 8;

// Returns the expected channel position in an interleaved stream.  Values of -1
// mean the channel at that index is not used for that layout.  Values range
// from 0 to ChannelLayoutToChannelCount(layout) - 1.
MEDIA_SHMEM_EXPORT int ChannelOrder(ChannelLayout layout, Channels channel);

// Returns the number of channels in a given ChannelLayout or 0 if the
// channel layout can't be mapped to a valid value. Currently, 0
// is returned for CHANNEL_LAYOUT_NONE, CHANNEL_LAYOUT_UNSUPPORTED,
// CHANNEL_LAYOUT_DISCRETE, and CHANNEL_LAYOUT_BITSTREAM. For these cases,
// additional steps must be taken to manually figure out the corresponding
// number of channels.
MEDIA_SHMEM_EXPORT int ChannelLayoutToChannelCount(ChannelLayout layout);

// Given the number of channels, return the best layout,
// or return CHANNEL_LAYOUT_UNSUPPORTED if there is no good match.
MEDIA_SHMEM_EXPORT ChannelLayout GuessChannelLayout(int channels);

// Returns a string representation of the channel layout.
MEDIA_SHMEM_EXPORT const char* ChannelLayoutToString(ChannelLayout layout);

}  // namespace media

#endif  // MEDIA_BASE_CHANNEL_LAYOUT_H_
