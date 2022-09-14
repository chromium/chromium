// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FORMATS_DTS_DTS_UTIL_H_
#define MEDIA_FORMATS_DTS_DTS_UTIL_H_

#include <stddef.h>
#include <stdint.h>

#include "media/base/audio_codecs.h"
#include "media/base/media_export.h"

namespace media {

namespace dts {

// Returns the total number of audio samples in the given buffer, which
// could contain several complete DTS frames.
MEDIA_EXPORT int ParseTotalSampleCount(const uint8_t* data,
                                       size_t size,
                                       AudioCodec dts_codec_type);

}  // namespace dts

}  // namespace media

#endif  // MEDIA_FORMATS_DTS_DTS_UTIL_H_
