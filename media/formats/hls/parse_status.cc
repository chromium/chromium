// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/formats/hls/parse_status.h"

#include <string_view>

#include "base/notreached.h"

namespace media::hls {

#define PARSE_STATUS_CODE_CASE(x) \
  case ParseStatusCode::x:        \
    return #x

std::string_view ParseStatusCodeToString(ParseStatusCode code) {
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
    PARSE_STATUS_CODE_CASE(kFailedToParseStableId);
    PARSE_STATUS_CODE_CASE(kFailedToParseInstreamId);
    PARSE_STATUS_CODE_CASE(kFailedToParseAudioChannels);
    PARSE_STATUS_CODE_CASE(kFailedToParseHexadecimalString);
    PARSE_STATUS_CODE_CASE(kInvalidPlaylistVersion);
    PARSE_STATUS_CODE_CASE(kUnknownPlaylistType);
    PARSE_STATUS_CODE_CASE(kMalformedAttributeList);
    PARSE_STATUS_CODE_CASE(kAttributeListHasDuplicateNames);
    PARSE_STATUS_CODE_CASE(kMalformedVariableName);
    PARSE_STATUS_CODE_CASE(kInvalidUri);
    PARSE_STATUS_CODE_CASE(kPlaylistMissingM3uTag);
    PARSE_STATUS_CODE_CASE(kMediaPlaylistMissingTargetDuration);
    PARSE_STATUS_CODE_CASE(kTargetDurationExceedsMax);
    PARSE_STATUS_CODE_CASE(kMediaSegmentMissingInfTag);
    PARSE_STATUS_CODE_CASE(kMediaSegmentExceedsTargetDuration);
    PARSE_STATUS_CODE_CASE(kPlaylistHasDuplicateTags);
    PARSE_STATUS_CODE_CASE(kPlaylistHasUnsupportedVersion);
    PARSE_STATUS_CODE_CASE(kPlaylistHasVersionMismatch);
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
    PARSE_STATUS_CODE_CASE(kValueOverflowsTimeDelta);
    PARSE_STATUS_CODE_CASE(kPlaylistOverflowsTimeDelta);
    PARSE_STATUS_CODE_CASE(kSkipBoundaryTooLow);
    PARSE_STATUS_CODE_CASE(kHoldBackDistanceTooLow);
    PARSE_STATUS_CODE_CASE(kPartTargetDurationExceedsTargetDuration);
    PARSE_STATUS_CODE_CASE(kPartHoldBackDistanceTooLow);
    PARSE_STATUS_CODE_CASE(kPartInfTagWithoutPartHoldBack);
    PARSE_STATUS_CODE_CASE(kPlaylistHasUnexpectedDeltaUpdate);
    PARSE_STATUS_CODE_CASE(kRenditionGroupHasMultipleDefaultRenditions);
    PARSE_STATUS_CODE_CASE(kRenditionGroupHasDuplicateRenditionNames);
    PARSE_STATUS_CODE_CASE(kRenditionGroupDoesNotExist);
    PARSE_STATUS_CODE_CASE(kUnsupportedEncryptionMethod);
  }

  NOTREACHED();
}

#undef PARSE_STATUS_CODE_CASE

}  // namespace media::hls
