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
    kMeasureLinesUntilBfcOffset,
    // The line-clamp context is enabled, but no forced truncation
    // will happen. This is different from kDisabled in that
    // `text-overflow: ellipsis` will not take effect inside it.
    kDontTruncate,
  };

  struct UntilClamp {
    bool operator==(const UntilClamp& other) const {
      return lines == other.lines && remaining_blocks == other.remaining_blocks;
    }

    // The number of lines until the clamp point.
    int lines = 0;
    // The number of remaining block boxes after the last line until the clamp
    // point. (This is needed for `line-clamp: auto`, since the clamp point
    // might be after a block with no lines.)
    int remaining_blocks = 0;
  };

  bool IsLineClampContext() const { return state != kDisabled; }

  std::optional<UntilClamp> StateUntilClamp() const {
    if (state == kClampByLines || state == kMeasureLinesUntilBfcOffset) {
      return until_clamp;
    }
    return std::optional<UntilClamp>();
  }

  // Returns nullopt when state != kClampByLines.
  std::optional<int> LinesUntilClamp() const {
    if (state == kClampByLines) {
      return until_clamp.lines;
    }
    return std::optional<int>();
  }

  bool IsAtClampPoint() const {
    return state == kClampByLines && until_clamp.lines == 1 &&
           until_clamp.remaining_blocks == 0;
  }

  bool IsPastClampPoint() const {
    return state == kClampByLines && until_clamp.lines <= 0 &&
           until_clamp.remaining_blocks <= 0;
  }

  bool ShouldHideForPaint() const {
    return RuntimeEnabledFeatures::CSSLineClampEnabled() && IsPastClampPoint();
  }

  // (Pending CSSWG discussion:
  // https://github.com/w3c/csswg-drafts/issues/10868)
  bool ShouldPositionedOofHideForPaint() const {
    if (show_positioned_oof_just_after_clamp && until_clamp.lines == 0 &&
        until_clamp.remaining_blocks == 0) {
      return false;
    }
    return ShouldHideForPaint();
  }

  bool operator==(const LineClampData& other) const {
    if (state != other.state) {
      return false;
    }
    switch (state) {
      case kClampByLines:
        return until_clamp == other.until_clamp &&
               show_positioned_oof_just_after_clamp ==
                   other.show_positioned_oof_just_after_clamp;
      case kMeasureLinesUntilBfcOffset:
        return until_clamp == other.until_clamp &&
               clamp_bfc_offset == other.clamp_bfc_offset;
      default:
        return true;
    }
  }

  // The BFC offset where the current block container should clamp.
  // (Might not be the same BFC offset as other block containers in the same
  // BFC, depending on the bottom bmp).
  // Only valid if state == kClampByBfcOffset
  LayoutUnit clamp_bfc_offset;

  // If state == kClampByLines, the number of lines and blocks until the clamp
  // point. If lines == 1 and remaining_blocks == 0, we're at the clamp point.
  // With state == kMeasureLinesUntilBfcOffset, the number of lines found in the
  // BFC so far.
  UntilClamp until_clamp;

  // If state == kClampByLines, true if positioned out-of-flow boxes immediately
  // after the clamp point are still shown. (Pending CSSWG discussion:
  // https://github.com/w3c/csswg-drafts/issues/10868)
  bool show_positioned_oof_just_after_clamp = false;

  State state = kDisabled;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_LINE_CLAMP_DATA_H_
