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
// Determines if the `end_index` should advance when determining pairs for gap
// decorations.
//
// https://drafts.csswg.org/css-gaps-1/#determine-pairs-of-gap-decoration-endpoints
bool ShouldMoveIntersectionEndForward(GridTrackSizingDirection track_direction,
                                      wtf_size_t gap_index,
                                      wtf_size_t end_index,
                                      RuleBreak rule_break,
                                      const GapGeometry& gap_geometry,
                                      const Vector<LayoutUnit>& intersections) {
  BlockedStatus blocked_status = gap_geometry.GetIntersectionBlockedStatus(
      track_direction, gap_index, end_index, intersections);

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
// decorations are painted correctly based on `rule_break`.
void AdjustIntersectionIndexPair(GridTrackSizingDirection track_direction,
                                 wtf_size_t& start,
                                 wtf_size_t& end,
                                 wtf_size_t intersection_count,
                                 wtf_size_t gap_index,
                                 RuleBreak rule_break,
                                 const GapGeometry& gap_geometry,
                                 const Vector<LayoutUnit>& intersections) {
  // If rule_break is `kNone`, cover the entire intersection range.
  const wtf_size_t last_intersection_index = intersection_count - 1;
  if (rule_break == RuleBreak::kNone) {
    start = 0;
    end = last_intersection_index;
    return;
  }

  // `start` should be the first intersection point that is not blocked
  // after.
  while (start < intersection_count &&
         (gap_geometry
              .GetIntersectionBlockedStatus(track_direction, gap_index, start,
                                            intersections)
              .HasBlockedStatus(BlockedStatus::kBlockedAfter))) {
    ++start;
  }

  // If `start` is the last intersection point, there are no gaps to
  // paint.
  if (start == last_intersection_index) {
    return;
  }

  end = start + 1;

  // Advance `end` based on the rule_break type.
  while (end < last_intersection_index &&
         ShouldMoveIntersectionEndForward(track_direction, gap_index, end,
                                          rule_break, gap_geometry,
                                          intersections)) {
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
  Length rule_outset =
      is_column_gap ? style.ColumnRuleOutset() : style.RowRuleOutset();
  RuleBreak rule_break =
      is_column_gap ? style.ColumnRuleBreak() : style.RowRuleBreak();

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
                                  gap_geometry, intersections);
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
      const LayoutUnit start_width =
          gap_geometry.IsEdgeIntersection(
              gap_index, start, intersections.size(), is_main, intersections)
              ? LayoutUnit()
              : cross_gutter_width;
      const LayoutUnit end_width =
          gap_geometry.IsEdgeIntersection(gap_index, end, intersections.size(),
                                          is_main, intersections)
              ? LayoutUnit()
              : cross_gutter_width;

      // Outset values are used to offset the end points of gap decorations.
      // Percentage values are resolved against the crossing gap width of the
      // intersection point.
      // https://drafts.csswg.org/css-gaps-1/#propdef-column-rule-outset
      const LayoutUnit start_outset = ValueForLength(rule_outset, start_width);
      const LayoutUnit end_outset = ValueForLength(rule_outset, end_width);

      // Compute the gap decorations offset as half of the `crossing_gap_width`
      // minus the outset.
      // https://drafts.csswg.org/css-gaps-1/#compute-the-offset
      const LayoutUnit decoration_start_offset =
          (start_width / 2) - start_outset;
      const LayoutUnit decoration_end_offset = (end_width / 2) - end_outset;

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
