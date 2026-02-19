// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/gap/gap_geometry.h"

#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/platform/wtf/text/strcat.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder_stream.h"

namespace blink {

PhysicalRect GapGeometry::ComputeInkOverflowForGaps(
    WritingDirectionMode writing_direction,
    const PhysicalSize& container_size,
    LayoutUnit inline_thickness,
    LayoutUnit block_thickness) const {
  // One of the two gap lists must be non-empty. If both are empty,
  // it means there are no gaps in the container, hence we wouldn't have a
  // gap geometry.
  CHECK(!main_gaps_.empty() || !cross_gaps_.empty());

  LayoutUnit inline_start = content_inline_start_;
  LayoutUnit inline_size = content_inline_end_ - content_inline_start_;
  LayoutUnit block_start = content_block_start_;
  LayoutUnit block_size = content_block_end_ - content_block_start_;

  // Inflate the bounds to account for the gap decorations thickness.
  inline_start -= inline_thickness / 2;
  inline_size += inline_thickness;
  block_start -= block_thickness / 2;
  block_size += block_thickness;

  LogicalRect logical_rect(inline_start, block_start, inline_size, block_size);
  WritingModeConverter converter(writing_direction, container_size);
  PhysicalRect physical_rect = converter.ToPhysical(logical_rect);

  return physical_rect;
}

String GapGeometry::ToString(bool verbose) const {
  StringBuilder builder;
  builder << "MainGaps: [";
  for (const auto& main_gap : main_gaps_) {
    builder << main_gap.ToString(verbose) << ", ";
  }
  builder << "] ";
  builder << "CrossGaps: [";
  for (const auto& cross_gap : cross_gaps_) {
    builder << cross_gap.ToString(verbose) << ", ";
  }
  builder << "] ";
  return builder.ReleaseString();
}

bool GapGeometry::IsMultiColSpanner(wtf_size_t gap_index,
                                    GridTrackSizingDirection direction) const {
  if (GetContainerType() == ContainerType::kMultiColumn &&
      IsMainDirection(direction)) {
    return main_gaps_[gap_index].IsSpannerMainGap();
  }

  return false;
}

LayoutUnit GapGeometry::ComputeInsetEnd(
    const ComputedStyle& style,
    wtf_size_t gap_index,
    wtf_size_t intersection_index,
    const Vector<GapIntersection>& intersections,
    bool is_column_gap,
    bool is_main,
    LayoutUnit cross_width) const {
  // Outset values are used to offset the end points of gap decorations.
  // Percentage values are resolved against the crossing gap width of the
  // intersection point.
  // https://drafts.csswg.org/css-gaps-1/#propdef-column-rule-inset
  if (IsEdgeIntersection(gap_index, intersection_index, intersections.size(),
                         is_main, intersections)) {
    return ValueForLength((is_column_gap ? style.ColumnRuleEdgeInsetEnd()
                                         : style.RowRuleEdgeInsetEnd()),
                          cross_width);
  } else {
    return ValueForLength((is_column_gap ? style.ColumnRuleInteriorInsetEnd()
                                         : style.RowRuleInteriorInsetEnd()),
                          cross_width);
  }
}

LayoutUnit GapGeometry::ComputeInsetStart(
    const ComputedStyle& style,
    wtf_size_t gap_index,
    wtf_size_t intersection_index,
    const Vector<GapIntersection>& intersections,
    bool is_column_gap,
    bool is_main,
    LayoutUnit cross_width) const {
  // Outset values are used to offset the end points of gap decorations.
  // Percentage values are resolved against the crossing gap width of the
  // intersection point.
  // https://drafts.csswg.org/css-gaps-1/#propdef-column-rule-inset
  if (IsEdgeIntersection(gap_index, intersection_index, intersections.size(),
                         is_main, intersections)) {
    return ValueForLength((is_column_gap ? style.ColumnRuleEdgeInsetStart()
                                         : style.RowRuleEdgeInsetStart()),
                          cross_width);
  } else {
    return ValueForLength((is_column_gap ? style.ColumnRuleInteriorInsetStart()
                                         : style.RowRuleInteriorInsetStart()),
                          cross_width);
  }
}

void GapGeometry::SetContentInlineOffsets(LayoutUnit start_offset,
                                          LayoutUnit end_offset) {
  content_inline_start_ = start_offset;
  content_inline_end_ = end_offset;
}

void GapGeometry::SetContentBlockOffsets(LayoutUnit start_offset,
                                         LayoutUnit end_offset) {
  content_block_start_ = start_offset;
  content_block_end_ = end_offset;
}

LayoutUnit GapGeometry::GetGapCenterOffset(GridTrackSizingDirection direction,
                                           wtf_size_t gap_index) const {
  if (IsMainDirection(direction)) {
    LayoutUnit center = GetMainGaps()[gap_index].GetGapOffset();
    return center;
  } else {
    return direction == kForColumns
               ? GetCrossGaps()[gap_index].GetGapOffset().inline_offset
               : GetCrossGaps()[gap_index].GetGapOffset().block_offset;
  }
}

Vector<GapIntersection> GapGeometry::GenerateIntersectionListForGap(
    GridTrackSizingDirection direction,
    wtf_size_t gap_index) const {
  if (IsMainDirection(direction)) {
    return GenerateMainIntersectionList(direction, gap_index);
  }
  return GenerateCrossIntersectionList(direction, gap_index);
}

Vector<GapIntersection> GapGeometry::GenerateMainIntersectionList(
    GridTrackSizingDirection direction,
    wtf_size_t gap_index) const {
  Vector<GapIntersection> intersections;
  LayoutUnit content_start =
      direction == kForColumns ? content_block_start_ : content_inline_start_;
  intersections.push_back(GapIntersection(content_start));

  switch (GetContainerType()) {
    case ContainerType::kGrid: {
      // For grid containers:
      // - The main axis is rows, and cross gaps correspond to column gaps.
      // - Intersections occur at the inline offset of each column gap.
      CHECK_EQ(GetMainDirection(), kForRows);
      for (const auto& cross_gap : GetCrossGaps()) {
        intersections.push_back(
            GapIntersection(cross_gap.GetGapOffset().inline_offset));
      }
      break;
    }
    case ContainerType::kFlex: {
      GenerateMainIntersectionListForFlex(direction, gap_index, intersections);
      break;
    }
    case ContainerType::kMultiColumn:
      // For multicol containers, the main gaps are any gaps created by
      // `column-wrap` or by spanners. Intersections occur only at the start and
      // the end of the gap.
      CHECK_EQ(direction, kForRows);

      // `MainGap`s generated by a spanner in multicol should not be painted, as
      // they don't correspond to an actual "gap".
      if (GetMainGaps()[gap_index].IsSpannerMainGap()) {
        return Vector<GapIntersection>();
      }

      for (const auto& cross_gap : GetCrossGaps()) {
        intersections.push_back(GapIntersection(cross_gap.GetGapOffset().inline_offset));
      }

      break;
  }

  LayoutUnit content_end =
      direction == kForColumns ? content_block_end_ : content_inline_end_;
  intersections.push_back(GapIntersection(content_end));

  return intersections;
}

void GapGeometry::GenerateMainIntersectionListForFlex(
    GridTrackSizingDirection direction,
    wtf_size_t gap_index,
    Vector<GapIntersection>& intersections) const {
  MainGap main_gap = GetMainGaps()[gap_index];

  const bool has_cross_gaps_before = main_gap.HasCrossGapsBefore();
  const bool has_cross_gaps_after = main_gap.HasCrossGapsAfter();

  if (!has_cross_gaps_before && !has_cross_gaps_after) {
    return;
  }

  // TODO(samomekarajr): Consider having a util method for
  // GridTrackSizingDirection that swaps direction since it's a common
  // scenario.
  GridTrackSizingDirection cross_direction =
      direction == kForRows ? kForColumns : kForRows;

  const LayoutUnit cross_gap_size =
      direction == kForRows ? inline_gap_size_ : block_gap_size_;

  // In flexbox, cross gaps from adjacent flex lines can overlap in a
  // non-uniform fashion along the main axis. To determine where to paint gap
  // decorations, we merge the cross-gap intersection points from the lines
  // above and below the main gap into a single sorted list, tracking where
  // overlapping regions ("overlap windows") start and end.
  //
  // Each intersection is initially pushed as a preemptive open
  // (`kWindowOpen*`). If the next intersection overlaps, the open state is
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
    // Two consecutive intersections produce overlapping decorations when their
    // distance is less than `cross_gap_size`.
    //
    // TODO(samomekarajr): This needs to factor in different sized gaps when
    // that is implemented. For now, the implementation only supports uniform
    // gaps in flexbox, so this is sufficient.
    const bool overlaps_with_intersection =
        intersections.size() > 1 &&
        (intersection_offset - intersections.back().GetOffset() <
         cross_gap_size);

    if (overlaps_with_intersection) {
      if (intersections.back().IsOverlapWindowOpen()) {
        // Have entered a window: The open window state on the previous
        // intersection is confirmed. Add the current intersection as the
        // initial potential closing edge. Note that we will continue to
        // augment this intersection in place until we find the actual closing
        // edge, which can only be known when we hit an intersection that does
        // not overlap the current open window.
        intersections.push_back(GapIntersection(
            intersection_offset, is_above_main_gap
                                     ? OverlapWindowState::kWindowCloseAbove
                                     : OverlapWindowState::kWindowCloseBelow));
      } else {
        CHECK(intersections.back().IsOverlapWindowClose());
        // Interiors and possible closing edge of a window: If the current
        // intersection point overlaps the previous point(s), this means that
        // the last intersection wasn't the end of the overlap window. Update
        // the end point to the current intersection point, instead.
        intersections.back().SetOffset(intersection_offset);
        intersections.back().SetOverlapState(
            is_above_main_gap ? OverlapWindowState::kWindowCloseAbove
                              : OverlapWindowState::kWindowCloseBelow);
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
      intersections.push_back(GapIntersection(
          intersection_offset, is_above_main_gap
                                   ? OverlapWindowState::kWindowOpenAbove
                                   : OverlapWindowState::kWindowOpenBelow));
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
  // `GapIntersection`s: one with an `kWindowOpen*` state and one with a
  // `kWindowClose*` state. See `OverlapWindowState` for details on each value.
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
}

Vector<GapIntersection> GapGeometry::GenerateCrossIntersectionList(
    GridTrackSizingDirection direction,
    wtf_size_t gap_index) const {
  Vector<GapIntersection> intersections;
  switch (GetContainerType()) {
    case ContainerType::kGrid: {
      GenerateCrossIntersectionListForGrid(direction, intersections);
      break;
    }
    case ContainerType::kFlex: {
      GenerateCrossIntersectionListForFlex(direction, gap_index, intersections);
      break;
    }
    case ContainerType::kMultiColumn:
      GenerateCrossIntersectionListForMulticol(direction, gap_index,
                                               intersections);
      break;
  }

  return intersections;
}

void GapGeometry::GenerateCrossIntersectionListForGrid(
    GridTrackSizingDirection direction,
    Vector<GapIntersection>& intersections) const {
  // For a grid cross gap:
  // - Intersections include:
  // 1. The content-start edge
  // 2. The start offset of every main gap
  // 3. The content-end edge
  // - This works because grid main and cross gaps are aligned.
  intersections.ReserveInitialCapacity(main_gaps_.size() + 2);
  LayoutUnit content_start =
      direction == kForColumns ? content_block_start_ : content_inline_start_;

  intersections.push_back(GapIntersection(content_start));

  for (const auto& main_gap : GetMainGaps()) {
    intersections.push_back(GapIntersection(main_gap.GetGapOffset()));
  }

  LayoutUnit content_end =
      direction == kForColumns ? content_block_end_ : content_inline_end_;

  intersections.push_back(GapIntersection(content_end));
}

void GapGeometry::GenerateCrossIntersectionListForFlex(
    GridTrackSizingDirection direction,
    wtf_size_t gap_index,
    Vector<GapIntersection>& intersections) const {
  // For a flex cross gap:
  // - There are exactly two intersections:
  // 1. The gap's start offset
  // 2. Its computed end offset (either a main gap or the container's
  // content-end edge)
  //
  // See third_party/blink/renderer/core/layout/gap/README.md for more.
  intersections.ReserveInitialCapacity(2);
  CrossGap cross_gap = GetCrossGaps()[gap_index];
  LayoutUnit offset = direction == kForColumns
                          ? cross_gap.GetGapOffset().block_offset
                          : cross_gap.GetGapOffset().inline_offset;
  intersections.push_back(GapIntersection(offset));
  LayoutUnit end_offset_for_flex_cross_gap = ComputeEndOffsetForFlexCrossGap(
      gap_index, direction, cross_gap.EndsAtEdge());
  intersections.push_back(GapIntersection(end_offset_for_flex_cross_gap));
}

void GapGeometry::GenerateCrossIntersectionListForMulticol(
    GridTrackSizingDirection direction,
    wtf_size_t gap_index,
    Vector<GapIntersection>& intersections) const {
  // For multicol containers, the block offset of the intersections for a
  // `CrossGap` are the following:
  // - The start block offset of the cross gap.
  // - The offset of any main gaps that intersect this cross gap.
  CHECK_EQ(direction, kForColumns);

  // At most, any cross gap can intersect with all main gaps, plus the start and
  // end of the container.
  intersections.ReserveInitialCapacity(main_gaps_.size() + 2);

  CHECK_LT(gap_index, GetCrossGaps().size());
  const CrossGap cross_gap = GetCrossGaps()[gap_index];

  intersections.push_back(
      GapIntersection(cross_gap.GetGapOffset().block_offset));

  for (const auto& main_gap : GetMainGaps()) {
    intersections.push_back(GapIntersection(main_gap.GetGapOffset()));

    // We mark intersections that are adjacent to spanner main gaps as an
    // "edge". This is so that the inset applies correctly to these
    // intersections. This is because at least right now, percentage insets in
    // grid with spanners apply that percentage to the crossing gap width.
    // Intersections with spanners in multicol dont have a crossing gap, and as
    // such need to be treated as "edge" intersections which also share that
    // same property.
    if (main_gap.IsSpannerMainGap()) {
      multicol_spanner_adjacent_intersections_.insert(intersections.size() - 1);
    }
  }

  intersections.push_back(GapIntersection(content_block_end_));
}

LayoutUnit GapGeometry::ComputeEndOffsetForFlexCrossGap(
    wtf_size_t cross_gap_index,
    GridTrackSizingDirection direction,
    bool cross_gap_is_at_end) const {
  if (main_gap_running_index_ == kNotFound || cross_gap_is_at_end) {
    // If the cross gap is an end-edge gap, its end offset is the container's
    // content end.
    return direction == kForRows ? content_inline_end_ : content_block_end_;
  }

  // Determine whether the current cross gap falls before the main gap
  // currently being tracked.
  const MainGaps& main_gaps = GetMainGaps();
  CHECK_LT(main_gap_running_index_, main_gaps.size());
  wtf_size_t last_cross_before_index =
      main_gaps[main_gap_running_index_].GetCrossGapBeforeEnd();

  // If the cross gap does not fall before the currently tracked main gap,
  // advance `main_gap_running_index_` to the next main gap that has cross
  // gap(s) before it.
  if (cross_gap_index > last_cross_before_index) {
    do {
      ++main_gap_running_index_;

      if (main_gap_running_index_ == main_gaps.size()) {
        main_gap_running_index_ = kNotFound;
        return content_block_end_;
      }
      // Main gaps placed at the end of spanners don't have any cross gaps
      // associated with them, so we skip them. The same may be the case at the
      // beginning of spanners, if a spanner was pushed to the next row, so that
      // it follows a row gap.
    } while (!main_gaps[main_gap_running_index_].HasCrossGapsBefore());
  }

  CHECK_LT(main_gap_running_index_, main_gaps.size());
  return main_gaps[main_gap_running_index_].GetGapOffset();
}

bool GapGeometry::IsEdgeIntersection(
    wtf_size_t gap_index,
    wtf_size_t intersection_index,
    wtf_size_t intersection_count,
    bool is_main_gap,
    const Vector<GapIntersection>& intersections) const {
  DCHECK_GT(intersection_count, 0u);
  const wtf_size_t last_intersection_index = intersection_count - 1;
  // For flex and multicol main-axis gaps, and for grid in general, the first
  // and last intersections are considered edges.
  if (is_main_gap || GetContainerType() == ContainerType::kGrid) {
    return intersection_index == 0 ||
           intersection_index == last_intersection_index;
  }

  if (GetContainerType() == ContainerType::kFlex) {
    DCHECK(!is_main_gap);
    // For flex cross-axis gaps:
    // - First, determine the edge state of the gap (start, end, or both).
    // - Based on this state, decide which intersections qualify as edges:
    //     * kBoth: Both first and last intersections are edges.
    //     * kStart: Only the first intersection is an edge.
    //     * kEnd: Only the last intersection is an edge.
    //
    // TODO(samomekarajr): Introducing the edge state to main_gap, can avoid the
    // special logic for flex cross gaps here. We can simply check the edge
    // state of the gap to determine if the first and/or last intersection are
    // edges.
    CrossGap::EdgeIntersectionState cross_gap_edge_state =
        GetCrossGaps()[gap_index].GetEdgeIntersectionState();
    if (cross_gap_edge_state == CrossGap::EdgeIntersectionState::kBoth) {
      return intersection_index == 0 ||
             intersection_index == last_intersection_index;
    } else if (cross_gap_edge_state ==
               CrossGap::EdgeIntersectionState::kStart) {
      return intersection_index == 0;
    } else if (cross_gap_edge_state == CrossGap::EdgeIntersectionState::kEnd) {
      return intersection_index == last_intersection_index;
    }
  }

  if (GetContainerType() == ContainerType::kMultiColumn) {
    CHECK(!is_main_gap);
    // For multicol cross gaps, we may have additional edge intersections.
    // These occur when the cross gap intersects with a spanner main gap.
    return intersection_index == 0 ||
           intersection_index == last_intersection_index ||
           multicol_spanner_adjacent_intersections_.Contains(
               intersection_index);
  }

  return false;
}

GapSegmentState GapGeometry::GetIntersectionGapSegmentState(
    GridTrackSizingDirection track_direction,
    wtf_size_t primary_index,
    wtf_size_t secondary_index) const {
  const GapSegmentStateRanges* gap_segment_state_ranges = nullptr;

  if (IsMainDirection(track_direction)) {
    CHECK(primary_index < main_gaps_.size());
    if (main_gaps_[primary_index].HasGapSegmentStateRanges()) {
      gap_segment_state_ranges =
          &main_gaps_[primary_index].GetGapSegmentStateRanges();
    }
  } else {
    CHECK(primary_index < cross_gaps_.size());
    if (cross_gaps_[primary_index].HasGapSegmentStateRanges()) {
      gap_segment_state_ranges =
          &cross_gaps_[primary_index].GetGapSegmentStateRanges();
    }
  }

  // If no ranges exist for this gap, assume `kNone` (both sides
  // occupied).
  if (!gap_segment_state_ranges) {
    return GapSegmentState(GapSegmentState::kNone);
  }

  // Binary search to find the range containing secondary_index.
  // `ranges` is sorted and processed in order at paint time.
  //
  // TODO(crbug.com/440123087): Since these are access in order we could
  // potentially do it through an iterator to make this O(1).
  auto it = std::lower_bound(
      gap_segment_state_ranges->begin(), gap_segment_state_ranges->end(),
      secondary_index,
      [](const auto& range, wtf_size_t index) { return range.end <= index; });

  if (it != gap_segment_state_ranges->end() && secondary_index >= it->start &&
      secondary_index < it->end) {
    return it->state;
  }

  return GapSegmentState(GapSegmentState::kNone);
}

bool GapGeometry::IsTrackCovered(GridTrackSizingDirection track_direction,
                                 wtf_size_t primary_index,
                                 wtf_size_t secondary_index) const {
  GapSegmentState gap_state = GetIntersectionGapSegmentState(
      track_direction, primary_index, secondary_index);

  return gap_state.HasGapStatus(GapSegmentState::kBlocked);
}

BlockedStatus GapGeometry::GetIntersectionBlockedStatus(
    GridTrackSizingDirection track_direction,
    wtf_size_t primary_index,
    wtf_size_t secondary_index,
    const Vector<GapIntersection>& intersections) const {
  BlockedStatus status;

  if (secondary_index > 0 &&
      IsTrackCovered(track_direction, primary_index, secondary_index - 1)) {
    status.SetBlockedStatus(BlockedStatus::kBlockedBefore);
  }

  if (IsTrackCovered(track_direction, primary_index, secondary_index)) {
    status.SetBlockedStatus(BlockedStatus::kBlockedAfter);
  }

  return status;
}

void GapGeometry::AdjustCrossGapsRangesForFragmentation(
    wtf_size_t last_track_in_previous_fragment,
    wtf_size_t first_track_in_next_fragment,
    Vector<wtf_size_t>& column_gaps_segment_ranges_start_indices) {
  for (wtf_size_t i = 0; i < cross_gaps_.size(); ++i) {
    CrossGap& cross_gap = cross_gaps_[i];
    if (cross_gap.HasGapSegmentStateRanges()) {
      cross_gap.AdjustGapSegmentStateRangesForFragmentation(
          last_track_in_previous_fragment, first_track_in_next_fragment,
          column_gaps_segment_ranges_start_indices[i]);
    }
  }
}

bool GapGeometry::MulticolCrossGapIntersectionsEndAtSpanner(
    wtf_size_t intersection_index,
    const Vector<GapIntersection>& intersections) const {
  CHECK(GetContainerType() == ContainerType::kMultiColumn);

  // In multicol, a gap of intersections will be spanner adjacent if and only if
  // there are 3 intersections in the gap, and we are at the middle
  // intersection. This is because all multicol CrossGaps will have only 2
  // intersections, except if they are adjacent to a spanner, in which case they
  // will have 3 intersections: One at the start of the gap, one at the start of
  // the spanner, and one at the end of the spanner. The middle intersection is
  // the one that is spanner adjacent.
  return intersections.size() == 3 && intersection_index == 1;
}

}  // namespace blink
