// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/formats/hls/parse_status.h"

#include "base/notreached.h"
#include "base/strings/string_piece.h"

namespace media::hls {

#define PARSE_STATUS_CODE_CASE(x) \
  case ParseStatusCode::x:        \
    return #x

base::StringPiece ParseStatusCodeToString(ParseStatusCode code) {
  switch (code) {
    PARSE_STATUS_CODE_CASE(kReachedEOF);
    PARSE_STATUS_CODE_CASE(kInvalidEOL);
    PARSE_STATUS_CODE_CASE(kMalformedTag);
    PARSE_STATUS_CODE_CASE(kFailedToParseDecimalInteger);
    PARSE_STATUS_CODE_CASE(kFailedToParseDecimalFloatingPoint);
    PARSE_STATUS_CODE_CASE(kFailedToParseSignedDecimalFloatingPoint);
    PARSE_STATUS_CODE_CASE(kInvalidPlaylistVersion);
    PARSE_STATUS_CODE_CASE(kMalformedAttributeList);
    PARSE_STATUS_CODE_CASE(kAttributeListHasDuplicateNames);
    PARSE_STATUS_CODE_CASE(kMalformedVariableName);
    PARSE_STATUS_CODE_CASE(kInvalidUri);
    PARSE_STATUS_CODE_CASE(kPlaylistMissingM3uTag);
    PARSE_STATUS_CODE_CASE(kMediaSegmentMissingInfTag);
    PARSE_STATUS_CODE_CASE(kPlaylistHasDuplicateTags);
    PARSE_STATUS_CODE_CASE(kPlaylistHasUnsupportedVersion);
    PARSE_STATUS_CODE_CASE(kMediaPlaylistHasMultivariantPlaylistTag);
  }

  NOTREACHED();
  return "";
}

#undef PARSE_STATUS_CODE_CASE

}  // namespace media::hls
