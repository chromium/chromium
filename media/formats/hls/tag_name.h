// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FORMATS_HLS_TAG_NAME_H_
#define MEDIA_FORMATS_HLS_TAG_NAME_H_

#include <cstdint>

#include "base/strings/string_piece.h"
#include "media/base/media_export.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

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
enum class CommonTagName : TagName {
  kMinValue = 0,
  kM3u = kMinValue,
  kXVersion,
  kXIndependentSegments,
  kXDefine,
  kMaxValue = kXDefine,
};

// Tags exclusive to multivariant playlists.
enum class MultivariantPlaylistTagName : TagName {
  kMinValue = static_cast<TagName>(CommonTagName::kMaxValue) + 1,
  kXMedia = kMinValue,
  kXStreamInf,
  kXIFrameStreamInf,
  kXSessionData,
  kXSessionKey,
  kXContentSteering,
  kMaxValue = kXContentSteering,
};

// Tags exclusive to media playlists.
enum class MediaPlaylistTagName : TagName {
  kMinValue = static_cast<TagName>(MultivariantPlaylistTagName::kMaxValue) + 1,
  kInf = kMinValue,
  kXTargetDuration,
  kXEndList,
  kXIFramesOnly,
  kXDiscontinuity,
  kXGap,
  kXPlaylistType,
  kXMediaSequence,
  kMaxValue = kXMediaSequence,
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
// If the tag is not recognized, returns `absl::nullopt`.
MEDIA_EXPORT absl::optional<TagName> ParseTagName(base::StringPiece name);

// Prints the corresponding string representation of the given `TagName`.
MEDIA_EXPORT base::StringPiece TagNameToString(TagName name);

constexpr TagName kMinTagName = ToTagName(CommonTagName::kMinValue);
constexpr TagName kMaxTagName = ToTagName(MediaPlaylistTagName::kMaxValue);

}  // namespace media::hls

#endif  // MEDIA_FORMATS_HLS_TAG_NAME_H_
