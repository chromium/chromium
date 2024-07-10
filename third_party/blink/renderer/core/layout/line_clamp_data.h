// Copyright 2024 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_LINE_CLAMP_DATA_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_LINE_CLAMP_DATA_H_

#include <optional>

#include "third_party/blink/renderer/platform/geometry/layout_unit.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {

struct LineClampData {
  DISALLOW_NEW();

  LineClampData() {}

  enum State {
    kDisabled,
    kClampByLines,
    kClampByBfcOffset,
    // The line-clamp context is enabled, but no forced truncation
    // will happen. This is different from kDisabled in that
    // `text-overflow: ellipsis` will not take effect inside it.
    kDontTruncate,
  };

  bool IsLineClampContext() const { return state != kDisabled; }

  std::optional<int> LinesUntilClamp() const {
    if (state == kClampByLines) {
      return lines_until_clamp;
    }
    return std::optional<int>();
  }

  bool IsAtClampPoint(LayoutUnit bfc_offset) const {
    switch (state) {
      case kClampByLines:
        return lines_until_clamp == 1;
      case kClampByBfcOffset:
        return clamp_bfc_offset == bfc_offset;
      default:
        return false;
    }
  }

  bool IsPastClampPoint(LayoutUnit bfc_offset, bool is_float = false) const {
    switch (state) {
      case kClampByLines:
        return lines_until_clamp <= 0;
      case kClampByBfcOffset:
        if (is_float) {
          return clamp_bfc_offset <= bfc_offset;
        }
        return clamp_bfc_offset < bfc_offset;
      default:
        return false;
    }
  }

  bool ShouldHideForPaint(LayoutUnit bfc_offset, bool is_float = false) const {
    return RuntimeEnabledFeatures::CSSLineClampEnabled() &&
           IsPastClampPoint(bfc_offset, is_float);
  }

  bool operator==(const LineClampData& other) const {
    if (state != other.state) {
      return false;
    }
    switch (state) {
      case kClampByLines:
        return lines_until_clamp == other.lines_until_clamp;
      case kClampByBfcOffset:
        return clamp_bfc_offset == other.clamp_bfc_offset;
      default:
        return true;
    }
  }

  union {
    // The number of lines until the clamp point. A value of 1 indicates the
    // current line should be clamped. This may go negative.
    // Only valid if state == kClampByLines
    int lines_until_clamp;

    // The BFC offset where the current block container should clamp.
    // (Might not be the same BFC offset as other block containers in the same
    // BFC, depending on the bottom bmp).
    // Only valid if state == kClampByBfcOffset
    LayoutUnit clamp_bfc_offset;
  };

  State state = kDisabled;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_LINE_CLAMP_DATA_H_
