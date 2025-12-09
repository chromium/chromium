// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/grid/grid_layout_algorithm.h"

#include "base/containers/span.h"
#include "third_party/blink/renderer/core/layout/constraint_space_builder.h"
#include "third_party/blink/renderer/core/layout/disable_layout_side_effects_scope.h"
#include "third_party/blink/renderer/core/layout/fragmentation_utils.h"
#include "third_party/blink/renderer/core/layout/grid/grid_baseline_accumulator.h"
#include "third_party/blink/renderer/core/layout/grid/grid_break_token_data.h"
#include "third_party/blink/renderer/core/layout/grid/grid_item.h"
#include "third_party/blink/renderer/core/layout/grid/grid_layout_utils.h"
#include "third_party/blink/renderer/core/layout/grid/grid_track_sizing_algorithm.h"
#include "third_party/blink/renderer/core/layout/layout_utils.h"
#include "third_party/blink/renderer/core/layout/length_utils.h"
#include "third_party/blink/renderer/core/layout/logical_box_fragment.h"
#include "third_party/blink/renderer/core/layout/relative_utils.h"

namespace blink {

GridLayoutAlgorithm::GridLayoutAlgorithm(const LayoutAlgorithmParams& params)
    : LayoutAlgorithm(params) {
  DCHECK(params.space.IsNewFormattingContext());

  const auto& node = Node();
  const auto& constraint_space = GetConstraintSpace();
  const auto border_scrollbar_padding = BorderScrollbarPadding();

  // At various stages of the algorithm we need to know the grid available-size.
  // If it's initially indefinite, we need to know the min/max sizes as well.
  // Initialize all these to the same value.
  grid_available_size_ = grid_min_available_size_ = grid_max_available_size_ =
      ChildAvailableSize();
  ComputeAvailableSizes(border_scrollbar_padding, node, constraint_space,
                        container_builder_, grid_available_size_,
                        grid_min_available_size_, grid_max_available_size_);

  // If block-size containment applies compute the block-size ignoring
  // children (just based on the row definitions).
  if (grid_available_size_.block_size == kIndefiniteSize &&
      node.ShouldApplyBlockSizeContainment()) {
    contain_intrinsic_block_size_ = ComputeIntrinsicBlockSizeIgnoringChildren();

    // Resolve the block-size, and set the available sizes.
    const LayoutUnit block_size = ComputeBlockSizeForFragment(
        constraint_space, node, BorderPadding(), *contain_intrinsic_block_size_,
        container_builder_.InlineSize());

    grid_available_size_.block_size = grid_min_available_size_.block_size =
        grid_max_available_size_.block_size =
            (block_size - border_scrollbar_padding.BlockSum())
                .ClampNegativeToZero();
  }
}

namespace {

bool HasBlockSizeDependentGridItem(const GridItems& grid_items) {
  for (const auto& grid_item : grid_items.IncludeSubgriddedItems()) {
    if (grid_item.is_sizing_dependent_on_block_size)
      return true;
  }
  return false;
}

}  // namespace

const LayoutResult* GridLayoutAlgorithm::Layout() {
  const auto* result = LayoutInternal();
  if (result->Status() == LayoutResult::kDisableFragmentation) {
    DCHECK(GetConstraintSpace().HasBlockFragmentation());
    return RelayoutWithoutFragmentation<GridLayoutAlgorithm>();
  }
  return result;
}

const LayoutResult* GridLayoutAlgorithm::LayoutInternal() {
  PaintLayerScrollableArea::DelayScrollOffsetClampScope delay_clamp_scope;

  GridItems grid_items;
  LayoutUnit intrinsic_block_size;
  GridLayoutSubtree layout_subtree;
  HeapVector<Member<LayoutBox>> oof_children;

  if (IsBreakInside(GetBreakToken())) {
    // TODO(layout-dev): When we support variable inline size fragments we'll
    // need to re-run `ComputeGridGeometry` for the different inline size while
    // making sure that we don't recalculate the automatic repetitions (which
    // depend on the available size), as this might change the grid structure
    // significantly (e.g., pull a child up into the first row).
    const auto* grid_data =
        To<GridBreakTokenData>(GetBreakToken()->TokenData());

    grid_items = grid_data->grid_items;
    layout_subtree = grid_data->grid_layout_subtree;
    intrinsic_block_size = grid_data->intrinsic_block_size;

    if (Style().BoxDecorationBreak() == EBoxDecorationBreak::kClone &&
        !GetBreakToken()->IsAtBlockEnd()) {
      // In the cloning box decorations model, the intrinsic block size of a
      // node grows by the size of the box decorations each time it fragments.
      intrinsic_block_size += BorderScrollbarPadding().BlockSum();
    }
  } else {
    layout_subtree =
        ComputeGridGeometry(&grid_items, &intrinsic_block_size, &oof_children);
  }

  const auto& layout_data = layout_subtree.LayoutData();
  LayoutUnit offset_in_stitched_container;
  LayoutUnit previous_offset_in_stitched_container;
  Vector<GridItemPlacementData> grid_items_placement_data;
  Vector<LayoutUnit> row_offset_adjustments;
  Vector<EBreakBetween> row_break_between;
  // Holds the gap geometry for the unfragmented grid pass before fragmentation
  // is applied, if applicable.
  const GapGeometry* full_gap_geometry = nullptr;
  Vector<wtf_size_t> track_idx_to_set_idx;
  Vector<wtf_size_t> column_gaps_segment_ranges_start_indices;
  LayoutUnit cumulative_gap_offset_adjustment = LayoutUnit();
  wtf_size_t first_unprocessed_row_gap_idx = 0;

  if (InvolvedInBlockFragmentation(container_builder_)) [[unlikely]] {
    // Either retrieve all items offsets, or generate them using the
    // non-fragmented `PlaceGridItems` pass.
    if (IsBreakInside(GetBreakToken())) {
      const auto* grid_data =
          To<GridBreakTokenData>(GetBreakToken()->TokenData());

      offset_in_stitched_container = previous_offset_in_stitched_container =
          grid_data->offset_in_stitched_container;
      grid_items_placement_data = grid_data->grid_items_placement_data;
      row_offset_adjustments = grid_data->row_offset_adjustments;
      row_break_between = grid_data->row_break_between;
      oof_children = grid_data->oof_children;
      full_gap_geometry = grid_data->full_gap_geometry;
      track_idx_to_set_idx = grid_data->track_idx_to_set_idx;
      column_gaps_segment_ranges_start_indices =
          grid_data->column_gaps_segment_ranges_start_indices;
      cumulative_gap_offset_adjustment =
          grid_data->cumulative_gap_offset_adjustment;
      first_unprocessed_row_gap_idx = grid_data->first_unprocessed_row_gap_idx;
    } else {
      row_offset_adjustments =
          Vector<LayoutUnit>(layout_data.Rows().GetSetCount() + 1);
      // `EndLineOfImplicitGrid()` is equivalent to the total track count.
      // TODO(samomekarajr): Add this and number of gaps to the
      // `GridTrackCollection` API.
      const wtf_size_t total_column_track_count =
          layout_data.Columns().EndLineOfImplicitGrid();
      if (total_column_track_count > 1) {
        column_gaps_segment_ranges_start_indices =
            Vector<wtf_size_t>(total_column_track_count - 1, 0);
      }
      PlaceGridItems(grid_items, layout_subtree, &row_break_between,
                     &grid_items_placement_data, &full_gap_geometry,
                     &track_idx_to_set_idx);
    }

    PlaceGridItemsForFragmentation(
        grid_items, layout_subtree, row_break_between, full_gap_geometry,
        &track_idx_to_set_idx, &column_gaps_segment_ranges_start_indices,
        &grid_items_placement_data, &row_offset_adjustments,
        &intrinsic_block_size, &offset_in_stitched_container,
        &cumulative_gap_offset_adjustment, &first_unprocessed_row_gap_idx);
  } else {
    PlaceGridItems(grid_items, layout_subtree, &row_break_between);
  }

  const auto& node = Node();
  const auto& border_padding = BorderPadding();
  const auto& constraint_space = GetConstraintSpace();

  const auto block_size = ComputeBlockSizeForFragment(
      constraint_space, node, border_padding, intrinsic_block_size,
      container_builder_.InlineSize());

  // For scrollable overflow purposes grid is unique in that the "inflow-bounds"
  // are the size of the grid, and *not* where the inflow grid items are placed.
  // Explicitly set the inflow-bounds to the grid size.
  if (node.IsScrollContainer()) {
    LogicalOffset offset = {layout_data.Columns().GetSetOffset(0),
                            layout_data.Rows().GetSetOffset(0)};

    LogicalSize size = {layout_data.Columns().CalculateSetSpanSize(),
                        layout_data.Rows().CalculateSetSpanSize()};

    container_builder_.SetInflowBounds(LogicalRect(offset, size));
  }
  container_builder_.SetMayHaveDescendantAboveBlockStart(false);

  // Grid is slightly different to other layout modes in that the contents of
  // the grid won't change if the initial block size changes definiteness (for
  // example). We can safely mark ourselves as not having any children
  // dependent on the block constraints.
  container_builder_.SetHasDescendantThatDependsOnPercentageBlockSize(false);

  if (constraint_space.HasKnownFragmentainerBlockSize()) {
    // `FinishFragmentation` uses `BoxFragmentBuilder::IntrinsicBlockSize` to
    // determine the final size of this fragment.
    container_builder_.SetIntrinsicBlockSize(
        offset_in_stitched_container - previous_offset_in_stitched_container +
        BorderScrollbarPadding().block_end);
  } else {
    container_builder_.SetIntrinsicBlockSize(intrinsic_block_size);
  }

  // When row-gap suppression occurs within a subgrid, the suppression affects
  // the subgrid’s intrinsic block size. However, if the total block size is not
  // updated to reflect this change, the subgrid will render incorrectly because
  // it will still occupy space that should have been accounted for via the
  // suppression. Hence, in such cases the total block size should be aligned
  // with the intrinsic block size after suppression, as this represents the
  // actual size of the subgrid once gap adjustments have been applied.
  if (RuntimeEnabledFeatures::CSSGridGapSuppressionEnabled() &&
      node.HasCachedPlacementData() &&
      !node.CachedPlacementData().HasStandaloneAxis(
          GridTrackSizingDirection::kForRows)) {
    container_builder_.SetFragmentsTotalBlockSize(intrinsic_block_size);
  } else {
    container_builder_.SetFragmentsTotalBlockSize(block_size);
  }

  if (InvolvedInBlockFragmentation(container_builder_)) [[unlikely]] {
    auto status = FinishFragmentation(&container_builder_);
    if (status == BreakStatus::kDisableFragmentation) {
      return container_builder_.Abort(LayoutResult::kDisableFragmentation);
    }
    DCHECK_EQ(status, BreakStatus::kContinue);
  } else {
#if DCHECK_IS_ON()
    // If we're not participating in a fragmentation context, no block
    // fragmentation related fields should have been set.
    container_builder_.CheckNoBlockFragmentation();
#endif
  }

  // Set our break-before/break-after.
  if (constraint_space.ShouldPropagateChildBreakValues()) {
    container_builder_.SetInitialBreakBefore(row_break_between.front());
    container_builder_.SetPreviousBreakAfter(row_break_between.back());
  }

  if (!oof_children.empty())
    PlaceOutOfFlowItems(layout_data, block_size, oof_children);

  // Copy grid layout data for use in computed style and devtools.
  container_builder_.TransferGridLayoutData(
      std::make_unique<GridLayoutData>(layout_data));

  SetReadingFlowNodes(grid_items);

  if (constraint_space.HasBlockFragmentation()) {
    container_builder_.SetBreakTokenData(
        MakeGarbageCollected<GridBreakTokenData>(
            std::move(grid_items), std::move(layout_subtree),
            intrinsic_block_size, offset_in_stitched_container,
            grid_items_placement_data, row_offset_adjustments,
            row_break_between, oof_children, full_gap_geometry,
            track_idx_to_set_idx, column_gaps_segment_ranges_start_indices,
            cumulative_gap_offset_adjustment, first_unprocessed_row_gap_idx));
  }

  container_builder_.HandleOofsAndSpecialDescendants();
  return container_builder_.ToBoxFragment();
}

MinMaxSizesResult GridLayoutAlgorithm::ComputeMinMaxSizes(
    const MinMaxSizesFloatInput&) {
  const auto& node = Node();
  const LayoutUnit override_intrinsic_inline_size =
      node.OverrideIntrinsicContentInlineSize();

  auto FixedMinMaxSizes = [&](LayoutUnit size) -> MinMaxSizesResult {
    size += BorderScrollbarPadding().InlineSum();
    return {{size, size}, /* depends_on_block_constraints */ false};
  };

  if (override_intrinsic_inline_size != kIndefiniteSize) {
    return FixedMinMaxSizes(override_intrinsic_inline_size);
  }

  if (const auto* layout_subtree =
          GetConstraintSpace().GetGridLayoutSubtree()) {
    return FixedMinMaxSizes(
        layout_subtree->LayoutData().Columns().CalculateSetSpanSize());
  }

  // If we have inline size containment ignore all children.
  auto grid_sizing_tree = node.ShouldApplyInlineSizeContainment()
                              ? BuildGridSizingTreeIgnoringChildren()
                              : BuildGridSizingTree();

  bool depends_on_block_constraints = false;
  auto ComputeTotalColumnSize =
      [&](SizingConstraint sizing_constraint) -> LayoutUnit {
    InitializeTrackSizes(&grid_sizing_tree);

    bool needs_additional_pass = false;
    CompleteTrackSizingAlgorithm(kForColumns, sizing_constraint,
                                 &grid_sizing_tree, &needs_additional_pass);

    if (needs_additional_pass ||
        HasBlockSizeDependentGridItem(grid_sizing_tree.GetGridItems())) {
      // If we need to calculate the row geometry, then we have a dependency on
      // our block constraints.
      depends_on_block_constraints = true;
      CompleteTrackSizingAlgorithm(kForRows, sizing_constraint,
                                   &grid_sizing_tree, &needs_additional_pass);

      if (needs_additional_pass) {
        InitializeTrackSizes(&grid_sizing_tree, kForColumns);
        CompleteTrackSizingAlgorithm(kForColumns, sizing_constraint,
                                     &grid_sizing_tree);
      }
    }
    return grid_sizing_tree.LayoutData().Columns().CalculateSetSpanSize();
  };

  MinMaxSizes sizes{ComputeTotalColumnSize(SizingConstraint::kMinContent),
                    ComputeTotalColumnSize(SizingConstraint::kMaxContent)};
  sizes += BorderScrollbarPadding().InlineSum();
  return {sizes, depends_on_block_constraints};
}

MinMaxSizes GridLayoutAlgorithm::ComputeSubgridMinMaxSizes(
    const GridSizingSubtree& sizing_subtree) const {
  DCHECK(sizing_subtree.HasValidRootFor(Node()));

  return {ComputeSubgridIntrinsicSize(sizing_subtree, kForColumns,
                                      SizingConstraint::kMinContent),
          ComputeSubgridIntrinsicSize(sizing_subtree, kForColumns,
                                      SizingConstraint::kMaxContent)};
}

LayoutUnit GridLayoutAlgorithm::ComputeSubgridIntrinsicBlockSize(
    const GridSizingSubtree& sizing_subtree) const {
  DCHECK(sizing_subtree.HasValidRootFor(Node()));

  return ComputeSubgridIntrinsicSize(sizing_subtree, kForRows,
                                     SizingConstraint::kMaxContent);
}

namespace {

GridArea SubgriddedAreaInParent(const SubgriddedItemData& opt_subgrid_data) {
  if (!opt_subgrid_data.IsSubgrid()) {
    return GridArea();
  }

  auto subgridded_area_in_parent = opt_subgrid_data->resolved_position;

  if (!opt_subgrid_data->has_subgridded_columns) {
    subgridded_area_in_parent.columns = GridSpan::IndefiniteGridSpan();
  }
  if (!opt_subgrid_data->has_subgridded_rows) {
    subgridded_area_in_parent.rows = GridSpan::IndefiniteGridSpan();
  }

  if (!opt_subgrid_data->is_parallel_with_root_grid) {
    std::swap(subgridded_area_in_parent.columns,
              subgridded_area_in_parent.rows);
  }
  return subgridded_area_in_parent;
}

FragmentGeometry CalculateInitialFragmentGeometryForSubgrid(
    const GridItemData& subgrid_data,
    const ConstraintSpace& space,
    const GridSizingSubtree& sizing_subtree = kNoGridSizingSubtree) {
  DCHECK(subgrid_data.IsSubgrid());

  const auto& node = To<GridNode>(subgrid_data.node);
  {
    const bool subgrid_has_standalone_columns =
        subgrid_data.is_parallel_with_root_grid
            ? !subgrid_data.has_subgridded_columns
            : !subgrid_data.has_subgridded_rows;

    // We won't be able to resolve the intrinsic sizes of a subgrid if its
    // tracks are subgridded, i.e., their sizes can't be resolved by the subgrid
    // itself, or if `sizing_subtree` is not provided, i.e., the grid sizing
    // tree it's not completed at this step of the sizing algorithm.
    if (subgrid_has_standalone_columns && sizing_subtree) {
      return CalculateInitialFragmentGeometry(
          space, node, /* break_token */ nullptr,
          [&](SizeType) -> MinMaxSizesResult {
            return node.ComputeSubgridMinMaxSizes(sizing_subtree, space);
          });
    }
  }

  bool needs_to_compute_min_max_sizes = false;

  const auto fragment_geometry = CalculateInitialFragmentGeometry(
      space, node, /* break_token */ nullptr,
      [&needs_to_compute_min_max_sizes](SizeType) -> MinMaxSizesResult {
        // We can't call `ComputeMinMaxSizes` for a subgrid with an incomplete
        // grid sizing tree, as its intrinsic size relies on its subtree. If we
        // end up in this function, we need to use an intrinsic fragment
        // geometry instead to avoid a cyclic dependency.
        needs_to_compute_min_max_sizes = true;
        return MinMaxSizesResult();
      });

  if (needs_to_compute_min_max_sizes) {
    return CalculateInitialFragmentGeometry(space, node,
                                            /* break_token */ nullptr,
                                            /* is_intrinsic */ true);
  }
  return fragment_geometry;
}

}  // namespace

void GridLayoutAlgorithm::BuildGridSizingSubtree(
    GridSizingTree* sizing_tree,
    HeapVector<Member<LayoutBox>>* opt_oof_children,
    const SubgriddedItemData& opt_subgrid_data,
    const GridLineResolver* opt_parent_line_resolver,
    bool must_invalidate_placement_cache,
    bool must_ignore_children) const {
  DCHECK(sizing_tree);

  const auto& node = Node();
  const auto& style = node.Style();

  sizing_tree->AddToPreorderTraversal(node);

  const auto subgrid_area = SubgriddedAreaInParent(opt_subgrid_data);
  const auto column_auto_repetitions =
      ComputeAutomaticRepetitions(subgrid_area.columns, kForColumns);
  const auto row_auto_repetitions =
      ComputeAutomaticRepetitions(subgrid_area.rows, kForRows);
  const auto writing_mode = GetConstraintSpace().GetWritingMode();

  // Initialize this grid's line resolver.
  const auto line_resolver =
      opt_parent_line_resolver
          ? GridLineResolver(style, *opt_parent_line_resolver, subgrid_area,
                             column_auto_repetitions, row_auto_repetitions)
          : GridLineResolver(style, column_auto_repetitions,
                             row_auto_repetitions);

  GridItems grid_items;
  GridLayoutData layout_data;
  bool has_nested_subgrid = false;
  wtf_size_t column_start_offset = 0;
  wtf_size_t row_start_offset = 0;

  if (!must_ignore_children) {
    // Construct grid items that are not subgridded.
    grid_items =
        node.ConstructGridItems(line_resolver, &must_invalidate_placement_cache,
                                opt_oof_children, &has_nested_subgrid);

    const auto& placement_data = node.CachedPlacementData();
    column_start_offset = placement_data.column_start_offset;
    row_start_offset = placement_data.row_start_offset;
  }

  auto BuildSizingCollection = [&](GridTrackSizingDirection track_direction) {
    GridRangeBuilder range_builder(
        style, track_direction, line_resolver.AutoRepetitions(track_direction),
        (track_direction == kForColumns) ? column_start_offset
                                         : row_start_offset);

    bool must_create_baselines = false;
    for (auto& grid_item : grid_items.IncludeSubgriddedItems()) {
      if (grid_item.IsConsideredForSizing(track_direction)) {
        must_create_baselines |= grid_item.IsBaselineSpecified(track_direction);
      }

      if (grid_item.MustCachePlacementIndices(track_direction)) {
        auto& range_indices = grid_item.RangeIndices(track_direction);
        range_builder.EnsureTrackCoverage(grid_item.StartLine(track_direction),
                                          grid_item.SpanSize(track_direction),
                                          &range_indices.begin,
                                          &range_indices.end);
      }
    }

    layout_data.SetTrackCollection(std::make_unique<GridSizingTrackCollection>(
        range_builder.FinalizeRanges(), track_direction,
        must_create_baselines));
  };

  const bool has_standalone_columns = subgrid_area.columns.IsIndefinite();
  const bool has_standalone_rows = subgrid_area.rows.IsIndefinite();

  if (has_standalone_columns) {
    BuildSizingCollection(kForColumns);
  }
  if (has_standalone_rows) {
    BuildSizingCollection(kForRows);
  }

  if (!has_nested_subgrid) {
    sizing_tree->SetSizingNodeData(node, std::move(grid_items),
                                   std::move(layout_data));
    return;
  }

  InitializeTrackCollection(opt_subgrid_data, kForColumns, &layout_data);
  InitializeTrackCollection(opt_subgrid_data, kForRows, &layout_data);

  if (has_standalone_columns) {
    layout_data.SizingCollection(kForColumns).CacheDefiniteSetsGeometry();
  }
  if (has_standalone_rows) {
    layout_data.SizingCollection(kForRows).CacheDefiniteSetsGeometry();
  }

  // `AppendSubgriddedItems` rely on the cached placement data of a subgrid to
  // construct its grid items, so we need to build their subtrees beforehand.
  for (auto& grid_item : grid_items) {
    if (!grid_item.IsSubgrid()) {
      continue;
    }

    // TODO(ethavar): Currently we have an issue where we can't correctly cache
    // the set indices of this grid item to determine its available space. This
    // happens because subgridded items are not considered by the range builder
    // since they can't be placed before we recurse into subgrids.
    grid_item.ComputeSetIndices(layout_data.Columns());
    grid_item.ComputeSetIndices(layout_data.Rows());

    const auto space = CreateConstraintSpaceForLayout(grid_item, layout_data);
    const auto fragment_geometry =
        CalculateInitialFragmentGeometryForSubgrid(grid_item, space);

    const GridLayoutAlgorithm subgrid_algorithm(
        {grid_item.node, fragment_geometry, space});

    subgrid_algorithm.BuildGridSizingSubtree(
        sizing_tree, /*opt_oof_children=*/nullptr,
        SubgriddedItemData(grid_item, layout_data, writing_mode),
        &line_resolver, must_invalidate_placement_cache);

    // After we accommodate subgridded items in their respective sizing track
    // collections, their placement indices might be incorrect, so we want to
    // recompute them when we call `InitializeTrackSizes`.
    grid_item.ResetPlacementIndices();
  }

  node.AppendSubgriddedItems(&grid_items);

  // We need to recreate the track builder collections to ensure track coverage
  // for subgridded items; it would be ideal to have them accounted for already,
  // but we might need the track collections to compute a subgrid's automatic
  // repetitions, so we do this process twice to avoid a cyclic dependency.
  if (has_standalone_columns) {
    BuildSizingCollection(kForColumns);
  }
  if (has_standalone_rows) {
    BuildSizingCollection(kForRows);
  }

  sizing_tree->SetSizingNodeData(node, std::move(grid_items),
                                 std::move(layout_data));
}

GridSizingTree GridLayoutAlgorithm::BuildGridSizingTree(
    HeapVector<Member<LayoutBox>>* opt_oof_children) const {
  DCHECK(!GetConstraintSpace().GetGridLayoutSubtree());

  GridSizingTree sizing_tree;
  BuildGridSizingSubtree(&sizing_tree, opt_oof_children);
  return sizing_tree;
}

GridSizingTree GridLayoutAlgorithm::BuildGridSizingTreeIgnoringChildren()
    const {
  DCHECK(!GetConstraintSpace().GetGridLayoutSubtree());

  GridSizingTree sizing_tree;
  BuildGridSizingSubtree(&sizing_tree, /*opt_oof_children=*/nullptr,
                         /*opt_subgrid_data=*/kNoSubgriddedItemData,
                         /*opt_parent_line_resolver=*/nullptr,
                         /*must_invalidate_placement_cache=*/false,
                         /*must_ignore_children=*/true);
  return sizing_tree;
}

GridLayoutSubtree GridLayoutAlgorithm::ComputeGridGeometry(
    GridItems* grid_items,
    LayoutUnit* intrinsic_block_size,
    HeapVector<Member<LayoutBox>>* oof_children) {
  DCHECK(grid_items);
  DCHECK(intrinsic_block_size);
  DCHECK(oof_children);

  DCHECK(grid_items->IsEmpty());
  DCHECK_NE(grid_available_size_.inline_size, kIndefiniteSize);

  const auto& node = Node();
  const auto& constraint_space = GetConstraintSpace();
  const auto& border_scrollbar_padding = BorderScrollbarPadding();

  auto CalculateIntrinsicBlockSize = [&](const GridItems& grid_items,
                                         const GridLayoutData& layout_data) {
    if (contain_intrinsic_block_size_) {
      return *contain_intrinsic_block_size_;
    }

    auto intrinsic_block_size = layout_data.Rows().CalculateSetSpanSize() +
                                border_scrollbar_padding.BlockSum();

    // TODO(layout-dev): This isn't great but matches legacy. Ideally this
    // would only apply when we have only flexible track(s).
    if (grid_items.IsEmpty() && node.HasLineIfEmpty()) {
      intrinsic_block_size = std::max(
          intrinsic_block_size, border_scrollbar_padding.BlockSum() +
                                    node.EmptyLineBlockSize(GetBreakToken()));
    }

    return ClampIntrinsicBlockSize(constraint_space, node, GetBreakToken(),
                                   border_scrollbar_padding,
                                   intrinsic_block_size);
  };

  // If we have a layout subtree in the constraint space, it means we are in a
  // subgrid whose geometry is already computed. We can exit early by simply
  // copying the layout data and constructing our grid items.
  if (const auto* layout_subtree = constraint_space.GetGridLayoutSubtree()) {
    const auto& layout_data = layout_subtree->LayoutData();

    if (!node.ChildLayoutBlockedByDisplayLock()) {
      bool must_invalidate_placement_cache = false;
      *grid_items = node.ConstructGridItems(node.CachedLineResolver(),
                                            &must_invalidate_placement_cache,
                                            oof_children);

      DCHECK(!must_invalidate_placement_cache)
          << "We shouldn't need to invalidate the placement cache if we relied "
             "on the cached line resolver; it must produce the same placement.";

      GridTrackSizingAlgorithm::CacheGridItemsProperties(layout_data.Columns(),
                                                         grid_items);
      GridTrackSizingAlgorithm::CacheGridItemsProperties(layout_data.Rows(),
                                                         grid_items);
    }

    *intrinsic_block_size =
        CalculateIntrinsicBlockSize(*grid_items, layout_data);
    return *layout_subtree;
  }

  auto grid_sizing_tree = node.ChildLayoutBlockedByDisplayLock()
                              ? BuildGridSizingTreeIgnoringChildren()
                              : BuildGridSizingTree(oof_children);

  InitializeTrackSizes(&grid_sizing_tree);

  bool needs_additional_pass = false;
  CompleteTrackSizingAlgorithm(kForColumns, SizingConstraint::kLayout,
                               &grid_sizing_tree, &needs_additional_pass);
  CompleteTrackSizingAlgorithm(kForRows, SizingConstraint::kLayout,
                               &grid_sizing_tree, &needs_additional_pass);

  const auto& layout_data = grid_sizing_tree.LayoutData();
  *intrinsic_block_size =
      CalculateIntrinsicBlockSize(grid_sizing_tree.GetGridItems(), layout_data);

  const auto& container_style = Style();
  const bool applies_auto_min_size =
      !container_style.AspectRatio().IsAuto() &&
      container_style.IsOverflowVisibleOrClip() &&
      container_style.LogicalMinHeight().HasAuto();

  if (grid_available_size_.block_size == kIndefiniteSize ||
      applies_auto_min_size) {
    const auto block_size = ComputeBlockSizeForFragment(
        constraint_space, node, BorderPadding(), *intrinsic_block_size,
        container_builder_.InlineSize());

    DCHECK_NE(block_size, kIndefiniteSize);

    grid_available_size_.block_size = grid_min_available_size_.block_size =
        grid_max_available_size_.block_size =
            (block_size - border_scrollbar_padding.BlockSum())
                .ClampNegativeToZero();

    // If we have any rows, gaps which will resolve differently if we have a
    // definite |grid_available_size_| re-compute the grid using the
    // |block_size| calculated above.
    needs_additional_pass |=
        (container_style.RowGap() && container_style.RowGap()->HasPercent()) ||
        layout_data.Rows().IsDependentOnAvailableSize();

    // If we are a flex-item, we may have our initial block-size forced to be
    // indefinite, however grid layout always re-computes the grid using the
    // final "used" block-size.
    // We can detect this case by checking if computing our block-size (with an
    // indefinite intrinsic size) is definite.
    //
    // TODO(layout-dev): A small optimization here would be to do this only if
    // we have 'auto' tracks which fill the remaining available space.
    if (constraint_space.IsInitialBlockSizeIndefinite()) {
      needs_additional_pass |=
          ComputeBlockSizeForFragment(
              constraint_space, node, BorderPadding(),
              /* intrinsic_block_size */ kIndefiniteSize,
              container_builder_.InlineSize()) != kIndefiniteSize;
    }

    // After resolving the block-size, if we don't need to rerun the track
    // sizing algorithm, simply apply any content alignment to its rows.
    if (!needs_additional_pass &&
        container_style.AlignContent() !=
            ComputedStyleInitialValues::InitialAlignContent()) {
      auto& track_collection = layout_data.SizingCollection(kForRows);

      // Re-compute the row geometry now that we resolved the available block
      // size. "align-content: space-evenly", etc, require the resolved size.
      auto first_set_geometry =
          GridTrackSizingAlgorithm::ComputeFirstSetGeometry(
              track_collection, container_style, grid_available_size_,
              border_scrollbar_padding);

      track_collection.FinalizeSetsGeometry(first_set_geometry.start_offset,
                                            first_set_geometry.gutter_size);
    }
  }

  if (needs_additional_pass) {
    InitializeTrackSizes(&grid_sizing_tree, kForColumns);
    CompleteTrackSizingAlgorithm(kForColumns, SizingConstraint::kLayout,
                                 &grid_sizing_tree);

    InitializeTrackSizes(&grid_sizing_tree, kForRows);
    CompleteTrackSizingAlgorithm(kForRows, SizingConstraint::kLayout,
                                 &grid_sizing_tree);
  }

  // Calculate final alignment baselines of the entire grid sizing tree.
  CompleteFinalBaselineAlignment(&grid_sizing_tree);

  *grid_items = std::move(grid_sizing_tree.GetGridItems());
  return GridLayoutSubtree(grid_sizing_tree.FinalizeTree());
}

LayoutUnit GridLayoutAlgorithm::ComputeIntrinsicBlockSizeIgnoringChildren()
    const {
  const auto& node = Node();
  const LayoutUnit override_intrinsic_block_size =
      node.OverrideIntrinsicContentBlockSize();
  DCHECK(node.ShouldApplyBlockSizeContainment());

  // First check 'contain-intrinsic-size'.
  if (override_intrinsic_block_size != kIndefiniteSize)
    return BorderScrollbarPadding().BlockSum() + override_intrinsic_block_size;

  auto grid_sizing_tree = BuildGridSizingTreeIgnoringChildren();

  InitializeTrackSizes(&grid_sizing_tree, kForRows);
  CompleteTrackSizingAlgorithm(kForRows, SizingConstraint::kLayout,
                               &grid_sizing_tree);

  return grid_sizing_tree.LayoutData().Rows().CalculateSetSpanSize() +
         BorderScrollbarPadding().BlockSum();
}

namespace {

const LayoutResult* LayoutGridItemForMeasure(
    const GridItemData& grid_item,
    const ConstraintSpace& constraint_space,
    SizingConstraint sizing_constraint) {
  const auto& node = grid_item.node;

  // Disable side effects during MinMax computation to avoid potential "MinMax
  // after layout" crashes. This is not necessary during the layout pass, and
  // would have a negative impact on performance if used there.
  //
  // TODO(ikilpatrick): For subgrid, ideally we don't want to disable side
  // effects as it may impact performance significantly; this issue can be
  // avoided by introducing additional cache slots (see crbug.com/1272533).
  std::optional<DisableLayoutSideEffectsScope> disable_side_effects;
  if (!node.GetLayoutBox()->NeedsLayout() &&
      (sizing_constraint != SizingConstraint::kLayout ||
       grid_item.is_subgridded_to_parent_grid)) {
    disable_side_effects.emplace();
  }
  return node.Layout(constraint_space);
}

LayoutUnit Baseline(const GridItemData& grid_item,
                    const GridLayoutData& layout_data,
                    GridTrackSizingDirection track_direction) {
  // "If a box spans multiple shared alignment contexts, then it participates
  //  in first/last baseline alignment within its start-most/end-most shared
  //  alignment context along that axis"
  // https://www.w3.org/TR/css-align-3/#baseline-sharing-group
  const auto& track_collection = (track_direction == kForColumns)
                                     ? layout_data.Columns()
                                     : layout_data.Rows();

  const auto& [begin_set_index, end_set_index] =
      grid_item.SetIndices(track_direction);

  return (grid_item.BaselineGroup(track_direction) == BaselineGroup::kMajor)
             ? track_collection.MajorBaseline(begin_set_index)
             : track_collection.MinorBaseline(end_set_index - 1);
}

LayoutUnit GetExtraMarginForBaseline(const BoxStrut& margins,
                                     const SubgriddedItemData& subgridded_item,
                                     GridTrackSizingDirection track_direction,
                                     WritingMode writing_mode) {
  const auto& track_collection = (track_direction == kForColumns)
                                     ? subgridded_item.Columns(writing_mode)
                                     : subgridded_item.Rows(writing_mode);
  const auto& [begin_set_index, end_set_index] =
      subgridded_item->SetIndices(track_collection.Direction());

  const LayoutUnit extra_margin =
      (subgridded_item->BaselineGroup(track_direction) == BaselineGroup::kMajor)
          ? track_collection.StartExtraMargin(begin_set_index)
          : track_collection.EndExtraMargin(end_set_index);

  return extra_margin +
         (subgridded_item->IsLastBaselineSpecified(track_direction)
              ? margins.block_end
              : margins.block_start);
}

LayoutUnit GetLogicalBaseline(const GridItemData& grid_item,
                              const LogicalBoxFragment& baseline_fragment,
                              GridTrackSizingDirection track_direction) {
  const auto font_baseline = grid_item.parent_grid_font_baseline;

  return grid_item.IsLastBaselineSpecified(track_direction)
             ? baseline_fragment.BlockSize() -
                   baseline_fragment.LastBaselineOrSynthesize(font_baseline)
             : baseline_fragment.FirstBaselineOrSynthesize(font_baseline);
}

LayoutUnit GetSynthesizedLogicalBaseline(
    const GridItemData& grid_item,
    LayoutUnit block_size,
    GridTrackSizingDirection track_direction) {
  const auto synthesized_baseline = LogicalBoxFragment::SynthesizedBaseline(
      grid_item.parent_grid_font_baseline,
      grid_item.BaselineWritingDirection(track_direction).IsFlippedLines(),
      block_size);

  return grid_item.IsLastBaselineSpecified(track_direction)
             ? block_size - synthesized_baseline
             : synthesized_baseline;
}

LayoutUnit ComputeBlockSizeForSubgrid(const GridSizingSubtree& sizing_subtree,
                                      const GridItemData& subgrid_data,
                                      const ConstraintSpace& space) {
  DCHECK(sizing_subtree);
  DCHECK(subgrid_data.IsSubgrid());

  const auto& node = To<GridNode>(subgrid_data.node);
  return ComputeBlockSizeForFragment(
      space, node,
      ComputeBorders(space, node) + ComputePadding(space, node.Style()),
      node.ComputeSubgridIntrinsicBlockSize(sizing_subtree, space),
      space.AvailableSize().inline_size);
}

}  // namespace

LayoutUnit GridLayoutAlgorithm::ContributionSizeForGridItem(
    const GridSizingSubtree& sizing_subtree,
    GridItemContributionType contribution_type,
    GridTrackSizingDirection track_direction,
    SizingConstraint sizing_constraint,
    GridItemData* grid_item) const {
  DCHECK(grid_item);
  DCHECK(grid_item->IsConsideredForSizing(track_direction));

  const auto& node = grid_item->node;
  const auto& item_style = node.Style();
  const auto& constraint_space = GetConstraintSpace();

  const bool is_for_columns = track_direction == kForColumns;
  const bool is_parallel_with_track_direction =
      is_for_columns == grid_item->is_parallel_with_root_grid;

  const auto writing_mode = constraint_space.GetWritingMode();
  const auto subgridded_item =
      grid_item->is_subgridded_to_parent_grid
          ? sizing_subtree.LookupSubgriddedItemData(*grid_item)
          : SubgriddedItemData(*grid_item, sizing_subtree.LayoutData(),
                               writing_mode);

  // TODO(ikilpatrick): We'll need to record if any child used an indefinite
  // size for its contribution, such that we can then do the 2nd pass on the
  // track-sizing algorithm.
  const auto space =
      CreateConstraintSpaceForMeasure(subgridded_item, track_direction);

  LayoutUnit baseline_shim;
  auto CalculateBaselineShim = [&](LayoutUnit baseline) -> void {
    const auto track_baseline =
        Baseline(*grid_item, sizing_subtree.LayoutData(), track_direction);

    if (track_baseline == LayoutUnit::Min())
      return;

    const auto extra_margin = GetExtraMarginForBaseline(
        ComputeMarginsFor(space, item_style,
                          grid_item->BaselineWritingDirection(track_direction)),
        subgridded_item, track_direction, writing_mode);

    // Determine the delta between the baselines; subtract out the margin so it
    // doesn't get added a second time at the end of this method.
    baseline_shim = track_baseline - baseline - extra_margin;
  };

  auto MinMaxSizesFunc = [&](SizeType type) -> MinMaxSizesResult {
    if (grid_item->IsSubgrid()) {
      return To<GridNode>(node).ComputeSubgridMinMaxSizes(
          sizing_subtree.SubgridSizingSubtree(*grid_item), space);
    }
    return node.ComputeMinMaxSizes(item_style.GetWritingMode(), type, space);
  };

  auto MinOrMaxContentSize = [&](bool is_min_content) -> LayoutUnit {
    const auto result = ComputeMinAndMaxContentContributionForSelf(
        node, space, MinMaxSizesFunc);

    // The min/max contribution may depend on the block-size of the grid-area:
    // <div style="display: inline-grid; grid-template-columns: auto auto;">
    //   <div style="height: 100%">
    //     <img style="height: 50%;" />
    //   </div>
    //   <div>
    //     <div style="height: 100px;"></div>
    //   </div>
    // </div>
    // Mark ourselves as requiring an additional pass to re-resolve the column
    // tracks for this case.
    if (grid_item->is_parallel_with_root_grid &&
        result.depends_on_block_constraints) {
      grid_item->is_sizing_dependent_on_block_size = true;
    }

    const auto content_size =
        is_min_content ? result.sizes.min_size : result.sizes.max_size;

    if (grid_item->IsBaselineAligned(track_direction)) {
      CalculateBaselineShim(GetSynthesizedLogicalBaseline(
          *grid_item, content_size, track_direction));
    }
    return content_size + baseline_shim;
  };

  auto MinContentSize = [&]() -> LayoutUnit {
    return MinOrMaxContentSize(/*is_min_content=*/true);
  };
  auto MaxContentSize = [&]() -> LayoutUnit {
    return MinOrMaxContentSize(/*is_min_content=*/false);
  };

  // This function will determine the correct block-size of a grid-item.
  // TODO(ikilpatrick): This should try and skip layout when possible. Notes:
  //  - We'll need to do a full layout for tables.
  //  - We'll need special logic for replaced elements.
  //  - We'll need to respect the aspect-ratio when appropriate.
  auto BlockContributionSize = [&]() -> LayoutUnit {
    DCHECK(!is_parallel_with_track_direction);

    if (grid_item->IsSubgrid()) {
      return ComputeBlockSizeForSubgrid(
          sizing_subtree.SubgridSizingSubtree(*grid_item), *grid_item, space);
    }

    // TODO(ikilpatrick): This check is potentially too broad, i.e. a fixed
    // inline size with no %-padding doesn't need the additional pass.
    if (is_for_columns)
      grid_item->is_sizing_dependent_on_block_size = true;

    const LayoutResult* result = nullptr;
    if (space.AvailableSize().inline_size == kIndefiniteSize) {
      // If we are orthogonal grid item, resolving against an indefinite size,
      // set our inline size to our max-content contribution size.
      const auto fallback_space = CreateConstraintSpaceForMeasure(
          subgridded_item, track_direction,
          /*opt_fixed_inline_size=*/MaxContentSize());

      result = LayoutGridItemForMeasure(*grid_item, fallback_space,
                                        sizing_constraint);
    } else {
      result = LayoutGridItemForMeasure(*grid_item, space, sizing_constraint);
    }

    LogicalBoxFragment baseline_fragment(
        grid_item->BaselineWritingDirection(track_direction),
        To<PhysicalBoxFragment>(result->GetPhysicalFragment()));

    if (grid_item->IsBaselineAligned(track_direction)) {
      CalculateBaselineShim(
          GetLogicalBaseline(*grid_item, baseline_fragment, track_direction));
    }
    return baseline_fragment.BlockSize() + baseline_shim;
  };

  const auto& track_collection = is_for_columns
                                     ? subgridded_item.Columns(writing_mode)
                                     : subgridded_item.Rows(writing_mode);

  const auto margins = ComputeMarginsFor(space, item_style, constraint_space);
  const auto& [begin_set_index, end_set_index] =
      subgridded_item->SetIndices(track_collection.Direction());

  const auto margin_sum =
      (is_for_columns ? margins.InlineSum() : margins.BlockSum()) +
      track_collection.StartExtraMargin(begin_set_index) +
      track_collection.EndExtraMargin(end_set_index);

  LayoutUnit contribution;
  switch (contribution_type) {
    case GridItemContributionType::kForContentBasedMinimums:
    case GridItemContributionType::kForIntrinsicMaximums:
      contribution = is_parallel_with_track_direction ? MinContentSize()
                                                      : BlockContributionSize();
      break;
    case GridItemContributionType::kForIntrinsicMinimums: {
      // See https://drafts.csswg.org/css-grid/#min-size-auto for more details
      // on the special logic applied for intrinsic minimums.
      //
      // Per the spec link above, we apply the automatic min when:
      // - it spans at least one track in that axis whose min track sizing
      // function is auto.
      // - if it spans more than one track in that axis, none of those tracks
      // are flexible.
      const bool special_spanning_criteria =
          !grid_item->IsSpanningAutoMinimumTrack(track_direction) ||
          (grid_item->IsSpanningFlexibleTrack(track_direction) &&
           grid_item->SpanSize(track_direction) > 1);

      const LayoutUnit min_content_contribution =
          is_parallel_with_track_direction ? MinContentSize()
                                           : BlockContributionSize();
      const LayoutUnit max_content_contribution =
          is_parallel_with_track_direction ? MaxContentSize()
                                           : min_content_contribution;

      MinMaxSizesResult subgrid_minmax_sizes;
      if (grid_item->IsSubgrid()) {
        const GridSizingSubtree& subgrid_sizing_subtree =
            sizing_subtree.SubgridSizingSubtree(*grid_item);
        if (subgrid_sizing_subtree.LayoutData().IsSubgridWithStandaloneAxis(
                kForColumns)) {
          subgrid_minmax_sizes = To<GridNode>(node).ComputeSubgridMinMaxSizes(
              subgrid_sizing_subtree, space);
        }
      }

      bool maybe_clamp = false;
      contribution = CalculateIntrinsicMinimumContribution(
          is_parallel_with_track_direction, special_spanning_criteria,
          min_content_contribution, max_content_contribution, space,
          subgrid_minmax_sizes, grid_item, maybe_clamp);

      if (!maybe_clamp) {
        break;
      }

      // Further clamp the minimum size to less than or equal to the
      // stretch fit into the grid area’s maximum size in that dimension,
      // as represented by the sum of those grid tracks’ max track sizing
      // functions plus any intervening fixed gutters.
      auto spanned_tracks_definite_max_size =
          track_collection.CalculateSetSpanSize(begin_set_index, end_set_index);
      if (spanned_tracks_definite_max_size != kIndefiniteSize) {
        contribution += margin_sum;
        const auto border_padding =
            ComputeBorders(space, node) + ComputePadding(space, item_style);
        const auto border_padding_sum = is_parallel_with_track_direction
                                            ? border_padding.InlineSum()
                                            : border_padding.BlockSum();

        contribution = ClampIntrinsicMinSize(
            contribution,
            /*min_clamp_size=*/margin_sum + baseline_shim + border_padding_sum,
            spanned_tracks_definite_max_size);
        contribution -= margin_sum;
      }
      break;
    }
    case GridItemContributionType::kForMaxContentMinimums:
    case GridItemContributionType::kForMaxContentMaximums:
      contribution = is_parallel_with_track_direction ? MaxContentSize()
                                                      : BlockContributionSize();
      break;
    case GridItemContributionType::kForFreeSpace:
      NOTREACHED() << "`kForFreeSpace` should only be used to distribute extra "
                      "space in maximize tracks and stretch auto tracks steps.";
  }
  return (contribution + margin_sum).ClampNegativeToZero();
}

// https://drafts.csswg.org/css-grid-2/#auto-repeat
wtf_size_t GridLayoutAlgorithm::ComputeAutomaticRepetitions(
    const GridSpan& subgrid_span,
    GridTrackSizingDirection track_direction) const {
  const bool is_for_columns = track_direction == kForColumns;

  const auto& style = Style();
  const auto& track_list = is_for_columns
                               ? style.GridTemplateColumns().GetTrackList()
                               : style.GridTemplateRows().GetTrackList();

  if (!track_list.HasAutoRepeater())
    return 0;

  // Subgrids compute auto repetitions differently than standalone grids.
  // See https://drafts.csswg.org/css-grid-2/#auto-repeat.
  if (track_list.IsSubgriddedAxis()) {
    if (subgrid_span.IsIndefinite()) {
      // From https://drafts.csswg.org/css-grid-2/#subgrid-listing
      // "If there is no parent grid, ..., the used value is the initial
      // value, 'none', and the grid container is not a subgrid.
      return 0;
    }

    return ComputeAutomaticRepetitionsForSubgrid(subgrid_span.IntegerSpan(),
                                                 track_direction);
  }

  const LayoutUnit gutter_size = GridTrackSizingAlgorithm::CalculateGutterSize(
      style, grid_available_size_, track_direction);

  return CalculateAutomaticRepetitions(
      track_list, gutter_size,
      is_for_columns ? grid_available_size_.inline_size
                     : grid_available_size_.block_size,
      is_for_columns ? grid_min_available_size_.inline_size
                     : grid_min_available_size_.block_size,
      is_for_columns ? grid_max_available_size_.inline_size
                     : grid_max_available_size_.block_size);
}

wtf_size_t GridLayoutAlgorithm::ComputeAutomaticRepetitionsForSubgrid(
    wtf_size_t subgrid_span_size,
    GridTrackSizingDirection track_direction) const {
  // "On a subgridded axis, the auto-fill keyword is only valid once per
  // <line-name-list>, and repeats enough times for the name list to match the
  // subgrid’s specified grid span (falling back to 0 if the span is already
  // fulfilled).
  // https://drafts.csswg.org/css-grid-2/#auto-repeat
  const auto& computed_track_list = (track_direction == kForColumns)
                                        ? Style().GridTemplateColumns()
                                        : Style().GridTemplateRows();
  const auto& track_list = computed_track_list.GetTrackList();
  DCHECK(track_list.HasAutoRepeater());

  const wtf_size_t non_auto_repeat_line_count =
      track_list.NonAutoRepeatLineCount();

  if (non_auto_repeat_line_count > subgrid_span_size) {
    // No more room left for auto repetitions due to the number of non-auto
    // repeat named grid lines (the span is already fulfilled).
    return 0;
  }

  const wtf_size_t tracks_per_repeat = track_list.AutoRepeatTrackCount();
  if (tracks_per_repeat > subgrid_span_size) {
    // No room left for auto repetitions because each repetition is too large.
    return 0;
  }

  const wtf_size_t tracks_left_over_for_auto_repeat =
      subgrid_span_size - non_auto_repeat_line_count + 1;
  DCHECK_GT(tracks_per_repeat, 0u);
  return static_cast<wtf_size_t>(
      std::floor(tracks_left_over_for_auto_repeat / tracks_per_repeat));
}

void GridLayoutAlgorithm::ComputeGridItemBaselines(
    const GridLayoutTreePtr& layout_tree,
    const GridSizingSubtree& sizing_subtree,
    GridTrackSizingDirection track_direction,
    SizingConstraint sizing_constraint) const {
  auto& track_collection = sizing_subtree.SizingCollection(track_direction);

  if (!track_collection.HasBaselines()) {
    return;
  }

  const auto writing_mode = GetConstraintSpace().GetWritingMode();
  track_collection.ResetBaselines();

  for (auto& grid_item :
       sizing_subtree.GetGridItems().IncludeSubgriddedItems()) {
    if (!grid_item.IsBaselineSpecified(track_direction) ||
        !grid_item.IsConsideredForSizing(track_direction)) {
      continue;
    }

    GridLayoutSubtree subgrid_layout_subtree;
    if (grid_item.IsSubgrid()) {
      subgrid_layout_subtree = GridLayoutSubtree(
          layout_tree, sizing_subtree.LookupSubgridIndex(grid_item));

      if (subgrid_layout_subtree.HasUnresolvedGeometry()) {
        // Calling `Layout` for a nested subgrid rely on the geometry of its
        // respective layout subtree to be fully resolved. Otherwise, the
        // subgrid won't be able to resolve its intrinsic sizes.
        continue;
      }
    }

    const auto subgridded_item =
        grid_item.is_subgridded_to_parent_grid
            ? sizing_subtree.LookupSubgriddedItemData(grid_item)
            : SubgriddedItemData(grid_item, sizing_subtree.LayoutData(),
                                 writing_mode);

    LayoutUnit inline_offset, block_offset;
    LogicalSize containing_grid_area_size = {
        subgridded_item->CalculateAvailableSize(subgridded_item.Columns(),
                                                &inline_offset),
        subgridded_item->CalculateAvailableSize(subgridded_item.Rows(),
                                                &block_offset)};

    // TODO(kschmi) : Add a cache slot parameter to
    //  `CreateConstraintSpaceForLayout` to avoid variables above.
    const auto space =
        CreateConstraintSpace(LayoutResultCacheSlot::kMeasure, *subgridded_item,
                              containing_grid_area_size,
                              /* fixed_available_size */ kIndefiniteLogicalSize,
                              std::move(subgrid_layout_subtree));

    // Skip this item if we aren't able to resolve our inline size.
    if (CalculateInitialFragmentGeometry(space, grid_item.node,
                                         /* break_token */ nullptr)
            .border_box_size.inline_size == kIndefiniteSize) {
      continue;
    }

    const auto* result =
        LayoutGridItemForMeasure(grid_item, space, sizing_constraint);

    const auto baseline_writing_direction =
        grid_item.BaselineWritingDirection(track_direction);
    const LogicalBoxFragment baseline_fragment(
        baseline_writing_direction,
        To<PhysicalBoxFragment>(result->GetPhysicalFragment()));

    const bool has_synthesized_baseline =
        !baseline_fragment.FirstBaseline().has_value();
    grid_item.SetAlignmentFallback(track_direction, has_synthesized_baseline);

    if (!grid_item.IsBaselineAligned(track_direction)) {
      continue;
    }

    const LayoutUnit extra_margin = GetExtraMarginForBaseline(
        ComputeMarginsFor(space, grid_item.node.Style(),
                          baseline_writing_direction),
        subgridded_item, track_direction, writing_mode);

    const LayoutUnit baseline =
        extra_margin +
        GetLogicalBaseline(grid_item, baseline_fragment, track_direction);

    // "If a box spans multiple shared alignment contexts, then it participates
    //  in first/last baseline alignment within its start-most/end-most shared
    //  alignment context along that axis"
    // https://www.w3.org/TR/css-align-3/#baseline-sharing-group
    const auto& [begin_set_index, end_set_index] =
        grid_item.SetIndices(track_direction);
    if (grid_item.BaselineGroup(track_direction) == BaselineGroup::kMajor) {
      track_collection.SetMajorBaseline(begin_set_index, baseline);
    } else {
      track_collection.SetMinorBaseline(end_set_index - 1, baseline);
    }
  }
}

std::unique_ptr<GridLayoutTrackCollection>
GridLayoutAlgorithm::CreateSubgridTrackCollection(
    const SubgriddedItemData& subgrid_data,
    GridTrackSizingDirection track_direction) const {
  DCHECK(subgrid_data.IsSubgrid());

  const bool is_for_columns_in_parent = subgrid_data->is_parallel_with_root_grid
                                            ? track_direction == kForColumns
                                            : track_direction == kForRows;

  const auto& style = Style();
  const auto& parent_track_collection =
      is_for_columns_in_parent ? subgrid_data.Columns() : subgrid_data.Rows();
  const auto& range_indices = is_for_columns_in_parent
                                  ? subgrid_data->column_range_indices
                                  : subgrid_data->row_range_indices;

  return std::make_unique<GridLayoutTrackCollection>(
      parent_track_collection.CreateSubgridTrackCollection(
          range_indices.begin, range_indices.end,
          GridTrackSizingAlgorithm::CalculateGutterSize(
              style, grid_available_size_, track_direction,
              parent_track_collection.GutterSize()),
          ComputeMarginsForSelf(GetConstraintSpace(), style),
          BorderScrollbarPadding(), track_direction,
          is_for_columns_in_parent
              ? subgrid_data->is_opposite_direction_in_root_grid_columns
              : subgrid_data->is_opposite_direction_in_root_grid_rows));
}

void GridLayoutAlgorithm::InitializeTrackCollection(
    const SubgriddedItemData& opt_subgrid_data,
    GridTrackSizingDirection track_direction,
    GridLayoutData* layout_data) const {
  if (layout_data->HasSubgriddedAxis(track_direction)) {
    // If we don't have a sizing collection for this axis, then we're in a
    // subgrid that must inherit the track collection of its parent grid.
    DCHECK(opt_subgrid_data.IsSubgrid());

    layout_data->SetTrackCollection(
        CreateSubgridTrackCollection(opt_subgrid_data, track_direction));
    return;
  }

  auto& track_collection = layout_data->SizingCollection(track_direction);
  track_collection.BuildSets(Style(), grid_available_size_);
}

namespace {

GridTrackSizingDirection RelativeDirectionInSubgrid(
    GridTrackSizingDirection track_direction,
    const GridItemData& subgrid_data) {
  DCHECK(subgrid_data.IsSubgrid());

  const bool is_for_columns = subgrid_data.is_parallel_with_root_grid ==
                              (track_direction == kForColumns);
  return is_for_columns ? kForColumns : kForRows;
}

std::optional<GridTrackSizingDirection> RelativeDirectionFilterInSubgrid(
    const std::optional<GridTrackSizingDirection>& opt_track_direction,
    const GridItemData& subgrid_data) {
  DCHECK(subgrid_data.IsSubgrid());

  if (opt_track_direction) {
    return RelativeDirectionInSubgrid(*opt_track_direction, subgrid_data);
  }
  return std::nullopt;
}

}  // namespace

void GridLayoutAlgorithm::InitializeTrackSizes(
    const GridSizingSubtree& sizing_subtree,
    const SubgriddedItemData& opt_subgrid_data,
    const std::optional<GridTrackSizingDirection>& opt_track_direction) const {
  DCHECK(sizing_subtree.HasValidRootFor(Node()));

  auto& grid_items = sizing_subtree.GetGridItems();
  auto& layout_data = sizing_subtree.LayoutData();

  auto InitAndCacheTrackSizes = [&](GridTrackSizingDirection track_direction) {
    InitializeTrackCollection(opt_subgrid_data, track_direction, &layout_data);

    if (layout_data.HasSubgriddedAxis(track_direction)) {
      const auto& track_collection = (track_direction == kForColumns)
                                         ? layout_data.Columns()
                                         : layout_data.Rows();
      for (auto& grid_item : grid_items) {
        grid_item.ComputeSetIndices(track_collection);
      }
    } else {
      auto& track_collection = layout_data.SizingCollection(track_direction);
      GridTrackSizingAlgorithm::CacheGridItemsProperties(track_collection,
                                                         &grid_items);

      // If all tracks have a definite size upfront, we can use the current set
      // sizes as the used track sizes (applying alignment, if present).
      if (!track_collection.HasNonDefiniteTrack()) {
        auto first_set_geometry =
            GridTrackSizingAlgorithm::ComputeFirstSetGeometry(
                track_collection, Style(), grid_available_size_,
                BorderScrollbarPadding());

        track_collection.FinalizeSetsGeometry(first_set_geometry.start_offset,
                                              first_set_geometry.gutter_size);
      } else {
        track_collection.CacheInitializedSetsGeometry(
            (track_direction == kForColumns)
                ? BorderScrollbarPadding().inline_start
                : BorderScrollbarPadding().block_start);
      }

      if (track_collection.HasBaselines()) {
        track_collection.ResetBaselines();
      }
    }
  };

  if (opt_track_direction) {
    InitAndCacheTrackSizes(*opt_track_direction);
  } else {
    InitAndCacheTrackSizes(kForColumns);
    InitAndCacheTrackSizes(kForRows);
  }

  ForEachSubgrid(
      sizing_subtree,
      [&](const GridLayoutAlgorithm& subgrid_algorithm,
          const GridSizingSubtree& subgrid_subtree,
          const SubgriddedItemData& subgrid_data) {
        subgrid_algorithm.InitializeTrackSizes(
            subgrid_subtree, subgrid_data,
            RelativeDirectionFilterInSubgrid(opt_track_direction,
                                             *subgrid_data));
      },
      /* should_compute_min_max_sizes */ false);
}

void GridLayoutAlgorithm::InitializeTrackSizes(
    GridSizingTree* sizing_tree,
    const std::optional<GridTrackSizingDirection>& opt_track_direction) const {
  InitializeTrackSizes(GridSizingSubtree(sizing_tree),
                       /*opt_subgrid_data=*/kNoSubgriddedItemData,
                       opt_track_direction);
}

namespace {

struct BlockSizeDependentGridItem {
  GridItemIndices row_set_indices;
  LayoutUnit cached_block_size;
};

Vector<BlockSizeDependentGridItem> BlockSizeDependentGridItems(
    const GridItems& grid_items,
    const GridSizingTrackCollection& track_collection) {
  DCHECK_EQ(track_collection.Direction(), kForRows);

  Vector<BlockSizeDependentGridItem> dependent_items;
  dependent_items.ReserveInitialCapacity(grid_items.Size());

  // TODO(ethavar): We need to take into account the block size dependent
  // subgridded items that might change its contribution size in a nested
  // subgrid's standalone axis, but doing so implies a more refined change.
  // We'll revisit this issue in a later patch, in the meantime we simply
  // want to skip over subgridded items to avoid DCHECKs.
  for (const auto& grid_item : grid_items) {
    if (!grid_item.is_sizing_dependent_on_block_size)
      continue;

    const auto& set_indices = grid_item.SetIndices(kForRows);
    BlockSizeDependentGridItem dependent_item = {
        set_indices, track_collection.CalculateSetSpanSize(set_indices.begin,
                                                           set_indices.end)};
    dependent_items.emplace_back(std::move(dependent_item));
  }
  return dependent_items;
}

bool MayChangeBlockSizeDependentGridItemContributions(
    const Vector<BlockSizeDependentGridItem>& dependent_items,
    const GridSizingTrackCollection& track_collection) {
  DCHECK_EQ(track_collection.Direction(), kForRows);

  for (const auto& grid_item : dependent_items) {
    const LayoutUnit block_size = track_collection.CalculateSetSpanSize(
        grid_item.row_set_indices.begin, grid_item.row_set_indices.end);

    DCHECK_NE(block_size, kIndefiniteSize);
    if (block_size != grid_item.cached_block_size)
      return true;
  }
  return false;
}

}  // namespace

void GridLayoutAlgorithm::ComputeUsedTrackSizes(
    const GridSizingSubtree& sizing_subtree,
    GridTrackSizingDirection track_direction,
    SizingConstraint sizing_constraint) const {
  DCHECK(sizing_subtree.HasValidRootFor(Node()));

  const auto& style = Style();
  auto& track_collection =
      sizing_subtree.LayoutData().SizingCollection(track_direction);
  track_collection.BuildSets(style, grid_available_size_);

  auto AccomodateExtraMargin = [&](LayoutUnit extra_margin,
                                   wtf_size_t set_index) {
    auto& set = track_collection.GetSetAt(set_index);

    if (set.track_size.HasIntrinsicMinTrackBreadth() &&
        set.BaseSize() < extra_margin) {
      set.IncreaseBaseSize(extra_margin);
    }
  };

  auto& grid_items = sizing_subtree.GetGridItems();
  for (auto& grid_item : grid_items.IncludeSubgriddedItems()) {
    if (!grid_item.IsSpanningIntrinsicTrack(track_direction) ||
        !grid_item.MustConsiderGridItemsForSizing(track_direction)) {
      continue;
    }

    // A subgrid should accommodate its extra margins in the subgridded axis
    // since it might not have children on its edges to account for them.
    DCHECK(grid_item.IsSubgrid());

    const bool is_for_columns_in_subgrid =
        RelativeDirectionInSubgrid(track_direction, grid_item) == kForColumns;

    const auto& subgrid_layout_data =
        sizing_subtree.SubgridSizingSubtree(grid_item).LayoutData();
    const auto& subgrid_track_collection = is_for_columns_in_subgrid
                                               ? subgrid_layout_data.Columns()
                                               : subgrid_layout_data.Rows();

    auto start_extra_margin = subgrid_track_collection.StartExtraMargin();
    auto end_extra_margin = subgrid_track_collection.EndExtraMargin();

    if (grid_item.IsOppositeDirectionInRootGrid(track_direction)) {
      std::swap(start_extra_margin, end_extra_margin);
    }

    const auto& set_indices = grid_item.SetIndices(track_direction);
    if (set_indices.begin < set_indices.end - 1) {
      AccomodateExtraMargin(start_extra_margin, set_indices.begin);
      AccomodateExtraMargin(end_extra_margin, set_indices.end - 1);
    } else {
      AccomodateExtraMargin(start_extra_margin + end_extra_margin,
                            set_indices.begin);
    }
  }

  GridTrackSizingAlgorithm(style, grid_available_size_,
                           grid_min_available_size_, sizing_constraint)
      .ComputeUsedTrackSizes(
          [&](GridItemContributionType contribution_type,
              GridItemData* grid_item) {
            return ContributionSizeForGridItem(
                sizing_subtree, contribution_type, track_direction,
                sizing_constraint, grid_item);
          },
          &track_collection, &grid_items);
}

void GridLayoutAlgorithm::CompleteTrackSizingAlgorithm(
    const GridSizingSubtree& sizing_subtree,
    const SubgriddedItemData& opt_subgrid_data,
    GridTrackSizingDirection track_direction,
    SizingConstraint sizing_constraint,
    bool* opt_needs_additional_pass) const {
  DCHECK(sizing_subtree.HasValidRootFor(Node()));

  auto& layout_data = sizing_subtree.LayoutData();

  const bool is_for_columns = track_direction == kForColumns;
  const bool has_non_definite_track =
      is_for_columns ? layout_data.Columns().HasNonDefiniteTrack()
                     : layout_data.Rows().HasNonDefiniteTrack();

  if (has_non_definite_track) {
    if (layout_data.HasSubgriddedAxis(track_direction)) {
      // If we don't have a sizing collection for this axis, then we're in a
      // subgrid that must inherit the track collection of its parent grid.
      DCHECK(opt_subgrid_data.IsSubgrid());

      layout_data.SetTrackCollection(
          CreateSubgridTrackCollection(opt_subgrid_data, track_direction));
    } else {
      ComputeUsedTrackSizes(sizing_subtree, track_direction, sizing_constraint);

      // After computing row sizes, if we're still trying to determine whether
      // we need to perform and additional pass, check if there is a grid item
      // whose contributions may change with the new available block size.
      const bool needs_to_check_block_size_dependent_grid_items =
          !is_for_columns && opt_needs_additional_pass &&
          !(*opt_needs_additional_pass);

      Vector<BlockSizeDependentGridItem> block_size_dependent_grid_items;
      auto& track_collection = layout_data.SizingCollection(track_direction);

      if (needs_to_check_block_size_dependent_grid_items) {
        block_size_dependent_grid_items = BlockSizeDependentGridItems(
            sizing_subtree.GetGridItems(), track_collection);
      }

      auto first_set_geometry =
          GridTrackSizingAlgorithm::ComputeFirstSetGeometry(
              track_collection, Style(), grid_available_size_,
              BorderScrollbarPadding());

      track_collection.FinalizeSetsGeometry(first_set_geometry.start_offset,
                                            first_set_geometry.gutter_size);

      if (needs_to_check_block_size_dependent_grid_items) {
        *opt_needs_additional_pass =
            MayChangeBlockSizeDependentGridItemContributions(
                block_size_dependent_grid_items, track_collection);
      }
    }
  }

  ForEachSubgrid(
      sizing_subtree, [&](const GridLayoutAlgorithm& subgrid_algorithm,
                          const GridSizingSubtree& subgrid_subtree,
                          const SubgriddedItemData& subgrid_data) {
        subgrid_algorithm.CompleteTrackSizingAlgorithm(
            subgrid_subtree, subgrid_data,
            RelativeDirectionInSubgrid(track_direction, *subgrid_data),
            sizing_constraint, opt_needs_additional_pass);
      });
}

namespace {

// A subgrid's `MinMaxSizes` cache is stored in its respective `LayoutGrid` and
// gets invalidated via the `IsSubgridMinMaxSizesCacheDirty` flag.
//
// However, a subgrid might need to invalidate the cache if it inherited a
// different track collection in its subgridded axis, which might cause its
// intrinsic sizes to change. This invalidation goes from parent to children,
// which is not accounted for by the invalidation logic in `LayoutObject`.
//
// This method addresses such issue by traversing the tree in postorder checking
// whether the cache at each subgrid level is reusable or not: if the subgrid
// has a valid cache, but its input tracks for the subgridded axis changed,
// then we'll invalidate the cache for that subgrid and its ancestors.
bool ValidateMinMaxSizesCache(const GridNode& grid_node,
                              const GridSizingSubtree& sizing_subtree,
                              GridTrackSizingDirection track_direction) {
  DCHECK(sizing_subtree.HasValidRootFor(grid_node));

  bool should_invalidate_min_max_sizes_cache = false;

  // Only iterate over items if this grid has nested subgrids.
  if (auto next_subgrid_subtree = sizing_subtree.FirstChild()) {
    for (const auto& grid_item : sizing_subtree.GetGridItems()) {
      if (!grid_item.IsSubgrid()) {
        continue;
      }

      DCHECK(next_subgrid_subtree);
      should_invalidate_min_max_sizes_cache |= ValidateMinMaxSizesCache(
          To<GridNode>(grid_item.node), next_subgrid_subtree,
          RelativeDirectionInSubgrid(track_direction, grid_item));
      next_subgrid_subtree = next_subgrid_subtree.NextSibling();
    }
  }

  const auto& layout_data = sizing_subtree.LayoutData();
  if (layout_data.IsSubgridWithStandaloneAxis(track_direction)) {
    // If no nested subgrid marked this subtree to be invalidated already, check
    // that the cached intrinsic sizes are reusable by the current sizing tree.
    if (!should_invalidate_min_max_sizes_cache) {
      should_invalidate_min_max_sizes_cache =
          grid_node.ShouldInvalidateSubgridMinMaxSizesCacheFor(layout_data);
    }

    if (should_invalidate_min_max_sizes_cache) {
      grid_node.InvalidateSubgridMinMaxSizesCache();
    }
  }
  return should_invalidate_min_max_sizes_cache;
}

}  // namespace

void GridLayoutAlgorithm::CompleteTrackSizingAlgorithm(
    GridTrackSizingDirection track_direction,
    SizingConstraint sizing_constraint,
    GridSizingTree* sizing_tree,
    bool* opt_needs_additional_pass) const {
  const auto sizing_subtree = GridSizingSubtree(sizing_tree);

  ValidateMinMaxSizesCache(Node(), sizing_subtree, track_direction);

  ComputeBaselineAlignment(sizing_tree->FinalizeTree(), sizing_subtree,
                           /*opt_subgrid_data=*/kNoSubgriddedItemData,
                           track_direction, sizing_constraint);

  CompleteTrackSizingAlgorithm(sizing_subtree,
                               /*opt_subgrid_data=*/kNoSubgriddedItemData,
                               track_direction, sizing_constraint,
                               opt_needs_additional_pass);
}

void GridLayoutAlgorithm::ComputeBaselineAlignment(
    const GridLayoutTreePtr& layout_tree,
    const GridSizingSubtree& sizing_subtree,
    const SubgriddedItemData& opt_subgrid_data,
    const std::optional<GridTrackSizingDirection>& opt_track_direction,
    SizingConstraint sizing_constraint) const {
  DCHECK(sizing_subtree.HasValidRootFor(Node()));

  auto& layout_data = sizing_subtree.LayoutData();

  auto ComputeOrRecreateBaselines =
      [&](GridTrackSizingDirection track_direction) {
        if (layout_data.HasSubgriddedAxis(track_direction)) {
          DCHECK(opt_subgrid_data.IsSubgrid());
          // Recreate the subgrid track collection if there are baselines which
          // need to be inherited.
          const bool is_for_columns_in_parent =
              opt_subgrid_data->is_parallel_with_root_grid
                  ? track_direction == kForColumns
                  : track_direction == kForRows;
          const auto& parent_track_collection = is_for_columns_in_parent
                                                    ? opt_subgrid_data.Columns()
                                                    : opt_subgrid_data.Rows();
          if (parent_track_collection.HasBaselines()) {
            layout_data.SetTrackCollection(CreateSubgridTrackCollection(
                opt_subgrid_data, track_direction));
          }
        } else {
          ComputeGridItemBaselines(layout_tree, sizing_subtree, track_direction,
                                   sizing_constraint);
        }
      };

  if (opt_track_direction) {
    ComputeOrRecreateBaselines(*opt_track_direction);
  } else {
    ComputeOrRecreateBaselines(kForColumns);
    ComputeOrRecreateBaselines(kForRows);
  }

  ForEachSubgrid(sizing_subtree,
                 [&](const GridLayoutAlgorithm& subgrid_algorithm,
                     const GridSizingSubtree& subgrid_subtree,
                     const SubgriddedItemData& subgrid_data) {
                   subgrid_algorithm.ComputeBaselineAlignment(
                       layout_tree, subgrid_subtree, subgrid_data,
                       RelativeDirectionFilterInSubgrid(opt_track_direction,
                                                        *subgrid_data),
                       sizing_constraint);
                 });
}

void GridLayoutAlgorithm::CompleteFinalBaselineAlignment(
    GridSizingTree* sizing_tree) const {
  ComputeBaselineAlignment(
      sizing_tree->FinalizeTree(), GridSizingSubtree(sizing_tree),
      /*opt_subgrid_data=*/kNoSubgriddedItemData,
      /*opt_track_direction=*/std::nullopt, SizingConstraint::kLayout);
}

template <typename CallbackFunc>
void GridLayoutAlgorithm::ForEachSubgrid(
    const GridSizingSubtree& sizing_subtree,
    const CallbackFunc& callback_func,
    bool should_compute_min_max_sizes) const {
  // Exit early if this subtree doesn't have nested subgrids.
  auto next_subgrid_subtree = sizing_subtree.FirstChild();
  if (!next_subgrid_subtree) {
    return;
  }

  const auto& layout_data = sizing_subtree.LayoutData();

  for (const auto& grid_item : sizing_subtree.GetGridItems()) {
    if (!grid_item.IsSubgrid()) {
      continue;
    }

    const auto space = CreateConstraintSpaceForLayout(grid_item, layout_data);
    const auto fragment_geometry = CalculateInitialFragmentGeometryForSubgrid(
        grid_item, space,
        should_compute_min_max_sizes ? next_subgrid_subtree
                                     : kNoGridSizingSubtree);

    const GridLayoutAlgorithm subgrid_algorithm(
        {grid_item.node, fragment_geometry, space});

    DCHECK(next_subgrid_subtree);
    callback_func(subgrid_algorithm, next_subgrid_subtree,
                  SubgriddedItemData(grid_item, layout_data,
                                     GetConstraintSpace().GetWritingMode()));

    next_subgrid_subtree = next_subgrid_subtree.NextSibling();
  }
}

LayoutUnit GridLayoutAlgorithm::ComputeSubgridIntrinsicSize(
    const GridSizingSubtree& sizing_subtree,
    GridTrackSizingDirection track_direction,
    SizingConstraint sizing_constraint) const {
  DCHECK(sizing_subtree.HasValidRootFor(Node()));

  ComputeUsedTrackSizes(sizing_subtree, track_direction, sizing_constraint);
  const auto border_scrollbar_padding =
      (track_direction == kForColumns) ? BorderScrollbarPadding().InlineSum()
                                       : BorderScrollbarPadding().BlockSum();

  return border_scrollbar_padding + sizing_subtree.LayoutData()
                                        .SizingCollection(track_direction)
                                        .TotalTrackSize();
}

ConstraintSpace GridLayoutAlgorithm::CreateConstraintSpace(
    LayoutResultCacheSlot cache_slot,
    const GridItemData& grid_item,
    const LogicalSize& containing_grid_area_size,
    const LogicalSize& fixed_available_size,
    GridLayoutSubtree&& opt_layout_subtree,
    bool min_block_size_should_encompass_intrinsic_size,
    std::optional<LayoutUnit> opt_child_block_offset) const {
  const auto& container_constraint_space = GetConstraintSpace();

  ConstraintSpaceBuilder builder(
      container_constraint_space, grid_item.node.Style().GetWritingDirection(),
      /* is_new_fc */ true, /* adjust_inline_size_if_needed */ false);

  builder.SetCacheSlot(cache_slot);
  builder.SetIsPaintedAtomically(true);

  {
    auto available_size = containing_grid_area_size;
    if (fixed_available_size.inline_size != kIndefiniteSize) {
      available_size.inline_size = fixed_available_size.inline_size;
      builder.SetIsFixedInlineSize(true);
    }

    if (fixed_available_size.block_size != kIndefiniteSize) {
      available_size.block_size = fixed_available_size.block_size;
      builder.SetIsFixedBlockSize(true);
    }
    builder.SetAvailableSize(available_size);
  }

  if (opt_layout_subtree) {
    DCHECK(grid_item.IsSubgrid());
    DCHECK(!opt_layout_subtree.HasUnresolvedGeometry());
    builder.SetGridLayoutSubtree(std::move(opt_layout_subtree));
  }

  builder.SetPercentageResolutionSize(containing_grid_area_size);
  builder.SetInlineAutoBehavior(grid_item.column_auto_behavior);
  builder.SetBlockAutoBehavior(grid_item.row_auto_behavior);

  if (container_constraint_space.HasBlockFragmentation() &&
      opt_child_block_offset) {
    if (min_block_size_should_encompass_intrinsic_size)
      builder.SetMinBlockSizeShouldEncompassIntrinsicSize();

    SetupSpaceBuilderForFragmentation(container_builder_, grid_item.node,
                                      *opt_child_block_offset, &builder);
  }
  return builder.ToConstraintSpace();
}

ConstraintSpace GridLayoutAlgorithm::CreateConstraintSpaceForLayout(
    const GridItemData& grid_item,
    const GridLayoutData& layout_data,
    GridLayoutSubtree&& opt_layout_subtree,
    LogicalRect* containing_grid_area,
    LayoutUnit unavailable_block_size,
    bool min_block_size_should_encompass_intrinsic_size,
    std::optional<LayoutUnit> opt_child_block_offset) const {
  LayoutUnit inline_offset, block_offset;

  LogicalSize containing_grid_area_size = {
      grid_item.CalculateAvailableSize(layout_data.Columns(), &inline_offset),
      grid_item.CalculateAvailableSize(layout_data.Rows(), &block_offset)};

  if (containing_grid_area) {
    containing_grid_area->offset.inline_offset = inline_offset;
    containing_grid_area->offset.block_offset = block_offset;
    containing_grid_area->size = containing_grid_area_size;
  }

  if (containing_grid_area_size.block_size != kIndefiniteSize) {
    containing_grid_area_size.block_size -= unavailable_block_size;
    DCHECK_GE(containing_grid_area_size.block_size, LayoutUnit());
  }

  auto fixed_available_size = kIndefiniteLogicalSize;

  if (grid_item.IsSubgrid()) {
    const auto [fixed_inline_size, fixed_block_size] = ShrinkLogicalSize(
        containing_grid_area_size,
        ComputeMarginsFor(grid_item.node.Style(),
                          containing_grid_area_size.inline_size,
                          GetConstraintSpace().GetWritingDirection()));

    fixed_available_size = {
        grid_item.has_subgridded_columns ? fixed_inline_size : kIndefiniteSize,
        grid_item.has_subgridded_rows ? fixed_block_size : kIndefiniteSize};
  }

  return CreateConstraintSpace(
      LayoutResultCacheSlot::kLayout, grid_item, containing_grid_area_size,
      fixed_available_size, std::move(opt_layout_subtree),
      min_block_size_should_encompass_intrinsic_size, opt_child_block_offset);
}

ConstraintSpace GridLayoutAlgorithm::CreateConstraintSpaceForMeasure(
    const SubgriddedItemData& subgridded_item,
    GridTrackSizingDirection track_direction,
    std::optional<LayoutUnit> opt_fixed_inline_size) const {
  auto containing_grid_area_size = kIndefiniteLogicalSize;
  const auto writing_mode = GetConstraintSpace().GetWritingMode();

  if (track_direction == kForColumns) {
    containing_grid_area_size.block_size =
        subgridded_item->CalculateAvailableSize(
            subgridded_item.Rows(writing_mode));
  } else {
    containing_grid_area_size.inline_size =
        subgridded_item->CalculateAvailableSize(
            subgridded_item.Columns(writing_mode));
  }

  auto fixed_available_size =
      subgridded_item.IsSubgrid()
          ? ShrinkLogicalSize(
                containing_grid_area_size,
                ComputeMarginsFor(subgridded_item->node.Style(),
                                  containing_grid_area_size.inline_size,
                                  GetConstraintSpace().GetWritingDirection()))
          : kIndefiniteLogicalSize;

  if (opt_fixed_inline_size) {
    const auto item_writing_mode =
        subgridded_item->node.Style().GetWritingMode();
    auto& fixed_size = IsParallelWritingMode(item_writing_mode, writing_mode)
                           ? fixed_available_size.inline_size
                           : fixed_available_size.block_size;

    DCHECK_EQ(fixed_size, kIndefiniteSize);
    fixed_size = *opt_fixed_inline_size;
  }

  return CreateConstraintSpace(LayoutResultCacheSlot::kMeasure,
                               *subgridded_item, containing_grid_area_size,
                               fixed_available_size);
}

namespace {

class GapAccumulator {
  STACK_ALLOCATED();

 public:
  GapAccumulator() = default;

  // Builds the list of "main" gaps for Grid. In the MC (Main-Cross)
  // gap geometry model, we pick rows as the main axis (an arbitrary but
  // consistent choice) and columns as cross axis. This approach avoids
  // duplication and keeps storage minimal since intersections are computed
  // on-demand during paint.
  //
  // See third_party/blink/renderer/core/layout/gap/README.md for more.
  void BuildMainGaps(const GridLayoutData& layout_data) {
    const auto& rows = layout_data.Rows();
    const Vector<LayoutUnit> row_tracks =
        LayoutGrid::ComputeExpandedPositions(rows);
    row_gutter_size_ = rows.GutterSize();
    wtf_size_t row_track_count = row_tracks.size();

    // Initialize `cross_gaps_aggregator_` to track cell states along the cross
    // axis (columns). We pass in the number of row tracks because when we
    // aggregate column cell states, they are aggregated along the column for
    // each row in the grid.
    cross_gaps_aggregator_ =
        GapSegmentStateAggregator(/*cell_count=*/row_track_count - 1);

    // CSS Gaps[1] defines an intersection point to exist in the center of gaps.
    // Hence, we get the midpoint for each row gap for the derivation of
    // intersection points. The first gap ends at the second track, and the last
    // gap ends at the second-to-last track. So gaps are defined in the track
    // range [1, `row_track_count` - 1).
    //
    // [1] https://www.w3.org/TR/css-gaps-1/#gap-intersection-point
    // TODO(samomekarajr): This is currently O(nlogn) but can be optimized to
    // be O(n) if we find the first range index and increment it as we go.
    for (wtf_size_t i = 1; i < row_track_count - 1; ++i) {
      const wtf_size_t range_index = rows.RangeIndexFromGridLine(i);
      if (rows.RangeProperties(range_index)
              .HasProperty(TrackSpanProperties::kIsCollapsed)) {
        continue;
      }

      LayoutUnit row_midpoint =
          LayoutUnit(row_tracks[i] - (row_gutter_size_ / 2.0f));
      MainGap main_gap = MainGap(row_midpoint);
      main_gaps_.push_back(main_gap);
    }

    content_block_start_ = row_tracks[0];
    content_block_end_ = row_tracks[row_track_count - 1];
  }

  void BuildCrossGaps(const GridLayoutData& layout_data) {
    const auto& columns = layout_data.Columns();
    const Vector<LayoutUnit> col_tracks =
        LayoutGrid::ComputeExpandedPositions(columns);
    col_gutter_size_ = columns.GutterSize();
    wtf_size_t col_track_count = col_tracks.size();

    // Initialize `main_gaps_aggregator_` to track cell states along the main
    // axis (rows). We pass in the number of column tracks because when we
    // aggregate row cell states, they are aggregated along the row for
    // each column in the grid.
    main_gaps_aggregator_ =
        GapSegmentStateAggregator(/*cell_count=*/col_track_count - 1);

    // CSS Gaps defines an intersection point to exist in the center
    // of gaps. Hence, we get the midpoint for each column gap for the
    // derivation of intersection points. The first gap ends at the second
    // track, and the last gap ends at the second-to-last track. So gaps are
    // defined in the track range [1, `col_track_count` - 1).
    // See: https://www.w3.org/TR/css-gaps-1/#gap-intersection-point
    // TODO(samomekarajr): This is currently O(nlogn) but can be optimized to
    // be O(n) if we find the first range index and increment it as we go.
    for (wtf_size_t i = 1; i < col_track_count - 1; ++i) {
      const wtf_size_t range_index = columns.RangeIndexFromGridLine(i);
      if (columns.RangeProperties(range_index)
              .HasProperty(TrackSpanProperties::kIsCollapsed)) {
        continue;
      }
      LayoutUnit col_midpoint =
          LayoutUnit(col_tracks[i] - (col_gutter_size_ / 2.0f));
      LogicalOffset cross_gap_offset =
          LogicalOffset(col_midpoint, LayoutUnit());
      CrossGap cross_gap = CrossGap(cross_gap_offset);
      cross_gaps_.push_back(cross_gap);
    }

    content_inline_start_ = col_tracks[0];
    content_inline_end_ = col_tracks[col_track_count - 1];
  }

  void BuildGapGeometry(const GridLayoutData& layout_data) {
    BuildMainGaps(layout_data);
    BuildCrossGaps(layout_data);
  }

  // Aggregates the intervals of gaps blocked by a `grid_item`. This identifies
  // which gaps are intersected by a spanning item and records the track ranges
  // within those gaps that are blocked.
  //
  // For example:
  // - If a grid item spans columns [0, 3] and rows [3, 5]:
  //     - It crosses column gaps at indices [0, 1]. For each of these column
  //     gaps, the blocked row range is [3, 5].
  //     - It crosses row gaps at index [3]. For this row gap, the blocked
  //     column range is [0, 3].
  //
  // For an item spanning tracks [start_line, end_line], the gap indices it
  // crosses are [start_line, end_line - 1).
  void AggregateCellStates(const GridItemData& grid_item) {
    main_gaps_aggregator_.ProcessItem(grid_item.Span(kForRows),
                                      grid_item.Span(kForColumns));
    cross_gaps_aggregator_.ProcessItem(grid_item.Span(kForColumns),
                                       grid_item.Span(kForRows));
  }

  // Returns a mapping from row gap indices to their corresponding set indices.
  // The returned vector represents the mapping where the index in the vector
  // corresponds to the row gap index, and the value at that index is the
  // corresponding set index. This follows a similar implementation as
  // `LayoutGrid::ComputeExpandedPositions` and
  // `LayoutGrid::CollectTrackSizesForComputedStyle` but adapted to row gaps.
  Vector<wtf_size_t> GetRowGapToSetIndicesMap(
      const GridLayoutData& layout_data) {
    const auto& rows = layout_data.Rows();

    const wtf_size_t range_count = rows.RangeCount();
    Vector<wtf_size_t> gap_idx_to_set_idx;

    for (wtf_size_t range_idx = 0; range_idx < range_count; ++range_idx) {
      const wtf_size_t range_set_count = rows.RangeSetCount(range_idx);
      const wtf_size_t begin_set_index = rows.RangeBeginSetIndex(range_idx);
      const wtf_size_t tracks_in_range = rows.RangeTrackCount(range_idx);

      for (wtf_size_t track_idx_in_range = 0;
           track_idx_in_range < tracks_in_range; ++track_idx_in_range) {
        // Skip the last track in the last range since there's no gap after
        // the final track.
        if (range_idx == range_count - 1 &&
            track_idx_in_range == tracks_in_range - 1) {
          break;
        }

        // Determine which set this track belongs to by using the
        // `begin_set_index` plus the track's set position within this range.
        // The set position is determined using the modulo operator since sets
        // preserve the order in which track definitions appear in their range.
        // If a range has no sets, we exclude it from the list because gaps
        // are not emitted for collapsed tracks.
        if (range_set_count) {
          wtf_size_t set_idx =
              begin_set_index + (track_idx_in_range % range_set_count);
          gap_idx_to_set_idx.emplace_back(set_idx);
        }
        CHECK_LE(gap_idx_to_set_idx.size(),
                 static_cast<wtf_size_t>(kGridMaxTracks));
        if (gap_idx_to_set_idx.size() == kGridMaxTracks) {
          // Return early to prevent exceeding the maximum allowed grid tracks
          // limit.
          return gap_idx_to_set_idx;
        }
      }
    }

    return gap_idx_to_set_idx;
  }

  const GapGeometry* FinalizeGapGeometry() {
    const bool has_main_gaps =
        !main_gaps_.empty() && row_gutter_size_ > LayoutUnit();
    const bool has_cross_gaps =
        !cross_gaps_.empty() && col_gutter_size_ > LayoutUnit();
    // `GapGeometry` requires both row(main) and column(cross) gaps to be valid.

    if (!has_cross_gaps && !has_main_gaps) {
      return nullptr;
    }

    GapGeometry* gap_geometry =
        MakeGarbageCollected<GapGeometry>(GapGeometry::ContainerType::kGrid);

    gap_geometry->SetInlineGapSize(col_gutter_size_);
    gap_geometry->SetBlockGapSize(row_gutter_size_);

    // Finalize the `GapSegmentStateRanges` for each gap using the aggregated
    // cell states collected during `AggregateCellStates`.
    for (wtf_size_t gap_index = 0; gap_index < main_gaps_.size(); ++gap_index) {
      main_gaps_aggregator_.FinalizeGapSegmentStateRangesFor(
          main_gaps_[gap_index], gap_index);
    }

    for (wtf_size_t gap_index = 0; gap_index < cross_gaps_.size();
         ++gap_index) {
      cross_gaps_aggregator_.FinalizeGapSegmentStateRangesFor(
          cross_gaps_[gap_index], gap_index);
    }

    if (row_gutter_size_ > LayoutUnit() && !main_gaps_.empty()) {
      gap_geometry->SetMainGaps(std::move(main_gaps_));
    }

    if (col_gutter_size_ > LayoutUnit() && !cross_gaps_.empty()) {
      gap_geometry->SetCrossGaps(std::move(cross_gaps_));
    }

    gap_geometry->SetContentInlineOffsets(content_inline_start_,
                                          content_inline_end_);
    gap_geometry->SetContentBlockOffsets(content_block_start_,
                                         content_block_end_);

    return gap_geometry;
  }

 private:
  MainGaps main_gaps_;
  CrossGaps cross_gaps_;

  LayoutUnit content_block_start_;
  LayoutUnit content_block_end_;
  LayoutUnit content_inline_start_;
  LayoutUnit content_inline_end_;

  LayoutUnit col_gutter_size_;
  LayoutUnit row_gutter_size_;

  GapSegmentStateAggregator main_gaps_aggregator_;
  GapSegmentStateAggregator cross_gaps_aggregator_;
};

}  // namespace

void GridLayoutAlgorithm::PlaceGridItems(
    const GridItems& grid_items,
    const GridLayoutSubtree& layout_subtree,
    Vector<EBreakBetween>* out_row_break_between,
    Vector<GridItemPlacementData>* out_grid_items_placement_data,
    const GapGeometry** out_unfragmented_gap_geometry,
    Vector<wtf_size_t>* out_track_idx_to_set_idx) {
  DCHECK(out_row_break_between);

  const auto& container_space = GetConstraintSpace();
  const auto& layout_data = layout_subtree.LayoutData();
  const bool should_propagate_child_break_values =
      container_space.ShouldPropagateChildBreakValues();

  if (should_propagate_child_break_values) {
    *out_row_break_between = Vector<EBreakBetween>(
        layout_data.Rows().GetSetCount() + 1, EBreakBetween::kAuto);
  }

  GridBaselineAccumulator baseline_accumulator(Style().GetFontBaseline());
  const auto container_writing_direction =
      container_space.GetWritingDirection();
  auto next_subgrid_subtree = layout_subtree.FirstChild();

  std::optional<GapAccumulator> gap_accumulator;

  // Construct gap geometry if we have gap decoration rules or if we are in a
  // fragmentation context, because the gap geometry is needed to suppress gaps,
  // regardless of the presence of decorations.
  //
  // TODO(samomekarajr): This can be optimized to avoid building gap geometry
  // fully for different scenarios (e.g. if there are no gaps but there are
  // decorations).
  if ((RuntimeEnabledFeatures::CSSGapDecorationEnabled() &&
       Style().HasGapRule()) ||
      (RuntimeEnabledFeatures::CSSGridGapSuppressionEnabled() &&
       out_unfragmented_gap_geometry)) {
    gap_accumulator = GapAccumulator();
    gap_accumulator->BuildGapGeometry(layout_data);

    if (out_track_idx_to_set_idx) {
      *out_track_idx_to_set_idx =
          gap_accumulator->GetRowGapToSetIndicesMap(layout_data);
    }
  }

  for (const auto& grid_item : grid_items) {
    GridLayoutSubtree child_layout_subtree;

    if (grid_item.IsSubgrid()) {
      DCHECK(next_subgrid_subtree);
      child_layout_subtree = next_subgrid_subtree;
      next_subgrid_subtree = next_subgrid_subtree.NextSibling();
    }

    LogicalRect containing_grid_area;
    const auto space = CreateConstraintSpaceForLayout(
        grid_item, layout_data, std::move(child_layout_subtree),
        &containing_grid_area);

    const auto& item_style = grid_item.node.Style();
    const auto margins = ComputeMarginsFor(space, item_style, container_space);

    auto* result = grid_item.node.Layout(space);
    const auto& physical_fragment =
        To<PhysicalBoxFragment>(result->GetPhysicalFragment());
    LogicalBoxFragment fragment(container_writing_direction, physical_fragment);

    auto BaselineOffset = [&](GridTrackSizingDirection track_direction,
                              LayoutUnit size) -> LayoutUnit {
      if (!grid_item.IsBaselineAligned(track_direction)) {
        return LayoutUnit();
      }

      LogicalBoxFragment baseline_fragment(
          grid_item.BaselineWritingDirection(track_direction),
          physical_fragment);
      // The baseline offset is the difference between the grid item's baseline
      // and its track baseline.
      const LayoutUnit baseline_delta =
          Baseline(grid_item, layout_data, track_direction) -
          GetLogicalBaseline(grid_item, baseline_fragment, track_direction);
      if (grid_item.BaselineGroup(track_direction) == BaselineGroup::kMajor)
        return baseline_delta;

      // BaselineGroup::kMinor
      const LayoutUnit item_size = (track_direction == kForColumns)
                                       ? fragment.InlineSize()
                                       : fragment.BlockSize();
      return size - baseline_delta - item_size;
    };

    LayoutUnit inline_baseline_offset =
        BaselineOffset(kForColumns, containing_grid_area.size.inline_size);
    LayoutUnit block_baseline_offset =
        BaselineOffset(kForRows, containing_grid_area.size.block_size);

    // Apply the grid-item's alignment (if any).
    containing_grid_area.offset += LogicalOffset(
        AlignmentOffset(containing_grid_area.size.inline_size,
                        fragment.InlineSize(), margins.inline_start,
                        margins.inline_end, inline_baseline_offset,
                        grid_item.Alignment(kForColumns),
                        grid_item.IsOverflowSafe(kForColumns)),
        AlignmentOffset(
            containing_grid_area.size.block_size, fragment.BlockSize(),
            margins.block_start, margins.block_end, block_baseline_offset,
            grid_item.Alignment(kForRows), grid_item.IsOverflowSafe(kForRows)));

    // Grid is special in that %-based offsets resolve against the grid-area.
    // Determine the relative offset here (instead of in the builder). This is
    // safe as grid *also* has special inflow-bounds logic (otherwise this
    // wouldn't work).
    LogicalOffset relative_offset = LogicalOffset();
    if (item_style.GetPosition() == EPosition::kRelative) {
      relative_offset += ComputeRelativeOffsetForBoxFragment(
          physical_fragment, container_writing_direction,
          containing_grid_area.size);
    }

    // If |out_grid_items_placement_data| is present we just want to record the
    // initial position of all the children for the purposes of fragmentation.
    // Don't add these to the builder.
    if (out_grid_items_placement_data) {
      out_grid_items_placement_data->emplace_back(
          containing_grid_area.offset, relative_offset,
          result->HasDescendantThatDependsOnPercentageBlockSize());
    } else {
      container_builder_.AddResult(*result, containing_grid_area.offset,
                                   margins, relative_offset);
      baseline_accumulator.Accumulate(grid_item, fragment,
                                      containing_grid_area.offset.block_offset);
    }

    if (should_propagate_child_break_values) {
      auto item_break_before = JoinFragmentainerBreakValues(
          item_style.BreakBefore(), result->InitialBreakBefore());
      auto item_break_after = JoinFragmentainerBreakValues(
          item_style.BreakAfter(), result->FinalBreakAfter());

      const auto& set_indices = grid_item.SetIndices(kForRows);
      (*out_row_break_between)[set_indices.begin] =
          JoinFragmentainerBreakValues(
              (*out_row_break_between)[set_indices.begin], item_break_before);
      (*out_row_break_between)[set_indices.end] = JoinFragmentainerBreakValues(
          (*out_row_break_between)[set_indices.end], item_break_after);
    }

    if (gap_accumulator) {
      gap_accumulator->AggregateCellStates(grid_item);
    }
  }

  if (gap_accumulator) {
    if (const auto* gap_geometry = gap_accumulator->FinalizeGapGeometry()) {
      // If `out_unfragmented_gap_geometry` is present we just want to record
      // the initial position of all gaps for the purposes of fragmentation.
      // Don't add these to the builder.
      if (RuntimeEnabledFeatures::CSSGridGapSuppressionEnabled() &&
          out_unfragmented_gap_geometry) {
        *out_unfragmented_gap_geometry = gap_geometry;
      } else {
        container_builder_.SetGapGeometry(gap_geometry);
      }
    }
  }

  // Propagate the baselines.
  if (layout_data.Rows().HasBaselines()) {
    baseline_accumulator.AccumulateRows(layout_data.Rows());
  }
  if (auto first_baseline = baseline_accumulator.FirstBaseline())
    container_builder_.SetFirstBaseline(*first_baseline);
  if (auto last_baseline = baseline_accumulator.LastBaseline())
    container_builder_.SetLastBaseline(*last_baseline);
}

// This is only used in GridLayoutAlgorithm::PlaceGridItemsForFragmentation(),
// but placed here to add WTF VectorTraits.
struct ResultAndOffsets {
  DISALLOW_NEW();

 public:
  ResultAndOffsets(const LayoutResult* result,
                   LogicalOffset offset,
                   LogicalOffset relative_offset)
      : result(result), offset(offset), relative_offset(relative_offset) {}

  void Trace(Visitor* visitor) const { visitor->Trace(result); }

  Member<const LayoutResult> result;
  LogicalOffset offset;
  LogicalOffset relative_offset;
};

void GridLayoutAlgorithm::PlaceGridItemsForFragmentation(
    const GridItems& grid_items,
    const GridLayoutSubtree& layout_subtree,
    const Vector<EBreakBetween>& row_break_between,
    const GapGeometry* full_gap_geometry,
    const Vector<wtf_size_t>* track_idx_to_set_idx,
    Vector<wtf_size_t>* column_gaps_segment_ranges_start_indices,
    Vector<GridItemPlacementData>* grid_items_placement_data,
    Vector<LayoutUnit>* row_offset_adjustments,
    LayoutUnit* intrinsic_block_size,
    LayoutUnit* offset_in_stitched_container,
    LayoutUnit* cumulative_gap_offset_adjustment,
    wtf_size_t* first_unprocessed_row_gap_idx) {
  DCHECK(grid_items_placement_data && row_offset_adjustments &&
         intrinsic_block_size && offset_in_stitched_container);

  // TODO(ikilpatrick): Update `SetHasSeenAllChildren` and early exit if true.
  const auto& constraint_space = GetConstraintSpace();
  const auto container_writing_direction =
      constraint_space.GetWritingDirection();

  // The following roughly comes from:
  // https://drafts.csswg.org/css-grid-1/#fragmentation-alg
  //
  // We are interested in cases where the grid-item *may* expand due to
  // fragmentation (lines pushed down by a fragmentation line, etc).
  auto MinBlockSizeShouldEncompassIntrinsicSize =
      [&](const GridItemData& grid_item,
          bool has_descendant_that_depends_on_percentage_block_size) -> bool {
    // If this item has (any) descendant that is percentage based, we can end
    // up in a situation where we'll constantly try and expand the row. E.g.
    // <div style="display: grid;">
    //   <div style="min-height: 100px;">
    //     <div style="height: 200%;"></div>
    //   </div>
    // </div>
    if (has_descendant_that_depends_on_percentage_block_size)
      return false;

    if (grid_item.node.IsMonolithic())
      return false;

    const auto& item_style = grid_item.node.Style();

    // NOTE: We currently assume that writing-mode roots are monolithic, but
    // this may change in the future.
    DCHECK_EQ(container_writing_direction.GetWritingMode(),
              item_style.GetWritingMode());

    // Only allow growth on "auto" block-size items, unless box decorations are
    // to be cloned. Even a fixed block-size item can grow if box decorations
    // are cloned (as long as box-sizing is content-box).
    if (!item_style.LogicalHeight().HasAutoOrContentOrIntrinsic() &&
        item_style.BoxDecorationBreak() != EBoxDecorationBreak::kClone) {
      return false;
    }

    // Only allow growth on items which only span a single row.
    if (grid_item.SpanSize(kForRows) > 1)
      return false;

    // If we have a fixed maximum track, we assume that we've hit this maximum,
    // and as such shouldn't grow.
    if (grid_item.IsSpanningFixedMaximumTrack(kForRows) &&
        !grid_item.IsSpanningIntrinsicTrack(kForRows))
      return false;

    return !grid_item.IsSpanningFixedMinimumTrack(kForRows) ||
           Style().LogicalHeight().HasAutoOrContentOrIntrinsic();
  };

  wtf_size_t previous_expansion_row_set_index = kNotFound;
  auto IsExpansionMakingProgress = [&](wtf_size_t row_set_index) -> bool {
    return previous_expansion_row_set_index == kNotFound ||
           row_set_index > previous_expansion_row_set_index;
  };

  HeapVector<ResultAndOffsets> result_and_offsets;
  GridBaselineAccumulator baseline_accumulator(Style().GetFontBaseline());
  LayoutUnit max_row_expansion;
  LayoutUnit max_item_block_end;
  wtf_size_t expansion_row_set_index;
  wtf_size_t breakpoint_row_set_index;
  bool has_subsequent_children;

  auto UpdateBreakpointRowSetIndex = [&](wtf_size_t row_set_index) {
    if (row_set_index >= breakpoint_row_set_index)
      return;

    breakpoint_row_set_index = row_set_index;
  };

  LayoutUnit fragmentainer_space = FragmentainerSpaceLeftForChildren();
  LayoutUnit cloned_block_start_decoration;
  if (fragmentainer_space != kIndefiniteSize) {
    // Cloned block-start box decorations take up space at the beginning of a
    // fragmentainer, and are baked into fragmentainer_space, but this is not
    // part of the content progress.
    cloned_block_start_decoration =
        ClonedBlockStartDecoration(container_builder_);
    fragmentainer_space -= cloned_block_start_decoration;
  }

  const auto fragmentainer_block_size = FragmentainerCapacityForChildren();
  const auto& layout_data = layout_subtree.LayoutData();

  base::span<const Member<const BreakToken>> child_break_tokens;
  if (GetBreakToken()) {
    child_break_tokens = GetBreakToken()->ChildBreakTokens();
  }

  auto PlaceItems = [&]() {
    // Reset our state.
    result_and_offsets.clear();
    baseline_accumulator = GridBaselineAccumulator(Style().GetFontBaseline());
    max_row_expansion = LayoutUnit();
    max_item_block_end = LayoutUnit();
    expansion_row_set_index = kNotFound;
    breakpoint_row_set_index = kNotFound;
    has_subsequent_children = false;

    auto next_subgrid_subtree = layout_subtree.FirstChild();
    auto child_break_token_it = base::span(child_break_tokens).begin();
    auto placement_data_it = base::span(*grid_items_placement_data).begin();

    for (const auto& grid_item : grid_items) {
      // Grab the offsets and break-token (if present) for this child.
      auto& item_placement_data = *(placement_data_it++);
      const BlockBreakToken* break_token = nullptr;
      if (child_break_token_it != child_break_tokens.end()) {
        if ((*child_break_token_it)->InputNode() == grid_item.node)
          break_token = To<BlockBreakToken>((child_break_token_it++)->Get());
      }

      LayoutUnit child_block_offset;
      if (IsBreakInside(break_token)) {
        child_block_offset = BorderScrollbarPadding().block_start;
      } else {
        // Include any cloned block-start box decorations. The item offset
        // offset is in the imaginary stitched container that we would have had
        // had we not been fragmented, and now we want actual layout offsets for
        // the current fragment.
        child_block_offset = item_placement_data.offset.block_offset -
                             *offset_in_stitched_container +
                             cloned_block_start_decoration;
      }
      LayoutUnit fragmentainer_block_offset =
          FragmentainerOffsetForChildren() + child_block_offset;
      const bool min_block_size_should_encompass_intrinsic_size =
          MinBlockSizeShouldEncompassIntrinsicSize(
              grid_item,
              item_placement_data
                  .has_descendant_that_depends_on_percentage_block_size);

      LayoutUnit unavailable_block_size;
      if (IsBreakInside(GetBreakToken()) && IsBreakInside(break_token)) {
        // If a sibling grid item has overflowed the fragmentainer (in a
        // previous fragment) due to monolithic content, the grid container has
        // been stretched to encompass it, but the other grid items (like this
        // one) have not (we still want the non-overflowed items to fragment
        // properly). The available space left in the row needs to be shrunk, in
        // order to compensate for this, or this item might overflow the grid
        // row.
        const auto* grid_data =
            To<GridBreakTokenData>(GetBreakToken()->TokenData());
        unavailable_block_size = grid_data->offset_in_stitched_container -
                                 (item_placement_data.offset.block_offset +
                                  break_token->ConsumedBlockSize());
      }

      GridLayoutSubtree subgrid_layout_subtree;
      if (grid_item.IsSubgrid()) {
        DCHECK(next_subgrid_subtree);
        subgrid_layout_subtree = next_subgrid_subtree;
        next_subgrid_subtree = next_subgrid_subtree.NextSibling();
      }

      LogicalRect grid_area;
      const auto space = CreateConstraintSpaceForLayout(
          grid_item, layout_data, std::move(subgrid_layout_subtree), &grid_area,
          unavailable_block_size,
          min_block_size_should_encompass_intrinsic_size, child_block_offset);

      // Make the grid area relative to this fragment.
      const auto item_row_set_index = grid_item.SetIndices(kForRows).begin;
      grid_area.offset.block_offset +=
          (*row_offset_adjustments)[item_row_set_index] +
          *cumulative_gap_offset_adjustment - *offset_in_stitched_container;

      // Check to see if this child should be placed within this fragmentainer.
      // We base this calculation on the grid-area rather than the offset.
      // The row can either be:
      //  - Above, we've handled it already in a previous fragment.
      //  - Below, we'll handle it within a subsequent fragment.
      //
      // NOTE: Basing this calculation of the row position has the effect that
      // a child with a negative margin will be placed in the fragmentainer
      // with its row, but placed above the block-start edge of the
      // fragmentainer.
      if (fragmentainer_space != kIndefiniteSize &&
          grid_area.offset.block_offset >= fragmentainer_space) {
        if (constraint_space.IsInsideBalancedColumns() &&
            !constraint_space.IsInitialColumnBalancingPass()) {
          // Although we know that this item isn't going to fit here, we're
          // inside balanced multicol, so we need to figure out how much more
          // fragmentainer space we'd need to fit more content.
          DisableLayoutSideEffectsScope disable_side_effects;
          auto* result = grid_item.node.Layout(space, break_token);
          PropagateSpaceShortage(constraint_space, result,
                                 fragmentainer_block_offset,
                                 fragmentainer_block_size, &container_builder_);
        }
        has_subsequent_children = true;
        continue;
      }
      if (grid_area.offset.block_offset < LayoutUnit() && !break_token)
        continue;

      auto* result = grid_item.node.Layout(space, break_token);
      DCHECK_EQ(result->Status(), LayoutResult::kSuccess);
      result_and_offsets.emplace_back(
          result,
          LogicalOffset(item_placement_data.offset.inline_offset,
                        child_block_offset),
          item_placement_data.relative_offset);

      const LogicalBoxFragment fragment(
          container_writing_direction,
          To<PhysicalBoxFragment>(result->GetPhysicalFragment()));
      baseline_accumulator.Accumulate(grid_item, fragment, child_block_offset);

      // If the row has container separation we are able to push it into the
      // next fragmentainer. If it doesn't we, need to take the current
      // breakpoint (even if it is undesirable).
      const bool row_has_container_separation =
          grid_area.offset.block_offset > LayoutUnit();

      if (row_has_container_separation &&
          item_row_set_index < breakpoint_row_set_index) {
        const auto break_between = row_break_between[item_row_set_index];

        // The row may have a forced break, move it to the next fragmentainer.
        if (IsForcedBreakValue(constraint_space, break_between)) {
          container_builder_.SetHasForcedBreak();
          UpdateBreakpointRowSetIndex(item_row_set_index);
          continue;
        }

        container_builder_.SetPreviousBreakAfter(break_between);
        const BreakAppeal appeal_before = CalculateBreakAppealBefore(
            constraint_space, grid_item.node, *result, container_builder_,
            row_has_container_separation);

        // TODO(layout-dev): Explain the special usage of
        // MovePastBreakpoint(). No fragment builder passed?
        if (!::blink::MovePastBreakpoint(constraint_space, grid_item.node,
                                         *result, fragmentainer_block_offset,
                                         fragmentainer_block_size,
                                         appeal_before,
                                         /*builder=*/nullptr)) {
          UpdateBreakpointRowSetIndex(item_row_set_index);

          // We are choosing to add an early breakpoint at a row. Propagate our
          // space shortage to the column balancer.
          PropagateSpaceShortage(constraint_space, result,
                                 fragmentainer_block_offset,
                                 fragmentainer_block_size, &container_builder_);

          // We may have "break-before:avoid" or similar on this row. Instead
          // of just breaking on this row, search upwards for a row with a
          // better EBreakBetween.
          if (IsAvoidBreakValue(constraint_space, break_between)) {
            for (int index = item_row_set_index - 1; index >= 0; --index) {
              // Only consider rows within this fragmentainer.
              LayoutUnit offset = layout_data.Rows().GetSetOffset(index) +
                                  (*row_offset_adjustments)[index] +
                                  *cumulative_gap_offset_adjustment -
                                  *offset_in_stitched_container;
              if (offset <= LayoutUnit())
                break;

              // Forced row breaks should have been already handled, accept any
              // row with an "auto" break-between.
              if (row_break_between[index] == EBreakBetween::kAuto) {
                UpdateBreakpointRowSetIndex(index);
                break;
              }
            }
          }
          continue;
        }
      }

      // We should only try to expand this grid's rows below if we have no grid
      // layout subtree, as a subgrid cannot alter its subgridded tracks.
      const bool is_standalone_grid = !constraint_space.GetGridLayoutSubtree();

      // This item may want to expand due to fragmentation. Record how much we
      // should grow the row by (if applicable).
      if (is_standalone_grid &&
          min_block_size_should_encompass_intrinsic_size &&
          item_row_set_index <= expansion_row_set_index &&
          IsExpansionMakingProgress(item_row_set_index) &&
          fragmentainer_space != kIndefiniteSize &&
          grid_area.BlockEndOffset() <= fragmentainer_space) {
        // Check if we've found a different row to expand.
        if (expansion_row_set_index != item_row_set_index) {
          expansion_row_set_index = item_row_set_index;
          max_row_expansion = LayoutUnit();
        }

        LayoutUnit item_expansion;
        if (result->GetPhysicalFragment().GetBreakToken()) {
          // This item may have a break, and will want to expand into the next
          // fragmentainer, (causing the row to expand into the next
          // fragmentainer). We can't use the size of the fragment, as we don't
          // know how large the subsequent fragments will be (and how much
          // they'll expand the row).
          //
          // Instead of using the size of the fragment, expand the row to the
          // rest of the fragmentainer, with an additional epsilon. This epsilon
          // will ensure that we continue layout for children in this row in
          // the next fragmentainer. Without it we'd drop those subsequent
          // fragments.
          item_expansion =
              (fragmentainer_space - grid_area.BlockEndOffset()).AddEpsilon();
        } else {
          item_expansion = fragment.BlockSize() - grid_area.BlockEndOffset();
        }
        max_row_expansion = std::max(max_row_expansion, item_expansion);
      }

      // Keep track of the tallest item, in case it overflows the fragmentainer
      // with monolithic content.
      max_item_block_end = std::max(max_item_block_end,
                                    child_block_offset + fragment.BlockSize());
    }
  };

  // Adjust by |delta| the pre-computed item-offset for all grid items with a
  // row begin index greater or equal than |row_index|.
  auto AdjustItemOffsets = [&](wtf_size_t row_index, LayoutUnit delta) {
    auto current_item = grid_items.begin();

    for (auto& item_placement_data : *grid_items_placement_data) {
      if (row_index <= (current_item++)->SetIndices(kForRows).begin)
        item_placement_data.offset.block_offset += delta;
    }
  };

  // Adjust our grid break-token data to accommodate the larger item in the row.
  // Returns true if this function adjusted the break-token data in any way.
  auto ExpandRow = [&]() -> bool {
    if (max_row_expansion == 0)
      return false;

    DCHECK_GT(max_row_expansion, 0);
    DCHECK(IsExpansionMakingProgress(expansion_row_set_index));

    *intrinsic_block_size += max_row_expansion;
    AdjustItemOffsets(expansion_row_set_index + 1, max_row_expansion);
    layout_data.Rows().AdjustSetOffsets(expansion_row_set_index + 1,
                                        max_row_expansion);

    previous_expansion_row_set_index = expansion_row_set_index;
    return true;
  };

  // Shifts the row where we wish to take a breakpoint (indicated by
  // |breakpoint_row_set_index|) into the next fragmentainer.
  // Returns true if this function adjusted the break-token data in any way.
  auto ShiftBreakpointIntoNextFragmentainer = [&]() -> bool {
    if (breakpoint_row_set_index == kNotFound)
      return false;

    LayoutUnit row_offset =
        layout_data.Rows().GetSetOffset(breakpoint_row_set_index) +
        (*row_offset_adjustments)[breakpoint_row_set_index] +
        *cumulative_gap_offset_adjustment;

    const LayoutUnit fragment_relative_row_offset =
        row_offset - *offset_in_stitched_container;

    // We may be within the initial column-balancing pass (where we have an
    // indefinite fragmentainer size). If we have a forced break, re-run
    // |PlaceItems()| assuming the breakpoint offset is the fragmentainer size.
    if (fragmentainer_space == kIndefiniteSize) {
      fragmentainer_space = fragment_relative_row_offset;
      return true;
    }

    const LayoutUnit row_offset_delta =
        fragmentainer_space - fragment_relative_row_offset;

    // An expansion may have occurred in |ExpandRow| which already pushed this
    // row into the next fragmentainer.
    if (row_offset_delta <= LayoutUnit())
      return false;

    row_offset += row_offset_delta;
    *intrinsic_block_size += row_offset_delta;
    AdjustItemOffsets(breakpoint_row_set_index, row_offset_delta);

    auto it =
        base::span(*row_offset_adjustments).begin() + breakpoint_row_set_index;
    while (it != row_offset_adjustments->end())
      *(it++) += row_offset_delta;

    return true;
  };

  // PlaceGaps simply places all gaps that fit within the current
  // fragmentainer. The main idea is that we start from the first unprocessed
  // gap, and place gaps until we find one that doesn't fit. If the last gap
  // placed is split by the fragmentainer boundary or is the last content in
  // this fragmentainer, we suppress it and adjust the `intrinsic_block_size`
  // and item offsets by the delta of the gap that might have spilled over to
  // the next fragmentainer.
  //
  // TODO(samomekarajr): We currently suppress "free space" due to alignment as
  // we would gaps. This is because the track sizing algorithm records free
  // space as part of the gutters. This needs to be investigated further to
  // determine what. the right behavior should be in these cases.
  auto PlaceGaps = [&]() {
    if (!full_gap_geometry || fragmentainer_space == kIndefiniteSize) {
      return;
    }

    const MainGaps& main_gaps = full_gap_geometry->GetMainGaps();
    if (main_gaps.empty()) {
      return;
    }

    MainGaps fragment_main_gaps;
    LayoutUnit half_row_gap_size = full_gap_geometry->GetBlockGapSize() / 2;

    // Determines whether the last placed gap needs to be suppressed because it
    // crosses the fragmentainer boundary or is the last content.
    auto MaybeSuppressLastGap = [&](wtf_size_t row_set_idx_for_gap) {
      CHECK(!fragment_main_gaps.empty());
      LayoutUnit last_gap_end_offset =
          fragment_main_gaps.back().GetGapOffset() + half_row_gap_size;

      LayoutUnit next_row_offset =
          layout_data.Rows().GetSetOffset(row_set_idx_for_gap + 1) +
          (*row_offset_adjustments)[row_set_idx_for_gap + 1] +
          *cumulative_gap_offset_adjustment;
      // Make gap offset relative to this fragmentainer.
      next_row_offset -= *offset_in_stitched_container;

      // In order to determine if the last gap placed is split or is the last
      // content in this fragmentainer, we look at the row after the gap.
      // If the row after the gap is in this fragmentainer, then the gap is
      // not split, hence not suppressed. If the row after the gap is not in
      // this fragmentainer, then the gap is either split or is the last
      // content in this fragmentainer, hence it is suppressed. We can't just
      // rely on the `next_row_offset` alone since it is based on the
      // `SetIndices`, (which is based on the presence of items) so we add a
      // condition to also check if the last gap's end offset is at or beyond
      // the fragmentainer space.
      if (next_row_offset >= fragmentainer_space ||
          last_gap_end_offset >= fragmentainer_space) {
        fragment_main_gaps.pop_back();
        LayoutUnit spillover_delta =
            (last_gap_end_offset - fragmentainer_space).ClampNegativeToZero();
        if (spillover_delta > LayoutUnit()) {
          *cumulative_gap_offset_adjustment -= spillover_delta;
          *intrinsic_block_size -= spillover_delta;
          AdjustItemOffsets(row_set_idx_for_gap + 1, -spillover_delta);
        }
      }
    };

    wtf_size_t current_processed_gap_set_idx = kNotFound;
    const wtf_size_t initial_unprocessed_row_gap_idx =
        *first_unprocessed_row_gap_idx;
    for (wtf_size_t gap_index = *first_unprocessed_row_gap_idx;
         gap_index < main_gaps.size(); ++gap_index) {
      LayoutUnit row_gap_midpoint = main_gaps[gap_index].GetGapOffset() +
                                    *cumulative_gap_offset_adjustment;
      CHECK_LT(gap_index, track_idx_to_set_idx->size());
      current_processed_gap_set_idx = (*track_idx_to_set_idx)[gap_index];
      row_gap_midpoint +=
          (*row_offset_adjustments)[current_processed_gap_set_idx];
      // Make the gap offset relative to this fragmentainer.
      row_gap_midpoint -= *offset_in_stitched_container;
      const LayoutUnit row_gap_start_offset =
          row_gap_midpoint - half_row_gap_size;
      // If the gap start is beyond the fragmentainer space, this is the first
      // gap we know doesn't fit in this fragmentainer, so we should break.
      if (row_gap_start_offset > fragmentainer_space) {
        // If we have placed gaps in this fragment, we need to check if the last
        // placed gap needs to be suppressed. Hence, we get the set index for
        // the previous gap since that will be the gap to consider for
        // suppression.
        if (fragment_main_gaps.size() > 0) {
          current_processed_gap_set_idx =
              (*track_idx_to_set_idx)[gap_index - 1];
        }
        break;
      }

      fragment_main_gaps.push_back(
          MainGap(main_gaps[gap_index], row_gap_midpoint));
      *first_unprocessed_row_gap_idx = gap_index + 1;
    }

    // We need to check the last placed gap to determine if it needs to be
    // suppressed.
    if (!fragment_main_gaps.empty()) {
      MaybeSuppressLastGap(current_processed_gap_set_idx);
    }

    // Create gap geometry for this fragmentainer if we have gaps.
    if ((RuntimeEnabledFeatures::CSSGapDecorationEnabled() &&
         Style().HasGapRule()) &&
        (!fragment_main_gaps.empty() ||
         !full_gap_geometry->GetCrossGaps().empty())) {
      // Update content block offsets for this fragmentainer.
      // - Block start: Use the original gap geometry's start for the first
      // fragment and zero for subsequent fragments.
      // - Block end: Use the smaller of the fragmentainer space and the grid's
      // remaining block size.
      LayoutUnit fragment_block_start =
          *offset_in_stitched_container > LayoutUnit()
              ? full_gap_geometry->GetContentBlockStart()
              : LayoutUnit();
      LayoutUnit fragment_block_end =
          std::min(fragmentainer_space,
                   *intrinsic_block_size - *offset_in_stitched_container);

      if (max_item_block_end > fragmentainer_space) {
        // If we have monolithic content that overflowed the fragmentainer,
        // ensure that we encompass it in the block-end offset.
        fragment_block_end = max_item_block_end;
      }

      GapGeometry* fragment_gap_geometry = MakeGarbageCollected<GapGeometry>(
          *full_gap_geometry, std::move(fragment_main_gaps),
          fragment_block_start, fragment_block_end);

      fragment_gap_geometry->AdjustCrossGapsRangesForFragmentation(
          initial_unprocessed_row_gap_idx, *first_unprocessed_row_gap_idx,
          *column_gaps_segment_ranges_start_indices);
      container_builder_.SetGapGeometry(fragment_gap_geometry);
    }
  };

  PlaceItems();

  // See if we need to expand any rows, and if so re-run |PlaceItems()|. We
  // track the previous row we expanded, so this loop should eventually break.
  while (ExpandRow())
    PlaceItems();

  // See if we need to take a row break-point, and if-so re-run |PlaceItems()|.
  // We only need to do this once.
  if (ShiftBreakpointIntoNextFragmentainer()) {
    PlaceItems();
  } else if (fragmentainer_space != kIndefiniteSize) {
    // Encompass any fragmentainer overflow (caused by monolithic content)
    // that hasn't been accounted for. We want this to contribute to the
    // grid container fragment size, and it is also needed to shift any
    // breakpoints all the way into the next fragmentainer.
    fragmentainer_space =
        std::max(fragmentainer_space,
                 max_item_block_end - cloned_block_start_decoration);
  }

  if (RuntimeEnabledFeatures::CSSGridGapSuppressionEnabled()) {
    PlaceGaps();
  }

  if (has_subsequent_children)
    container_builder_.SetHasSubsequentChildren();

  // Add all the results into the builder.
  for (auto& result_and_offset : result_and_offsets) {
    container_builder_.AddResult(
        *result_and_offset.result, result_and_offset.offset,
        /* margins */ std::nullopt, result_and_offset.relative_offset);
  }

  // Propagate the baselines.
  if (auto first_baseline = baseline_accumulator.FirstBaseline())
    container_builder_.SetFirstBaseline(*first_baseline);
  if (auto last_baseline = baseline_accumulator.LastBaseline())
    container_builder_.SetLastBaseline(*last_baseline);

  if (fragmentainer_space != kIndefiniteSize) {
    *offset_in_stitched_container += fragmentainer_space;
  }
}

void GridLayoutAlgorithm::PlaceOutOfFlowItems(
    const GridLayoutData& layout_data,
    const LayoutUnit block_size,
    HeapVector<Member<LayoutBox>>& oof_children) {
  DCHECK(!oof_children.empty());

  HeapVector<Member<LayoutBox>> oofs;
  std::swap(oofs, oof_children);

  bool should_process_block_end = true;
  if (InvolvedInBlockFragmentation(container_builder_)) [[unlikely]] {
    should_process_block_end = !container_builder_.DidBreakSelf() &&
                               !container_builder_.ShouldBreakInside();
  }

  const auto& node = Node();
  const auto& container_style = Style();
  const auto& placement_data = node.CachedPlacementData();
  const bool is_absolute_container = node.IsAbsoluteContainer();
  const bool is_fixed_container = node.IsAbsoluteContainer();

  const LayoutUnit previous_consumed_block_size =
      GetBreakToken() ? GetBreakToken()->ConsumedBlockSize() : LayoutUnit();
  const LogicalSize total_fragment_size = {container_builder_.InlineSize(),
                                           block_size};
  const auto default_containing_block_size =
      ShrinkLogicalSize(total_fragment_size, BorderScrollbarPadding());

  for (LayoutBox* oof_child : oofs) {
    GridItemData* out_of_flow_item = MakeGarbageCollected<GridItemData>(
        BlockNode(oof_child), container_style);
    DCHECK(out_of_flow_item->IsOutOfFlow());

    std::optional<LogicalRect> containing_block_rect;
    const auto position = out_of_flow_item->node.Style().GetPosition();

    // If the current grid is also the containing-block for the OOF-positioned
    // item, pick up the static-position from the grid-area.
    if ((is_absolute_container && position == EPosition::kAbsolute) ||
        (is_fixed_container && position == EPosition::kFixed)) {
      containing_block_rect.emplace(ComputeOutOfFlowItemContainingRect(
          placement_data, layout_data, container_style,
          container_builder_.Borders(), total_fragment_size, out_of_flow_item));
    }

    LogicalStaticPosition static_pos;
    static_pos.offset = containing_block_rect
                            ? containing_block_rect->offset
                            : BorderScrollbarPadding().StartOffset();
    const auto containing_block_size = containing_block_rect
                                           ? containing_block_rect->size
                                           : default_containing_block_size;

    AlignmentOffsetForOutOfFlow(out_of_flow_item->Alignment(kForColumns),
                                out_of_flow_item->Alignment(kForRows),
                                containing_block_size, &static_pos);

    // Make the child offset relative to our fragment.
    static_pos.offset.block_offset -= previous_consumed_block_size;

    // We will attempt to add OOFs in the fragment in which their static
    // position belongs. However, the last fragment has the most up-to-date grid
    // geometry information (e.g. any expanded rows, etc), so for center aligned
    // items or items with a grid-area that is not in the first or last
    // fragment, we could end up with an incorrect static position.
    if (should_process_block_end ||
        static_pos.offset.block_offset <= FragmentainerCapacityForChildren()) {
      container_builder_.AddOutOfFlowChildCandidate(out_of_flow_item->node,
                                                    static_pos);
    } else {
      oof_children.emplace_back(oof_child);
    }
  }
}

void GridLayoutAlgorithm::SetReadingFlowNodes(const GridItems& grid_items) {
  const auto& style = Style();
  const EReadingFlow reading_flow = style.ReadingFlow();
  if (reading_flow != EReadingFlow::kGridRows &&
      reading_flow != EReadingFlow::kGridColumns &&
      reading_flow != EReadingFlow::kGridOrder) {
    return;
  }

  HeapVector<Member<blink::Node>> reading_flow_nodes;
  reading_flow_nodes.ReserveInitialCapacity(grid_items.Size());
  // Add grid item if it is a DOM node
  auto add_item_if_needed = [&](const GridItemData& grid_item) {
    if (blink::Node* node = grid_item.node.GetDOMNode()) {
      reading_flow_nodes.push_back(node);
    }
  };

  if (reading_flow == EReadingFlow::kGridRows ||
      reading_flow == EReadingFlow::kGridColumns) {
    HeapVector<Member<const GridItemData>, 16> reordered_grid_items;
    reordered_grid_items.ReserveInitialCapacity(grid_items.Size());
    for (const auto& grid_item : grid_items) {
      reordered_grid_items.push_back(&grid_item);
    }
    // We reorder grid items by their row/column indices.
    // If reading-flow is grid-rows, we should sort by row, then column.
    // If reading-flow is grid-columns, we should sort by column, then
    // row.
    GridTrackSizingDirection reading_direction_first = kForRows;
    GridTrackSizingDirection reading_direction_second = kForColumns;
    if (reading_flow == EReadingFlow::kGridColumns) {
      reading_direction_first = kForColumns;
      reading_direction_second = kForRows;
    }
    auto compare_grid_items_for_reading_flow =
        [reading_direction_first, reading_direction_second](const auto& lhs,
                                                            const auto& rhs) {
          if (lhs->SetIndices(reading_direction_first).begin ==
              rhs->SetIndices(reading_direction_first).begin) {
            return lhs->SetIndices(reading_direction_second).begin <
                   rhs->SetIndices(reading_direction_second).begin;
          }
          return lhs->SetIndices(reading_direction_first).begin <
                 rhs->SetIndices(reading_direction_first).begin;
        };
    std::stable_sort(reordered_grid_items.begin(), reordered_grid_items.end(),
                     compare_grid_items_for_reading_flow);
    for (const auto& grid_item : reordered_grid_items) {
      add_item_if_needed(*grid_item);
    }
  } else {
    for (const auto& grid_item : grid_items) {
      add_item_if_needed(grid_item);
    }
  }
  container_builder_.SetReadingFlowNodes(std::move(reading_flow_nodes));
}

// static
LogicalRect GridLayoutAlgorithm::ComputeOutOfFlowItemContainingRect(
    const GridPlacementData& placement_data,
    const GridLayoutData& layout_data,
    const ComputedStyle& grid_style,
    const BoxStrut& borders,
    const LogicalSize& border_box_size,
    GridItemData* out_of_flow_item) {
  DCHECK(out_of_flow_item && out_of_flow_item->IsOutOfFlow());

  out_of_flow_item->ComputeOutOfFlowItemPlacement(layout_data.Columns(),
                                                  placement_data, grid_style);
  out_of_flow_item->ComputeOutOfFlowItemPlacement(layout_data.Rows(),
                                                  placement_data, grid_style);

  LogicalRect containing_rect;

  ComputeOutOfFlowOffsetAndSize(
      *out_of_flow_item, layout_data.Columns(), borders, border_box_size,
      &containing_rect.offset.inline_offset, &containing_rect.size.inline_size);

  ComputeOutOfFlowOffsetAndSize(
      *out_of_flow_item, layout_data.Rows(), borders, border_box_size,
      &containing_rect.offset.block_offset, &containing_rect.size.block_size);

  return containing_rect;
}

}  // namespace blink

WTF_ALLOW_CLEAR_UNUSED_SLOTS_WITH_MEM_FUNCTIONS(blink::ResultAndOffsets)
