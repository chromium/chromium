// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/gap/gap_geometry.h"

#include <algorithm>

#include "base/notreached.h"
#include "third_party/blink/renderer/core/css/css_gap_decoration_property_utils.h"
#include "third_party/blink/renderer/core/layout/gap/flex_gap_geometry.h"
#include "third_party/blink/renderer/core/layout/gap/gap_utils.h"
#include "third_party/blink/renderer/core/layout/gap/multicol_gap_geometry.h"
#include "third_party/blink/renderer/core/layout/physical_box_fragment.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/platform/wtf/text/strcat.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder_stream.h"

namespace blink {

namespace {

// TODO(javiercon): clean up now that we have a lot of layout algorithm-specific
// code, potentially look into subclassing gap geometry for each container type.
//
// Returns whether a lane is empty along a whole segment, meaning the segment
// lies inside one of the lane's empty openings. A segment that only partly
// overlaps empty space is not empty.
bool ConsumeOpeningsAndCheckEmpty(
    base::span<const TrackOpeningCoordinates>& remaining_empty_openings,
    LayoutUnit start,
    LayoutUnit end) {
  CHECK_LT(start, end);

  // Segments are checked in increasing order. An opening that ends before
  // this segment starts cannot contain this or any later segment, so skip it
  // for good.
  while (!remaining_empty_openings.empty() &&
         remaining_empty_openings.front().end_position <= start) {
    remaining_empty_openings = remaining_empty_openings.subspan(1u);
  }

  // A lane with no empty openings left is occupied along this segment.
  if (remaining_empty_openings.empty()) {
    return false;
  }
  const TrackOpeningCoordinates& opening = remaining_empty_openings.front();
  return opening.start_position <= start && end <= opening.end_position;
}

}  // namespace

bool GapGeometry::operator==(const GapGeometry& other) const {
  // The container types must be equal before the subclass casts below.
  if (container_type_ != other.container_type_ ||
      inline_gap_size_ != other.inline_gap_size_ ||
      block_gap_size_ != other.block_gap_size_ ||
      main_gaps_ != other.main_gaps_ || cross_gaps_ != other.cross_gaps_ ||
      grid_lanes_empty_openings_ != other.grid_lanes_empty_openings_ ||
      spanner_main_gap_count_ != other.spanner_main_gap_count_ ||
      gap_placement_reversal_ != other.gap_placement_reversal_ ||
      content_inline_start_ != other.content_inline_start_ ||
      content_inline_end_ != other.content_inline_end_ ||
      content_block_start_ != other.content_block_start_ ||
      content_block_end_ != other.content_block_end_ ||
      main_direction_ != other.main_direction_) {
    return false;
  }

  // TODO(javiercon): Replace this switch with a virtual method that each
  // subclass overrides.
  switch (container_type_) {
    case ContainerType::kFlex:
      return To<FlexGapGeometry>(*this).HasEqualSubclassFields(
          To<FlexGapGeometry>(other));
    case ContainerType::kGrid:
    case ContainerType::kGridLanes:
    case ContainerType::kMultiColumn:
      return true;
  }
}

void GapGeometry::SetGridLanesEmptyOpenings(
    GridLanesEmptyOpenings empty_openings) {
  CHECK_EQ(container_type_, ContainerType::kGridLanes);
  CHECK_EQ(empty_openings.size(), MainGapCount() + 1);
  grid_lanes_empty_openings_ = std::move(empty_openings);
}

const Vector<TrackOpeningCoordinates>& GapGeometry::EmptyOpeningsForLane(
    wtf_size_t lane_index) const {
  return grid_lanes_empty_openings_[lane_index];
}

void GapGeometry::MarkGridLanesEmptySegments(
    GridTrackSizingDirection track_direction,
    wtf_size_t gap_index,
    Vector<GapIntersection>& intersections) const {
  CHECK_EQ(container_type_, ContainerType::kGridLanes);
  CHECK(IsMainDirection(track_direction));

  base::span<const TrackOpeningCoordinates> before_openings =
      EmptyOpeningsForLane(gap_index);
  base::span<const TrackOpeningCoordinates> after_openings =
      EmptyOpeningsForLane(gap_index + 1);

  // A lane without empty openings is occupied along every segment, so there is
  // nothing to mark on its side.
  const bool check_before = !before_openings.empty();
  const bool check_after = !after_openings.empty();
  if (!check_before && !check_after) {
    return;
  }

  LayoutUnit segment_start =
      intersections[0].GetOffset() +
      GetCrossWidthForIntersection(track_direction, gap_index,
                                   /*intersection_index=*/0,
                                   /*is_main_gap=*/true, intersections) /
          2;
  for (wtf_size_t i = 0; i + 1 < intersections.size(); ++i) {
    const LayoutUnit next_half_cross_width =
        GetCrossWidthForIntersection(track_direction, gap_index, i + 1,
                                     /*is_main_gap=*/true, intersections) /
        2;
    const LayoutUnit segment_end =
        intersections[i + 1].GetOffset() - next_half_cross_width;

    // Where the crossing gutters of the adjacent lanes overlap, the space
    // between the two intersections has no positive length. It is not a real
    // segment, so it is not marked, and it cannot hide a rule.
    if (segment_end > segment_start) {
      GapSegmentState segment_state = intersections[i].SegmentState();
      if (check_before && ConsumeOpeningsAndCheckEmpty(
                              before_openings, segment_start, segment_end)) {
        segment_state |= GapSegmentState::kEmptyBefore;
      }
      if (check_after && ConsumeOpeningsAndCheckEmpty(
                             after_openings, segment_start, segment_end)) {
        segment_state |= GapSegmentState::kEmptyAfter;
      }
      intersections[i].SetSegmentState(segment_state);
    }

    segment_start = intersections[i + 1].GetOffset() + next_half_cross_width;
  }
}

GridLanesMainGapSegmentWalker::GridLanesMainGapSegmentWalker(
    const GapGeometry& gap_geometry,
    wtf_size_t main_gap_index)
    : gap_geometry_(gap_geometry),
      cross_direction_(OppositeDirection(gap_geometry.GetMainDirection())) {
  if (cross_direction_ == kForRows) {
    content_start_ = gap_geometry.GetContentBlockStart();
    content_end_ = gap_geometry.GetContentBlockEnd();
    cross_gap_width_ = gap_geometry.GetBlockGapSize();
  } else {
    content_start_ = gap_geometry.GetContentInlineStart();
    content_end_ = gap_geometry.GetContentInlineEnd();
    cross_gap_width_ = gap_geometry.GetInlineGapSize();
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

LayoutUnit GridLanesMainGapSegmentWalker::NextCrossGapOffset() const {
  LayoutUnit offset = content_end_;
  if (!before_.AtEnd()) {
    offset = std::min(offset, CrossGapOffset(before_.CrossGapIndex()));
  }
  if (!after_.AtEnd()) {
    offset = std::min(offset, CrossGapOffset(after_.CrossGapIndex()));
  }
  return offset;
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
  const LayoutUnit offset = NextCrossGapOffset();
  if (offset >= content_end_) {
    finished_ = true;
    return segment;
  }

  ConsumeRunAtOffset(before_, offset);
  ConsumeRunAtOffset(after_, offset);

  segment.end_offset = offset;
  segment.continues_overlap_window = next_gutter_overlaps_current_;

  // Touching gutters and content boundaries do not extend the overlap window.
  const LayoutUnit next_offset = NextCrossGapOffset();
  next_gutter_overlaps_current_ =
      next_offset < content_end_ && next_offset - offset < cross_gap_width_;
  return segment;
}

bool GapGeometry::HasRowGapFragmentation(
    const PhysicalBoxFragment& box_fragment,
    bool is_main) const {
  if (box_fragment.IsOnlyForNode()) {
    return false;
  }

  // For grid and multicol, fragmentation only affects row gap indices (i.e.
  // main gaps).
  if (container_type_ == ContainerType::kGrid ||
      container_type_ == ContainerType::kMultiColumn) {
    return is_main;
  }

  // Row-flex fragments main gaps while column-flex fragments cross gaps within
  // each line.
  if (container_type_ == GapGeometry::ContainerType::kFlex) {
    return main_direction_ == kForColumns ? !is_main : is_main;
  }

  return false;
}

bool GapGeometry::IsPaintableGap(GridTrackSizingDirection direction,
                                 wtf_size_t gap_index) const {
  // Only multicol main gaps created by spanners can be synthetic spanner
  // boundaries. Cross gaps can always be painted.
  return !IsMainDirection(direction) ||
         !main_gaps_[gap_index].IsSpannerMainGap();
}

wtf_size_t GapGeometry::PaintableGapCount(
    GridTrackSizingDirection direction) const {
  // Cross gaps are never synthetic spanner boundaries. Only multicol main gaps
  // can be, and those do not consume decoration values.
  if (!IsMainDirection(direction)) {
    return cross_gaps_.size();
  }
  CHECK_LE(spanner_main_gap_count_, main_gaps_.size());
  return main_gaps_.size() - spanner_main_gap_count_;
}

std::optional<wtf_size_t> GapGeometry::StoredGapSlotCount(
    GridTrackSizingDirection direction) const {
  if (IsMainDirection(direction)) {
    return std::nullopt;
  }
  // TODO(javiercon): Replace this cast with a virtual method that
  // FlexGapGeometry overrides.
  const auto* flex_gap_geometry = DynamicTo<FlexGapGeometry>(*this);
  if (!flex_gap_geometry ||
      !flex_gap_geometry->HasFragmentedFlexCrossGapDecorationIndices()) {
    return std::nullopt;
  }
  return flex_gap_geometry->FragmentedFlexCrossGapCount();
}

wtf_size_t GapGeometry::CountSpannerMainGaps(const MainGaps& main_gaps) {
  wtf_size_t count = 0;
  for (const MainGap& main_gap : main_gaps) {
    if (main_gap.IsSpannerMainGap()) {
      ++count;
    }
  }
  return count;
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
  if (const std::optional<wtf_size_t> stored_gap_slot_count =
          StoredGapSlotCount(track_direction)) {
    CHECK_EQ(gap_slot_count, *stored_gap_slot_count);
    // TODO(javiercon): Replace this cast with a virtual method that
    // FlexGapGeometry overrides.
    return {
        To<FlexGapGeometry>(*this).FragmentedFlexCrossGapDecorationValueIndexAt(
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

  if (container_type_ != ContainerType::kFlex || !IsMainDirection(direction)) {
    return base_size;
  }

  // TODO(javiercon): Replace this cast with a virtual method that
  // FlexGapGeometry overrides.
  const std::optional<LayoutUnit> max_flex_cross_gap_size =
      To<FlexGapGeometry>(*this).MaxFlexCrossGapSize();
  if (!max_flex_cross_gap_size) {
    return base_size;
  }

  // For flex containers, per-line cross gap sizes can differ due to content
  // distribution. Use the max across all lines for a conservative bound.
  return std::max(base_size, *max_flex_cross_gap_size);
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

  // TODO(javiercon): Replace this switch with a virtual method when each
  // subclass contains its container-specific methods.
  switch (GetContainerType()) {
    case ContainerType::kGridLanes:
      GenerateMainIntersectionListForGridLanes(direction, gap_index,
                                               intersections, cursor);
      break;
    case ContainerType::kGrid:
    case ContainerType::kMultiColumn:
      GenerateMainIntersectionListForGridAndMulticol(direction, intersections,
                                                     cursor);
      break;
    case ContainerType::kFlex:
      To<FlexGapGeometry>(*this).GenerateMainIntersectionListForFlex(
          direction, gap_index, intersections, cursor);
      break;
  }
}

void GapGeometry::GenerateMainIntersectionListForGridLanes(
    GridTrackSizingDirection direction,
    wtf_size_t gap_index,
    Vector<GapIntersection>& intersections,
    GapSegmentStateCursor& cursor) const {
  GridLanesMainGapSegmentWalker walker(*this, gap_index);
  intersections.reserve(walker.IntersectionCapacity());
  const LayoutUnit content_start =
      direction == kForColumns ? content_block_start_ : content_inline_start_;
  intersections.emplace_back(content_start, cursor.GetNextGapSegmentState());

  // Segments are initially generated using the center of each crossing
  // gutter. Intersections from overlapping gutters are then reduced to an
  // opening/closing pair. Advance the state cursor for every intersection
  // produced by the walker, including those not kept in the pair, so spanner
  // states remain aligned with the correct segments.
  while (auto segment = walker.Next()) {
    const GapSegmentState outgoing_state = cursor.GetNextGapSegmentState();
    if (!segment->continues_overlap_window) {
      intersections.emplace_back(segment->end_offset, outgoing_state,
                                 OverlapWindowState::kNone);
      continue;
    }

    // An existing window is stored as an opening/closing pair. If the last
    // intersection closes a window, extend that window to the new gutter.
    if (intersections.back().IsOverlapWindowClose()) {
      CHECK_GE(intersections.size(), 3u);
      GapIntersection& opening = intersections[intersections.size() - 2];
      CHECK(opening.IsOverlapWindowOpen());
      GapIntersection& closing = intersections.back();

      // Moving the closing intersection to the new gutter puts the segment
      // after its current position inside the window. Add that segment's
      // blocked state to the opening before updating the closing position.
      GapSegmentState interior_state = opening.SegmentState();
      interior_state |= closing.SegmentState();
      opening.SetSegmentState(interior_state);
      closing.SetOffset(segment->end_offset);
      closing.SetSegmentState(outgoing_state);
      continue;
    }

    // The first overlapping pair establishes the window. Subsequent centers
    // move only its closing point, but retain both the interior blocked
    // state and the outgoing state separately.
    intersections.back().SetOverlapState(OverlapWindowState::kWindowOpen);
    intersections.emplace_back(segment->end_offset, outgoing_state,
                               OverlapWindowState::kWindowClose);
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

void GapGeometry::GenerateCrossIntersectionList(
    GridTrackSizingDirection direction,
    wtf_size_t gap_index,
    Vector<GapIntersection>& intersections,
    std::optional<wtf_size_t> cross_gap_owner_index) const {
  GapSegmentStateCursor cursor(
      GetGapSegmentStateRangesForGap(direction, gap_index));

  // TODO(javiercon): Replace this switch with a virtual method when each
  // subclass contains its container-specific methods.
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
      To<FlexGapGeometry>(*this).GenerateCrossIntersectionListForFlex(
          direction, gap_index, intersections, cursor, *cross_gap_owner_index);
      break;
    }
    case ContainerType::kMultiColumn:
      To<MulticolGapGeometry>(*this).GenerateCrossIntersectionListForMulticol(
          direction, gap_index, intersections, cursor);
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

bool GapGeometry::IsSpannerBoundaryIntersection(wtf_size_t intersection_index,
                                                bool is_main_gap) const {
  // Junction intersection `i` of a multicol `CrossGap` corresponds to main gap
  // `i - 1`. Only multicol has spanner main gaps.
  if (spanner_main_gap_count_ == 0 || is_main_gap || intersection_index == 0 ||
      intersection_index > main_gaps_.size()) {
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
         IsSpannerBoundaryIntersection(intersection_index, is_main_gap) ||
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
  const GapIntersection& intersection = intersections[intersection_index];
  // Flex and grid-lanes main-direction overlap intersections use the full
  // window. Other intersections use the width of the crossing gutter.
  if ((GetContainerType() != ContainerType::kFlex &&
       GetContainerType() != ContainerType::kGridLanes) ||
      !IsMainDirection(track_direction) || !intersection.HasOverlapState()) {
    return GetCrossWidthForIntersection(track_direction, gap_index,
                                        intersection_index, is_main_gap,
                                        intersections);
  }

  CHECK(!IsIntersectionAtContainerEdge(gap_index, intersection_index,
                                       intersections.size(), is_main_gap,
                                       intersections));

  // For flex and grid-lanes main-direction overlap intersections, compute the
  // interior width as the size of the overlap window, which is defined by the
  // two intersections that bound it. The start and end of the window are
  // determined by their offsets and crossing gutter widths. The opening and
  // closing intersections are adjacent in the list.
  const wtf_size_t opening_index = intersection.IsOverlapWindowOpen()
                                       ? intersection_index
                                       : intersection_index - 1;
  const GapIntersection& open_intersection = intersections[opening_index];
  const GapIntersection& close_intersection = intersections[opening_index + 1];
  CHECK(open_intersection.IsOverlapWindowOpen());
  CHECK(close_intersection.IsOverlapWindowClose());

  // TODO(javiercon): Use one formula for flex and grid-lanes. The flex formula
  // halves each width, and `LayoutUnit` division truncates, so it can truncate
  // twice. Halving the sum of the widths truncates only once, but the sum must
  // not saturate.

  // Grid lanes' gutters have a uniform width.
  if (GetContainerType() == ContainerType::kGridLanes) {
    return close_intersection.GetOffset() - open_intersection.GetOffset() +
           GetCrossWidthForIntersection(track_direction, gap_index,
                                        intersection_index, is_main_gap,
                                        intersections);
  }

  const LayoutUnit open_gap_width =
      CrossingGapSizeAt(track_direction, gap_index, open_intersection);
  const LayoutUnit close_gap_width =
      CrossingGapSizeAt(track_direction, gap_index, close_intersection);

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
      IsSpannerBoundaryIntersection(intersection_index, is_main_gap)) {
    return LayoutUnit();
  }

  return CrossingGapSizeAt(track_direction, gap_index,
                           intersections[intersection_index]);
}

LayoutUnit GapGeometry::CrossingGapSizeAt(
    GridTrackSizingDirection track_direction,
    wtf_size_t gap_index,
    const GapIntersection& intersection) const {
  // Flex lines can have different effective gap sizes because of content
  // distribution. Use the size of the line on the side of `intersection`.
  if (GetContainerType() == ContainerType::kFlex &&
      IsMainDirection(track_direction)) {
    // TODO(javiercon): Replace this cast with a virtual method that
    // FlexGapGeometry overrides.
    return To<FlexGapGeometry>(*this).GetFlexCrossGapSize(
        intersection.IsAboveMainGap() ? gap_index : gap_index + 1);
  }

  return track_direction == kForRows ? GetInlineGapSize() : GetBlockGapSize();
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
