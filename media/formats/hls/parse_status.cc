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
    PARSE_STATUS_CODE_CASE(kFailedToParseDecimalResolution);
    PARSE_STATUS_CODE_CASE(kFailedToParseQuotedString);
    PARSE_STATUS_CODE_CASE(kFailedToParseByteRange);
    PARSE_STATUS_CODE_CASE(kInvalidPlaylistVersion);
    PARSE_STATUS_CODE_CASE(kUnknownPlaylistType);
    PARSE_STATUS_CODE_CASE(kMalformedAttributeList);
    PARSE_STATUS_CODE_CASE(kAttributeListHasDuplicateNames);
    PARSE_STATUS_CODE_CASE(kMalformedVariableName);
    PARSE_STATUS_CODE_CASE(kInvalidUri);
    PARSE_STATUS_CODE_CASE(kPlaylistMissingM3uTag);
    PARSE_STATUS_CODE_CASE(kMediaPlaylistMissingTargetDuration);
    PARSE_STATUS_CODE_CASE(kMediaSegmentMissingInfTag);
    PARSE_STATUS_CODE_CASE(kMediaSegmentExceedsTargetDuration);
    PARSE_STATUS_CODE_CASE(kPlaylistHasDuplicateTags);
    PARSE_STATUS_CODE_CASE(kPlaylistHasUnsupportedVersion);
    PARSE_STATUS_CODE_CASE(kMediaPlaylistHasMultivariantPlaylistTag);
    PARSE_STATUS_CODE_CASE(kMultivariantPlaylistHasMediaPlaylistTag);
    PARSE_STATUS_CODE_CASE(kVariableUndefined);
    PARSE_STATUS_CODE_CASE(kVariableDefinedMultipleTimes);
    PARSE_STATUS_CODE_CASE(kImportedVariableInParentlessPlaylist);
    PARSE_STATUS_CODE_CASE(kImportedVariableUndefined);
    PARSE_STATUS_CODE_CASE(kXStreamInfTagNotFollowedByUri);
    PARSE_STATUS_CODE_CASE(kVariantMissingStreamInfTag);
    PARSE_STATUS_CODE_CASE(kMediaSegmentBeforeMediaSequenceTag);
    PARSE_STATUS_CODE_CASE(kMediaSegmentBeforeDiscontinuitySequenceTag);
    PARSE_STATUS_CODE_CASE(kDiscontinuityTagBeforeDiscontinuitySequenceTag);
    PARSE_STATUS_CODE_CASE(kByteRangeRequiresOffset);
    PARSE_STATUS_CODE_CASE(kByteRangeInvalid);
  }

  NOTREACHED();
  return "";
}

#undef PARSE_STATUS_CODE_CASE

}  // namespace media::hls
