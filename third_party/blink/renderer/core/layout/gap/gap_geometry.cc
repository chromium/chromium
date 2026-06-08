// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/gap/gap_geometry.h"

#include <algorithm>

#include "third_party/blink/renderer/core/css/css_gap_decoration_property_utils.h"
#include "third_party/blink/renderer/core/layout/gap/gap_utils.h"
#include "third_party/blink/renderer/core/layout/physical_box_fragment.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/platform/wtf/text/strcat.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder_stream.h"

namespace blink {

bool GapGeometry::HasRowGapFragmentation(
    const PhysicalBoxFragment& box_fragment,
    bool is_main) const {
  if (box_fragment.IsOnlyForNode()) {
    return false;
  }

  // For grid, fragmentation only affects grid rows gaps indices (i.e. main
  // gaps).
  if (container_type_ == ContainerType::kGrid) {
    return is_main;
  }

  // TODO(samomekarajr): Implement for flex and multicol in a follow-up CL.
  return false;
}

PhysicalRect GapGeometry::ComputeInkOverflowForGaps(
    WritingDirectionMode writing_direction,
    const PhysicalSize& container_size,
    LayoutUnit inline_thickness,
    LayoutUnit block_thickness,
    const GapDecorationInkOutsets& outsets) const {
  // One of the two gap lists must be non-empty. If both are empty,
  // it means there are no gaps in the container, hence we wouldn't have a
  // gap geometry.
  CHECK(!main_gaps_.empty() || !cross_gaps_.empty());

  LayoutUnit inline_start = content_inline_start_;
  LayoutUnit inline_size = content_inline_end_ - content_inline_start_;
  LayoutUnit block_start = content_block_start_;
  LayoutUnit block_size = content_block_end_ - content_block_start_;

  // Inflate the bounds to account for the gap decorations thickness and any
  // negative insets that push decorations past the content box edges.
  inline_start -= inline_thickness / 2 + outsets.inline_start;
  inline_size += inline_thickness + outsets.InlineOutsetThickness();
  block_start -= block_thickness / 2 + outsets.block_start;
  block_size += block_thickness + outsets.BlockOutsetThickness();

  LogicalRect logical_rect(inline_start, block_start, inline_size, block_size);
  WritingModeConverter converter(writing_direction, container_size);
  PhysicalRect physical_rect = converter.ToPhysical(logical_rect);

  return physical_rect;
}

LayoutUnit GapGeometry::GetCrossingGapSize(
    GridTrackSizingDirection direction) const {
  // Column rules cross row gaps; row rules cross column gaps.
  const LayoutUnit base_size =
      direction == kForColumns ? block_gap_size_ : inline_gap_size_;

  if (container_type_ != ContainerType::kFlex || !IsMainDirection(direction) ||
      !flex_cross_gap_sizes_ || flex_cross_gap_sizes_->empty()) {
    return base_size;
  }

  // For flex containers, per-line cross gap sizes can differ due to content
  // distribution. Use the max across all lines for a conservative bound.
  return std::max(base_size, *std::ranges::max_element(*flex_cross_gap_sizes_));
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
    bool is_cap_intersection,
    bool is_column_gap,
    bool is_main,
    bool has_joining_decoration,
    LayoutUnit cross_gap_width,
    LayoutUnit cross_decoration_width) const {
  // Inset values are used to offset the end points of gap decorations.
  // Percentage values are resolved against the crossing gap width of the
  // intersection point.
  // https://drafts.csswg.org/css-gaps-1/#propdef-column-rule-inset
  const Length& inset =
      is_cap_intersection ? (is_column_gap ? style.ColumnRuleInsetCapEnd()
                                           : style.RowRuleInsetCapEnd())
                          : (is_column_gap ? style.ColumnRuleInsetJunctionEnd()
                                           : style.RowRuleInsetJunctionEnd());

  if (inset.IsOverlapJoin()) {
    return ComputeOverlapJoinInset(has_joining_decoration, is_main,
                                   cross_gap_width, cross_decoration_width);
  }
  return ValueForLength(inset, cross_gap_width);
}

LayoutUnit GapGeometry::ComputeInsetStart(
    const ComputedStyle& style,
    wtf_size_t gap_index,
    wtf_size_t intersection_index,
    const Vector<GapIntersection>& intersections,
    bool is_cap_intersection,
    bool is_column_gap,
    bool is_main,
    bool has_joining_decoration,
    LayoutUnit cross_gap_width,
    LayoutUnit cross_decoration_width) const {
  // Inset values are used to offset the end points of gap decorations.
  // Percentage values are resolved against the crossing gap width of the
  // intersection point.
  // https://drafts.csswg.org/css-gaps-1/#propdef-column-rule-inset
  const Length& inset =
      is_cap_intersection
          ? (is_column_gap ? style.ColumnRuleInsetCapStart()
                           : style.RowRuleInsetCapStart())
          : (is_column_gap ? style.ColumnRuleInsetJunctionStart()
                           : style.RowRuleInsetJunctionStart());

  if (inset.IsOverlapJoin()) {
    return ComputeOverlapJoinInset(has_joining_decoration, is_main,
                                   cross_gap_width, cross_decoration_width);
  }
  return ValueForLength(inset, cross_gap_width);
}

LayoutUnit GapGeometry::ComputeOverlapJoinInset(
    bool has_joining_decoration,
    bool is_main,
    LayoutUnit cross_gap_width,
    LayoutUnit cross_decoration_width) const {
  if (!has_joining_decoration) {
    return LayoutUnit();
  }

  // For flex and multicol main-direction gaps, main gaps don't overlap with
  // the cross gap, so resolve as -50% of the cross gap width.
  if (is_main && (GetContainerType() == ContainerType::kFlex ||
                  GetContainerType() == ContainerType::kMultiColumn)) {
    return -cross_gap_width / 2;
  }

  // For grid and flex/multicol cross-direction gaps, cross gaps can overlap
  // with main gap(s), so extend by half the cross gap width plus half the cross
  // decoration width.
  return (-cross_gap_width / 2) - (cross_decoration_width / 2);
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

void GapGeometry::GenerateIntersectionListForGap(
    GridTrackSizingDirection direction,
    wtf_size_t gap_index,
    Vector<GapIntersection>& intersections) const {
  // Reset the buffer's logical size but keep capacity, so we can reuse
  // a single Vector across loop iterations without reallocating.
  intersections.Shrink(0);
  if (IsMainDirection(direction)) {
    GenerateMainIntersectionList(direction, gap_index, intersections);
  } else {
    GenerateCrossIntersectionList(direction, gap_index, intersections);
  }
}

void GapGeometry::GenerateMainIntersectionList(
    GridTrackSizingDirection direction,
    wtf_size_t gap_index,
    Vector<GapIntersection>& intersections) const {
  GapSegmentStateCursor cursor(
      GetGapSegmentStateRangesForGap(direction, gap_index));
  // Multicol spanner main gaps don't correspond to a paintable gap.
  if (GetContainerType() == ContainerType::kMultiColumn) {
    CHECK_EQ(direction, kForRows);
    if (GetMainGaps()[gap_index].IsSpannerMainGap()) {
      return;
    }
  }

  intersections.reserve(GetCrossGaps().size() + 2);

  LayoutUnit content_start =
      direction == kForColumns ? content_block_start_ : content_inline_start_;
  intersections.emplace_back(content_start, cursor.GetNextGapSegmentState());

  switch (GetContainerType()) {
    case ContainerType::kGrid:
    case ContainerType::kMultiColumn:
      // For grid, the main axis is rows and intersections occur at the inline
      // offset of each column gap. For multicol, the main gaps are created by
      // `column-wrap` or by spanners (spanner main gaps were filtered above);
      // intersections occur at the inline offset of each cross gap. In both
      // cases the cross-gap inline offsets give the interior intersections.
      CHECK_EQ(GetMainDirection(), kForRows);
      for (const auto& cross_gap : GetCrossGaps()) {
        intersections.emplace_back(cross_gap.GetGapOffset().inline_offset,
                                   cursor.GetNextGapSegmentState());
      }
      break;
    case ContainerType::kFlex:
      GenerateMainIntersectionListForFlex(direction, gap_index, intersections);
      break;
  }

  LayoutUnit content_end =
      direction == kForColumns ? content_block_end_ : content_inline_end_;
  intersections.emplace_back(content_end, cursor.GetNextGapSegmentState());
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
}

void GapGeometry::GenerateCrossIntersectionList(
    GridTrackSizingDirection direction,
    wtf_size_t gap_index,
    Vector<GapIntersection>& intersections) const {
  GapSegmentStateCursor cursor(
      GetGapSegmentStateRangesForGap(direction, gap_index));
  switch (GetContainerType()) {
    case ContainerType::kGrid: {
      GenerateCrossIntersectionListForGrid(direction, intersections, cursor);
      break;
    }
    case ContainerType::kFlex: {
      GenerateCrossIntersectionListForFlex(direction, gap_index, intersections,
                                           cursor);
      break;
    }
    case ContainerType::kMultiColumn:
      GenerateCrossIntersectionListForMulticol(direction, gap_index,
                                               intersections, cursor);
      break;
  }
}

void GapGeometry::GenerateCrossIntersectionListForGrid(
    GridTrackSizingDirection direction,
    Vector<GapIntersection>& intersections,
    GapSegmentStateCursor& cursor) const {
  // For a grid cross gap:
  // - Intersections include:
  // 1. The content-start edge
  // 2. The start offset of every main gap
  // 3. The content-end edge
  // - This works because grid main and cross gaps are aligned.
  intersections.reserve(main_gaps_.size() + 2);
  LayoutUnit content_start =
      direction == kForColumns ? content_block_start_ : content_inline_start_;

  intersections.emplace_back(content_start, cursor.GetNextGapSegmentState());

  for (const auto& main_gap : GetMainGaps()) {
    intersections.emplace_back(main_gap.GetGapOffset(),
                               cursor.GetNextGapSegmentState());
  }

  LayoutUnit content_end =
      direction == kForColumns ? content_block_end_ : content_inline_end_;

  intersections.emplace_back(content_end, cursor.GetNextGapSegmentState());
}

void GapGeometry::GenerateCrossIntersectionListForFlex(
    GridTrackSizingDirection direction,
    wtf_size_t gap_index,
    Vector<GapIntersection>& intersections,
    GapSegmentStateCursor& cursor) const {
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
  CrossGap cross_gap = GetCrossGaps()[gap_index];
  LayoutUnit offset = direction == kForColumns
                          ? cross_gap.GetGapOffset().block_offset
                          : cross_gap.GetGapOffset().inline_offset;
  intersections.emplace_back(offset, cursor.GetNextGapSegmentState());
  LayoutUnit end_offset_for_flex_cross_gap = ComputeEndOffsetForFlexCrossGap(
      gap_index, direction, cross_gap.EndsAtEdge());
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
            : main_gap_running_index_ - 1);
  }

  // Set `main_gap_index` for the end of the cross gap.
  const bool is_end_edge =
      edge_state == CrossGap::EdgeIntersectionState::kEnd ||
      edge_state == CrossGap::EdgeIntersectionState::kBoth;
  if (!is_end_edge && main_gap_running_index_ != kNotFound) {
    intersections[1].SetMainGapIndex(main_gap_running_index_);
  }
}

void GapGeometry::GenerateCrossIntersectionListForMulticol(
    GridTrackSizingDirection direction,
    wtf_size_t gap_index,
    Vector<GapIntersection>& intersections,
    GapSegmentStateCursor& cursor) const {
  // For multicol containers, the block offset of the intersections for a
  // `CrossGap` are the following:
  // - The start block offset of the cross gap.
  // - The offset of any main gaps that intersect this cross gap.
  CHECK_EQ(direction, kForColumns);

  // At most, any cross gap can intersect with all main gaps, plus the start and
  // end of the container.
  intersections.reserve(main_gaps_.size() + 2);

  CHECK_LT(gap_index, GetCrossGaps().size());
  const CrossGap cross_gap = GetCrossGaps()[gap_index];

  intersections.emplace_back(cross_gap.GetGapOffset().block_offset,
                             cursor.GetNextGapSegmentState());

  for (const auto& main_gap : GetMainGaps()) {
    intersections.emplace_back(main_gap.GetGapOffset(),
                               cursor.GetNextGapSegmentState());

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

  intersections.emplace_back(content_block_end_,
                             cursor.GetNextGapSegmentState());
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

bool GapGeometry::IsIntersectionAtContainerEdge(
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

bool GapGeometry::IsCapIntersection(
    GridTrackSizingDirection cross_direction,
    wtf_size_t gap_index,
    wtf_size_t intersection_index,
    bool is_main_gap,
    RuleVisibilityItems rule_visibility,
    RuleVisibilityItems cross_rule_visibility,
    const Vector<GapIntersection>& intersections) const {
  return IsIntersectionAtContainerEdge(gap_index, intersection_index,
                                       intersections.size(), is_main_gap,
                                       intersections) ||
         !CSSGapDecorationUtils::HasCrossGapSegment(
             cross_direction, gap_index, intersection_index, rule_visibility,
             cross_rule_visibility, *this, intersections);
}

LayoutUnit GapGeometry::GetCrossDecorationWidthForIntersection(
    wtf_size_t gap_index,
    wtf_size_t intersection_index,
    bool is_main_gap,
    const Vector<GapIntersection>& intersections,
    bool is_cap_intersection,
    const Vector<int>& cross_decoration_widths) const {
  if (is_cap_intersection) {
    return LayoutUnit();
  }

  const GapIntersection& intersection = intersections[intersection_index];

  // For flex cross gaps, the intersection carries the associated main gap
  // index directly, since cross gaps don't map 1:1 to main gaps by position.
  if (intersection.HasMainGapIndex()) {
    return LayoutUnit(cross_decoration_widths[intersection.GetMainGapIndex()]);
  }

  // For grid and multicol, junction intersection `i` corresponds to cross gap
  // `i - 1`.
  return LayoutUnit(cross_decoration_widths[intersection_index - 1]);
}

LayoutUnit GapGeometry::GetMaxInsetWidth(
    GridTrackSizingDirection track_direction,
    wtf_size_t gap_index,
    wtf_size_t intersection_index,
    bool is_main_gap,
    const Vector<GapIntersection>& intersections) const {
  // For all intersection points other than flex main-direction overlap
  // intersections, the max inset width is the same as the width of the cross
  // gutter width since the gaps are always uniform.
  const GapIntersection& intersection = intersections[intersection_index];
  if (GetContainerType() != ContainerType::kFlex ||
      !IsMainDirection(track_direction) || !intersection.HasOverlapState()) {
    return GetCrossWidthForIntersection(track_direction, gap_index,
                                        intersection_index, is_main_gap,
                                        intersections);
  }

  CHECK(!IsIntersectionAtContainerEdge(gap_index, intersection_index,
                                       intersections.size(), is_main_gap,
                                       intersections));

  // For flex main-direction overlap intersections, compute the interior width
  // as the distance of the overlap window, which is defined by the two
  // intersections that bound the window. The start and end of the window are
  // determined by the offsets of the two overlap intersections.
  const GapIntersection& open_intersection =
      intersections[intersection.IsOverlapWindowOpen()
                        ? intersection_index
                        : intersection_index - 1];
  const GapIntersection& close_intersection =
      intersections[intersection.IsOverlapWindowClose()
                        ? intersection_index
                        : intersection_index + 1];
  CHECK(open_intersection.IsOverlapWindowOpen());
  CHECK(close_intersection.IsOverlapWindowClose());

  // Get the per-line gap size for each intersection based on which side
  // of the main gap it originates from.
  const LayoutUnit open_gap_width = GetFlexCrossGapSize(
      open_intersection.IsAboveMainGap() ? gap_index : gap_index + 1);
  const LayoutUnit close_gap_width = GetFlexCrossGapSize(
      close_intersection.IsAboveMainGap() ? gap_index : gap_index + 1);

  return close_intersection.GetOffset() + (close_gap_width / 2) -
         (open_intersection.GetOffset() - (open_gap_width / 2));
}

LayoutUnit GapGeometry::GetCrossWidthForIntersection(
    GridTrackSizingDirection track_direction,
    wtf_size_t gap_index,
    wtf_size_t intersection_index,
    bool is_main_gap,
    const Vector<GapIntersection>& intersections) const {
  if (IsIntersectionAtContainerEdge(gap_index, intersection_index,
                                    intersections.size(), is_main_gap,
                                    intersections)) {
    return LayoutUnit();
  }

  const LayoutUnit cross_gutter_width =
      track_direction == kForRows ? GetInlineGapSize() : GetBlockGapSize();

  // For grid, multicol and flex cross gaps, cross width is always the cross
  // gutter width.
  if (GetContainerType() != ContainerType::kFlex ||
      !IsMainDirection(track_direction)) {
    return cross_gutter_width;
  }

  // For flex main intersections, return the per-line cross gap size.
  return GetFlexCrossGapSize(intersections[intersection_index].IsAboveMainGap()
                                 ? gap_index
                                 : gap_index + 1);
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

  // Binary search to find the range containing `secondary_index`.
  // TODO(crbug.com/440123087): We still need this call for when computing the
  // cross gap state ranges for certain scenarios (like `overlap-join`). We
  // potentially can get rid of the binary search here and instead simply call
  // `BlockedStatusFromGapStates` by using a cursor to compute the status of the
  // relevant cross gap / intersection pair of each `GapIntersection` object,
  // similar to what we are doing with the `GapSegmentStateCursor` when
  // generating the intersection lists.
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

// static
BlockedStatus GapGeometry::BlockedStatusFromGapStates(
    const Vector<GapIntersection>& intersections,
    wtf_size_t index) {
  CHECK_LT(index, intersections.size());
  BlockedStatus status;
  if (index > 0 && intersections[index - 1].SegmentState().HasGapStatus(
                       GapSegmentState::kBlocked)) {
    status.SetBlockedStatus(BlockedStatus::kBlockedBefore);
  }
  if (intersections[index].SegmentState().HasGapStatus(
          GapSegmentState::kBlocked)) {
    status.SetBlockedStatus(BlockedStatus::kBlockedAfter);
  }
  return status;
}

const GapSegmentStateRanges* GapGeometry::GetGapSegmentStateRangesForGap(
    GridTrackSizingDirection track_direction,
    wtf_size_t gap_index) const {
  if (IsMainDirection(track_direction)) {
    CHECK_LT(gap_index, main_gaps_.size());
    if (main_gaps_[gap_index].HasGapSegmentStateRanges()) {
      return &main_gaps_[gap_index].GetGapSegmentStateRanges();
    }
  } else {
    CHECK_LT(gap_index, cross_gaps_.size());
    if (cross_gaps_[gap_index].HasGapSegmentStateRanges()) {
      return &cross_gaps_[gap_index].GetGapSegmentStateRanges();
    }
  }
  return nullptr;
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

}  // namespace blink
