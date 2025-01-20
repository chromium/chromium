// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FORMATS_HLS_PARSE_STATUS_H_
#define MEDIA_FORMATS_HLS_PARSE_STATUS_H_

#include <string_view>

#include "media/base/media_export.h"
#include "media/base/status.h"

namespace media::hls {

enum class ParseStatusCode : StatusCodeType {
  kReachedEOF = 0,
  kInvalidEOL = 1,
  kMalformedTag = 2,
  kFailedToParseDecimalInteger = 3,
  kFailedToParseDecimalFloatingPoint = 4,
  kFailedToParseSignedDecimalFloatingPoint = 5,
  kFailedToParseDecimalResolution = 6,
  kFailedToParseQuotedString = 7,
  kFailedToParseByteRange = 8,
  kFailedToParseStableId = 9,
  kFailedToParseInstreamId = 10,
  kFailedToParseAudioChannels = 11,
  kFailedToParseHexadecimalString = 12,
  kInvalidPlaylistVersion = 13,
  kUnknownPlaylistType = 14,
  kMalformedAttributeList = 15,
  kAttributeListHasDuplicateNames = 16,
  kMalformedVariableName = 17,
  kInvalidUri = 18,
  kPlaylistMissingM3uTag = 19,
  kMediaPlaylistMissingTargetDuration = 20,
  kTargetDurationExceedsMax = 21,
  kMediaSegmentMissingInfTag = 22,
  kMediaSegmentExceedsTargetDuration = 23,
  kPlaylistHasDuplicateTags = 24,
  kPlaylistHasUnsupportedVersion = 25,
  kPlaylistHasVersionMismatch = 26,
  kMediaPlaylistHasMultivariantPlaylistTag = 27,
  kMultivariantPlaylistHasMediaPlaylistTag = 28,
  kVariableUndefined = 29,
  kVariableDefinedMultipleTimes = 30,
  kImportedVariableInParentlessPlaylist = 31,
  kImportedVariableUndefined = 32,
  kXStreamInfTagNotFollowedByUri = 33,
  kVariantMissingStreamInfTag = 34,
  kMediaSegmentBeforeMediaSequenceTag = 35,
  kMediaSegmentBeforeDiscontinuitySequenceTag = 36,
  kDiscontinuityTagBeforeDiscontinuitySequenceTag = 37,
  kByteRangeRequiresOffset = 38,
  kByteRangeInvalid = 39,
  kValueOverflowsTimeDelta = 40,
  kPlaylistOverflowsTimeDelta = 41,
  kSkipBoundaryTooLow = 42,
  kHoldBackDistanceTooLow = 43,
  kPartTargetDurationExceedsTargetDuration = 44,
  kPartHoldBackDistanceTooLow = 45,
  kPartInfTagWithoutPartHoldBack = 46,
  kPlaylistHasUnexpectedDeltaUpdate = 47,
  kRenditionGroupHasMultipleDefaultRenditions = 48,
  kRenditionGroupHasDuplicateRenditionNames = 49,
  kRenditionGroupDoesNotExist = 50,
  kUnsupportedEncryptionMethod = 51,
};

struct ParseStatusTraits {
  using Codes = ParseStatusCode;
  static constexpr StatusGroupType Group() { return "hls::ParseStatus"; }
};

using ParseStatus = TypedStatus<ParseStatusTraits>;

MEDIA_EXPORT std::string_view ParseStatusCodeToString(ParseStatusCode code);

}  // namespace media::hls

#endif  // MEDIA_FORMATS_HLS_PARSE_STATUS_H_
