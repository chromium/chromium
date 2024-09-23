// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FORMATS_HLS_TAG_NAME_H_
#define MEDIA_FORMATS_HLS_TAG_NAME_H_

#include <cstdint>
#include <optional>
#include <string_view>

#include "media/base/media_export.h"

namespace media::hls {

enum class TagKind {
  kMinValue = 0,
  kCommonTag = kMinValue,
  kMultivariantPlaylistTag,
  kMediaPlaylistTag,
  kMaxValue = kMediaPlaylistTag,
};

// Underlying type for all `k*TagName` constants. Each name constant must be
// unique across all kinds, and the full set of constants must be contiguous.
using TagName = uint32_t;

// Tags common to multivariant and media playlists.
// For organization, this list is in alphabetical order.
enum class CommonTagName : TagName {
  kMinValue = 0,
  kM3u = kMinValue,
  kXDefine,
  kXIndependentSegments,
  kXStart,
  kXVersion,
  kMaxValue = kXVersion,
};

// Tags exclusive to multivariant playlists.
// For organization, this list is in alphabetical order.
enum class MultivariantPlaylistTagName : TagName {
  kMinValue = static_cast<TagName>(CommonTagName::kMaxValue) + 1,
  kXContentSteering = kMinValue,
  kXIFrameStreamInf,
  kXMedia,
  kXSessionData,
  kXSessionKey,
  kXStreamInf,
  kMaxValue = kXStreamInf,
};

// Tags exclusive to media playlists.
// For organization, this list is in alphabetical order.
enum class MediaPlaylistTagName : TagName {
  kMinValue = static_cast<TagName>(MultivariantPlaylistTagName::kMaxValue) + 1,
  kInf = kMinValue,
  kXBitrate,
  kXByteRange,
  kXDateRange,
  kXDiscontinuity,
  kXDiscontinuitySequence,
  kXEndList,
  kXGap,
  kXIFramesOnly,
  kXKey,
  kXMap,
  kXMediaSequence,
  kXPart,
  kXPartInf,
  kXPlaylistType,
  kXPreloadHint,
  kXProgramDateTime,
  kXRenditionReport,
  kXServerControl,
  kXSkip,
  kXTargetDuration,
  kMaxValue = kXTargetDuration,
};

constexpr TagKind GetTagKind(CommonTagName) {
  return TagKind::kCommonTag;
}

constexpr TagName ToTagName(CommonTagName name) {
  return static_cast<TagName>(name);
}

constexpr TagKind GetTagKind(MultivariantPlaylistTagName) {
  return TagKind::kMultivariantPlaylistTag;
}

constexpr TagName ToTagName(MultivariantPlaylistTagName name) {
  return static_cast<TagName>(name);
}

constexpr TagKind GetTagKind(MediaPlaylistTagName) {
  return TagKind::kMediaPlaylistTag;
}

constexpr TagName ToTagName(MediaPlaylistTagName name) {
  return static_cast<TagName>(name);
}

MEDIA_EXPORT TagKind GetTagKind(TagName name);

// No-op conversion function for generic code.
constexpr TagName ToTagName(TagName name) {
  return name;
}

// Parses the tag name, converting it to one of the `*TagName` enum values.
// If the tag is not recognized, returns `std::nullopt`.
MEDIA_EXPORT std::optional<TagName> ParseTagName(std::string_view name);

// Prints the corresponding string representation of the given `TagName`.
MEDIA_EXPORT std::string_view TagNameToString(TagName name);

constexpr TagName kMinTagName = ToTagName(CommonTagName::kMinValue);
constexpr TagName kMaxTagName = ToTagName(MediaPlaylistTagName::kMaxValue);

}  // namespace media::hls

#endif  // MEDIA_FORMATS_HLS_TAG_NAME_H_
