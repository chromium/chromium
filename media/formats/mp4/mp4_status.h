// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FORMATS_MP4_MP4_STATUS_H_
#define MEDIA_FORMATS_MP4_MP4_STATUS_H_

#include "media/base/status.h"

namespace media {

struct MP4StatusTraits {
  enum class Codes : StatusCodeType {
    kOk = 0,
    kUnsupportedStream = 1,
    kFailedToParse = 2,

    // SPS and PPS are common to H264 and H265.
    kInvalidSPS = 3,
    kInvalidPPS = 4,
    kFailedToLookupSPS = 5,
    kFailedToLookupPPS = 6,

    // Slice headers are h264 only (I think, but we only have an h264 parser)
    kInvalidSliceHeader = 7,

    kBufferTooSmall = 8,

    // H265 specific
    kInvalidVPS = 9,
    kFailedToLookupVPS = 10,
  };
  static constexpr StatusGroupType Group() { return "MP4Status"; }
};

using MP4Status = TypedStatus<MP4StatusTraits>;

}  // namespace media

#endif  // MEDIA_FORMATS_MP4_MP4_STATUS_H_
