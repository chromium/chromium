// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FORMATS_MPEG_ADTS_CONSTANTS_H_
#define MEDIA_FORMATS_MPEG_ADTS_CONSTANTS_H_

#include <stddef.h>

#include "media/base/channel_layout.h"
#include "media/base/media_export.h"

namespace media {

enum {
  kADTSHeaderMinSize = 7,
  kADTSHeaderSizeNoCrc = 7,
  kADTSHeaderSizeWithCrc = 9,
  kSamplesPerAACFrame = 1024,
};

MEDIA_EXPORT extern const int kADTSFrequencyTable[];
MEDIA_EXPORT extern const size_t kADTSFrequencyTableSize;

MEDIA_EXPORT extern const media::ChannelLayout kADTSChannelLayoutTable[];
MEDIA_EXPORT extern const size_t kADTSChannelLayoutTableSize;

}  // namespace media

#endif  // MEDIA_FORMATS_MPEG_ADTS_CONSTANTS_H_
