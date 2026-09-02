// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/gap/gap_geometry.h"

#include <algorithm>

#include "base/notreached.h"
#include "third_party/blink/renderer/core/css/css_gap_decoration_property_utils.h"
#include "third_party/blink/renderer/core/layout/gap/gap_utils.h"
#include "third_party/blink/renderer/core/layout/physical_box_fragment.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/platform/wtf/text/strcat.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder_stream.h"

namespace blink {

GridLanesMainGapSegmentWalker::GridLanesMainGapSegmentWalker(
    const GapGeometry& gap_geometry,
    wtf_size_t main_gap_index)
    : gap_geometry_(gap_geometry),
      // TODO(javiercon): Consider having a util method for
      // GridTrackSizingDirection that swaps direction since it's a common
      // scenario.
      cross_direction_(gap_geometry.GetMainDirection() == kForColumns
                           ? kForRows
                           : kForColumns) {
  if (cross_direction_ == kForRows) {
    content_start_ = gap_geometry.GetContentBlockStart();
    content_end_ = gap_geometry.GetContentBlockEnd();
  } else {
    content_start_ = gap_geometry.GetContentInlineStart();
    content_end_ = gap_geometry.GetContentInlineEnd();
  }

  CHECK_EQ(gap_geometry.GetContainerType(),
           GapGeometry::ContainerType::kGridLanes);
  CHECK_LT(main_gap_index, gap_geometry.MainGapCount());
  const MainGap& main_gap = gap_geometry.MainGapAt(main_gap_index);
  if (main_gap.HasCrossGapsBefore()) {
    before_ = CrossGapRunCursor(main_gap.GetCrossGapBeforeStart(),
                                main_gap.GetCrossGapBeforeEnd(),
                                gap_geometry.CrossGapCount());
  }
  if (main_gap.HasCrossGapsAfter()) {
    after_ = CrossGapRunCursor(main_gap.GetCrossGapAfterStart(),
                               main_gap.GetCrossGapAfterEnd(),
                               gap_geometry.CrossGapCount());
  }
  // The 2 accounts for the content-start and content-end intersections.
  intersection_capacity_ = 2 + before_.Size() + after_.Size();
  SkipGapsAtOrBeforeContentStart();
}

LayoutUnit GridLanesMainGapSegmentWalker::CrossGapOffset(
    wtf_size_t index) const {
  CHECK_LT(index, gap_geometry_.CrossGapCount());
  return gap_geometry_.GetCrossGaps()[index].GetGapOffset(cross_direction_);
}

void GridLanesMainGapSegmentWalker::SkipGapsAtOrBeforeContentStart() {
  SkipRunAtOrBeforeContentStart(before_);
  SkipRunAtOrBeforeContentStart(after_);
}

void GridLanesMainGapSegmentWalker::SkipRunAtOrBeforeContentStart(
    CrossGapRunCursor& run) {
  while (!run.AtEnd() &&
         CrossGapOffset(run.CrossGapIndex()) <= content_start_) {
    run.Advance();
  }
}

void GridLanesMainGapSegmentWalker::ConsumeRunAtOffset(CrossGapRunCursor& run,
                                                       LayoutUnit offset) {
  while (!run.AtEnd() && CrossGapOffset(run.CrossGapIndex()) == offset) {
    run.Advance();
  }
  CHECK(run.AtEnd() || CrossGapOffset(run.CrossGapIndex()) > offset);
}

std::optional<const GridLanesMainGapSegmentWalker::Segment>
GridLanesMainGapSegmentWalker::Next() {
  if (finished_) {
    return std::nullopt;
  }

  Segment segment{content_end_, before_.ConsumedCount(),
                  after_.ConsumedCount()};
  if (before_.AtEnd() && after_.AtEnd()) {
    finished_ = true;
    return segment;
  }

  LayoutUnit offset;
  if (before_.AtEnd()) {
    offset = CrossGapOffset(after_.CrossGapIndex());
  } else if (after_.AtEnd()) {
    offset = CrossGapOffset(before_.CrossGapIndex());
  } else {
    offset = std::min(CrossGapOffset(before_.CrossGapIndex()),
                      CrossGapOffset(after_.CrossGapIndex()));
  }

  if (offset >= content_end_) {
    finished_ = true;
    return segment;
  }

  ConsumeRunAtOffset(before_, offset);
  ConsumeRunAtOffset(after_, offset);

  segment.end_offset = offset;
  return segment;
}

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

  // Row-flex fragments main gaps while column-flex fragments cross gaps within
  // each line.
  if (container_type_ == GapGeometry::ContainerType::kFlex) {
    return main_direction_ == kForColumns ? !is_main : is_main;
  }

  // TODO(samomekarajr): Implement for multicol in a follow-up CL.
  return false;
}

bool GapGeometry::NeedsDecorationValueAssignmentMapping(
    GridTrackSizingDirection track_direction) const {
  // Grid-lanes `CrossGap`s use lane-local assignment slots, so each lane must
  // map its first geometric gap back to slot zero, even without reversal.
  if (GetContainerType() == kGridLanes && !IsMainDirection(track_direction)) {
    return true;
  }
  if (!gap_placement_reversal_) {
    return false;
  }
  return !IsMainDirection(track_direction) ||
         gap_placement_reversal_->reverse_main_assignment_order;
}

GapGeometry::DecorationValueAssignment
GapGeometry::DecorationValueAssignmentForGap(
    GridTrackSizingDirection track_direction,
    wtf_size_t fragment_relative_gap_index,
    wtf_size_t stitched_gap_index,
    wtf_size_t gap_slot_count,
    std::optional<wtf_size_t> cross_gap_owner_index) const {
  CHECK_LT(stitched_gap_index, gap_slot_count);

  // Each entry in `fragmented_flex_cross_gap_decoration_indices_` stores the
  // `CrossGap`'s index in the complete decoration value sequence after flex
  // reversals are applied. This can differ from `stitched_gap_index`, which
  // remains in geometric paint order. Use the entry keyed by
  // `fragment_relative_gap_index` because fragmentation can change the gap's
  // fragment-relative geometric index.
  if (!IsMainDirection(track_direction) &&
      HasFragmentedFlexCrossGapDecorationIndices()) {
    CHECK_EQ(gap_slot_count, FragmentedFlexCrossGapCount());
    return {FragmentedFlexCrossGapDecorationValueIndexAt(
                fragment_relative_gap_index),
            gap_slot_count};
  }

  if (IsMainDirection(track_direction)) {
    // Main gaps use one container-wide assignment sequence for their color,
    // style, and width lists. If placement reverses their order, mirror the
    // stitched gap index before selecting values from those lists.
    const wtf_size_t decoration_value_index =
        NeedsDecorationValueAssignmentMapping(track_direction)
            ? DecorationValueIndexForReversedMainGap(stitched_gap_index,
                                                     gap_slot_count)
            : stitched_gap_index;
    return {decoration_value_index, gap_slot_count};
  }

  // Grid-lanes assigns gap-decoration values to `CrossGap`s independently per
  // lane. Rules never continue across lanes, so the owning lane's `CrossGap`
  // count replaces the container-wide `gap_slot_count`.
  if (GetContainerType() == kGridLanes) {
    CHECK(cross_gap_owner_index.has_value());
    const GapIndexRange owner_range =
        CrossGapRangeForOwner(*cross_gap_owner_index);

    CHECK_GE(stitched_gap_index, owner_range.start);
    wtf_size_t local_decoration_value_index =
        stitched_gap_index - owner_range.start;
    CHECK_LT(local_decoration_value_index, owner_range.count);

    if (gap_placement_reversal_ &&
        gap_placement_reversal_->reverse_within_owner) {
      // `fill-reverse` starts assignment at the opposite end of each lane, so
      // mirror the lane-local index within that lane's slot sequence.
      local_decoration_value_index =
          owner_range.count - 1 - local_decoration_value_index;
    }
    return {local_decoration_value_index, owner_range.count};
  }

  // Flex `CrossGap`s are assigned from one sequence spanning the whole
  // container.
  if (!NeedsDecorationValueAssignmentMapping(track_direction)) {
    return {stitched_gap_index, gap_slot_count};
  }
  CHECK(cross_gap_owner_index.has_value());
  return {DecorationValueIndexForCrossGap(
              stitched_gap_index, CrossGapRangeForOwner(*cross_gap_owner_index),
              gap_slot_count),
          gap_slot_count};
}

GapGeometry::GapIndexRange GapGeometry::CrossGapRangeForOwner(
    wtf_size_t owner_index) const {
  if (main_gaps_.empty()) {
    CHECK_EQ(owner_index, 0u);
    CHECK(!cross_gaps_.empty());
    return {0, cross_gaps_.size()};
  }

  if (owner_index < main_gaps_.size()) {
    const MainGap& main_gap = main_gaps_[owner_index];
    CHECK(main_gap.HasCrossGapsBefore());
    return {main_gap.GetCrossGapBeforeStart(),
            main_gap.GetCrossGapBeforeCount()};
  }

  CHECK_EQ(owner_index, main_gaps_.size());
  const MainGap& last_main_gap = main_gaps_.back();
  CHECK(last_main_gap.HasCrossGapsAfter());
  return {last_main_gap.GetCrossGapAfterStart(),
          last_main_gap.GetCrossGapAfterCount()};
}

wtf_size_t GapGeometry::DecorationValueIndexForReversedMainGap(
    wtf_size_t stitched_gap_index,
    wtf_size_t gap_slot_count) const {
  CHECK(gap_placement_reversal_);
  CHECK(gap_placement_reversal_->reverse_main_assignment_order);
  CHECK_LT(stitched_gap_index, gap_slot_count);
  // Mirror the zero-based geometric index within the assignment sequence, so
  // the first geometric gap uses the last slot and vice versa.
  return gap_slot_count - 1 - stitched_gap_index;
}

wtf_size_t GapGeometry::DecorationValueIndexForCrossGap(
    wtf_size_t stitched_gap_index,
    GapIndexRange line_range,
    wtf_size_t gap_slot_count) const {
  CHECK(gap_placement_reversal_);
  CHECK_LT(stitched_gap_index, gap_slot_count);
  CHECK_GE(stitched_gap_index, line_range.start);
  CHECK_LE(line_range.count, gap_slot_count - line_range.start);

  // Geometric order is the order in which gaps are stored and painted, based
  // on their logical positions in the container.
  const wtf_size_t geometric_index_in_line =
      stitched_gap_index - line_range.start;
  CHECK_LT(geometric_index_in_line, line_range.count);

  // Next, reverse the gap's index within its line for a reversed
  // `flex-direction`.
  wtf_size_t placement_index_in_line = geometric_index_in_line;
  if (gap_placement_reversal_->reverse_within_owner) {
    placement_index_in_line = line_range.count - 1 - placement_index_in_line;
  }

  // Finally, move the complete line group to its placement-order position for
  // `wrap-reverse`.
  const wtf_size_t line_start_in_placement_order =
      gap_placement_reversal_->reverse_main_assignment_order
          ? gap_slot_count - line_range.start - line_range.count
          : line_range.start;
  const wtf_size_t decoration_value_index =
      line_start_in_placement_order + placement_index_in_line;
  return decoration_value_index;
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
    Vector<GapIntersection>& intersections,
    std::optional<wtf_size_t> cross_gap_owner_index) const {
  // Reset the buffer's logical size but keep capacity, so we can reuse
  // a single Vector across loop iterations without reallocating.
  intersections.Shrink(0);
  if (IsMainDirection(direction)) {
    GenerateMainIntersectionList(direction, gap_index, intersections);
  } else {
    GenerateCrossIntersectionList(direction, gap_index, intersections,
                                  cross_gap_owner_index);
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

  switch (GetContainerType()) {
    case ContainerType::kGridLanes: {
      GridLanesMainGapSegmentWalker walker(*this, gap_index);
      intersections.reserve(walker.IntersectionCapacity());
      const LayoutUnit content_start = direction == kForColumns
                                           ? content_block_start_
                                           : content_inline_start_;
      intersections.emplace_back(content_start,
                                 cursor.GetNextGapSegmentState());
      while (auto segment = walker.Next()) {
        intersections.emplace_back(segment->end_offset,
                                   cursor.GetNextGapSegmentState());
      }
      break;
    }
    case ContainerType::kGrid:
    case ContainerType::kMultiColumn:
      GenerateMainIntersectionListForGridAndMulticol(direction, intersections,
                                                     cursor);
      break;
    case ContainerType::kFlex:
      GenerateMainIntersectionListForFlex(direction, gap_index, intersections,
                                          cursor);
      break;
  }
}

void GapGeometry::GenerateMainIntersectionListForGridAndMulticol(
    GridTrackSizingDirection direction,
    Vector<GapIntersection>& intersections,
    GapSegmentStateCursor& cursor) const {
  intersections.reserve(GetCrossGaps().size() + 2);

  LayoutUnit content_start =
      direction == kForColumns ? content_block_start_ : content_inline_start_;
  intersections.emplace_back(content_start, cursor.GetNextGapSegmentState());

  // Grid tracks and multicol columns align, so every main gap intersects every
  // cross gap.
  CHECK_EQ(GetMainDirection(), kForRows);
  for (const auto& cross_gap : GetCrossGaps()) {
    intersections.emplace_back(cross_gap.GetGapOffset().inline_offset,
                               cursor.GetNextGapSegmentState());
  }

  LayoutUnit content_end =
      direction == kForColumns ? content_block_end_ : content_inline_end_;
  intersections.emplace_back(content_end, cursor.GetNextGapSegmentState());
}

void GapGeometry::GenerateMainIntersectionListForFlex(
    GridTrackSizingDirection direction,
    wtf_size_t gap_index,
    Vector<GapIntersection>& intersections,
    GapSegmentStateCursor& cursor) const {
  MainGap main_gap = GetMainGaps()[gap_index];

  const bool has_cross_gaps_before = main_gap.HasCrossGapsBefore();
  const bool has_cross_gaps_after = main_gap.HasCrossGapsAfter();
  const wtf_size_t num_cross_gaps_before =
      has_cross_gaps_before ? main_gap.GetCrossGapBeforeCount() : 0u;
  const wtf_size_t num_cross_gaps_after =
      has_cross_gaps_after ? main_gap.GetCrossGapAfterCount() : 0u;
  intersections.reserve(num_cross_gaps_before + num_cross_gaps_after + 2);

  LayoutUnit content_start =
      direction == kForColumns ? content_block_start_ : content_inline_start_;
  intersections.emplace_back(content_start, cursor.GetNextGapSegmentState());

  if (!has_cross_gaps_before && !has_cross_gaps_after) {
    LayoutUnit content_end =
        direction == kForColumns ? content_block_end_ : content_inline_end_;
    intersections.emplace_back(content_end, cursor.GetNextGapSegmentState());
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

  LayoutUnit content_end =
      direction == kForColumns ? content_block_end_ : content_inline_end_;
  intersections.emplace_back(content_end, cursor.GetNextGapSegmentState());
}

void GapGeometry::GenerateCrossIntersectionList(
    GridTrackSizingDirection direction,
    wtf_size_t gap_index,
    Vector<GapIntersection>& intersections,
    std::optional<wtf_size_t> cross_gap_owner_index) const {
  GapSegmentStateCursor cursor(
      GetGapSegmentStateRangesForGap(direction, gap_index));

  switch (GetContainerType()) {
    case ContainerType::kGridLanes: {
      CHECK(cross_gap_owner_index);
      GenerateCrossIntersectionListForGridLanes(*cross_gap_owner_index,
                                                intersections);
      break;
    }
    case ContainerType::kGrid: {
      CHECK(!cross_gap_owner_index);
      GenerateCrossIntersectionListForGrid(direction, intersections, cursor);
      break;
    }
    case ContainerType::kFlex: {
      CHECK(cross_gap_owner_index);
      GenerateCrossIntersectionListForFlex(direction, gap_index, intersections,
                                           cursor, *cross_gap_owner_index);
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
  CrossGap cross_gap = GetCrossGaps()[gap_index];
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
  }

  intersections.emplace_back(content_block_end_,
                             cursor.GetNextGapSegmentState());
}

LayoutUnit GapGeometry::GridAxisOffsetForLaneBoundary(
    wtf_size_t lane_boundary) const {
  CHECK_EQ(container_type_, ContainerType::kGridLanes);

  // A lane boundary separates adjacent non-collapsed tracks. Interior lane
  // boundaries correspond to `MainGap`s; the outer lane boundaries are content
  // edges.
  const bool grid_axis_is_inline = main_direction_ == kForColumns;
  const LayoutUnit content_start =
      grid_axis_is_inline ? content_inline_start_ : content_block_start_;
  const LayoutUnit content_end =
      grid_axis_is_inline ? content_inline_end_ : content_block_end_;
  const wtf_size_t lane_count = main_gaps_.size() + 1;

  if (lane_boundary == 0) {
    return content_start;
  }
  if (lane_boundary == lane_count) {
    return content_end;
  }
  return MainGapAt(lane_boundary - 1).GetGapOffset();
}

void GapGeometry::GenerateCrossIntersectionListForGridLanes(
    wtf_size_t lane,
    Vector<GapIntersection>& intersections) const {
  // A grid-lanes `CrossGap` is one stacking-axis gutter confined to a single
  // lane, so it generates exactly two intersections: one at each of the lane's
  // grid-axis boundaries. It never crosses a grid-axis gutter.
  const wtf_size_t lane_count = main_gaps_.size() + 1;
  CHECK_LT(lane, lane_count);
  CHECK(intersections.empty());

  intersections.reserve(2);

  // The start intersection borders the preceding `MainGap` or content edge.
  GapIntersection start(GridAxisOffsetForLaneBoundary(lane));
  if (lane > 0) {
    start.SetMainGapIndex(lane - 1);
  }
  intersections.push_back(start);

  // The end intersection borders the following `MainGap` or content edge.
  GapIntersection end(GridAxisOffsetForLaneBoundary(lane + 1));
  if (lane + 1 < lane_count) {
    end.SetMainGapIndex(lane);
  }
  intersections.push_back(end);
}

LayoutUnit GapGeometry::ComputeEndOffsetForFlexCrossGap(
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
    return direction == kForRows ? content_inline_end_ : content_block_end_;
  }

  return main_gaps[main_gap_index].GetGapOffset();
}

bool GapGeometry::IsIntersectionAtContainerEdge(
    wtf_size_t gap_index,
    wtf_size_t intersection_index,
    wtf_size_t intersection_count,
    bool is_main_gap,
    const Vector<GapIntersection>& intersections) const {
  CHECK_GT(intersection_count, 0u);
  const wtf_size_t last_intersection_index = intersection_count - 1;
  // For main-axis gaps, and for grid and multicol cross-axis gaps, the first
  // and last intersections are considered edges.
  if (is_main_gap || GetContainerType() == ContainerType::kGrid ||
      GetContainerType() == ContainerType::kMultiColumn) {
    return intersection_index == 0 ||
           intersection_index == last_intersection_index;
  }

  if (GetContainerType() == ContainerType::kGridLanes) {
    CHECK(!is_main_gap);
    // An intersection in grid-lanes is at the container edge iff it carries no
    // `main_gap_index` association.
    return !intersections[intersection_index].HasMainGapIndex();
  }

  if (GetContainerType() == ContainerType::kFlex) {
    CHECK(!is_main_gap);
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

  return false;
}

bool GapGeometry::IsMulticolSpannerBoundaryIntersection(
    wtf_size_t intersection_index,
    bool is_main_gap) const {
  if (GetContainerType() != ContainerType::kMultiColumn || is_main_gap ||
      intersection_index == 0 || intersection_index > main_gaps_.size()) {
    return false;
  }

  return main_gaps_[intersection_index - 1].IsSpannerMainGap();
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
         IsMulticolSpannerBoundaryIntersection(intersection_index,
                                               is_main_gap) ||
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

  if (GetContainerType() == ContainerType::kGridLanes && is_main_gap) {
    // TODO(javiercon): Support main-gap `overlap-join` for grid-lanes.
    return LayoutUnit();
  }

  // For flex cross gaps, the intersection carries the associated main gap
  // index directly, since cross gaps don't map 1:1 to main gaps by position.
  // Grid-lanes `CrossGap` interior points likewise carry a `main_gap_index`.
  if (intersection.HasMainGapIndex()) {
    CHECK(GetContainerType() == ContainerType::kGridLanes ||
          GetContainerType() == ContainerType::kFlex);
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
                                    intersections) ||
      IsMulticolSpannerBoundaryIntersection(intersection_index, is_main_gap)) {
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
  const GapSegmentStateRanges* gap_segment_state_ranges =
      GetGapSegmentStateRangesForGap(track_direction, primary_index);

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
