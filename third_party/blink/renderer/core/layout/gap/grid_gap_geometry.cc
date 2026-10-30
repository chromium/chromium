// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/gap/grid_gap_geometry.h"

namespace blink {

void GridGapGeometry::AdjustCrossGapsRangesForFragmentation(
    wtf_size_t last_track_in_previous_fragment,
    wtf_size_t first_track_in_next_fragment,
    Vector<wtf_size_t>& column_gaps_segment_ranges_start_indices) {
  for (wtf_size_t i = 0; i < CrossGapCount(); ++i) {
    CrossGap& cross_gap = CrossGapAt(i);
    if (cross_gap.HasGapSegmentStateRanges()) {
      cross_gap.AdjustGapSegmentStateRangesForFragmentation(
          last_track_in_previous_fragment, first_track_in_next_fragment,
          column_gaps_segment_ranges_start_indices[i]);
    }
  }
}

void GridGapGeometry::GenerateCrossIntersectionListForGrid(
    GridTrackSizingDirection direction,
    Vector<GapIntersection>& intersections,
    GapSegmentStateCursor& cursor) const {
  // For a grid cross gap:
  // - Intersections include:
  // 1. The content-start edge
  // 2. The start offset of every main gap
  // 3. The content-end edge
  // - This works because grid main and cross gaps are aligned.
  intersections.reserve(GetMainGaps().size() + 2);
  LayoutUnit content_start = direction == kForColumns ? GetContentBlockStart()
                                                      : GetContentInlineStart();

  intersections.emplace_back(content_start, cursor.GetNextGapSegmentState());

  for (const auto& main_gap : GetMainGaps()) {
    intersections.emplace_back(main_gap.GetGapOffset(),
                               cursor.GetNextGapSegmentState());
  }

  LayoutUnit content_end =
      direction == kForColumns ? GetContentBlockEnd() : GetContentInlineEnd();

  intersections.emplace_back(content_end, cursor.GetNextGapSegmentState());
}

}  // namespace blink
