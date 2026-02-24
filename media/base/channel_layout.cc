// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/channel_layout.h"

#include <stddef.h>

#include <algorithm>
#include <array>

#include "base/check_op.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "media/base/limits.h"

namespace media {

namespace {

// The channel orderings for each layout as specified by FFmpeg. Each value
// represents the index of each channel in each layout.  Values of -1 mean the
// channel at that index is not used for that layout. For example, the left side
// surround sound channel in FFmpeg's 5.1 layout is in the 5th position (because
// the order is L, R, C, LFE, LS, RS), so
// kChannelOrderings[CHANNEL_LAYOUT_5_1][SIDE_LEFT] = 4;
constexpr std::array<std::array<int, CHANNELS_MAX + 1>, CHANNEL_LAYOUT_MAX + 1>
    kChannelOrderings = {{
        // FL | FR | FC | LFE | BL | BR | FLofC | FRofC | BC | SL | SR

        // CHANNEL_LAYOUT_NONE
        {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},

        // CHANNEL_LAYOUT_UNSUPPORTED
        {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},

        // CHANNEL_LAYOUT_MONO
        {-1, -1, 0, -1, -1, -1, -1, -1, -1, -1, -1},

        // CHANNEL_LAYOUT_STEREO
        {0, 1, -1, -1, -1, -1, -1, -1, -1, -1, -1},

        // CHANNEL_LAYOUT_2_1
        {0, 1, -1, -1, -1, -1, -1, -1, 2, -1, -1},

        // CHANNEL_LAYOUT_SURROUND
        {0, 1, 2, -1, -1, -1, -1, -1, -1, -1, -1},

        // CHANNEL_LAYOUT_4_0
        {0, 1, 2, -1, -1, -1, -1, -1, 3, -1, -1},

        // CHANNEL_LAYOUT_2_2
        {0, 1, -1, -1, -1, -1, -1, -1, -1, 2, 3},

        // CHANNEL_LAYOUT_QUAD
        {0, 1, -1, -1, 2, 3, -1, -1, -1, -1, -1},

        // CHANNEL_LAYOUT_5_0
        {0, 1, 2, -1, -1, -1, -1, -1, -1, 3, 4},

        // CHANNEL_LAYOUT_5_1
        {0, 1, 2, 3, -1, -1, -1, -1, -1, 4, 5},

        // FL | FR | FC | LFE | BL | BR | FLofC | FRofC | BC | SL | SR

        // CHANNEL_LAYOUT_5_0_BACK
        {0, 1, 2, -1, 3, 4, -1, -1, -1, -1, -1},

        // CHANNEL_LAYOUT_5_1_BACK
        {0, 1, 2, 3, 4, 5, -1, -1, -1, -1, -1},

        // CHANNEL_LAYOUT_7_0
        {0, 1, 2, -1, 3, 4, -1, -1, -1, 5, 6},

        // CHANNEL_LAYOUT_7_1
        {0, 1, 2, 3, 4, 5, -1, -1, -1, 6, 7},

        // CHANNEL_LAYOUT_7_1_WIDE
        {0, 1, 2, 3, -1, -1, 4, 5, -1, 6, 7},

        // CHANNEL_LAYOUT_STEREO_DOWNMIX
        {0, 1, -1, -1, -1, -1, -1, -1, -1, -1, -1},

        // CHANNEL_LAYOUT_2POINT1
        {0, 1, -1, 2, -1, -1, -1, -1, -1, -1, -1},

        // CHANNEL_LAYOUT_3_1
        {0, 1, 2, 3, -1, -1, -1, -1, -1, -1, -1},

        // CHANNEL_LAYOUT_4_1
        {0, 1, 2, 3, -1, -1, -1, -1, 4, -1, -1},

        // CHANNEL_LAYOUT_6_0
        {0, 1, 2, -1, -1, -1, -1, -1, 3, 4, 5},

        // CHANNEL_LAYOUT_6_0_FRONT
        {0, 1, -1, -1, -1, -1, 2, 3, -1, 4, 5},

        // FL | FR | FC | LFE | BL | BR | FLofC | FRofC | BC | SL | SR

        // CHANNEL_LAYOUT_HEXAGONAL
        {0, 1, 2, -1, 3, 4, -1, -1, 5, -1, -1},

        // CHANNEL_LAYOUT_6_1
        {0, 1, 2, 3, -1, -1, -1, -1, 4, 5, 6},

        // CHANNEL_LAYOUT_6_1_BACK
        {0, 1, 2, 3, 4, 5, -1, -1, 6, -1, -1},

        // CHANNEL_LAYOUT_6_1_FRONT
        {0, 1, -1, 2, -1, -1, 3, 4, -1, 5, 6},

        // CHANNEL_LAYOUT_7_0_FRONT
        {0, 1, 2, -1, -1, -1, 3, 4, -1, 5, 6},

        // CHANNEL_LAYOUT_7_1_WIDE_BACK
        {0, 1, 2, 3, 4, 5, 6, 7, -1, -1, -1},

        // CHANNEL_LAYOUT_OCTAGONAL
        {0, 1, 2, -1, 3, 4, -1, -1, 5, 6, 7},

        // CHANNEL_LAYOUT_DISCRETE
        {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},

        // CHANNEL_LAYOUT_STEREO_AND_KEYBOARD_MIC, deprecated
        {0, 1, 2, -1, -1, -1, -1, -1, -1, -1, -1},

        // CHANNEL_LAYOUT_4_1_QUAD_SIDE
        {0, 1, -1, 2, -1, -1, -1, -1, -1, 3, 4},

        // CHANNEL_LAYOUT_BITSTREAM
        {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},

        // FL | FR | FC | LFE | BL | BR | FLofC | FRofC | BC | SL | SR

        // CHANNEL_LAYOUT_5_1_4, downmixed to six channels (5.1)
        {0, 1, 2, 3, -1, -1, -1, -1, -1, 4, 5},

        // CHANNEL_LAYOUT_1_1
        {-1, -1, 0, 1, -1, -1, -1, -1, -1, -1, -1},

        // CHANNEL_LAYOUT_3_1_BACK
        {0, 1, -1, 2, -1, -1, -1, -1, 3, -1, -1},
    }};

constexpr auto kLayoutToChannels = []() {
  std::array<int, CHANNEL_LAYOUT_MAX + 1> counts;
  std::ranges::transform(
      kChannelOrderings, counts.begin(), [](const auto& row) {
        return static_cast<int>(
            std::ranges::count_if(row, [](int c) { return c != -1; }));
      });
  return counts;
}();

// Helper to compute bitmask for a layout at compile-time.
constexpr ChannelMask ComputeChannelMask(ChannelLayout layout) {
  ChannelMask mask = 0;
  for (int c = 0; c <= Channels::CHANNELS_MAX; ++c) {
    if (kChannelOrderings[layout][c] != -1) {
      mask |= 1ULL << c;
    }
  }
  return mask;
}

// Map of all channel layouts to their respective masks.
constexpr auto kChannelMaskToLayoutMap = []() {
  std::array<std::pair<ChannelMask, ChannelLayout>, CHANNEL_LAYOUT_MAX + 1>
      entries;
  for (int i = 0; i <= CHANNEL_LAYOUT_MAX; ++i) {
    ChannelLayout layout = static_cast<ChannelLayout>(i);
    entries[i] = {ComputeChannelMask(layout), layout};
  }
  return entries;
}();

int ComputeChannelCount(ChannelLayout channel_layout, int channels) {
  if (channel_layout == CHANNEL_LAYOUT_DISCRETE) {
    CHECK_GT(channels, 0);
    CHECK_LE(channels, limits::kMaxChannels);

    return channels;
  } else if (channel_layout == CHANNEL_LAYOUT_5_1_4_DOWNMIX && channels != 0) {
    // `channels` should really only be 6 here, but we might end up with the
    // original 5.1.4 channel count. For now, handle this gracefully, and force
    // the value down to 6. Eventually, we should remove this special case
    // altogether.
    CHECK(channels == 6 || channels == 10);

    // TODO(crbug.com/486962136): Track whether this condition arises in the
    // wild, and remove this branch entirely.
    CHECK_NE(channels, 10, base::NotFatalUntil::M151);
    return 6;
  }
  const int calculated_channel_count =
      ChannelLayoutToChannelCount(channel_layout);
  CHECK(channel_layout == CHANNEL_LAYOUT_UNSUPPORTED ||
        calculated_channel_count == channels);
  return calculated_channel_count;
}

}  // namespace

int ChannelLayoutToChannelCount(ChannelLayout layout) {
  DCHECK_LT(static_cast<size_t>(layout), std::size(kLayoutToChannels));
  DCHECK_LE(kLayoutToChannels[layout], kMaxConcurrentChannels);
  return kLayoutToChannels[layout];
}

// Converts a channel count into a channel layout.
ChannelLayout GuessChannelLayout(int channels) {
  // Use discrete layout for higher channel counts to facilitate
  // audio passthrough, thus avoiding channel mixing.
  if (channels > kMaxConcurrentChannels && channels <= limits::kMaxChannels) {
    return CHANNEL_LAYOUT_DISCRETE;
  }
  switch (channels) {
    case 1:
      return CHANNEL_LAYOUT_MONO;
    case 2:
      return CHANNEL_LAYOUT_STEREO;
    case 3:
      return CHANNEL_LAYOUT_SURROUND;
    case 4:
      return CHANNEL_LAYOUT_QUAD;
    case 5:
      return CHANNEL_LAYOUT_5_0;
    case 6:
      return CHANNEL_LAYOUT_5_1;
    case 7:
      return CHANNEL_LAYOUT_6_1;
    case 8:
      return CHANNEL_LAYOUT_7_1;
    default:
      DVLOG(1) << "Unsupported channel count: " << channels;
  }
  return CHANNEL_LAYOUT_UNSUPPORTED;
}

ChannelLayout ChannelMaskToLayout(ChannelMask channel_mask) {
  for (const auto& entry : kChannelMaskToLayoutMap) {
    if (entry.first == channel_mask) {
      return entry.second;
    }
  }
  // If we don't find a standard ChannelLayout associated with the mask, return
  // a DISCRETE layout so that we can still handle the raw channel data.
  return CHANNEL_LAYOUT_DISCRETE;
}

int ChannelOrder(ChannelLayout layout, Channels channel) {
  DCHECK_LT(static_cast<size_t>(layout), std::size(kChannelOrderings));
  DCHECK_LT(static_cast<size_t>(channel), std::size(kChannelOrderings[0]));
  return kChannelOrderings[layout][channel];
}

const char* ChannelLayoutToString(ChannelLayout layout) {
  switch (layout) {
    case CHANNEL_LAYOUT_NONE:
      return "NONE";
    case CHANNEL_LAYOUT_UNSUPPORTED:
      return "UNSUPPORTED";
    case CHANNEL_LAYOUT_MONO:
      return "MONO";
    case CHANNEL_LAYOUT_STEREO:
      return "STEREO";
    case CHANNEL_LAYOUT_2_1:
      return "2.1";
    case CHANNEL_LAYOUT_SURROUND:
      return "SURROUND";
    case CHANNEL_LAYOUT_4_0:
      return "4.0";
    case CHANNEL_LAYOUT_2_2:
      return "QUAD_SIDE";
    case CHANNEL_LAYOUT_QUAD:
      return "QUAD";
    case CHANNEL_LAYOUT_5_0:
      return "5.0";
    case CHANNEL_LAYOUT_5_1:
      return "5.1";
    case CHANNEL_LAYOUT_5_0_BACK:
      return "5.0_BACK";
    case CHANNEL_LAYOUT_5_1_BACK:
      return "5.1_BACK";
    case CHANNEL_LAYOUT_7_0:
      return "7.0";
    case CHANNEL_LAYOUT_7_1:
      return "7.1";
    case CHANNEL_LAYOUT_7_1_WIDE:
      return "7.1_WIDE";
    case CHANNEL_LAYOUT_STEREO_DOWNMIX:
      return "STEREO_DOWNMIX";
    case CHANNEL_LAYOUT_2POINT1:
      return "2POINT1";
    case CHANNEL_LAYOUT_3_1:
      return "3.1";
    case CHANNEL_LAYOUT_4_1:
      return "4.1";
    case CHANNEL_LAYOUT_6_0:
      return "6.0";
    case CHANNEL_LAYOUT_6_0_FRONT:
      return "6.0_FRONT";
    case CHANNEL_LAYOUT_HEXAGONAL:
      return "HEXAGONAL";
    case CHANNEL_LAYOUT_6_1:
      return "6.1";
    case CHANNEL_LAYOUT_6_1_BACK:
      return "6.1_BACK";
    case CHANNEL_LAYOUT_6_1_FRONT:
      return "6.1_FRONT";
    case CHANNEL_LAYOUT_7_0_FRONT:
      return "7.0_FRONT";
    case CHANNEL_LAYOUT_7_1_WIDE_BACK:
      return "7.1_WIDE_BACK";
    case CHANNEL_LAYOUT_OCTAGONAL:
      return "OCTAGONAL";
    case CHANNEL_LAYOUT_DISCRETE:
      return "DISCRETE";
    case CHANNEL_LAYOUT_STEREO_AND_KEYBOARD_MIC:
      return "STEREO_AND_KEYBOARD_MIC";  // deprecated
    case CHANNEL_LAYOUT_4_1_QUAD_SIDE:
      return "4.1_QUAD_SIDE";
    case CHANNEL_LAYOUT_BITSTREAM:
      return "BITSTREAM";
    case CHANNEL_LAYOUT_5_1_4_DOWNMIX:
      return "5.1.4 DOWNMIX";
    case CHANNEL_LAYOUT_1_1:
      return "1.1";
    case CHANNEL_LAYOUT_3_1_BACK:
      return "3.1_BACK";
  }
  NOTREACHED() << "Invalid channel layout provided: " << layout;
}

ChannelLayoutConfig::ChannelLayoutConfig(const ChannelLayoutConfig& other) =
    default;
ChannelLayoutConfig& ChannelLayoutConfig::operator=(
    const ChannelLayoutConfig& other) = default;

ChannelLayoutConfig::ChannelLayoutConfig(ChannelLayout channel_layout,
                                         int channels)
    : channel_layout_(channel_layout),
      channels_(ComputeChannelCount(channel_layout, channels)) {}

ChannelLayoutConfig ChannelLayoutConfig::Guess(int channels) {
  return ChannelLayoutConfig(GuessChannelLayout(channels), channels);
}

}  // namespace media
