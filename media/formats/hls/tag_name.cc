// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/formats/hls/tag_name.h"
#include "base/notreached.h"

namespace media::hls {

namespace {
// Ensure that tag name enums are disjoint.
template <typename A, typename B>
constexpr bool are_disjoint() {
  return ToTagName(A::kMaxValue) < ToTagName(B::kMinValue) ||
         ToTagName(B::kMaxValue) < ToTagName(A::kMinValue);
}

static_assert(are_disjoint<CommonTagName, MultivariantPlaylistTagName>());
static_assert(are_disjoint<CommonTagName, MediaPlaylistTagName>());
static_assert(
    are_disjoint<MultivariantPlaylistTagName, MediaPlaylistTagName>());

// Ensure that the unknown tag name is disjoint with all other kinds.
static_assert(kUnknownTagName < ToTagName(CommonTagName::kMinValue));
static_assert(kUnknownTagName <
              ToTagName(MultivariantPlaylistTagName::kMinValue));
static_assert(kUnknownTagName < ToTagName(MediaPlaylistTagName::kMinValue));

}  // namespace

TagKind GetTagKind(TagName name) {
  if (name == kUnknownTagName) {
    return TagKind::kUnknown;
  }
  if (name >= ToTagName(CommonTagName::kMinValue) &&
      name <= ToTagName(CommonTagName::kMaxValue)) {
    return TagKind::kCommonTag;
  }
  if (name >= ToTagName(MultivariantPlaylistTagName::kMinValue) &&
      name <= ToTagName(MultivariantPlaylistTagName::kMaxValue)) {
    return TagKind::kMultivariantPlaylistTag;
  }
  if (name >= ToTagName(MediaPlaylistTagName::kMinValue) &&
      name <= ToTagName(MediaPlaylistTagName::kMaxValue)) {
    return TagKind::kMediaPlaylistTag;
  }

  NOTREACHED();
  return TagKind::kUnknown;
}

}  // namespace media::hls
