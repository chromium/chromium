// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FORMATS_HLS_TAG_NAME_H_
#define MEDIA_FORMATS_HLS_TAG_NAME_H_

#include <cstdint>
#include "media/base/media_export.h"

namespace media::hls {

enum class TagKind {
  kUnknown,
  kCommonTag,
  kMultivariantPlaylistTag,
  kMediaPlaylistTag,
  kMaxValue = kMediaPlaylistTag,
};

// Underlying type for all `k*TagName` constants. Each name constant must be
// unique across all kinds.
using TagName = uint32_t;

static constexpr TagName kUnknownTagName = 0;

// Tags common to multivariant and media playlists.
enum class CommonTagName : TagName {
  kMinValue = kUnknownTagName + 1,
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
  kXEndList,
  kXIFramesOnly,
  kXDiscontinuity,
  kXGap,
  kMaxValue = kXGap,
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

TagKind MEDIA_EXPORT GetTagKind(TagName name);

// No-op conversion function for generic code.
constexpr TagName ToTagName(TagName name) {
  return name;
}

}  // namespace media::hls

#endif  // MEDIA_FORMATS_HLS_TAG_NAME_H_
