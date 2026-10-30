// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/gap/flex_gap_geometry.h"

#include <algorithm>

namespace blink {

std::optional<LayoutUnit> FlexGapGeometry::MaxFlexCrossGapSize() const {
  if (!flex_cross_gap_sizes_ || flex_cross_gap_sizes_->empty()) {
    return std::nullopt;
  }
  return *std::ranges::max_element(*flex_cross_gap_sizes_);
}

void FlexGapGeometry::GenerateMainIntersectionListForFlex(
    GridTrackSizingDirection direction,
    wtf_size_t gap_index,
    Vector<GapIntersection>& intersections,
    GapSegmentStateCursor& cursor) const {
  const MainGap& main_gap = GetMainGaps()[gap_index];

  const bool has_cross_gaps_before = main_gap.HasCrossGapsBefore();
  const bool has_cross_gaps_after = main_gap.HasCrossGapsAfter();
  const wtf_size_t num_cross_gaps_before =
      has_cross_gaps_before ? main_gap.GetCrossGapBeforeCount() : 0u;
  const wtf_size_t num_cross_gaps_after =
      has_cross_gaps_after ? main_gap.GetCrossGapAfterCount() : 0u;
  intersections.reserve(num_cross_gaps_before + num_cross_gaps_after + 2);

  LayoutUnit content_start = direction == kForColumns ? GetContentBlockStart()
                                                      : GetContentInlineStart();
  intersections.emplace_back(content_start, cursor.GetNextGapSegmentState());

  if (!has_cross_gaps_before && !has_cross_gaps_after) {
    LayoutUnit content_end =
        direction == kForColumns ? GetContentBlockEnd() : GetContentInlineEnd();
    intersections.emplace_back(content_end, cursor.GetNextGapSegmentState());
    return;
  }

  GridTrackSizingDirection cross_direction = OppositeDirection(direction);

  std::optional<LayoutUnit> cross_gap_size_above;
  if (has_cross_gaps_before) {
    cross_gap_size_above = GetFlexCrossGapSize(gap_index);
  }

  std::optional<LayoutUnit> cross_gap_size_below;
  if (has_cross_gaps_after) {
    cross_gap_size_below = GetFlexCrossGapSize(gap_index + 1);
  }

  // In flexbox, cross gaps from adjacent flex lines can overlap in a
  // non-uniform fashion along the main axis. To determine where to paint gap
  // decorations, we merge the cross-gap intersection points from the lines
  // above and below the main gap into a single sorted list, tracking where
  // overlapping regions ("overlap windows") start and end.
  //
  // Each intersection is initially pushed as a preemptive open
  // (`kWindowOpen`). If the next intersection overlaps, the open state is
  // confirmed and the new intersection is added as an initial preemptive close
  // edge. If it does not overlap, the preemptive open is cleared back to a
  // regular intersection. Interior points of the window are added as close
  // edges and updated in-place as subsequent interior points arrive until we
  // find the actual end point of the overlap window.
  //
  // Find more details about "overlap windows" in the definition of
  // `OverlapWindowState` in gap_intersection.h.
  auto ProcessCrossGapIntersection = [&](LayoutUnit intersection_offset,
                                         bool is_above_main_gap) {
    CHECK(!intersections.empty());

    // Two consecutive intersections form overlap windows when their cross gaps
    // overlap. Because intersections are placed in the middle of a cross gap,
    // we'll have to go to the end edge of the previous intersection and the
    // start edge of the current intersection to accurately determine their
    // overlap status.
    bool overlaps_with_intersection = false;
    if (intersections.size() > 1) {
      const LayoutUnit current_cross_gap_size =
          is_above_main_gap ? cross_gap_size_above.value()
                            : cross_gap_size_below.value();
      const LayoutUnit prev_cross_gap_size =
          intersections.back().IsAboveMainGap() ? cross_gap_size_above.value()
                                                : cross_gap_size_below.value();
      overlaps_with_intersection =
          (intersection_offset - intersections.back().GetOffset() <
           (prev_cross_gap_size + current_cross_gap_size) / 2);
    }

    if (overlaps_with_intersection) {
      if (intersections.back().IsOverlapWindowOpen()) {
        // Have entered a window: The open window state on the previous
        // intersection is confirmed. Add the current intersection as the
        // initial potential closing edge. Note that we will continue to
        // augment this intersection in place until we find the actual closing
        // edge, which can only be known when we hit an intersection that does
        // not overlap the current open window.
        intersections.emplace_back(intersection_offset,
                                   OverlapWindowState::kWindowClose,
                                   is_above_main_gap);
      } else {
        CHECK(intersections.back().IsOverlapWindowClose());
        // Interiors and possible closing edge of a window: If the current
        // intersection point overlaps the previous point(s), this means that
        // the last intersection wasn't the end of the overlap window. Update
        // the end point to the current intersection point, instead.
        intersections.back().SetOffset(intersection_offset);
        intersections.back().SetOverlapState(OverlapWindowState::kWindowClose);
        intersections.back().SetIsAboveMainGap(is_above_main_gap);
      }
    } else {
      if (intersections.back().IsOverlapWindowOpen()) {
        // The previous intersection point actually wasn't the start of an
        // overlap window. Reset it back to a regular intersection.
        intersections.back().ResetOverlapState();
      }

      // Add the current intersection as a potential overlap window opening. If
      // the next intersection overlaps, this will be confirmed as the open edge
      // of a new overlap window. Otherwise, it will be cleared.
      intersections.emplace_back(intersection_offset,
                                 OverlapWindowState::kWindowOpen,
                                 is_above_main_gap);
    }
  };

  wtf_size_t cross_gaps_before_current_idx =
      has_cross_gaps_before ? main_gap.GetCrossGapBeforeStart() : kNotFound;
  wtf_size_t cross_gaps_before_end_idx =
      has_cross_gaps_before ? main_gap.GetCrossGapBeforeEnd() : 0;
  wtf_size_t cross_gaps_after_current_idx =
      has_cross_gaps_after ? main_gap.GetCrossGapAfterStart() : kNotFound;
  wtf_size_t cross_gaps_after_end_idx =
      has_cross_gaps_after ? main_gap.GetCrossGapAfterEnd() : 0;

  // Merge the cross gaps before and after the main gap into `intersections`,
  // ordered by offset. Intersections that don't overlap or overlap uniformly
  // are represented by a single `GapIntersection` with no overlap state.
  // Non-uniform overlaps produce an overlap window bounded by two
  // `GapIntersection`s: one with an `kWindowOpen` state and one with a
  // `kWindowClose` state. See `OverlapWindowState` for details on each value.
  while (cross_gaps_before_current_idx <= cross_gaps_before_end_idx &&
         cross_gaps_after_current_idx <= cross_gaps_after_end_idx) {
    LayoutUnit cross_gap_before_offset =
        GetCrossGaps()[cross_gaps_before_current_idx].GetGapOffset(
            cross_direction);
    LayoutUnit cross_gap_after_offset =
        GetCrossGaps()[cross_gaps_after_current_idx].GetGapOffset(
            cross_direction);

    // "before"/"after" indicates which side of the main gap the cross gap
    // belongs to, not its position along the main axis. A cross gap from
    // the "before" list can occur after one from the "after" list along
    // the main axis, which is why we compare offsets to merge them in a sorted
    // manner.
    if (cross_gap_before_offset <= cross_gap_after_offset) {
      ProcessCrossGapIntersection(cross_gap_before_offset,
                                  /*is_above_main_gap=*/true);
      ++cross_gaps_before_current_idx;

      // If both lists have the same offset, advance both pointers.
      if (cross_gap_before_offset == cross_gap_after_offset) {
        ++cross_gaps_after_current_idx;
      }
    } else {
      ProcessCrossGapIntersection(cross_gap_after_offset,
                                  /*is_above_main_gap=*/false);
      ++cross_gaps_after_current_idx;
    }
  }

  // Process intersections for whichever list still has remaining elements.
  while (cross_gaps_before_current_idx <= cross_gaps_before_end_idx) {
    ProcessCrossGapIntersection(
        GetCrossGaps()[cross_gaps_before_current_idx].GetGapOffset(
            cross_direction),
        /*is_above_main_gap=*/true);
    ++cross_gaps_before_current_idx;
  }
  while (cross_gaps_after_current_idx <= cross_gaps_after_end_idx) {
    ProcessCrossGapIntersection(
        GetCrossGaps()[cross_gaps_after_current_idx].GetGapOffset(
            cross_direction),
        /*is_above_main_gap=*/false);
    ++cross_gaps_after_current_idx;
  }

  // If the last intersection was marked as a potential open window, reset it
  // back to a regular intersection because it didn't overlap with any other
  // intersection point. If it is a close edge, the overlap window is already
  // properly ended.
  if (intersections.back().IsOverlapWindowOpen()) {
    intersections.back().ResetOverlapState();
  }

  LayoutUnit content_end =
      direction == kForColumns ? GetContentBlockEnd() : GetContentInlineEnd();
  intersections.emplace_back(content_end, cursor.GetNextGapSegmentState());
}

void FlexGapGeometry::GenerateCrossIntersectionListForFlex(
    GridTrackSizingDirection direction,
    wtf_size_t gap_index,
    Vector<GapIntersection>& intersections,
    GapSegmentStateCursor& cursor,
    wtf_size_t main_gap_index) const {
  // For a flex cross gap:
  // - There are exactly two intersections:
  // 1. The gap's start offset
  // 2. Its computed end offset (either a main gap or the container's
  // content-end edge)
  //
  // Each intersection carries an optional `main_gap_index` that identifies its
  // associated main gap. Edge intersections bordering the container remain
  // `std::nullopt`.
  //
  // See third_party/blink/renderer/core/layout/gap/README.md for more.
  intersections.reserve(2);
  const CrossGap& cross_gap = GetCrossGaps()[gap_index];
  LayoutUnit offset = direction == kForColumns
                          ? cross_gap.GetGapOffset().block_offset
                          : cross_gap.GetGapOffset().inline_offset;
  intersections.emplace_back(offset, cursor.GetNextGapSegmentState());
  LayoutUnit end_offset_for_flex_cross_gap = ComputeEndOffsetForFlexCrossGap(
      direction, cross_gap.EndsAtEdge(), main_gap_index);
  intersections.emplace_back(end_offset_for_flex_cross_gap,
                             cursor.GetNextGapSegmentState());

  // Each flex cross gap intersection needs to know which main gap it borders
  // so that `overlap-join` can look up the correct cross-direction decoration
  // width. Edge intersections (those bordering the container edge) have no
  // associated main gap and remain unset. For middle cross gaps, the start
  // intersection borders the main gap that precedes the current flex line,
  // while the end intersection borders the main gap that follows it. When the
  // cross gap touches the last flex line, the start intersection references
  // the final main gap.
  const CrossGap::EdgeIntersectionState edge_state =
      cross_gap.GetEdgeIntersectionState();

  // Set `main_gap_index` for the start of the cross gap. For flex, there are
  // always 2 intersections for each cross gap, one at the start and one at the
  // end.
  const bool is_start_edge =
      edge_state == CrossGap::EdgeIntersectionState::kStart ||
      edge_state == CrossGap::EdgeIntersectionState::kBoth;
  if (!is_start_edge) {
    intersections[0].SetMainGapIndex(
        edge_state == CrossGap::EdgeIntersectionState::kEnd
            ? GetMainGaps().size() - 1
            : main_gap_index - 1);
  }

  // Set `main_gap_index` for the end of the cross gap.
  const bool is_end_edge =
      edge_state == CrossGap::EdgeIntersectionState::kEnd ||
      edge_state == CrossGap::EdgeIntersectionState::kBoth;
  if (!is_end_edge && main_gap_index < GetMainGaps().size()) {
    intersections[1].SetMainGapIndex(main_gap_index);
  }
}

LayoutUnit FlexGapGeometry::ComputeEndOffsetForFlexCrossGap(
    GridTrackSizingDirection direction,
    bool cross_gap_is_at_end,
    wtf_size_t main_gap_index) const {
  const MainGaps& main_gaps = GetMainGaps();

  DCHECK_LE(main_gap_index, main_gaps.size());

  // `main_gap_index` identifies the main gap where this cross gap ends,
  // or equals `main_gaps.size()` when the cross gap ends at the content edge.
  if (main_gap_index == main_gaps.size() || cross_gap_is_at_end) {
    // If the cross gap is an end-edge gap, its end offset is the container's
    // content end.
    return direction == kForRows ? GetContentInlineEnd() : GetContentBlockEnd();
  }

  return main_gaps[main_gap_index].GetGapOffset();
}

}  // namespace blink
