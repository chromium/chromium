// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FORMATS_HLS_PARSE_STATUS_H_
#define MEDIA_FORMATS_HLS_PARSE_STATUS_H_

#include "media/base/status.h"

namespace media {
namespace hls {

enum class ParseStatusCode : StatusCodeType {
  kReachedEOF,
  kInvalidEOL,
  kMalformedTag,
  kFailedToParseDecimalInteger,
  kFailedToParseDecimalFloatingPoint,
  kFailedToParseSignedDecimalFloatingPoint,
  kPlaylistMissingM3uTag,
  kMediaSegmentMissingInfTag,
};

struct ParseStatusTraits {
  using Codes = ParseStatusCode;
  static constexpr StatusGroupType Group() { return "hls::ParseStatus"; }
};

using ParseStatus = TypedStatus<ParseStatusTraits>;

}  // namespace hls
}  // namespace media

#endif  // MEDIA_FORMATS_HLS_PARSE_STATUS_H_
