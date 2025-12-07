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
  kMalformedDate = 52,
  kNoTagBody = 53,
  kConflictingDefineTags = 54,
  kMissingRequiredSegmentInfoTrailingComma = 55,
  kMalformedDateRange = 56,

  // Invalid attribute value errors
  kInvalidPreloadHintType = 57,
  kInvalidDateRangeAttribute = 58,
  kInvalidSessionDateAttribute = 59,
  kInvalidStartAttribute = 60,
  kInvalidMediaAttribute = 61,
  kInvalidPartAttribute = 62,
  kInvalidPreloadHintAttribute = 63,
  kInvalidKeyAttribute = 64,
  kInvalidSessionDataAttribute = 65,
  kInvalidIFrameStreamInfAttribute = 66,
  kInvalidContentSteeringAttribute = 67,

  // Missing attribute errors
  kMissingDefineAttribute = 68,
  kMissingStreamInfAttribute = 69,
  kMissingMapAttribute = 70,
  kMissingPartInfAttribute = 71,
  kMissingSkipAttribute = 72,
  kMissingMediaAttribute = 73,
  kMissingPartAttribute = 74,
  kMissingPreloadHintAttribute = 75,
  kMissingKeyAttribute = 76,
  kMissingDateRangeAttribute = 77,
  kMissingContentSteeringAttribute = 78,
  kMissingSessionDataAttribute = 79,
  kMissingIFrameStreamInfAttribute = 80,
  kMissingStartAttribute = 81,

  // Conflicting attribute errors
  kConflictingServerControlAttributes = 82,
  kConflictingKeyTagAttributes = 83,
  kConflictingMediaAttributes = 84,
};

#define STRINGIFY_CODE(x) \
  case Codes::x:          \
    return #x

struct ParseStatusTraits {
  using Codes = ParseStatusCode;
  static constexpr StatusGroupType Group() { return "hls::ParseStatus"; }

  static constexpr std::string ReadableCodeName(Codes code) {
    switch (code) {
      STRINGIFY_CODE(kReachedEOF);
      STRINGIFY_CODE(kInvalidEOL);
      STRINGIFY_CODE(kMalformedTag);
      STRINGIFY_CODE(kFailedToParseDecimalInteger);
      STRINGIFY_CODE(kFailedToParseDecimalFloatingPoint);
      STRINGIFY_CODE(kFailedToParseSignedDecimalFloatingPoint);
      STRINGIFY_CODE(kFailedToParseDecimalResolution);
      STRINGIFY_CODE(kFailedToParseQuotedString);
      STRINGIFY_CODE(kFailedToParseByteRange);
      STRINGIFY_CODE(kFailedToParseStableId);
      STRINGIFY_CODE(kFailedToParseInstreamId);
      STRINGIFY_CODE(kFailedToParseAudioChannels);
      STRINGIFY_CODE(kFailedToParseHexadecimalString);
      STRINGIFY_CODE(kInvalidPlaylistVersion);
      STRINGIFY_CODE(kUnknownPlaylistType);
      STRINGIFY_CODE(kMalformedAttributeList);
      STRINGIFY_CODE(kAttributeListHasDuplicateNames);
      STRINGIFY_CODE(kMalformedVariableName);
      STRINGIFY_CODE(kInvalidUri);
      STRINGIFY_CODE(kPlaylistMissingM3uTag);
      STRINGIFY_CODE(kMediaPlaylistMissingTargetDuration);
      STRINGIFY_CODE(kTargetDurationExceedsMax);
      STRINGIFY_CODE(kMediaSegmentMissingInfTag);
      STRINGIFY_CODE(kMediaSegmentExceedsTargetDuration);
      STRINGIFY_CODE(kPlaylistHasDuplicateTags);
      STRINGIFY_CODE(kPlaylistHasUnsupportedVersion);
      STRINGIFY_CODE(kPlaylistHasVersionMismatch);
      STRINGIFY_CODE(kMediaPlaylistHasMultivariantPlaylistTag);
      STRINGIFY_CODE(kMultivariantPlaylistHasMediaPlaylistTag);
      STRINGIFY_CODE(kVariableUndefined);
      STRINGIFY_CODE(kVariableDefinedMultipleTimes);
      STRINGIFY_CODE(kImportedVariableInParentlessPlaylist);
      STRINGIFY_CODE(kImportedVariableUndefined);
      STRINGIFY_CODE(kXStreamInfTagNotFollowedByUri);
      STRINGIFY_CODE(kVariantMissingStreamInfTag);
      STRINGIFY_CODE(kMediaSegmentBeforeMediaSequenceTag);
      STRINGIFY_CODE(kMediaSegmentBeforeDiscontinuitySequenceTag);
      STRINGIFY_CODE(kDiscontinuityTagBeforeDiscontinuitySequenceTag);
      STRINGIFY_CODE(kByteRangeRequiresOffset);
      STRINGIFY_CODE(kByteRangeInvalid);
      STRINGIFY_CODE(kValueOverflowsTimeDelta);
      STRINGIFY_CODE(kPlaylistOverflowsTimeDelta);
      STRINGIFY_CODE(kSkipBoundaryTooLow);
      STRINGIFY_CODE(kHoldBackDistanceTooLow);
      STRINGIFY_CODE(kPartTargetDurationExceedsTargetDuration);
      STRINGIFY_CODE(kPartHoldBackDistanceTooLow);
      STRINGIFY_CODE(kPartInfTagWithoutPartHoldBack);
      STRINGIFY_CODE(kPlaylistHasUnexpectedDeltaUpdate);
      STRINGIFY_CODE(kRenditionGroupHasMultipleDefaultRenditions);
      STRINGIFY_CODE(kRenditionGroupHasDuplicateRenditionNames);
      STRINGIFY_CODE(kRenditionGroupDoesNotExist);
      STRINGIFY_CODE(kUnsupportedEncryptionMethod);
      STRINGIFY_CODE(kMalformedDate);
      STRINGIFY_CODE(kNoTagBody);
      STRINGIFY_CODE(kConflictingDefineTags);
      STRINGIFY_CODE(kMissingRequiredSegmentInfoTrailingComma);
      STRINGIFY_CODE(kMalformedDateRange);
      STRINGIFY_CODE(kInvalidPreloadHintType);
      STRINGIFY_CODE(kInvalidDateRangeAttribute);
      STRINGIFY_CODE(kInvalidSessionDateAttribute);
      STRINGIFY_CODE(kInvalidStartAttribute);
      STRINGIFY_CODE(kInvalidMediaAttribute);
      STRINGIFY_CODE(kInvalidPartAttribute);
      STRINGIFY_CODE(kInvalidPreloadHintAttribute);
      STRINGIFY_CODE(kInvalidKeyAttribute);
      STRINGIFY_CODE(kInvalidSessionDataAttribute);
      STRINGIFY_CODE(kInvalidIFrameStreamInfAttribute);
      STRINGIFY_CODE(kInvalidContentSteeringAttribute);
      STRINGIFY_CODE(kMissingDefineAttribute);
      STRINGIFY_CODE(kMissingStreamInfAttribute);
      STRINGIFY_CODE(kMissingMapAttribute);
      STRINGIFY_CODE(kMissingPartInfAttribute);
      STRINGIFY_CODE(kMissingSkipAttribute);
      STRINGIFY_CODE(kMissingMediaAttribute);
      STRINGIFY_CODE(kMissingPartAttribute);
      STRINGIFY_CODE(kMissingPreloadHintAttribute);
      STRINGIFY_CODE(kMissingKeyAttribute);
      STRINGIFY_CODE(kMissingDateRangeAttribute);
      STRINGIFY_CODE(kMissingContentSteeringAttribute);
      STRINGIFY_CODE(kMissingSessionDataAttribute);
      STRINGIFY_CODE(kMissingIFrameStreamInfAttribute);
      STRINGIFY_CODE(kMissingStartAttribute);
      STRINGIFY_CODE(kConflictingServerControlAttributes);
      STRINGIFY_CODE(kConflictingKeyTagAttributes);
      STRINGIFY_CODE(kConflictingMediaAttributes);
    }
  }
};

#undef STRINGIFY_CODE

using ParseStatus = TypedStatus<ParseStatusTraits>;

}  // namespace media::hls

#endif  // MEDIA_FORMATS_HLS_PARSE_STATUS_H_
