// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_GAP_MAIN_GAP_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_GAP_MAIN_GAP_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/gap/cross_gap.h"
#include "third_party/blink/renderer/platform/geometry/layout_unit.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

// Represents the gap in the primary axis. For example, in a row-based flex
// container the MainGaps represent the gaps between flex lines, while the
// CrossGaps represent the gaps between flex items in the same line. See
// third_party/blink/renderer/core/layout/gap/README.md for more information.
class CORE_EXPORT MainGap {
 public:
  MainGap(LayoutUnit offset) : gap_start_offset_(offset) {}

  LayoutUnit GetGapStartOffset() const { return gap_start_offset_; }

  CrossGapRange& RangeOfCrossGapsBefore() {
    return range_of_cross_gaps_before_;
  }

  CrossGapRange& RangeOfCrossGapsAfter() { return range_of_cross_gaps_after_; }

  blink::String ToString(bool verbose = false) const {
    blink::String str =
        blink::String("MainOffset(") + gap_start_offset_.ToString() + "); ";

    if (verbose) {
      str = str + "Before: " +
            blink::String::Number(range_of_cross_gaps_before_.start_index) +
            " -> " +
            blink::String::Number(range_of_cross_gaps_before_.end_index) + "; ";
      str = str + "After: " +
            blink::String::Number(range_of_cross_gaps_after_.start_index) +
            " -> " +
            blink::String::Number(range_of_cross_gaps_after_.end_index) + "; ";
    }

    return str;
  }

 private:
  // This represents the offset (block or inline) of the start point for the
  // gap. If the main direction is row it'll be the block offset otherwise
  // it'll be the inline.
  LayoutUnit gap_start_offset_;

  // In Grid, because rows and columns neatly align, we can avoid duplication by
  // storing cross gaps once and share them across all main gaps. As a result,
  // each main gap can be mapped to all cross gaps. Unlike Grid, each flex line
  // will have independent intersections introduced by the item flow. As such,
  // we cannot share cross axis gap intersections across gaps in the main axis.
  // As a result, each main gap is mapped to cross gaps that intersect it (i.e.
  // falling either before or after that main gap).
  CrossGapRange range_of_cross_gaps_before_;
  CrossGapRange range_of_cross_gaps_after_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_GAP_MAIN_GAP_H_
