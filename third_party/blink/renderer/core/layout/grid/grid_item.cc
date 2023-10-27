// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/grid/grid_item.h"

#include "third_party/blink/renderer/core/layout/grid/grid_placement.h"
#include "third_party/blink/renderer/platform/text/writing_mode_utils.h"

namespace blink {

namespace {

// Given an |item_style| determines the correct |AxisEdge| alignment.
// Additionally will determine:
//  - The behavior of 'auto' via the |auto_behavior| out-parameter.
//  - If the alignment is safe via the |is_overflow_safe| out-parameter.
AxisEdge AxisEdgeFromItemPosition(bool is_inline_axis,
                                  bool is_replaced,
                                  bool is_out_of_flow,
                                  const ComputedStyle& item_style,
                                  const ComputedStyle& root_grid_style,
                                  NGAutoBehavior* auto_behavior,
                                  bool* is_overflow_safe) {
  DCHECK(auto_behavior && is_overflow_safe);

  const auto& alignment = is_inline_axis
                              ? item_style.ResolvedJustifySelf(
                                    ItemPosition::kNormal, &root_grid_style)
                              : item_style.ResolvedAlignSelf(
                                    ItemPosition::kNormal, &root_grid_style);

  *auto_behavior = NGAutoBehavior::kFitContent;
  *is_overflow_safe = alignment.Overflow() == OverflowAlignment::kSafe;

  // Auto-margins take precedence over any alignment properties.
  if (item_style.MayHaveMargin() && !is_out_of_flow) {
    const bool is_start_auto =
        is_inline_axis
            ? item_style.MarginInlineStartUsing(root_grid_style).IsAuto()
            : item_style.MarginBlockStartUsing(root_grid_style).IsAuto();
    const bool is_end_auto =
        is_inline_axis
            ? item_style.MarginInlineEndUsing(root_grid_style).IsAuto()
            : item_style.MarginBlockEndUsing(root_grid_style).IsAuto();

    // 'auto' margin alignment is always "safe".
    if (is_start_auto || is_end_auto)
      *is_overflow_safe = true;

    if (is_start_auto && is_end_auto)
      return AxisEdge::kCenter;
    else if (is_start_auto)
      return AxisEdge::kEnd;
    else if (is_end_auto)
      return AxisEdge::kStart;
  }

  const auto root_grid_writing_direction =
      root_grid_style.GetWritingDirection();
  const auto item_position = alignment.GetPosition();

  switch (item_position) {
    case ItemPosition::kSelfStart:
    case ItemPosition::kSelfEnd: {
      // In order to determine the correct "self" axis-edge without a
      // complicated set of if-branches we use two converters.

      // First use the grid-item's writing-direction to convert the logical
      // edge into the physical coordinate space.
      LogicalToPhysical<AxisEdge> physical(item_style.GetWritingDirection(),
                                           AxisEdge::kStart, AxisEdge::kEnd,
                                           AxisEdge::kStart, AxisEdge::kEnd);

      // Then use the container's writing-direction to convert the physical
      // edges, into our logical coordinate space.
      PhysicalToLogical<AxisEdge> logical(root_grid_writing_direction,
                                          physical.Top(), physical.Right(),
                                          physical.Bottom(), physical.Left());

      if (is_inline_axis) {
        return item_position == ItemPosition::kSelfStart ? logical.InlineStart()
                                                         : logical.InlineEnd();
      }
      return item_position == ItemPosition::kSelfStart ? logical.BlockStart()
                                                       : logical.BlockEnd();
    }
    case ItemPosition::kCenter:
      return AxisEdge::kCenter;
    case ItemPosition::kFlexStart:
    case ItemPosition::kStart:
      return AxisEdge::kStart;
    case ItemPosition::kFlexEnd:
    case ItemPosition::kEnd:
      return AxisEdge::kEnd;
    case ItemPosition::kStretch:
      *auto_behavior = NGAutoBehavior::kStretchExplicit;
      return AxisEdge::kStart;
    case ItemPosition::kBaseline:
      return AxisEdge::kFirstBaseline;
    case ItemPosition::kLastBaseline:
      return AxisEdge::kLastBaseline;
    case ItemPosition::kLeft:
      DCHECK(is_inline_axis);
      return root_grid_writing_direction.IsLtr() ? AxisEdge::kStart
                                                 : AxisEdge::kEnd;
    case ItemPosition::kRight:
      DCHECK(is_inline_axis);
      return root_grid_writing_direction.IsRtl() ? AxisEdge::kStart
                                                 : AxisEdge::kEnd;
    case ItemPosition::kNormal:
      *auto_behavior = is_replaced ? NGAutoBehavior::kFitContent
                                   : NGAutoBehavior::kStretchImplicit;
      return AxisEdge::kStart;
    case ItemPosition::kLegacy:
    case ItemPosition::kAuto:
      NOTREACHED();
      return AxisEdge::kStart;
    case ItemPosition::kAnchorCenter:
      NOTREACHED();
      return AxisEdge::kCenter;
  }
}

}  // namespace

GridItemData::GridItemData(
    NGBlockNode node,
    const ComputedStyle& root_grid_style,
    FontBaseline parent_grid_font_baseline,
    bool parent_must_consider_grid_items_for_column_sizing,
    bool parent_must_consider_grid_items_for_row_sizing)
    : node(node),
      has_subgridded_columns(false),
      has_subgridded_rows(false),
      is_considered_for_column_sizing(false),
      is_considered_for_row_sizing(false),
      is_sizing_dependent_on_block_size(false),
      is_subgridded_to_parent_grid(false),
      must_consider_grid_items_for_column_sizing(false),
      must_consider_grid_items_for_row_sizing(false),
      parent_grid_font_baseline(parent_grid_font_baseline) {
  const auto& style = node.Style();

  const bool is_replaced = node.IsReplaced();
  const bool is_out_of_flow = node.IsOutOfFlowPositioned();

  // Determine the alignment for the grid item ahead of time (we may need to
  // know if it stretches to correctly determine any block axis contribution).
  bool is_overflow_safe;
  inline_axis_alignment = AxisEdgeFromItemPosition(
      /* is_inline_axis */ true, is_replaced, is_out_of_flow, style,
      root_grid_style, &inline_auto_behavior, &is_overflow_safe);
  is_inline_axis_overflow_safe = is_overflow_safe;

  block_axis_alignment = AxisEdgeFromItemPosition(
      /* is_inline_axis */ false, is_replaced, is_out_of_flow, style,
      root_grid_style, &block_auto_behavior, &is_overflow_safe);
  is_block_axis_overflow_safe = is_overflow_safe;

  const auto root_grid_writing_direction =
      root_grid_style.GetWritingDirection();
  const auto item_writing_mode = style.GetWritingMode();

  is_parallel_with_root_grid = IsParallelWritingMode(
      root_grid_writing_direction.GetWritingMode(), item_writing_mode);

  column_baseline_writing_mode = DetermineBaselineWritingMode(
      root_grid_writing_direction, item_writing_mode,
      /* is_parallel_context */ false);

  row_baseline_writing_mode = DetermineBaselineWritingMode(
      root_grid_writing_direction, item_writing_mode,
      /* is_parallel_context */ true);

  column_baseline_group = DetermineBaselineGroup(
      root_grid_writing_direction, column_baseline_writing_mode,
      /* is_parallel_context */ false,
      /* is_last_baseline */ inline_axis_alignment == AxisEdge::kLastBaseline);

  row_baseline_group = DetermineBaselineGroup(
      root_grid_writing_direction, row_baseline_writing_mode,
      /* is_parallel_context */ true,
      /* is_last_baseline */ block_axis_alignment == AxisEdge::kLastBaseline);

  // From https://drafts.csswg.org/css-grid-2/#subgrid-listing:
  //   "...if the grid container is otherwise forced to establish an independent
  //   formatting context... the grid container is not a subgrid."
  //
  // Only layout and paint containment establish an independent formatting
  // context as specified in:
  //   https://drafts.csswg.org/css-contain-2/#containment-layout
  //   https://drafts.csswg.org/css-contain-2/#containment-paint
  if (node.IsGrid() && !node.ShouldApplyLayoutContainment() &&
      !node.ShouldApplyPaintContainment()) {
    has_subgridded_columns =
        is_parallel_with_root_grid
            ? style.GridTemplateColumns().IsSubgriddedAxis()
            : style.GridTemplateRows().IsSubgriddedAxis();
    has_subgridded_rows = is_parallel_with_root_grid
                              ? style.GridTemplateRows().IsSubgriddedAxis()
                              : style.GridTemplateColumns().IsSubgriddedAxis();
  }

  // The `false, true, false, true` parameters get the converter to calculate
  // whether the subgrids and its root grid are opposite direction in all cases.
  const LogicalToLogical<bool> direction_converter(
      style.GetWritingDirection(), root_grid_writing_direction,
      /* inline_start */ false, /* inline_end */ true,
      /* block_start */ false, /* block_end */ true);

  is_opposite_direction_in_root_grid_columns =
      direction_converter.InlineStart();
  is_opposite_direction_in_root_grid_rows = direction_converter.BlockStart();

  // From https://drafts.csswg.org/css-grid-2/#subgrid-size-contribution:
  //   The subgrid itself [...] acts as if it was completely empty for track
  //   sizing purposes in the subgridded dimension.
  //
  // Mark any subgridded axis as not considered for sizing, effectively ignoring
  // its contribution in `GridLayoutAlgorithm::ResolveIntrinsicTrackSizes`.
  if (parent_must_consider_grid_items_for_column_sizing) {
    must_consider_grid_items_for_column_sizing = has_subgridded_columns;
    is_considered_for_column_sizing = !has_subgridded_columns;
  }

  if (parent_must_consider_grid_items_for_row_sizing) {
    must_consider_grid_items_for_row_sizing = has_subgridded_rows;
    is_considered_for_row_sizing = !has_subgridded_rows;
  }
}

void GridItemData::SetAlignmentFallback(
    GridTrackSizingDirection track_direction,
    bool has_synthesized_baseline) {
  // Alignment fallback is only possible when baseline alignment is specified.
  if (!IsBaselineSpecified(track_direction)) {
    return;
  }

  auto CanParticipateInBaselineAlignment = [&]() -> bool {
    // "If baseline alignment is specified on a grid item whose size in that
    // axis depends on the size of an intrinsically-sized track (whose size is
    // therefore dependent on both the item’s size and baseline alignment,
    // creating a cyclic dependency), that item does not participate in
    // baseline alignment, and instead uses its fallback alignment as if that
    // were originally specified. For this purpose, <flex> track sizes count
    // as “intrinsically-sized” when the grid container has an indefinite size
    // in the relevant axis."
    // https://drafts.csswg.org/css-grid-2/#row-align
    if (has_synthesized_baseline &&
        (IsSpanningIntrinsicTrack(track_direction) ||
         IsSpanningFlexibleTrack(track_direction))) {
      // Parallel grid items with a synthesized baseline support baseline
      // alignment only of the height doesn't depend on the track size.
      const auto& item_style = node.Style();
      const bool is_parallel_to_baseline_axis =
          is_parallel_with_root_grid == (track_direction == kForRows);

      if (is_parallel_to_baseline_axis) {
        return !item_style.LogicalHeight().IsPercentOrCalcOrStretch() &&
               !item_style.LogicalMinHeight().IsPercentOrCalcOrStretch() &&
               !item_style.LogicalMaxHeight().IsPercentOrCalcOrStretch();
      } else {
        return !item_style.LogicalWidth().IsPercentOrCalcOrStretch() &&
               !item_style.LogicalMinWidth().IsPercentOrCalcOrStretch() &&
               !item_style.LogicalMaxWidth().IsPercentOrCalcOrStretch();
      }
    }
    return true;
  };

  // Set fallback alignment to start edges if an item requests baseline
  // alignment but does not meet requirements for it.
  if (!CanParticipateInBaselineAlignment()) {
    const auto baseline_group = BaselineGroup(track_direction);
    if (track_direction == kForColumns) {
      inline_axis_alignment_fallback = baseline_group == BaselineGroup::kMajor
                                           ? AxisEdge::kStart
                                           : AxisEdge::kEnd;
      is_inline_axis_overflow_safe_fallback = true;
    } else {
      block_axis_alignment_fallback = baseline_group == BaselineGroup::kMajor
                                          ? AxisEdge::kStart
                                          : AxisEdge::kEnd;
      is_block_axis_overflow_safe_fallback = true;
    }
  } else {
    // Reset the alignment fallback if eligibility has changed.
    if (track_direction == kForColumns) {
      inline_axis_alignment_fallback.reset();
      is_inline_axis_overflow_safe_fallback.reset();
    } else {
      block_axis_alignment_fallback.reset();
      is_block_axis_overflow_safe_fallback.reset();
    }
  }
}

void GridItemData::ComputeSetIndices(
    const GridLayoutTrackCollection& track_collection) {
  DCHECK(!IsOutOfFlow());

  const auto track_direction = track_collection.Direction();
  DCHECK(MustCachePlacementIndices(track_direction));

  auto& range_indices = RangeIndices(track_direction);

#if DCHECK_IS_ON()
  if (range_indices.begin != kNotFound) {
    // Check the range index caching was correct by running a binary search.
    wtf_size_t computed_range_index =
        track_collection.RangeIndexFromGridLine(StartLine(track_direction));
    DCHECK_EQ(computed_range_index, range_indices.begin);

    computed_range_index =
        track_collection.RangeIndexFromGridLine(EndLine(track_direction) - 1);
    DCHECK_EQ(computed_range_index, range_indices.end);
  }
#endif

  if (range_indices.begin == kNotFound) {
    DCHECK_EQ(range_indices.end, kNotFound);

    range_indices.begin =
        track_collection.RangeIndexFromGridLine(StartLine(track_direction));
    range_indices.end =
        track_collection.RangeIndexFromGridLine(EndLine(track_direction) - 1);
  }

  DCHECK_LT(range_indices.end, track_collection.RangeCount());
  DCHECK_LE(range_indices.begin, range_indices.end);

  auto& set_indices =
      (track_direction == kForColumns) ? column_set_indices : row_set_indices;
  set_indices.begin = track_collection.RangeBeginSetIndex(range_indices.begin);
  set_indices.end = track_collection.RangeBeginSetIndex(range_indices.end) +
                    track_collection.RangeSetCount(range_indices.end);
}

void GridItemData::ComputeOutOfFlowItemPlacement(
    const GridLayoutTrackCollection& track_collection,
    const GridPlacementData& placement_data,
    const ComputedStyle& grid_style) {
  DCHECK(IsOutOfFlow());

  const bool is_for_columns = track_collection.Direction() == kForColumns;

  auto& start_offset = is_for_columns ? column_placement.offset_in_range.begin
                                      : row_placement.offset_in_range.begin;
  auto& end_offset = is_for_columns ? column_placement.offset_in_range.end
                                    : row_placement.offset_in_range.end;

  GridPlacement::ResolveOutOfFlowItemGridLines(track_collection, placement_data,
                                               grid_style, node.Style(),
                                               &start_offset, &end_offset);

#if DCHECK_IS_ON()
  if (start_offset != kNotFound && end_offset != kNotFound) {
    DCHECK_LE(end_offset, track_collection.EndLineOfImplicitGrid());
    DCHECK_LT(start_offset, end_offset);
  } else if (start_offset != kNotFound) {
    DCHECK_LE(start_offset, track_collection.EndLineOfImplicitGrid());
  } else if (end_offset != kNotFound) {
    DCHECK_LE(end_offset, track_collection.EndLineOfImplicitGrid());
  }
#endif

  // We only calculate the range placement if the line was not defined as 'auto'
  // and it is within the bounds of the grid, since an out of flow item cannot
  // create grid lines.
  const wtf_size_t range_count = track_collection.RangeCount();
  auto& start_range_index = is_for_columns ? column_placement.range_index.begin
                                           : row_placement.range_index.begin;
  if (start_offset != kNotFound) {
    if (!range_count) {
      // An undefined and empty grid has a single start/end grid line and no
      // ranges. Therefore, if the start offset isn't 'auto', the only valid
      // offset is zero.
      DCHECK_EQ(start_offset, 0u);
      start_range_index = 0;
    } else {
      // If the start line of an out of flow item is the last line of the grid,
      // we can just subtract one unit to the range count.
      start_range_index =
          (start_offset < track_collection.EndLineOfImplicitGrid())
              ? track_collection.RangeIndexFromGridLine(start_offset)
              : range_count - 1;
      start_offset -= track_collection.RangeStartLine(start_range_index);
    }
  }

  auto& end_range_index = is_for_columns ? column_placement.range_index.end
                                         : row_placement.range_index.end;
  if (end_offset != kNotFound) {
    if (!range_count) {
      // Similarly to the start offset, if we have an undefined, empty grid and
      // the end offset isn't 'auto', the only valid offset is zero.
      DCHECK_EQ(end_offset, 0u);
      end_range_index = 0;
    } else {
      // If the end line of an out of flow item is the first line of the grid,
      // then |last_spanned_range| is set to zero.
      end_range_index =
          end_offset ? track_collection.RangeIndexFromGridLine(end_offset - 1)
                     : 0;
      end_offset -= track_collection.RangeStartLine(end_range_index);
    }
  }
}

GridItems::GridItems(const GridItems& other) {
  item_data_.ReserveInitialCapacity(other.item_data_.size());
  for (const auto& grid_item : other.item_data_) {
    item_data_.emplace_back(std::make_unique<GridItemData>(*grid_item));
  }
}

void GridItems::Append(GridItems* other) {
  item_data_.reserve(item_data_.size() + other->item_data_.size());
  for (auto& grid_item : other->item_data_)
    item_data_.emplace_back(std::move(grid_item));
}

void GridItems::RemoveSubgriddedItems() {
  wtf_size_t new_item_count = 0;
  for (const auto& grid_item : item_data_) {
    if (grid_item->is_subgridded_to_parent_grid)
      break;
    ++new_item_count;
  }

#if DCHECK_IS_ON()
  for (wtf_size_t i = new_item_count; i < item_data_.size(); ++i)
    DCHECK(item_data_[i]->is_subgridded_to_parent_grid);
#endif
  item_data_.Shrink(new_item_count);
}

void GridItems::SortByOrderProperty() {
  auto CompareItemsByOrderProperty =
      [](const std::unique_ptr<GridItemData>& lhs,
         const std::unique_ptr<GridItemData>& rhs) {
        return lhs->node.Style().Order() < rhs->node.Style().Order();
      };
  std::stable_sort(item_data_.begin(), item_data_.end(),
                   CompareItemsByOrderProperty);
}

}  // namespace blink
