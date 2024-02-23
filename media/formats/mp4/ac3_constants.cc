// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/formats/mp4/ac3_constants.h"

namespace media {

const ChannelLayout
    kAC3AudioCodingModeTable[kAC3LFESize][kAC3AudioCodingModeSize] = {
        {
            CHANNEL_LAYOUT_STEREO,    // Ch1, Ch2 (aka. Dual Mono)
            CHANNEL_LAYOUT_MONO,      // C
            CHANNEL_LAYOUT_STEREO,    // L, R
            CHANNEL_LAYOUT_SURROUND,  // L, C, R
            CHANNEL_LAYOUT_2_1,       // L, R, S
            CHANNEL_LAYOUT_4_0,       // L, C, R, S
            CHANNEL_LAYOUT_2_2,       // L, R, Ls, Rs
            CHANNEL_LAYOUT_5_0        // L, C, R, Ls, Rs
        },
        {
            CHANNEL_LAYOUT_2POINT1,        // Ch1, Ch2, LFE
            CHANNEL_LAYOUT_1_1,            // C, LFE
            CHANNEL_LAYOUT_2POINT1,        // L, R, LFE
            CHANNEL_LAYOUT_3_1,            // L, C, R, LFE
            CHANNEL_LAYOUT_3_1_BACK,       // L, R, S, LFE
            CHANNEL_LAYOUT_4_1,            // L, C, R, S, LFE
            CHANNEL_LAYOUT_4_1_QUAD_SIDE,  // L, R, Ls, Rs, LFE
            CHANNEL_LAYOUT_5_1             // L, C, R, Ls, Rs, LFE
        }};

}  // namespace media
