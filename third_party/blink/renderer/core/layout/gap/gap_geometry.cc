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

LayoutUnit GapGeometry::ComputeInsetEnd(const ComputedStyle& style,
                                        wtf_size_t gap_index,
                                        wtf_size_t intersection_index,
                                        const Vector<LayoutUnit>& intersections,
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
    const Vector<LayoutUnit>& intersections,
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
    // In multicol the main gaps are placed in layout at the start of gaps
    // rather than at the middle, so we must adjust the `center` as such.
    if (GetContainerType() == ContainerType::kMultiColumn) {
      CHECK(direction == kForRows);
      center += GetBlockGapSize() / 2;
    }
    return center;
  } else {
    return direction == kForColumns
               ? GetCrossGaps()[gap_index].GetGapOffset().inline_offset
               : GetCrossGaps()[gap_index].GetGapOffset().block_offset;
  }
}

Vector<LayoutUnit> GapGeometry::GenerateIntersectionListForGap(
    GridTrackSizingDirection direction,
    wtf_size_t gap_index) const {
  if (IsMainDirection(direction)) {
    return GenerateMainIntersectionList(direction, gap_index);
  }
  return GenerateCrossIntersectionList(direction, gap_index);
}

Vector<LayoutUnit> GapGeometry::GenerateMainIntersectionList(
    GridTrackSizingDirection direction,
    wtf_size_t gap_index) const {
  Vector<LayoutUnit> intersections;
  LayoutUnit content_start =
      direction == kForColumns ? content_block_start_ : content_inline_start_;
  intersections.push_back(content_start);

  switch (GetContainerType()) {
    case ContainerType::kGrid: {
      // For grid containers:
      // - The main axis is rows, and cross gaps correspond to column gaps.
      // - Intersections occur at the inline offset of each column gap.
      CHECK_EQ(GetMainDirection(), kForRows);
      for (const auto& cross_gap : GetCrossGaps()) {
        intersections.push_back(cross_gap.GetGapOffset().inline_offset);
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
        return Vector<LayoutUnit>();
      }

      break;
  }

  LayoutUnit content_end =
      direction == kForColumns ? content_block_end_ : content_inline_end_;
  intersections.push_back(content_end);

  return intersections;
}

void GapGeometry::GenerateMainIntersectionListForFlex(
    GridTrackSizingDirection direction,
    wtf_size_t gap_index,
    Vector<LayoutUnit>& intersections) const {
  MainGap main_gap = GetMainGaps()[gap_index];
  // For a flex main gap:
  // - We need to include all cross gaps that intersect this main gap.
  // - Flex main gaps have two disjoint sets of cross gaps:
  // 1. Cross gaps that appear before the main gap
  // 2. Cross gaps that appear after the main gap
  // - We gather both sets and then sort them along the main axis to
  // maintain a monotonic order.
  //
  // See third_party/blink/renderer/core/layout/gap/README.md for more.
  CrossGaps cross_gaps;
  // TODO(samomekarajr): Can do merge two sorted lists here instead. This
  // will be be more efficient and avoid the extra copy loop below since we
  // would be merging directly into `intersections`.
  if (main_gap.HasCrossGapsBefore()) {
    for (wtf_size_t i = main_gap.GetCrossGapBeforeStart();
         i <= main_gap.GetCrossGapBeforeEnd(); ++i) {
      cross_gaps.push_back(GetCrossGaps()[i]);
    }
  }

  if (main_gap.HasCrossGapsAfter()) {
    for (wtf_size_t i = main_gap.GetCrossGapAfterStart();
         i <= main_gap.GetCrossGapAfterEnd(); ++i) {
      cross_gaps.push_back(GetCrossGaps()[i]);
    }
  }

  // TODO(samomekarajr): Consider having a util method for
  // GridTrackSizingDirection that swaps direction since it's a common
  // scenario.
  GridTrackSizingDirection cross_direction =
      direction == kForRows ? kForColumns : kForRows;
  std::sort(cross_gaps.begin(), cross_gaps.end(),
            [cross_direction](const CrossGap& a, const CrossGap& b) {
              return cross_direction == kForColumns
                         ? a.GetGapOffset().inline_offset <
                               b.GetGapOffset().inline_offset
                         : a.GetGapOffset().block_offset <
                               b.GetGapOffset().block_offset;
            });

  // Copy merged and sorted values into `intersections`.
  for (const auto& cross_gap : cross_gaps) {
    LogicalOffset cross_gap_start = cross_gap.GetGapOffset();
    LayoutUnit offset = cross_direction == kForColumns
                            ? cross_gap_start.inline_offset
                            : cross_gap_start.block_offset;
    intersections.push_back(offset);
  }
}

Vector<LayoutUnit> GapGeometry::GenerateCrossIntersectionList(
    GridTrackSizingDirection direction,
    wtf_size_t gap_index) const {
  Vector<LayoutUnit> intersections;
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
    Vector<LayoutUnit>& intersections) const {
  // For a grid cross gap:
  // - Intersections include:
  // 1. The content-start edge
  // 2. The start offset of every main gap
  // 3. The content-end edge
  // - This works because grid main and cross gaps are aligned.
  intersections.ReserveInitialCapacity(main_gaps_.size() + 2);
  LayoutUnit content_start =
      direction == kForColumns ? content_block_start_ : content_inline_start_;

  intersections.push_back(content_start);

  for (const auto& main_gap : GetMainGaps()) {
    intersections.push_back(main_gap.GetGapOffset());
  }

  LayoutUnit content_end =
      direction == kForColumns ? content_block_end_ : content_inline_end_;

  intersections.push_back(content_end);
}

void GapGeometry::GenerateCrossIntersectionListForFlex(
    GridTrackSizingDirection direction,
    wtf_size_t gap_index,
    Vector<LayoutUnit>& intersections) const {
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
  intersections.push_back(offset);
  LayoutUnit end_offset_for_flex_cross_gap =
      ComputeEndOffsetForFlexOrMulticolCrossGap(gap_index, direction,
                                                cross_gap.EndsAtEdge());
  intersections.push_back(end_offset_for_flex_cross_gap);
}

void GapGeometry::GenerateCrossIntersectionListForMulticol(
    GridTrackSizingDirection direction,
    wtf_size_t gap_index,
    Vector<LayoutUnit>& intersections) const {
  // For multicol containers, the block offset of the intersections for a
  // `CrossGap` are the following:
  // - The start block offset of the cross gap.
  // - The offset of any main gaps that intersect this cross gap.
  CHECK_EQ(direction, kForColumns);

  intersections.ReserveInitialCapacity(main_gaps_.size() + 2);

  CHECK_LT(gap_index, GetCrossGaps().size());
  const CrossGap cross_gap = GetCrossGaps()[gap_index];

  intersections.push_back(cross_gap.GetGapOffset().block_offset);

  // If there are no spanners or row gaps, the end offset is the content
  // end.
  if (main_gaps_.empty()) {
    intersections.push_back(content_block_end_);
    return;
  }

  LayoutUnit end_offset = ComputeEndOffsetForFlexOrMulticolCrossGap(
      gap_index, direction, /*cross_gap_is_at_end=*/false);

  intersections.push_back(end_offset);
  if (main_gap_running_index_ < main_gaps_.size() &&
      main_gaps_[main_gap_running_index_].IsStartSpannerMainGap()) {
    intersections.push_back(main_gaps_[main_gap_running_index_].GetGapOffset());
  }
}

LayoutUnit GapGeometry::ComputeEndOffsetForFlexOrMulticolCrossGap(
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
    const Vector<LayoutUnit>& intersections) const {
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
  } else if (GetContainerType() == ContainerType::kMultiColumn) {
    DCHECK(!is_main_gap);
    DCHECK_GE(intersection_count, 2u);
    DCHECK_LE(intersection_count, 3u);

    // All `CrossGap`s in multicol have either 2 or 3 intersections. 2 in cases
    // where it is not adjacent to a spanner, and 3 when it is. This is because
    // each spanner creates two main gaps (start and end), both of which
    // intersect with the cross gap. So cross gaps that don't intersect a
    // spanner will only have a start intersection and an end intersection,
    // which will be at the end of the content or before a row gap. On the other
    // hand, cross gaps adjacent to a spanner will have a start intersection, as
    // well as an intersection at the start and end of the spanner.
    //
    // For multicol cross-axis gaps:
    // - First, determine the edge state of the gap (start, end, or both).
    // - Based on this state, decide which intersections qualify as edges:
    //     * kBoth: All (2 or 3) intersections are considered edges.
    //     * kStart: Only the first intersection is an edge.
    //     * kEnd: Only the last intersection is an edge, or the last two if
    //     there are three intersections in this gap (since there is an adjacent
    //     spanner in that case).
    //
    // TODO(crbug.com/446616449): This might change depending on the spec
    // discussions happening in the linked bug, but it is trending to being this
    // way. https://github.com/w3c/csswg-drafts/issues/12784.
    CrossGap::EdgeIntersectionState cross_gap_edge_state =
        GetCrossGaps()[gap_index].GetEdgeIntersectionState();
    if (cross_gap_edge_state == CrossGap::EdgeIntersectionState::kBoth) {
      return true;
    } else if (cross_gap_edge_state ==
               CrossGap::EdgeIntersectionState::kStart) {
      return intersection_index == 0;
    } else if (cross_gap_edge_state == CrossGap::EdgeIntersectionState::kEnd) {
      return intersection_index != 0;
    }
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
    const Vector<LayoutUnit>& intersections) const {
  BlockedStatus status;

  // In muticol, the only intersections that are blocked are those that are
  // adjacent to a spanner. For painting purposes in multicol we only care about
  // those that are `kBlockedAfter`.
  if (GetContainerType() == ContainerType::kMultiColumn &&
      track_direction == kForColumns &&
      MulticolCrossGapIntersectionsEndAtSpanner(secondary_index,
                                                intersections)) {
    status.SetBlockedStatus(BlockedStatus::kBlockedAfter);
    return status;
  }

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
    const Vector<LayoutUnit>& intersections) const {
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
