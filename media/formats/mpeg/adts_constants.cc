// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/formats/mpeg/adts_constants.h"

#include <iterator>

namespace media {

// The following conversion table is extracted from ISO 14496 Part 3 -
// Table 1.16 - Sampling Frequency Index.
const int kADTSFrequencyTable[] = {96000, 88200, 64000, 48000, 44100,
                                   32000, 24000, 22050, 16000, 12000,
                                   11025, 8000,  7350};
const size_t kADTSFrequencyTableSize = std::size(kADTSFrequencyTable);

// The following conversion table is extracted from ISO 14496 Part 3 -
// Table 1.17 - Channel Configuration.
const media::ChannelLayout kADTSChannelLayoutTable[] = {
    media::CHANNEL_LAYOUT_NONE,     media::CHANNEL_LAYOUT_MONO,
    media::CHANNEL_LAYOUT_STEREO,   media::CHANNEL_LAYOUT_SURROUND,
    media::CHANNEL_LAYOUT_4_0,      media::CHANNEL_LAYOUT_5_0_BACK,
    media::CHANNEL_LAYOUT_5_1_BACK, media::CHANNEL_LAYOUT_7_1};
const size_t kADTSChannelLayoutTableSize = std::size(kADTSChannelLayoutTable);

}  // namespace media
