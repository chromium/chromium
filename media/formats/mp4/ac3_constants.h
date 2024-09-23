// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FORMATS_MP4_AC3_CONSTANTS_H_
#define MEDIA_FORMATS_MP4_AC3_CONSTANTS_H_

#include <stdint.h>

#include "media/base/channel_layout.h"
#include "media/base/media_export.h"

namespace media {

// Please refer to ETSI TS 102 366 V1.4.1
//     https://www.etsi.org/deliver/etsi_ts/102300_102399/102366/01.03.01_60/ts_102366v010301p.pdf
// for more details.
//
// F.6.2.13 chan_loc - 9bit - Table F.6.1 - chan_loc field bit assignments.
//
// The chan_loc field indicates channel locations (beyond the standard 5.1
// channels) that are carried by dependent substreams associated with an
// independent substream.
enum EC3DependentSubstreamChannelLocation {
  LC_RC = 1,     // Lc/Rc pair
  LRS_RRS = 2,   // Lrs/Rrs pair
  CS = 4,        // Cs
  TS = 8,        // Ts
  LSD_RSD = 16,  // Lsd/Rsd pair
  LW_RW = 32,    // Lw/Rw pair
  LVH_RVH = 64,  // Lvh/Rvh pair
  CVH = 128,     // Cvh
  LFE2 = 256,    // LFE2
};

// F.4.2.6 lfeon - Low frequency effects channel on - 1 bit
//
// Size of the lfeon field, used to determine if the lfe (sub woofer)
// channel exists or not.
MEDIA_EXPORT inline constexpr uint8_t kAC3LFESize = 2;

// 4.4.2.3 acmod - Audio coding mode - 3 bits.
//
// Size of the acmod field, used to indicates which of the main service
// channels are in use, ranging from 3/2 to 1/0
MEDIA_EXPORT inline constexpr uint8_t kAC3AudioCodingModeSize = 8;

// Conversion table of channel layout, the first dimension is lfeon and the
// second dimension is acmod.
//
// Taking the 5.1 channel layout as an example, it contains a LFE channel, so
// lfeon = 1, and the remaining five channels are: L, C, R, Ls, Rs with acmod =
// 7, so kAC3AudioCodingModeTable[1][7] = CHANNEL_LAYOUT_5_1.
MEDIA_EXPORT extern const ChannelLayout
    kAC3AudioCodingModeTable[kAC3LFESize][kAC3AudioCodingModeSize];

}  // namespace media

#endif  // MEDIA_FORMATS_MP4_AC3_CONSTANTS_H_
