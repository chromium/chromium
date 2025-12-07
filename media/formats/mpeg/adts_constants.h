// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FORMATS_MPEG_ADTS_CONSTANTS_H_
#define MEDIA_FORMATS_MPEG_ADTS_CONSTANTS_H_

#include <stddef.h>

#include <array>

#include "media/base/channel_layout.h"

namespace media {

enum {
  kADTSHeaderMinSize = 7,
  kADTSHeaderSizeNoCrc = 7,
  kADTSHeaderSizeWithCrc = 9,
  kSamplesPerAACFrame = 1024,
};

inline constexpr auto kADTSFrequencyTable =
    std::to_array<const int>({96000, 88200, 64000, 48000, 44100, 32000, 24000,
                              22050, 16000, 12000, 11025, 8000, 7350});

inline constexpr std::array kADTSChannelLayoutTable{
    media::CHANNEL_LAYOUT_NONE,     media::CHANNEL_LAYOUT_MONO,
    media::CHANNEL_LAYOUT_STEREO,   media::CHANNEL_LAYOUT_SURROUND,
    media::CHANNEL_LAYOUT_4_0,      media::CHANNEL_LAYOUT_5_0_BACK,
    media::CHANNEL_LAYOUT_5_1_BACK, media::CHANNEL_LAYOUT_7_1};

}  // namespace media

#endif  // MEDIA_FORMATS_MPEG_ADTS_CONSTANTS_H_
