// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GEOMETRY_ANCHOR_QUERY_ENUMS_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GEOMETRY_ANCHOR_QUERY_ENUMS_H_

namespace blink {

// Enum types for CSS anchor() and anchor-size() functions.
// See https://drafts.csswg.org/css-anchor-1/#anchoring

// TODO(crbug.com/1309178): Combine this with `CSSAnchorQueryType`.
enum class AnchorQueryType {
  kAnchor,
  kAnchorSize,
};

// TODO(crbug.com/1309178): We currently keep all keywords as is in the computed
// value of anchor(), but may try to simplify it (e.g., resolve logical
// keywords) in the future.
enum class AnchorValue {
  kTop,
  kLeft,
  kRight,
  kBottom,
  kStart,
  kEnd,
  kSelfStart,
  kSelfEnd,
  kCenter,
  kPercentage,
};

// TODO(crbug.com/1309178): We currently keep all keywords as is in the computed
// value of anchor-size(), but may try to simplify it (e.g., resolve logical
// keywords) in the future.
enum class AnchorSizeValue {
  kWidth,
  kHeight,
  kBlock,
  kInline,
  kSelfBlock,
  kSelfInline,
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GEOMETRY_ANCHOR_QUERY_ENUMS_H_
