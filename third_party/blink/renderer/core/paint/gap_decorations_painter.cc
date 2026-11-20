// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/gap_decorations_painter.h"

#include <optional>

#include "base/notreached.h"
#include "third_party/blink/renderer/core/css/css_gap_decoration_property_utils.h"
#include "third_party/blink/renderer/core/layout/flex/flex_break_token_data.h"
#include "third_party/blink/renderer/core/layout/fragmentation_utils.h"
#include "third_party/blink/renderer/core/layout/gap/gap_geometry.h"
#include "third_party/blink/renderer/core/layout/gap/gap_intersection.h"
#include "third_party/blink/renderer/core/layout/grid/grid_break_token_data.h"
#include "third_party/blink/renderer/core/layout/layout_box.h"
#include "third_party/blink/renderer/core/layout/multicol_break_token_data.h"
#include "third_party/blink/renderer/core/layout/physical_box_fragment.h"
#include "third_party/blink/renderer/core/paint/box_border_painter.h"
#include "third_party/blink/renderer/core/paint/paint_auto_dark_mode.h"
#include "third_party/blink/renderer/core/paint/paint_info.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/core/style/gap_data_list.h"

namespace blink {

namespace {

struct GapEndpointContext {
  STACK_ALLOCATED();

 public:
  GapEndpointContext(const ComputedStyle& style,
                     GridTrackSizingDirection track_direction,
                     const GapGeometry& gap_geometry)
      : style(style),
        track_direction(track_direction),
        cross_direction(OppositeDirection(track_direction)),
        is_column_gap(track_direction == kForColumns),
        is_main(gap_geometry.IsMainDirection(track_direction)),
        rule_visibility(CSSGapDecorationUtils::ResolveRuleVisibilityItemsValue(
            style,
            gap_geometry.GetContainerType(),
            track_direction)),
        cross_rule_visibility(
            CSSGapDecorationUtils::ResolveRuleVisibilityItemsValue(
                style,
                gap_geometry.GetContainerType(),
                cross_direction)),
        has_overlap_join(
            CSSGapDecorationUtils::HasOverlapJoin(style, is_column_gap)) {
    // When `overlap-join` is specified, the decoration extends to meet the
    // cross-direction decoration's edge at junction intersections. This
    // requires knowing the cross-direction rule widths at each intersection
    // point.

    // Pre-expand cross-direction rule widths for `overlap-join` resolution.
    // The crossing decoration's width determines how far each endpoint
    // extends.
    if (has_overlap_join) {
      if (!ComputedStyle::BorderStyleIsVisible(
              is_column_gap ? style.RowRuleStyle() : style.ColumnRuleStyle())) {
        single_cross_decoration_width = LayoutUnit();
        return;
      }

      const GapDataList<int>& cross_rule_widths =
          is_column_gap ? style.RowRuleWidth() : style.ColumnRuleWidth();
      // When all crossing rules share one width, store it directly instead of
      // building a per-gap width list and looking up each intersection's gap.
      if (cross_rule_widths.HasSingleValue()) {
        single_cross_decoration_width =
            LayoutUnit(cross_rule_widths.GetSingleValue());
      } else {
        cross_decoration_widths = gap_geometry.ResolveCrossDecorationWidths(
            cross_direction, cross_rule_widths);
      }
    }
  }

  const ComputedStyle& style;
  GridTrackSizingDirection track_direction;
  GridTrackSizingDirection cross_direction;
  bool is_column_gap;
  bool is_main;
  RuleVisibilityItems rule_visibility;
  RuleVisibilityItems cross_rule_visibility;
  bool has_overlap_join;
  std::optional<LayoutUnit> single_cross_decoration_width;
  Vector<int> cross_decoration_widths;
};

enum class SegmentEndpoint { kStart, kEnd };

LayoutUnit ComputeEndpointOffset(const GapEndpointContext& context,
                                 const GapGeometry& gap_geometry,
                                 wtf_size_t gap_index,
                                 wtf_size_t intersection_index,
                                 const Vector<GapIntersection>& intersections,
                                 SegmentEndpoint endpoint) {
  // `cross_width` is the width of the gap at the intersection point in
  // the cross axis, which is used to compute the gap decoration offset from
  // the intersection point.
  const LayoutUnit cross_width = gap_geometry.GetCrossWidthForIntersection(
      context.track_direction, gap_index, intersection_index, context.is_main,
      intersections);
  const bool is_cap_intersection = gap_geometry.IsCapIntersection(
      context.cross_direction, gap_index, intersection_index, context.is_main,
      context.rule_visibility, context.cross_rule_visibility, intersections);

  // The `*inset_width` is the base value against which percentage inset
  // values are resolved. It is `0` at container edges. For other
  // intersections it is typically the cross gap width at that point.
  // However, for flex and grid-lanes main-direction overlap intersections,
  // the inset width is the size of the overlap window.
  const LayoutUnit max_inset_width = gap_geometry.GetMaxInsetWidth(
      context.track_direction, gap_index, intersection_index, context.is_main,
      intersections);

  // When `overlap-join` is active, determine whether there is a cross-direction
  // joining decoration at the intersection. When true, the inset extends to
  // meet the cross decoration; otherwise it is 0.
  // Cap intersections have no crossing decoration to join with (either
  // at the container boundary or at dangling interior endpoints).
  const bool has_joining_decoration =
      context.has_overlap_join && !is_cap_intersection;

  // For `overlap-join`, determine the cross-direction decoration width at
  // each intersection. Cap intersections have no cross decoration.
  LayoutUnit cross_decoration_width;
  if (has_joining_decoration) {
    cross_decoration_width =
        context.single_cross_decoration_width
            ? *context.single_cross_decoration_width
            : gap_geometry.GetCrossDecorationWidthForIntersection(
                  gap_index, intersection_index, context.is_main, intersections,
                  context.cross_decoration_widths);
  }

  // Inset values are used to offset the end points of gap decorations.
  // Percentage values are resolved against the `*inset_width` of the
  // intersection point.
  // https://drafts.csswg.org/css-gaps-1/#propdef-column-rule-inset
  const LayoutUnit inset =
      endpoint == SegmentEndpoint::kStart
          ? gap_geometry.ComputeInsetStart(
                context.style, is_cap_intersection, context.is_column_gap,
                context.is_main, has_joining_decoration, max_inset_width,
                cross_decoration_width)
          : gap_geometry.ComputeInsetEnd(
                context.style, is_cap_intersection, context.is_column_gap,
                context.is_main, has_joining_decoration, max_inset_width,
                cross_decoration_width);
  return (cross_width / 2) + inset;
}

PhysicalRect GapSegmentRect(GridTrackSizingDirection track_direction,
                            const WritingModeConverter& converter,
                            const PhysicalOffset& paint_offset,
                            LayoutUnit center,
                            LayoutUnit thickness,
                            LayoutUnit secondary_start,
                            LayoutUnit secondary_size) {
  // Compute the primary axis values using the gap offsets.
  const LayoutUnit primary_start = center - (thickness / 2);
  const LayoutUnit primary_size = thickness;
  const bool is_column_gap = track_direction == kForColumns;

  // Columns paint a vertical strip at the center of the gap while rows
  // paint horizontal strip at the center of the gap
  const LayoutUnit inline_start =
      is_column_gap ? primary_start : secondary_start;
  const LayoutUnit inline_size = is_column_gap ? primary_size : secondary_size;
  const LayoutUnit block_start =
      is_column_gap ? secondary_start : primary_start;
  const LayoutUnit block_size = is_column_gap ? secondary_size : primary_size;

  const LogicalRect gap_logical(inline_start, block_start, inline_size,
                                block_size);
  PhysicalRect gap_rect = converter.ToPhysical(gap_logical);
  gap_rect.offset += paint_offset;
  return gap_rect;
}

// Returns the total number of row gaps, including gaps suppressed by
// fragmentation. Grid and flex store the total on the first fragment. For
// multicol, it is derived from the last one.
wtf_size_t StitchedRowGapCount(const PhysicalBoxFragment& box_fragment,
                               const GapGeometry& gap_geometry) {
  if (gap_geometry.GetContainerType() !=
      GapGeometry::ContainerType::kMultiColumn) {
    const BreakTokenAlgorithmData* first_fragment_data =
        GetFirstFragmentBreakTokenData(box_fragment);
    CHECK(first_fragment_data);
    // Only grid and flex break tokens carry the full row-gap count.
    if (const auto* grid_data =
            DynamicTo<GridBreakTokenData>(first_fragment_data)) {
      return grid_data->GetTotalRowGapCount();
    }
    return To<FlexBreakTokenData>(first_fragment_data)->GetTotalRowGapCount();
  }

  const auto* box = To<LayoutBox>(box_fragment.GetLayoutObject());
  const wtf_size_t fragment_count = box->PhysicalFragmentCount();
  CHECK_GT(fragment_count, 0u);
  const PhysicalBoxFragment& last_fragment =
      *box->GetPhysicalFragment(fragment_count - 1);

  wtf_size_t total_row_gap_count = 0;
  if (const BlockBreakToken* previous_break_token =
          FindPreviousBreakToken(last_fragment)) {
    if (const auto* data = DynamicTo<MulticolBreakTokenData>(
            previous_break_token->TokenData())) {
      total_row_gap_count = data->GetFirstUnprocessedRowGapIndex();
    }
  }
  if (const GapGeometry* last_gap_geometry = last_fragment.GetGapGeometry()) {
    total_row_gap_count += last_gap_geometry->PaintableGapCount(kForRows);
  }
  return total_row_gap_count;
}

wtf_size_t ResolveGapSlotCount(
    const PhysicalBoxFragment& box_fragment,
    const GapGeometry& gap_geometry,
    GridTrackSizingDirection track_direction,
    bool has_row_gap_fragmentation,
    std::optional<wtf_size_t> stored_gap_slot_count) {
  // TODO(javiercon): The choice between the stored, stitched, and
  // fragment-local gap slot counts below, and the stitched gap index in the gap
  // loop, is container knowledge. Move it behind one container-agnostic entry
  // point in the gap geometry.
  //
  // Total gap slot count used to resolve the current color, style, and width
  // lists. Grid-lanes may replace it with a lane-local count, since decoration
  // assignments reset across lanes. Gaps that cannot be painted, such as
  // multicol spanner main gaps, do not consume decoration values.
  if (stored_gap_slot_count) {
    return *stored_gap_slot_count;
  }
  if (has_row_gap_fragmentation) {
    return StitchedRowGapCount(box_fragment, gap_geometry);
  }
  return gap_geometry.PaintableGapCount(track_direction);
}

// Determines if the `start_index` should advance when determining pairs for gap
// decorations.
//
// https://drafts.csswg.org/css-gaps-1/#determine-pairs-of-gap-decoration-endpoints
bool ShouldMoveIntersectionStartForward(
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

  // A rule segment cannot start at the opening of a grid-lanes overlap window
  // because the start of the overlap window marks the end of a segment.
  if (gap_geometry.GetContainerType() ==
          GapGeometry::ContainerType::kGridLanes &&
      intersections[start_index].IsOverlapWindowOpen()) {
    return true;
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
    if (gap_geometry.GetContainerType() ==
            GapGeometry::ContainerType::kGridLanes &&
        intersections[end_index].IsOverlapWindowOpen()) {
      // Continue the rule across the window only if no spanner blocks either
      // the window or the segment after it.
      if (blocked_status.HasBlockedStatus(BlockedStatus::kBlockedAfter)) {
        return false;
      }

      const GapIntersection& closing = intersections[end_index + 1];
      CHECK(closing.IsOverlapWindowClose());
      return !closing.SegmentState().HasGapStatus(GapSegmentState::kBlocked);
    }
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

  // Artificial boundaries split visibility and spanner state, not crossing
  // gutters. An intersection break must not split an otherwise visible rule.
  // See the GridLanesGapGeometry class comment in grid_lanes_gap_geometry.h.
  if (intersections[end_index].IsArtificial()) {
    return true;
  }

  const GridTrackSizingDirection cross_direction =
      OppositeDirection(track_direction);

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
         ShouldMoveIntersectionStartForward(start, rule_break, rule_visibility,
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

struct GapDecoration {
  Color color;
  EBorderStyle style;
  LayoutUnit thickness;
};

// Resolves decoration values for paintable gaps in geometric paint order.
// Keeps the value accessors and gap cursors across successive Resolve() calls.
struct GapDecorationValueResolver {
  STACK_ALLOCATED();

 public:
  GapDecorationValueResolver(const ComputedStyle& style,
                             const GapGeometry& gap_geometry,
                             GridTrackSizingDirection track_direction,
                             wtf_size_t gap_slot_count)
      : width_accessor((track_direction == kForColumns ? style.ColumnRuleWidth()
                                                       : style.RowRuleWidth())
                           .GetGapDataList(),
                       gap_slot_count),
        style_accessor((track_direction == kForColumns ? style.ColumnRuleStyle()
                                                       : style.RowRuleStyle())
                           .GetGapDataList(),
                       gap_slot_count),
        color_accessor((track_direction == kForColumns ? style.ColumnRuleColor()
                                                       : style.RowRuleColor())
                           .GetGapDataList(),
                       gap_slot_count) {
    if (gap_geometry.AreGapsOwned(track_direction)) {
      cross_gap_owner_index = 0u;
    }
  }

  GapDecoration Resolve(wtf_size_t gap_index,
                        const PhysicalBoxFragment& box_fragment,
                        const ComputedStyle& style,
                        const GapGeometry& gap_geometry,
                        GridTrackSizingDirection track_direction,
                        bool needs_stitched_gap_index,
                        wtf_size_t gap_slot_count) {
    // For flex and grid-lanes cross gaps, advance the owner cursor to the flex
    // line or grid lane that owns this gap. We need the owner to generate the
    // gap's intersections, and for flex to resolve its stitched index across
    // fragments.
    if (cross_gap_owner_index) {
      gap_geometry.AdvanceCrossGapOwnerCursor(gap_index,
                                              *cross_gap_owner_index);
    }
    // `stitched_gap_index` is this gap's index in the full container's
    // geometric paint order.
    wtf_size_t stitched_gap_index = paint_order_gap_index++;
    // A stored gap slot count comes with stored decoration value indices, so
    // these gaps do not need a stitched index.
    if (needs_stitched_gap_index) {
      stitched_gap_index = box_fragment.GetLayoutObject()->StitchedRowGapIndex(
          box_fragment, stitched_gap_index, cross_gap_owner_index);
    }
    const GapGeometry::DecorationValueAssignment decoration_value_assignment =
        gap_geometry.DecorationValueAssignmentForGap(
            track_direction, gap_index, stitched_gap_index, gap_slot_count,
            cross_gap_owner_index);
    width_accessor.SetGapSlotCount(decoration_value_assignment.gap_count);
    style_accessor.SetGapSlotCount(decoration_value_assignment.gap_count);
    color_accessor.SetGapSlotCount(decoration_value_assignment.gap_count);
    const StyleColor rule_color =
        color_accessor.ValueAt(decoration_value_assignment.value_index);
    const EBorderStyle rule_style_value =
        style_accessor.ValueAt(decoration_value_assignment.value_index);
    const int rule_width_value =
        width_accessor.ValueAt(decoration_value_assignment.value_index);
    return {style.VisitedDependentGapColor(rule_color,
                                           track_direction == kForColumns),
            ComputedStyle::CollapsedBorderStyle(rule_style_value),
            LayoutUnit(rule_width_value)};
  }

  GapDataListValueAccessor<int> width_accessor;
  GapDataListValueAccessor<EBorderStyle> style_accessor;
  GapDataListValueAccessor<StyleColor> color_accessor;

  // For flex and grid-lanes cross gaps, track the owner (flex line or grid
  // lane) of the current cross gap as a forward-only cursor. The owner is
  // needed to generate the gap's intersections, and for flex it also resolves
  // the stitched gap index across fragments.
  std::optional<wtf_size_t> cross_gap_owner_index;

  // Multicol spanner gaps are stored but skipped below, so track the index
  // among painted gaps separately.
  wtf_size_t paint_order_gap_index = 0;
};

void PaintGapSegments(const GapEndpointContext& context,
                      const GapGeometry& gap_geometry,
                      wtf_size_t gap_index,
                      const GapDecoration& decoration,
                      const Vector<GapIntersection>& intersections,
                      RuleBreak rule_break,
                      const WritingModeConverter& converter,
                      const PhysicalOffset& paint_offset,
                      BoxSide box_side,
                      const AutoDarkMode& auto_dark_mode,
                      GraphicsContext& graphics_context) {
  const LayoutUnit center =
      gap_geometry.GetGapCenterOffset(context.track_direction, gap_index);
  const wtf_size_t intersection_count = intersections.size();
  CHECK_GE(intersection_count, 2u);

  const wtf_size_t last_intersection_index = intersection_count - 1;
  wtf_size_t start = 0;
  while (start < last_intersection_index) {
    wtf_size_t end = start;
    AdjustIntersectionIndexPair(
        context.track_direction, start, end, intersection_count, gap_index,
        rule_break, context.rule_visibility, gap_geometry, intersections);
    if (start >= end) {
      // Break because there's no gap segment to paint.
      break;
    }

    const LayoutUnit decoration_start_offset =
        ComputeEndpointOffset(context, gap_geometry, gap_index, start,
                              intersections, SegmentEndpoint::kStart);
    const LayoutUnit decoration_end_offset =
        ComputeEndpointOffset(context, gap_geometry, gap_index, end,
                              intersections, SegmentEndpoint::kEnd);

    // Compute the secondary axis values using the intersection offsets.
    const LayoutUnit secondary_start =
        intersections[start].GetOffset() + decoration_start_offset;
    const LayoutUnit secondary_size = intersections[end].GetOffset() -
                                      secondary_start - decoration_end_offset;

    const PhysicalRect gap_rect =
        GapSegmentRect(context.track_direction, converter, paint_offset, center,
                       decoration.thickness, secondary_start, secondary_size);
    BoxBorderPainter::DrawBoxSide(
        graphics_context, ToPixelSnappedRect(gap_rect), box_side,
        decoration.color, decoration.style, auto_dark_mode);

    start = end;
  }
}

}  // namespace

void GapDecorationsPainter::Paint(GridTrackSizingDirection track_direction,
                                  const PaintInfo& paint_info,
                                  const PhysicalRect& paint_rect,
                                  const GapGeometry& gap_geometry) {
  const ComputedStyle& style = box_fragment_.Style();
  const GapEndpointContext context(style, track_direction, gap_geometry);
  const RuleBreak rule_break = CSSGapDecorationUtils::ResolveRuleBreakValue(
      style, track_direction, gap_geometry.GetContainerType());
  WritingModeConverter converter(style.GetWritingDirection(),
                                 box_fragment_.Size());
  AutoDarkMode auto_dark_mode(
      PaintAutoDarkMode(style, DarkModeFilter::ElementRole::kBackground));
  const BoxSide box_side =
      CSSGapDecorationUtils::BoxSideFromDirection(style, track_direction);

  const wtf_size_t fragment_relative_gap_count =
      context.is_main ? gap_geometry.GetMainGaps().size()
                      : gap_geometry.GetCrossGaps().size();

  const bool has_row_gap_fragmentation =
      gap_geometry.HasRowGapFragmentation(box_fragment_, context.is_main);
  const std::optional<wtf_size_t> stored_gap_slot_count =
      gap_geometry.StoredGapSlotCount(track_direction);
  const bool needs_stitched_gap_index =
      has_row_gap_fragmentation && !stored_gap_slot_count;
  const wtf_size_t gap_slot_count =
      ResolveGapSlotCount(box_fragment_, gap_geometry, track_direction,
                          has_row_gap_fragmentation, stored_gap_slot_count);
  GapDecorationValueResolver decoration_resolver(
      style, gap_geometry, track_direction, gap_slot_count);

  // Reused across gaps. Grid and multicol allocate at most once per Paint;
  // flex grows capacity only when a later gap needs more.
  Vector<GapIntersection> intersections;

  for (wtf_size_t gap_index = 0; gap_index < fragment_relative_gap_count;
       ++gap_index) {
    // Skip gaps that cannot be painted, such as multicol spanner `MainGap`s.
    // Those gaps are only used to generate the `CrossGap` intersections.
    if (!gap_geometry.IsPaintableGap(track_direction, gap_index)) {
      continue;
    }
    const GapDecoration decoration = decoration_resolver.Resolve(
        gap_index, box_fragment_, style, gap_geometry, track_direction,
        needs_stitched_gap_index, gap_slot_count);

    gap_geometry.GenerateIntersectionListForGap(
        track_direction, gap_index, decoration_resolver.cross_gap_owner_index,
        context.rule_visibility, intersections);

    PaintGapSegments(context, gap_geometry, gap_index, decoration,
                     intersections, rule_break, converter, paint_rect.offset,
                     box_side, auto_dark_mode, paint_info.context);
  }
}

}  // namespace blink
