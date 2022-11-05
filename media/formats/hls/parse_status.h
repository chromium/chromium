// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FORMATS_HLS_PARSE_STATUS_H_
#define MEDIA_FORMATS_HLS_PARSE_STATUS_H_

#include "base/strings/string_piece.h"
#include "media/base/media_export.h"
#include "media/base/status.h"

namespace media::hls {

enum class ParseStatusCode : StatusCodeType {
  kReachedEOF,
  kInvalidEOL,
  kMalformedTag,
  kFailedToParseDecimalInteger,
  kFailedToParseDecimalFloatingPoint,
  kFailedToParseSignedDecimalFloatingPoint,
  kFailedToParseDecimalResolution,
  kFailedToParseQuotedString,
  kFailedToParseByteRange,
  kFailedToParseStableId,
  kFailedToParseInstreamId,
  kFailedToParseAudioChannels,
  kInvalidPlaylistVersion,
  kUnknownPlaylistType,
  kMalformedAttributeList,
  kAttributeListHasDuplicateNames,
  kMalformedVariableName,
  kInvalidUri,
  kPlaylistMissingM3uTag,
  kMediaPlaylistMissingTargetDuration,
  kTargetDurationExceedsMax,
  kMediaSegmentMissingInfTag,
  kMediaSegmentExceedsTargetDuration,
  kPlaylistHasDuplicateTags,
  kPlaylistHasUnsupportedVersion,
  kPlaylistHasVersionMismatch,
  kMediaPlaylistHasMultivariantPlaylistTag,
  kMultivariantPlaylistHasMediaPlaylistTag,
  kVariableUndefined,
  kVariableDefinedMultipleTimes,
  kImportedVariableInParentlessPlaylist,
  kImportedVariableUndefined,
  kXStreamInfTagNotFollowedByUri,
  kVariantMissingStreamInfTag,
  kMediaSegmentBeforeMediaSequenceTag,
  kMediaSegmentBeforeDiscontinuitySequenceTag,
  kDiscontinuityTagBeforeDiscontinuitySequenceTag,
  kByteRangeRequiresOffset,
  kByteRangeInvalid,
  kValueOverflowsTimeDelta,
  kPlaylistOverflowsTimeDelta,
  kSkipBoundaryTooLow,
  kHoldBackDistanceTooLow,
  kPartTargetDurationExceedsTargetDuration,
  kPartHoldBackDistanceTooLow,
  kPartInfTagWithoutPartHoldBack,
  kPlaylistHasUnexpectedDeltaUpdate,
  kRenditionGroupHasMultipleDefaultRenditions,
  kRenditionGroupHasDuplicateRenditionNames,
  kRenditionGroupDoesNotExist,
};

struct ParseStatusTraits {
  using Codes = ParseStatusCode;
  static constexpr StatusGroupType Group() { return "hls::ParseStatus"; }
};

using ParseStatus = TypedStatus<ParseStatusTraits>;

MEDIA_EXPORT base::StringPiece ParseStatusCodeToString(ParseStatusCode code);

}  // namespace media::hls

#endif  // MEDIA_FORMATS_HLS_PARSE_STATUS_H_
