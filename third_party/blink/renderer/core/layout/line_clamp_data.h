// Copyright 2024 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_LINE_CLAMP_DATA_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_LINE_CLAMP_DATA_H_

#include <optional>

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/geometry/layout_unit.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {
class LayoutObject;

struct LineClampData {
  DISALLOW_NEW();

  LineClampData() = default;

  CORE_EXPORT LineClampData(const LineClampData&);

  CORE_EXPORT LineClampData& operator=(const LineClampData&);

  enum State {
    kDisabled,
    kClampByLines,
    kClampAfterLayoutObject,
    kMeasureLinesUntilBfcOffset,
    kClampByLinesWithBfcOffset,
  };

  bool IsLineClampContext() const { return state != kDisabled; }

  bool IsClampByLines() const {
    return state == kClampByLines || state == kClampByLinesWithBfcOffset;
  }
  bool IsMeasureUntilBfcOffset() const {
    return state == kMeasureLinesUntilBfcOffset ||
           state == kClampByLinesWithBfcOffset;
  }

  std::optional<int> LinesUntilClamp(bool show_measured_lines = false) const {
    if (IsClampByLines() ||
        (show_measured_lines && state == kMeasureLinesUntilBfcOffset)) {
      return lines_until_clamp;
    }
    return std::optional<int>();
  }

  bool IsAtClampPoint() const {
    return IsClampByLines() && lines_until_clamp == 1;
  }

  bool IsPastClampPoint() const {
    return IsClampByLines() && lines_until_clamp <= 0;
  }

  bool ShouldHideForPaint() const {
    return RuntimeEnabledFeatures::CSSLineClampEnabled() && IsPastClampPoint();
  }

  bool operator==(const LineClampData& other) const {
    if (state != other.state) {
      return false;
    }
    switch (state) {
      case kClampByLines:
        return lines_until_clamp == other.lines_until_clamp;
      case kClampAfterLayoutObject:
        return clamp_after_layout_object == other.clamp_after_layout_object;
      case kMeasureLinesUntilBfcOffset:
      case kClampByLinesWithBfcOffset:
        return lines_until_clamp == other.lines_until_clamp &&
               clamp_bfc_offset == other.clamp_bfc_offset;
      default:
        return true;
    }
  }

  // If state == kClampByLines or kClampByLinesWithBfcOffset, the number of
  // lines until the clamp point. A value of 1 indicates the current line should
  // be clamped. May go negative.
  // With state == kMeasureLinesUntilBfcOffset, the number of lines found in the
  // BFC so far.
  int lines_until_clamp = 0;

  // The BFC offset where the current block container should clamp.
  // (Might not be the same BFC offset as other block containers in the same
  // BFC, depending on the bottom bmp).
  // Only valid if state == kMeasureLinesUntilBfcOffset or
  // kClampByLinesWithBfcOffset.
  LayoutUnit clamp_bfc_offset;

  // A LayoutObject immediately after which the container should clamp.
  // This is used to clamp after a lineless block when clamping by a height.
  //
  // This UntracedMember should not be dereferenced, it should only ever be used
  // to compare pointer equality.
  //
  // Even though it should not be dereferenced, we don't expect LineClampData
  // objects to live past the end of the layout phase; and the LayoutObject is
  // part of the input to that phase. So we can be somewhat confident that the
  // LayoutObject won't be GC'd and therefore that its address won't be reused
  // for a different LayoutObject during the LineClampData's lifetime. So using
  // it for pointer equality should not run into false positives.
  //
  // Only valid if state == kClampAfterLayoutObject.
  UntracedMember<const LayoutObject> clamp_after_layout_object;

  State state = kDisabled;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_LINE_CLAMP_DATA_H_
