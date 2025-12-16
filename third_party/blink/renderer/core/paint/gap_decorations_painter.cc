// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/gap_decorations_painter.h"

#include "third_party/blink/renderer/core/css/css_gap_decoration_property_utils.h"
#include "third_party/blink/renderer/core/layout/gap/gap_geometry.h"
#include "third_party/blink/renderer/core/layout/physical_box_fragment.h"
#include "third_party/blink/renderer/core/paint/box_border_painter.h"
#include "third_party/blink/renderer/core/paint/box_fragment_painter.h"
#include "third_party/blink/renderer/core/paint/paint_auto_dark_mode.h"
#include "third_party/blink/renderer/core/paint/paint_info.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/core/style/gap_data_list.h"

namespace blink {

namespace {

// Determines if the segment at `secondary_index` within the gap at `gap_index`
// is visible based on `rule_visibility`.
bool IsRuleSegmentVisible(const GridTrackSizingDirection track_direction,
                          wtf_size_t gap_index,
                          wtf_size_t secondary_index,
                          const RuleVisibilityItems rule_visibility,
                          const GapGeometry& gap_geometry) {
  GapSegmentState gap_state = gap_geometry.GetIntersectionGapSegmentState(
      track_direction, gap_index, secondary_index);

  switch (rule_visibility) {
    case RuleVisibilityItems::kAll:
      return true;
    case RuleVisibilityItems::kNone:
      return false;
    case RuleVisibilityItems::kAround:
      // Paint if either side of the segment is occupied (i.e. not empty on both
      // sides).
      return !gap_state.IsEmpty();
    case RuleVisibilityItems::kBetween:
      // Paint only when both sides of the segment are occupied (i.e. gap
      // segment state is none as it represents a segment occupied on both
      // sides).
      return gap_state.status_ == GapSegmentState::kNone;
  }

  NOTREACHED();
}

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
    const Vector<LayoutUnit>& intersections) {
  if (rule_break == RuleBreak::kNone) {
    return false;
  }

  const BlockedStatus blocked_status =
      gap_geometry.GetIntersectionBlockedStatus(track_direction, gap_index,
                                                start_index, intersections);
  // Advance start if the segment it's blocked after or not visible.
  if (blocked_status.HasBlockedStatus(BlockedStatus::kBlockedAfter) ||
      !IsRuleSegmentVisible(track_direction, gap_index, start_index,
                            rule_visibility, gap_geometry)) {
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
    const Vector<LayoutUnit>& intersections) {
  if (!IsRuleSegmentVisible(track_direction, gap_index, end_index,
                            rule_visibility, gap_geometry)) {
    return false;
  }

  if (rule_break == RuleBreak::kNone) {
    return true;
  }

  const BlockedStatus blocked_status =
      gap_geometry.GetIntersectionBlockedStatus(track_direction, gap_index,
                                                end_index, intersections);

  // For `kSpanningItem` rule break, decorations break only at "T"
  // intersections, so we simply check that the intersection isn't blocked
  // after.
  //
  // https://drafts.csswg.org/css-gaps-1/#determine-pairs-of-gap-decoration-endpoints
  if (rule_break == RuleBreak::kSpanningItem) {
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
                                 const Vector<LayoutUnit>& intersections) {
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
      style, gap_geometry.GetContainerType(), track_direction);

  RuleVisibilityItems rule_visibility = is_column_gap
                                            ? style.ColumnRuleVisibilityItems()
                                            : style.RowRuleVisibilityItems();

  WritingModeConverter converter(style.GetWritingDirection(),
                                 box_fragment_.Size());
  AutoDarkMode auto_dark_mode(
      PaintAutoDarkMode(style, DarkModeFilter::ElementRole::kBackground));
  const BoxSide box_side =
      CSSGapDecorationUtils::BoxSideFromDirection(style, track_direction);

  const LayoutUnit cross_gutter_width = track_direction == kForRows
                                            ? gap_geometry.GetInlineGapSize()
                                            : gap_geometry.GetBlockGapSize();

  const bool is_main = gap_geometry.IsMainDirection(track_direction);
  const wtf_size_t gap_count = is_main ? gap_geometry.GetMainGaps().size()
                                       : gap_geometry.GetCrossGaps().size();

  auto width_iterator =
      GapDataListIterator<int>(rule_widths.GetGapDataList(), gap_count);
  auto style_iterator = GapDataListIterator<EBorderStyle>(
      rule_styles.GetGapDataList(), gap_count);
  auto color_iterator =
      GapDataListIterator<StyleColor>(rule_colors.GetGapDataList(), gap_count);

  for (wtf_size_t gap_index = 0; gap_index < gap_count; ++gap_index) {
    // Make sure we skip any multicol `MainGap`s generated by spanners.
    // This is because those `MainGap`s are not painted, and only used to
    // generate the `CrossGap` intersections.
    if (gap_geometry.IsMultiColSpanner(gap_index, track_direction)) {
      continue;
    }
    const StyleColor rule_color = color_iterator.Next();
    const Color resolved_rule_color =
        style.VisitedDependentGapColor(rule_color, style, is_column_gap);
    const EBorderStyle rule_style =
        ComputedStyle::CollapsedBorderStyle(style_iterator.Next());
    const LayoutUnit rule_thickness = LayoutUnit(width_iterator.Next());

    const LayoutUnit center =
        gap_geometry.GetGapCenterOffset(track_direction, gap_index);

    const Vector<LayoutUnit> intersections =
        gap_geometry.GenerateIntersectionListForGap(track_direction, gap_index);

    const wtf_size_t last_intersection_index = intersections.size() - 1;
    wtf_size_t start = 0;
    while (start < last_intersection_index) {
      wtf_size_t end = start;
      AdjustIntersectionIndexPair(track_direction, start, end,
                                  intersections.size(), gap_index, rule_break,
                                  rule_visibility, gap_geometry, intersections);
      if (start >= end) {
        // Break because there's no gap segment to paint.
        break;
      }

      // The cross gutter size is used to determine the "crossing gap width" at
      // intersection points. The crossing gap width of an intersection point is
      // defined as:
      // * `0` if the intersection is at the content edge of the container.
      // * The cross gutter size if it is an intersection with another gap.
      // https://drafts.csswg.org/css-gaps-1/#crossing-gap-width
      //
      // TODO(crbug.com/446616449): Recently we have resolved to always use the
      // cross gutter size for resolving the "crossing gap width", however, it
      // is still an open question what this means for multicol containers where
      // intersection points don't actually intersect another gap. As a result,
      // for now, we continue to resolve the crossing gap width as `0` for any
      // intersection in multicol containers. Discussion about this can be found
      // in https://github.com/w3c/csswg-drafts/issues/12784.
      const LayoutUnit start_width =
          gap_geometry.GetContainerType() ==
                      GapGeometry::ContainerType::kMultiColumn ||
                  gap_geometry.IsEdgeIntersection(gap_index, start,
                                                  intersections.size(), is_main,
                                                  intersections)
              ? LayoutUnit()
              : cross_gutter_width;
      const LayoutUnit end_width =
          gap_geometry.GetContainerType() ==
                      GapGeometry::ContainerType::kMultiColumn ||
                  gap_geometry.IsEdgeIntersection(gap_index, end,
                                                  intersections.size(), is_main,
                                                  intersections)
              ? LayoutUnit()
              : cross_gutter_width;

      // Inset values are used to offset the end points of gap decorations.
      // Percentage values are resolved against the crossing gap width of the
      // intersection point.
      // https://drafts.csswg.org/css-gaps-1/#propdef-column-rule-inset
      LayoutUnit start_inset =
          gap_geometry.ComputeInsetStart(style, gap_index, start, intersections,
                                         is_column_gap, is_main, start_width);
      LayoutUnit end_inset =
          gap_geometry.ComputeInsetEnd(style, gap_index, end, intersections,
                                       is_column_gap, is_main, end_width);
      // Compute the gap decorations offset as half of the `crossing_gap_width`
      // plus the inset.
      // https://drafts.csswg.org/css-gaps-1/#compute-the-offset
      const LayoutUnit decoration_start_offset =
          (start_width / 2) + start_inset;
      const LayoutUnit decoration_end_offset = (end_width / 2) + end_inset;

      // Compute the primary axis values using the gap offsets.
      const LayoutUnit primary_start = center - (rule_thickness / 2);
      const LayoutUnit primary_size = rule_thickness;

      // Compute the secondary axis values using the intersection offsets.
      const LayoutUnit secondary_start =
          intersections[start] + decoration_start_offset;
      const LayoutUnit secondary_size =
          intersections[end] - secondary_start - decoration_end_offset;

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
