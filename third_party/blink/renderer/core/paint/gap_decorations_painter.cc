// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/gap_decorations_painter.h"

#include "base/notreached.h"
#include "third_party/blink/renderer/core/css/css_gap_decoration_property_utils.h"
#include "third_party/blink/renderer/core/layout/block_break_token.h"
#include "third_party/blink/renderer/core/layout/break_token_algorithm_data.h"
#include "third_party/blink/renderer/core/layout/fragmentation_utils.h"
#include "third_party/blink/renderer/core/layout/gap/gap_geometry.h"
#include "third_party/blink/renderer/core/layout/gap/gap_intersection.h"
#include "third_party/blink/renderer/core/layout/layout_box.h"
#include "third_party/blink/renderer/core/layout/physical_box_fragment.h"
#include "third_party/blink/renderer/core/paint/box_border_painter.h"
#include "third_party/blink/renderer/core/paint/paint_auto_dark_mode.h"
#include "third_party/blink/renderer/core/paint/paint_info.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/core/style/gap_data_list.h"

namespace blink {

namespace {

// Determines if the `start_index` should advance when determining pairs for gap
// decorations.
//
// https://drafts.csswg.org/css-gaps-1/#determine-pairs-of-gap-decoration-endpoints
bool ShouldMoveIntersectionStartForward(
    const GridTrackSizingDirection track_direction,
    wtf_size_t gap_index,
    wtf_size_t start_index,
    const RuleBreak rule_break,
    const RuleVisibilityItems rule_visibility,
    const GapGeometry& gap_geometry,
    const Vector<GapIntersection>& intersections) {
  const bool is_rule_segment_visible =
      CSSGapDecorationUtils::IsRuleSegmentVisible(
          intersections[start_index].SegmentState(), rule_visibility);

  // For flex containers, `start_index` cannot land on an open overlap state
  // i.e. the beginning of an overlap window, because that would start the
  // segment inside the overlapping region within the gap. It can land on a
  // close overlap state i.e. the end of an overlap window, because the overlap
  // has ended and it's a valid starting point for a new segment.
  if (gap_geometry.GetContainerType() == GapGeometry::ContainerType::kFlex) {
    if (intersections[start_index].IsOverlapWindowOpen()) {
      return true;
    } else if (intersections[start_index].IsOverlapWindowClose()) {
      return false;
    }
  }
  if (rule_break == RuleBreak::kNone) {
    // Even with no breaks at intersections, skip segments that are not visible
    // based on `rule-visibility-items`.
    return !is_rule_segment_visible;
  }

  const BlockedStatus blocked_status =
      GapGeometry::BlockedStatusFromGapStates(intersections, start_index);
  // Advance start if the segment it's blocked after or not visible.
  if (blocked_status.HasBlockedStatus(BlockedStatus::kBlockedAfter) ||
      !is_rule_segment_visible) {
    return true;
  }

  return false;
}

// Determines if the `end_index` should advance when determining pairs for gap
// decorations.
//
// https://drafts.csswg.org/css-gaps-1/#determine-pairs-of-gap-decoration-endpoints
bool ShouldMoveIntersectionEndForward(
    const GridTrackSizingDirection track_direction,
    wtf_size_t gap_index,
    wtf_size_t end_index,
    const RuleBreak rule_break,
    const RuleVisibilityItems rule_visibility,
    const GapGeometry& gap_geometry,
    const Vector<GapIntersection>& intersections) {
  if (!CSSGapDecorationUtils::IsRuleSegmentVisible(
          intersections[end_index].SegmentState(), rule_visibility)) {
    return false;
  }

  if (rule_break == RuleBreak::kNone) {
    return true;
  }

  const BlockedStatus blocked_status =
      GapGeometry::BlockedStatusFromGapStates(intersections, end_index);

  // For `kNormal` rule break, decorations break only at "T"
  // intersections, so we simply check that the intersection isn't blocked
  // after.
  //
  // https://drafts.csswg.org/css-gaps-1/#determine-pairs-of-gap-decoration-endpoints
  if (rule_break == RuleBreak::kNormal) {
    // Move forward only if the intersection is NOT blocked after.
    return !blocked_status.HasBlockedStatus(BlockedStatus::kBlockedAfter);
  }

  // For `kIntersection` rule break, decorations break at both "T" and
  // "cross" intersections, so we also need to check that the corresponding
  // intersection in the cross direction is flanked by spanning items.
  //
  // https://drafts.csswg.org/css-gaps-1/#determine-pairs-of-gap-decoration-endpoints
  DCHECK_EQ(rule_break, RuleBreak::kIntersection);

  if (gap_geometry.GetContainerType() == GapGeometry::ContainerType::kFlex) {
    // For flex, `end_index` cannot land on a close overlap state i.e. the end
    // of an overlap window, because the segment would extend across the
    // overlapping region within the gap. It can land on an open overlap state
    // i.e. the beginning of an overlap window, because it ends the segment
    // before the overlap starts.
    if (intersections[end_index].IsOverlapWindowClose()) {
      return true;
    } else if (intersections[end_index].IsOverlapWindowOpen()) {
      return false;
    }
    // For flex, intersections will never be blocked before or after by
    // other items, due to the absence of spanners. Therefore, we can
    // break at each intersection point.
    return false;
  }

  // If it's blocked after, don't move forward.
  if (blocked_status.HasBlockedStatus(BlockedStatus::kBlockedAfter)) {
    return false;
  }

  const GridTrackSizingDirection cross_direction =
      track_direction == kForColumns ? kForRows : kForColumns;

  // The following logic is only valid for grid containers.
  if (gap_geometry.GetContainerType() != GapGeometry::ContainerType::kGrid) {
    return false;
  }
  // Get the matching intersection in the cross direction by
  // swapping the indices. This transpose allows us determine if the
  // intersection is flanked by spanning items on opposing sides.
  // `end_index` should move forward if there are adjacent spanners in
  // the cross direction since that intersection won't form a T or cross
  // intersection.
  const BlockedStatus cross_gaps_blocked_status =
      gap_geometry.GetIntersectionBlockedStatus(cross_direction, end_index - 1,
                                                gap_index + 1, intersections);
  // Move forward if the cross intersection is flanked by spanners on both
  // sides.
  return cross_gaps_blocked_status.HasBlockedStatus(
             BlockedStatus::kBlockedAfter) &&
         cross_gaps_blocked_status.HasBlockedStatus(
             BlockedStatus::kBlockedBefore);
}

// Adjusts the (start, end) intersection pair to ensure that the gap
// decorations are painted correctly based on `rule_break` and
// `rule_visibility`.
void AdjustIntersectionIndexPair(GridTrackSizingDirection track_direction,
                                 wtf_size_t& start,
                                 wtf_size_t& end,
                                 wtf_size_t intersection_count,
                                 wtf_size_t gap_index,
                                 RuleBreak rule_break,
                                 RuleVisibilityItems rule_visibility,
                                 const GapGeometry& gap_geometry,
                                 const Vector<GapIntersection>& intersections) {
  const wtf_size_t last_intersection_index = intersection_count - 1;

  CHECK_LE(start, last_intersection_index);
  //  Advance `start` to the first intersection where painting can begin, based
  //  on blocked status from spanners and visibility from empty cells.
  while (start < last_intersection_index &&
         ShouldMoveIntersectionStartForward(track_direction, gap_index, start,
                                            rule_break, rule_visibility,
                                            gap_geometry, intersections)) {
    ++start;
  }

  // If `start` is at the last intersection point, there are no gap segments to
  // paint.
  if (start == last_intersection_index) {
    return;
  }

  // Advance `end` based on the `rule_break` and `rule_visibility`.
  end = start + 1;
  while (end < last_intersection_index &&
         ShouldMoveIntersectionEndForward(track_direction, gap_index, end,
                                          rule_break, rule_visibility,
                                          gap_geometry, intersections)) {
    ++end;
  }
}

}  // namespace

// TODO(samomekarajr): Consider refactoring the Paint method to improve
// modularity by ensuring each method handles a single responsibility.
void GapDecorationsPainter::Paint(GridTrackSizingDirection track_direction,
                                  const PaintInfo& paint_info,
                                  const PhysicalRect& paint_rect,
                                  const GapGeometry& gap_geometry) {
  const ComputedStyle& style = box_fragment_.Style();
  const bool is_column_gap = (track_direction == kForColumns);

  GapDataList<StyleColor> rule_colors =
      is_column_gap ? style.ColumnRuleColor() : style.RowRuleColor();
  GapDataList<EBorderStyle> rule_styles =
      is_column_gap ? style.ColumnRuleStyle() : style.RowRuleStyle();
  GapDataList<int> rule_widths =
      is_column_gap ? style.ColumnRuleWidth() : style.RowRuleWidth();
  RuleBreak rule_break = CSSGapDecorationUtils::ResolveRuleBreakValue(
      style, track_direction, gap_geometry.GetContainerType());

  RuleVisibilityItems rule_visibility =
      CSSGapDecorationUtils::ResolveRuleVisibilityItemsValue(
          style, gap_geometry.GetContainerType(), track_direction);

  const GridTrackSizingDirection cross_direction =
      track_direction == kForColumns ? kForRows : kForColumns;
  RuleVisibilityItems cross_rule_visibility =
      CSSGapDecorationUtils::ResolveRuleVisibilityItemsValue(
          style, gap_geometry.GetContainerType(), cross_direction);

  WritingModeConverter converter(style.GetWritingDirection(),
                                 box_fragment_.Size());
  AutoDarkMode auto_dark_mode(
      PaintAutoDarkMode(style, DarkModeFilter::ElementRole::kBackground));
  const BoxSide box_side =
      CSSGapDecorationUtils::BoxSideFromDirection(style, track_direction);

  const bool is_main = gap_geometry.IsMainDirection(track_direction);
  const wtf_size_t fragment_relative_gap_count =
      is_main ? gap_geometry.GetMainGaps().size()
              : gap_geometry.GetCrossGaps().size();

  const bool has_row_gap_fragmentation =
      gap_geometry.HasRowGapFragmentation(box_fragment_, is_main);

  wtf_size_t total_gap_count = fragment_relative_gap_count;
  if (has_row_gap_fragmentation) {
    const BreakTokenAlgorithmData* first_break_token_data =
        GetFirstFragmentBreakTokenData(box_fragment_);
    CHECK(first_break_token_data);
    total_gap_count = first_break_token_data->GetTotalRowGapCount();
  }

  // When `overlap-join` is specified, the decoration extends to meet the
  // cross-direction decoration's edge at junction intersections. This requires
  // knowing the cross-direction rule widths at each intersection point.
  const bool has_overlap_join =
      CSSGapDecorationUtils::HasOverlapJoin(style, is_column_gap);

  // Pre-expand cross-direction rule widths for `overlap-join` resolution. Each
  // junction intersection `i` corresponds to cross gap `i - 1`, whose
  // decoration width determines how far the decoration extends when
  // `overlap-join` is active.
  Vector<int> cross_decoration_widths;
  if (has_overlap_join) {
    const GapDataList<int>& cross_rule_widths =
        is_column_gap ? style.RowRuleWidth() : style.ColumnRuleWidth();
    const wtf_size_t cross_gap_count = is_main
                                           ? gap_geometry.GetCrossGaps().size()
                                           : gap_geometry.GetMainGaps().size();
    cross_decoration_widths = CSSGapDecorationUtils::GetExpandedWidths(
        cross_rule_widths, cross_gap_count);
  }

  auto width_iterator =
      GapDataListIterator<int>(rule_widths.GetGapDataList(), total_gap_count);
  auto style_iterator = GapDataListIterator<EBorderStyle>(
      rule_styles.GetGapDataList(), total_gap_count);
  auto color_iterator = GapDataListIterator<StyleColor>(
      rule_colors.GetGapDataList(), total_gap_count);

  // Reused across loop iterations: `GenerateIntersectionListForGap` resets the
  // size but preserves capacity, so this allocates at most once per Paint call
  // (or zero times if the loop is fully skipped, e.g. all multicol spanners).
  Vector<GapIntersection> intersections;

  for (wtf_size_t gap_index = 0; gap_index < fragment_relative_gap_count;
       ++gap_index) {
    // Make sure we skip any multicol `MainGap`s generated by spanners.
    // This is because those `MainGap`s are not painted, and only used to
    // generate the `CrossGap` intersections.
    if (gap_geometry.IsMultiColSpanner(gap_index, track_direction)) {
      continue;
    }
    // Gap decorations can take a list format for their styles, and that order
    // must be maintained when the container fragments. Resolve this gap's
    // `stitched_gap_index` (i.e. its index in the global unfragmented context)
    // and advance the iterators to that point so that the 'color', 'style' and
    // 'width' patterns are maintained across fragments.
    if (has_row_gap_fragmentation) {
      const wtf_size_t stitched_gap_index =
          box_fragment_.GetLayoutObject()->StitchedRowGapIndex(box_fragment_,
                                                               gap_index);
      color_iterator.AdvanceUpTo(stitched_gap_index);
      style_iterator.AdvanceUpTo(stitched_gap_index);
      width_iterator.AdvanceUpTo(stitched_gap_index);
    }
    const StyleColor rule_color = color_iterator.Next();
    const Color resolved_rule_color =
        style.VisitedDependentGapColor(rule_color, is_column_gap);
    const EBorderStyle rule_style =
        ComputedStyle::CollapsedBorderStyle(style_iterator.Next());
    const LayoutUnit rule_thickness = LayoutUnit(width_iterator.Next());

    const LayoutUnit center =
        gap_geometry.GetGapCenterOffset(track_direction, gap_index);

    gap_geometry.GenerateIntersectionListForGap(track_direction, gap_index,
                                                intersections);

    const wtf_size_t intersection_count = intersections.size();

    CHECK_GE(intersection_count, 2u);
    const wtf_size_t last_intersection_index = intersection_count - 1;
    wtf_size_t start = 0;
    while (start < last_intersection_index) {
      wtf_size_t end = start;
      AdjustIntersectionIndexPair(track_direction, start, end,
                                  intersection_count, gap_index, rule_break,
                                  rule_visibility, gap_geometry, intersections);
      if (start >= end) {
        // Break because there's no gap segment to paint.
        break;
      }

      const bool start_is_cap_intersection = gap_geometry.IsCapIntersection(
          cross_direction, gap_index, start, is_main, rule_visibility,
          cross_rule_visibility, intersections);
      const bool end_is_cap_intersection = gap_geometry.IsCapIntersection(
          cross_direction, gap_index, end, is_main, rule_visibility,
          cross_rule_visibility, intersections);

      // The `*inset_width` is the base value against which percentage inset
      // values are resolved. It is `0` for cap intersections (endpoints with
      // no crossing decoration to join with). For junction intersections it is
      // typically the cross gap width at that point. However, for flex
      // main-direction overlap intersections, the inset width is the size of
      // the overlap window.
      const LayoutUnit start_max_inset_width = gap_geometry.GetMaxInsetWidth(
          track_direction, gap_index, start, is_main, intersections);
      const LayoutUnit end_max_inset_width = gap_geometry.GetMaxInsetWidth(
          track_direction, gap_index, end, is_main, intersections);

      // For `overlap-join`, determine the cross-direction decoration width at
      // each intersection. Cap intersections have no cross decoration.
      LayoutUnit start_cross_decoration_width;
      LayoutUnit end_cross_decoration_width;
      if (has_overlap_join) {
        start_cross_decoration_width =
            gap_geometry.GetCrossDecorationWidthForIntersection(
                gap_index, start, is_main, intersections,
                start_is_cap_intersection, cross_decoration_widths);
        end_cross_decoration_width =
            gap_geometry.GetCrossDecorationWidthForIntersection(
                gap_index, end, is_main, intersections, end_is_cap_intersection,
                cross_decoration_widths);
      }

      // When `overlap-join` is active in a grid container with
      // `rule-visibility-items: between`, determine whether there is a
      // cross-direction joining decoration at the intersection. When true,
      // the inset extends to meet the cross decoration; otherwise it is 0.
      // Cap intersections have no crossing decoration to join with (either
      // at the container boundary or at dangling interior endpoints).
      const bool start_has_joining_decoration =
          has_overlap_join && !start_is_cap_intersection;
      const bool end_has_joining_decoration =
          has_overlap_join && !end_is_cap_intersection;

      // Inset values are used to offset the end points of gap decorations.
      // Percentage values are resolved against the `*inset_width` of the
      // intersection point.
      // https://drafts.csswg.org/css-gaps-1/#propdef-column-rule-inset
      LayoutUnit start_inset = gap_geometry.ComputeInsetStart(
          style, gap_index, start, intersections, start_is_cap_intersection,
          is_column_gap, is_main, start_has_joining_decoration,
          start_max_inset_width, start_cross_decoration_width);
      LayoutUnit end_inset = gap_geometry.ComputeInsetEnd(
          style, gap_index, end, intersections, end_is_cap_intersection,
          is_column_gap, is_main, end_has_joining_decoration,
          end_max_inset_width, end_cross_decoration_width);

      // `*_cross_width` is the width of the gap at the intersection point in
      // the cross axis, which is used to compute the gap decoration offset from
      // the intersection point.
      LayoutUnit start_cross_width = gap_geometry.GetCrossWidthForIntersection(
          track_direction, gap_index, start, is_main, intersections);
      LayoutUnit end_cross_width = gap_geometry.GetCrossWidthForIntersection(
          track_direction, gap_index, end, is_main, intersections);
      const LayoutUnit decoration_start_offset =
          (start_cross_width / 2) + start_inset;
      const LayoutUnit decoration_end_offset =
          (end_cross_width / 2) + end_inset;

      // Compute the primary axis values using the gap offsets.
      const LayoutUnit primary_start = center - (rule_thickness / 2);
      const LayoutUnit primary_size = rule_thickness;

      // Compute the secondary axis values using the intersection offsets.
      const LayoutUnit secondary_start =
          intersections[start].GetOffset() + decoration_start_offset;
      const LayoutUnit secondary_size = intersections[end].GetOffset() -
                                        secondary_start - decoration_end_offset;

      // Columns paint a vertical strip at the center of the gap while rows
      // paint horizontal strip at the center of the gap
      const LayoutUnit inline_start =
          is_column_gap ? primary_start : secondary_start;
      const LayoutUnit inline_size =
          is_column_gap ? primary_size : secondary_size;
      const LayoutUnit block_start =
          is_column_gap ? secondary_start : primary_start;
      const LayoutUnit block_size =
          is_column_gap ? secondary_size : primary_size;

      const LogicalRect gap_logical(inline_start, block_start, inline_size,
                                    block_size);
      PhysicalRect gap_rect = converter.ToPhysical(gap_logical);
      gap_rect.offset += paint_rect.offset;
      BoxBorderPainter::DrawBoxSide(
          paint_info.context, ToPixelSnappedRect(gap_rect), box_side,
          resolved_rule_color, rule_style, auto_dark_mode);

      start = end;
    }
  }
}

}  // namespace blink
