// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/formats/common/opus_constants.h"

namespace media {

// Ensure offset values are properly ordered.
static_assert(OPUS_EXTRADATA_VERSION_OFFSET < OPUS_EXTRADATA_SIZE,
              "Invalid opus version constant.");
static_assert(OPUS_EXTRADATA_CHANNELS_OFFSET < OPUS_EXTRADATA_SIZE,
              "Invalid opus channels constant.");
static_assert(OPUS_EXTRADATA_SKIP_SAMPLES_OFFSET < OPUS_EXTRADATA_SIZE,
              "Invalid opus skip samples constant.");
static_assert(OPUS_EXTRADATA_SAMPLE_RATE_OFFSET < OPUS_EXTRADATA_SIZE,
              "Invalid opus sample rate constant.");
static_assert(OPUS_EXTRADATA_GAIN_OFFSET < OPUS_EXTRADATA_SIZE,
              "Invalid opus gain constant.");
static_assert(OPUS_EXTRADATA_CHANNEL_MAPPING_OFFSET < OPUS_EXTRADATA_SIZE,
              "Invalid opus channel mapping offset");

const uint8_t kOpusVorbisChannelMap[OPUS_MAX_VORBIS_CHANNELS]
                                   [OPUS_MAX_VORBIS_CHANNELS] = {
                                       {0},
                                       {0, 1},
                                       {0, 2, 1},
                                       {0, 1, 2, 3},
                                       {0, 4, 1, 2, 3},
                                       {0, 4, 1, 2, 3, 5},
                                       {0, 4, 1, 2, 3, 5, 6},
                                       {0, 6, 1, 2, 3, 4, 5, 7},
};

}  // namespace media
