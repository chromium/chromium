// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/gap/multicol_gap_geometry.h"

namespace blink {

void MulticolGapGeometry::GenerateCrossIntersectionListForMulticol(
    GridTrackSizingDirection direction,
    wtf_size_t gap_index,
    Vector<GapIntersection>& intersections,
    GapSegmentStateCursor& cursor) const {
  CHECK_EQ(direction, kForColumns);

  // At most, any cross gap can intersect with all main gaps, plus the start and
  // end of the container.
  intersections.reserve(GetMainGaps().size() + 2);

  CHECK_LT(gap_index, GetCrossGaps().size());
  const CrossGap& cross_gap = GetCrossGaps()[gap_index];

  intersections.emplace_back(cross_gap.GetGapOffset().block_offset,
                             cursor.GetNextGapSegmentState());

  for (const auto& main_gap : GetMainGaps()) {
    intersections.emplace_back(main_gap.GetGapOffset(),
                               cursor.GetNextGapSegmentState());
  }

  intersections.emplace_back(GetContentBlockEnd(),
                             cursor.GetNextGapSegmentState());
}

}  // namespace blink
