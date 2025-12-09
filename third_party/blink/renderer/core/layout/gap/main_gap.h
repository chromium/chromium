// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_GAP_MAIN_GAP_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_GAP_MAIN_GAP_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/gap/cross_gap.h"
#include "third_party/blink/renderer/core/layout/gap/gap_utils.h"
#include "third_party/blink/renderer/platform/geometry/layout_unit.h"

namespace blink {

// Represents the type of a `MainGap` created by a multicol spanner.
// * `kStart`: The gap is at the start of the spanner.
// * `kEnd`: The gap is at the end of the spanner.
// * `kNone`: The gap is not associated with a spanner.
enum class SpannerMainGapType { kStart, kEnd, kNone };

// Represents the gap in the primary axis. For example, in a row-based flex
// container the MainGaps represent the gaps between flex lines, while the
// CrossGaps represent the gaps between flex items in the same line. See
// third_party/blink/renderer/core/layout/gap/README.md for more information.
class CORE_EXPORT MainGap {
 public:
  MainGap() = default;
  MainGap(LayoutUnit offset,
          SpannerMainGapType spanner_main_gap_type = SpannerMainGapType::kNone)
      : gap_offset_(offset), spanner_main_gap_type_(spanner_main_gap_type) {}

  // This copy-esque constructor allows creating a new MainGap instance based on
  // an existing one, while replacing the gap offset. This is useful for
  // fragmentation where most states remain the same, but the gap offset may
  // differ between fragments.
  MainGap(const MainGap& other, LayoutUnit new_offset)
      : gap_offset_(new_offset),
        range_of_cross_gaps_before_(other.range_of_cross_gaps_before_),
        range_of_cross_gaps_after_(other.range_of_cross_gaps_after_),
        gap_segment_state_ranges_(other.gap_segment_state_ranges_),
        spanner_main_gap_type_(other.spanner_main_gap_type_) {}

  void SetGapOffset(LayoutUnit offset) { gap_offset_ = offset; }
  LayoutUnit GetGapOffset() const { return gap_offset_; }

  bool HasCrossGapsBefore() const {
    return range_of_cross_gaps_before_.IsValid();
  }

  bool HasCrossGapsAfter() const {
    return range_of_cross_gaps_after_.IsValid();
  }

  wtf_size_t GetCrossGapBeforeStart() const;
  wtf_size_t GetCrossGapBeforeEnd() const;
  wtf_size_t GetCrossGapAfterStart() const;
  wtf_size_t GetCrossGapAfterEnd() const;

  void IncrementRangeOfCrossGapsBefore(wtf_size_t cross_gap_index) {
    range_of_cross_gaps_before_.Increment(cross_gap_index);
  }

  void IncrementRangeOfCrossGapsAfter(wtf_size_t cross_gap_index) {
    range_of_cross_gaps_after_.Increment(cross_gap_index);
  }

  void SetRangeOfCrossGapsBefore(const CrossGapRange& range) {
    range_of_cross_gaps_before_ = range;
  }
  const CrossGapRange& RangeOfCrossGapsBefore() const {
    return range_of_cross_gaps_before_;
  }

  void SetRangeOfCrossGapsAfter(const CrossGapRange& range) {
    range_of_cross_gaps_after_ = range;
  }

  blink::String ToString(bool verbose = false) const;

  bool IsStartSpannerMainGap() const {
    return spanner_main_gap_type_ == SpannerMainGapType::kStart;
  }
  bool IsEndSpannerMainGap() const {
    return spanner_main_gap_type_ == SpannerMainGapType::kEnd;
  }
  bool IsSpannerMainGap() const {
    return spanner_main_gap_type_ != SpannerMainGapType::kNone;
  }

  bool HasGapSegmentStateRanges() const {
    return gap_segment_state_ranges_.has_value();
  }

  const GapSegmentStateRanges& GetGapSegmentStateRanges() const;

  void AddGapSegmentStateRange(
      const GapSegmentStateRange& gap_segment_state_range);

 private:
  // This represents the midpoint offset (block or inline) of the gap. If the main
  // direction is row it'll be the block offset otherwise it'll be the inline.
  LayoutUnit gap_offset_;

  // In Grid, because rows and columns neatly align, we can avoid duplication by
  // storing cross gaps once and share them across all main gaps. As a result,
  // each main gap can be mapped to all cross gaps. Unlike Grid, each flex line
  // will have independent intersections introduced by the item flow. As such,
  // we cannot share cross axis gap intersections across gaps in the main axis.
  // As a result, each main gap is mapped to cross gaps that intersect it (i.e.
  // falling either before or after that main gap).
  CrossGapRange range_of_cross_gaps_before_;
  CrossGapRange range_of_cross_gaps_after_;

  // If present, holds slices of this main gap, each with a `GapSegmentState`
  // (Blocked / Empty). A main gap usually spans range [1, N) in one piece, but
  // the presence of spanning items or empty cells can break it into multiple
  // state-specific sub‑ranges.
  std::optional<GapSegmentStateRanges> gap_segment_state_ranges_;

  // Only used for multicol.
  SpannerMainGapType spanner_main_gap_type_ = SpannerMainGapType::kNone;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_GAP_MAIN_GAP_H_
