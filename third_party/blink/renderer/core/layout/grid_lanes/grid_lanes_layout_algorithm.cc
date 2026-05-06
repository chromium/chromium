// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#include "third_party/blink/renderer/core/layout/grid_lanes/grid_lanes_layout_algorithm.h"

#include "base/notreached.h"
#include "third_party/blink/renderer/core/layout/grid/grid_baseline_accumulator.h"
#include "third_party/blink/renderer/core/layout/grid/grid_data.h"
#include "third_party/blink/renderer/core/layout/grid/grid_item.h"
#include "third_party/blink/renderer/core/layout/grid/grid_layout_utils.h"
#include "third_party/blink/renderer/core/layout/grid/grid_track_collection.h"
#include "third_party/blink/renderer/core/layout/grid_lanes/grid_lanes_running_positions.h"
#include "third_party/blink/renderer/core/layout/grid_lanes/layout_grid_lanes.h"
#include "third_party/blink/renderer/core/layout/grid_lanes/stacking_baseline_accumulator.h"
#include "third_party/blink/renderer/core/layout/layout_utils.h"
#include "third_party/blink/renderer/core/layout/logical_box_fragment.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

GridLanesLayoutAlgorithm::GridLanesLayoutAlgorithm(
    const LayoutAlgorithmParams& params)
    : LayoutAlgorithm(params) {
  DCHECK(params.space.IsNewFormattingContext());
  const auto& node = Node();
  const auto& constraint_space = GetConstraintSpace();

  // At various stages of the algorithm we need to know the grid-lanes
  // available-size. If it's initially indefinite, we need to know the min/max
  // sizes as well. Initialize all these to the same value.
  grid_lanes_available_size_ = grid_lanes_min_available_size_ =
      grid_lanes_max_available_size_ = ChildAvailableSize();
  ComputeAvailableSizes(BorderScrollbarPadding(), node, constraint_space,
                        container_builder_, grid_lanes_available_size_,
                        grid_lanes_min_available_size_,
                        grid_lanes_max_available_size_);
  // If block-size containment applies, compute the block-size ignoring
  // children.
  if (grid_lanes_available_size_.block_size == kIndefiniteSize &&
      node.ShouldApplyBlockSizeContainment()) {
    contain_intrinsic_block_size_ = ComputeIntrinsicBlockSizeIgnoringChildren();
    // Resolve the block-size and set the available sizes.
    const LayoutUnit block_size = ComputeBlockSizeForFragment(
        constraint_space, node, BorderPadding(), *contain_intrinsic_block_size_,
        container_builder_.InlineSize());

    grid_lanes_available_size_.block_size =
        grid_lanes_min_available_size_.block_size =
            grid_lanes_max_available_size_.block_size =
                (block_size - BorderScrollbarPadding().BlockSum())
                    .ClampNegativeToZero();
  }
}

MinMaxSizesResult GridLanesLayoutAlgorithm::ComputeMinMaxSizes(
    const MinMaxSizesFloatInput&) {
  // If the intrinsic inline size has been overridden, use the provided value.
  const auto& node = Node();
  LayoutUnit override_intrinsic_inline_size =
      node.OverrideIntrinsicContentInlineSize();
  if (override_intrinsic_inline_size != kIndefiniteSize) {
    override_intrinsic_inline_size += BorderScrollbarPadding().InlineSum();
    MinMaxSizes sizes(override_intrinsic_inline_size,
                      override_intrinsic_inline_size);
    return MinMaxSizesResult{sizes,
                             /*depends_on_block_constraints=*/false};
  }

  const ComputedStyle& style = Style();
  const auto grid_axis_direction = style.GridLanesTrackSizingDirection();
  const bool is_for_columns = grid_axis_direction == kForColumns;

  GridItems* grid_items = nullptr;
  const GridLayoutSubtree* layout_subtree = nullptr;

  auto ComputeIntrinsicInlineSize = [&](SizingConstraint sizing_constraint) {
    const bool should_apply_inline_size_containment =
        node.ShouldApplyInlineSizeContainment();

    // TODO(almaher): Do we need to do something special for subgrid
    // related to GetGridLayoutSubtree()?

    layout_subtree = ComputeGridLanesGeometry(
        sizing_constraint, should_apply_inline_size_containment, &grid_items);
    CHECK(grid_items);

    auto* layout_data = layout_subtree->LayoutData();
    const auto& track_collection =
        is_for_columns ? layout_data->Columns() : layout_data->Rows();

    if (is_for_columns) {
      // Track sizing is done during the guess placement step, so at this point,
      // getting the width of all of the columns should correctly give us the
      // intrinsic inline size.
      return track_collection.CalculateSetSpanSize();
    } else {
      if (grid_items->IsEmpty()) {
        // If there are no grid-lanes items, the intrinsic inline size is only
        // border, scrollbar, and padding.
        return BorderScrollbarPadding().InlineSum();
      }

      GridLanesRunningPositions running_positions(
          track_collection, style,
          ResolveFlowToleranceForGridLanes(style, grid_lanes_available_size_));

      PlaceGridLanesItems(*grid_items, layout_subtree, *layout_data,
                          running_positions, sizing_constraint);
      // `stacking_axis_gap` represents the space between each of the items
      // in the row. We need to subtract this as it is always added to
      // `running_positions` whenever an item is placed, but the very last
      // addition should be deleted as there is no item after it.
      const auto stacking_axis_gap =
          GridTrackSizingAlgorithm::CalculateGutterSize(
              style, grid_lanes_available_size_, kForColumns);
      return running_positions.GetMaxPositionForSpan(
                 GridSpan::TranslatedDefiniteGridSpan(
                     /*start_line=*/0,
                     /*end_line=*/track_collection.EndLineOfImplicitGrid())) -
             stacking_axis_gap;
    }
  };

  // The min-content size of the stacking axis of a grid-lanes container should
  // be the same as its max-content size, similar to the "stacking" axis
  // in block layout. As such, for row containers, the min-content size is set
  // to max-content.
  const LayoutUnit max_content =
      ComputeIntrinsicInlineSize(SizingConstraint::kMaxContent);
  MinMaxSizes intrinsic_sizes{max_content, max_content};
  if (is_for_columns) {
    intrinsic_sizes.min_size =
        ComputeIntrinsicInlineSize(SizingConstraint::kMinContent);
  }
  intrinsic_sizes += BorderScrollbarPadding().InlineSum();

  return {intrinsic_sizes,
          /*depends_on_block_constraints=*/HasBlockSizeDependentGridItem(
              *grid_items)};
}

const LayoutResult* GridLanesLayoutAlgorithm::Layout() {
  HeapVector<Member<LayoutBox>> oof_children;
  const auto& node = Node();

  GridItems* grid_items = nullptr;
  const GridLayoutSubtree* layout_subtree =
      ComputeGridLanesGeometry(SizingConstraint::kLayout,
                               /*should_apply_inline_size_containment=*/false,
                               &grid_items, &oof_children);
    CHECK(grid_items);

    auto* layout_data = layout_subtree->LayoutData();
    const auto grid_axis_direction = Style().GridLanesTrackSizingDirection();

    if (!grid_items->IsEmpty()) {
      const auto& track_collection = grid_axis_direction == kForColumns
                                         ? layout_data->Columns()
                                         : layout_data->Rows();

      GridLanesRunningPositions running_positions(
          track_collection, Style(),
          ResolveFlowToleranceForGridLanes(Style(),
                                           grid_lanes_available_size_));

      PlaceGridLanesItems(*grid_items, layout_subtree, *layout_data,
                          running_positions, SizingConstraint::kLayout);
    }

  // TODO(layout-dev): This isn't great but matches legacy. Ideally this
  // would only apply when we have only flexible track(s).
  if (grid_items->IsEmpty() && node.HasLineIfEmpty()) {
    intrinsic_block_size_ = std::max(intrinsic_block_size_,
                                     node.EmptyLineBlockSize(GetBreakToken()));
  }

  // Account for border, scrollbar, and padding in the intrinsic block size.
  intrinsic_block_size_ += BorderScrollbarPadding().BlockSum();
  intrinsic_block_size_ =
      ClampIntrinsicBlockSize(GetConstraintSpace(), node, GetBreakToken(),
                              BorderScrollbarPadding(), intrinsic_block_size_);
  auto block_size = ComputeBlockSizeForFragment(
      GetConstraintSpace(), Node(), BorderPadding(),
      contain_intrinsic_block_size_.value_or(intrinsic_block_size_),
      container_builder_.InlineSize());
  container_builder_.SetFragmentsTotalBlockSize(block_size);
  container_builder_.SetIntrinsicBlockSize(intrinsic_block_size_);

  // Place out-of-flow items after setting the intrinsic block size, since
  // out-of-flow items don't contribute to the intrinsic size of the container.
  //
  // TODO(celestepan): Handle content alignment (justify-content /
  // align-content) and fill-reverse for OOF items. At the moment, we are
  // adjusting their offsets in `MoveChildrenInDirection`, which is called
  // earlier in `PlaceGridLanesItems`, but we don't populate the OOF children
  // until here.
  if (!oof_children.empty()) {
    PlaceOutOfFlowItems(*layout_data, block_size, oof_children);
  }

  container_builder_.SetGridLayoutData(layout_data);
  container_builder_.HandleOofsAndSpecialDescendants();
  return container_builder_.ToBoxFragment();
}

namespace {

LayoutUnit AlignContentOffset(
    LayoutUnit intrinsic_size,
    LayoutUnit container_size,
    LayoutUnit baseline_offset,
    const StyleContentAlignmentData& content_alignment) {
  // Note: There is only ever one alignment subject for these properties in the
  // stacking axis, so the unique align-content / justify-content values boil
  // down to start, center, end, and baseline alignment. (The behavior of normal
  // and stretch is identical to start, and the distributed alignment values
  // behave as their fallback alignments.) [1].
  //
  // [1]: https://www.w3.org/TR/css-grid-3/#alignment
  LayoutUnit free_space = container_size - intrinsic_size;

  // If overflow is 'safe', we have to make sure we don't overflow the
  // 'start' edge (potentially cause some data loss as the overflow is
  // unreachable).
  if (content_alignment.Overflow() == OverflowAlignment::kSafe) {
    free_space = free_space.ClampNegativeToZero();
  }

  switch (content_alignment.Distribution()) {
    case ContentDistributionType::kSpaceAround:
    case ContentDistributionType::kSpaceEvenly:
      return (free_space / 2);
    case ContentDistributionType::kSpaceBetween:
    case ContentDistributionType::kStretch:
    case ContentDistributionType::kDefault:
      break;
  }

  switch (content_alignment.GetPosition()) {
    case ContentPosition::kLeft:
    case ContentPosition::kStart:
    case ContentPosition::kFlexStart:
    case ContentPosition::kNormal:
      return LayoutUnit();
    case ContentPosition::kCenter:
      return (free_space / 2);
    case ContentPosition::kRight:
    case ContentPosition::kEnd:
    case ContentPosition::kFlexEnd:
      return free_space;
    case ContentPosition::kBaseline:
    case ContentPosition::kLastBaseline:
      return baseline_offset;
  }
  NOTREACHED();
}

// Returns the margin on the baseline side of `item` for baseline alignment
// calculations.
LayoutUnit GetBaselineSideMargin(const GridItemData& item,
                                 const BoxStrut& margins,
                                 GridTrackSizingDirection track_direction) {
  const bool is_for_columns = track_direction == kForColumns;
  const bool is_last_baseline = item.IsLastBaselineSpecified(track_direction);
  if (is_last_baseline) {
    return is_for_columns ? margins.inline_end : margins.block_end;
  }
  return is_for_columns ? margins.inline_start : margins.block_start;
}

LayoutUnit CalculateSynthesizedBaselineShim(
    const GridItemData& grid_item,
    LayoutUnit block_size,
    GridTrackSizingDirection track_direction,
    LayoutUnit shared_baseline,
    LayoutUnit extra_margin) {
  if (shared_baseline == LayoutUnit::Min()) {
    return LayoutUnit();
  }
  return shared_baseline -
         GetSynthesizedLogicalBaseline(grid_item, block_size, track_direction) -
         extra_margin;
}

}  // namespace

LayoutUnit GridLanesLayoutAlgorithm::CalculateItemInlineContribution(
    const GridItemData& grid_lanes_item,
    const GridLayoutTrackCollection& track_collection,
    SizingConstraint sizing_constraint) {
  CHECK_NE(sizing_constraint, SizingConstraint::kLayout);
  // We need to compute the available space for the item if we are using it
  // to compute min/max content sizes.
  //
  // TODO(almaher): `SubgriddedItemData` should incorporate the parent
  // subgrid's info.
  //
  // TODO(almaher): Plumb the parent grid's `GridLayoutData` here instead of
  // passing nullptr.
  const ConstraintSpace space_for_measure = CreateConstraintSpaceForMeasure(
      SubgriddedItemData(grid_lanes_item, /*parent_layout_data=*/nullptr,
                         GetConstraintSpace().GetWritingMode()),
      /*opt_fixed_inline_size=*/std::nullopt, &track_collection);
  const MinMaxSizes sizes = ComputeMinAndMaxContentContributionForSelf(
                                grid_lanes_item.node, space_for_measure)
                                .sizes;
  return (sizing_constraint == SizingConstraint::kMinContent) ? sizes.min_size
                                                              : sizes.max_size;
}

void GridLanesLayoutAlgorithm::PlaceGridLanesItems(
    GridItems& grid_items,
    const GridLayoutSubtree* layout_subtree,
    GridLayoutData& layout_data,
    GridLanesRunningPositions& running_positions,
    std::optional<SizingConstraint> sizing_constraint) {
  const auto& style = Style();
  const auto grid_axis_direction = style.GridLanesTrackSizingDirection();
  const auto& track_collection = grid_axis_direction == kForColumns
                                     ? layout_data.Columns()
                                     : layout_data.Rows();
  const bool is_for_columns = grid_axis_direction == kForColumns;
  const auto stacking_axis_gap = GridTrackSizingAlgorithm::CalculateGutterSize(
      style, grid_lanes_available_size_,
      is_for_columns ? kForRows : kForColumns);

  std::optional<StackingBaselineAccumulator> stacking_baseline_accumulator;
  std::optional<GridBaselineAccumulator> grid_baseline_accumulator;
  BaselineAccumulator* baseline_accumulator;
  if (is_for_columns) {
    stacking_baseline_accumulator.emplace(running_positions,
                                          grid_axis_direction);
    baseline_accumulator = &stacking_baseline_accumulator.value();
  } else {
    grid_baseline_accumulator.emplace(style.GetFontBaseline());
    baseline_accumulator = &grid_baseline_accumulator.value();
  }
  CHECK(baseline_accumulator);

  // Run a final placement pass of all items and add their layout results to
  // the container. This is the second placement pass if there are any items
  // requiring baseline alignment, and the only placement pass otherwise. Item
  // layout results are only added to the container during this final placement
  // pass, ensuring all alignment and baseline information is available before
  // items are positioned.
  RunGridLanesPlacementPhase(grid_items, layout_subtree, layout_data,
                             sizing_constraint, stacking_axis_gap,
                             PlacementPhase::kFinalPlacement,
                             baseline_accumulator, running_positions);

  // Propagate the baselines to the container.
  if (auto first_baseline = baseline_accumulator->FirstBaseline()) {
    container_builder_.SetFirstBaseline(*first_baseline);
  }
  if (auto last_baseline = baseline_accumulator->LastBaseline()) {
    container_builder_.SetLastBaseline(*last_baseline);
  }

  // Determine intrinsic size of the grid-lanes container. For the stacking
  // axis, remove the last gap that was added, since there is no item after it.
  const LayoutUnit stacking_axis_size =
      running_positions.GetMaxPositionForSpan(
          GridSpan::TranslatedDefiniteGridSpan(
              /*start_line=*/0,
              /*end_line=*/track_collection.EndLineOfImplicitGrid())) -
      stacking_axis_gap;

  // To determine the size of the grid axis, add the size of the tracks.
  const LayoutUnit grid_axis_size = track_collection.CalculateSetSpanSize();
  // For column grid-lanes, the block size is the stacking axis size. For row
  // grid-lanes, `intrinsic_block_size_` is already set in
  // `ComputeGridLanesGeometry` from the track collection.
  if (is_for_columns) {
    intrinsic_block_size_ = stacking_axis_size;
  }

  // Apply content alignment/justification. This is an additional offset
  // determined by the intrinsic inline or block size of the grid-lanes
  // container, so it must occur after that has been determined. This must also
  // occur after the container baselines have been set.
  const auto& content_alignment =
      is_for_columns ? style.AlignContent() : style.JustifyContent();
  const auto child_available_size = ChildAvailableSize();

  // At this stage for individual items, we only need to perform fill-reverse
  // for the case of columns with an indefinite stacking axis, which is in the
  // block direction. Every other case of fill-reverse will have been handled
  // earlier in `RunGridLanesPlacementPhase`.
  const bool is_fill_reverse = style.IsReverseGridLanesFillDirection();
  const bool apply_fill_reverse_to_children =
      is_fill_reverse && is_for_columns &&
      child_available_size.block_size == kIndefiniteSize;

  if (content_alignment != ComputedStyleInitialValues::InitialAlignContent() ||
      apply_fill_reverse_to_children) {
    const LayoutUnit container_stacking_axis_available_size =
        is_for_columns ? child_available_size.block_size
                       : child_available_size.inline_size;
    const LayoutUnit effective_stacking_axis_size =
        container_stacking_axis_available_size != kIndefiniteSize
            ? container_stacking_axis_available_size
            : stacking_axis_size;
    const LayoutUnit intrinsic_inline_size =
        is_for_columns ? grid_axis_size : stacking_axis_size;

    // For definite stacking axis, use the container's available size to
    // compute alignment. For indefinite stacking axis, use the intrinsic
    // stacking-axis size (alignment will have no free space unless the
    // resolved container size differs due to min-height/etc).
    LayoutUnit align_content_offset = AlignContentOffset(
        is_for_columns ? intrinsic_block_size_ : intrinsic_inline_size,
        effective_stacking_axis_size,
        baseline_accumulator->FirstBaseline().value_or(LayoutUnit()),
        content_alignment);

    // In fill-reverse, items either already are, or will be, positioned at the
    // end of the stacking axis. The content alignment offset computed above
    // assumes items start at the beginning of the tracks, so we negate it to
    // shift items in the correct direction.
    if (is_fill_reverse) {
      align_content_offset *= -1;
    }

    const LayoutUnit border_scrollbar_padding_start =
        is_for_columns ? BorderScrollbarPadding().block_start
                       : BorderScrollbarPadding().inline_start;
    std::optional<BoxFragmentBuilder::AdditionalOffsetAdjustment>
        additional_offset_adjustment;
    if (apply_fill_reverse_to_children) {
      additional_offset_adjustment.emplace(blink::BindRepeating(
          [](WritingDirectionMode writing_direction, bool is_block_direction,
             LayoutUnit stacking_axis_size,
             LayoutUnit border_scrollbar_padding_start,
             LogicalFragmentLink& child) {
            child.ReverseChildOffset(writing_direction, is_block_direction,
                                     stacking_axis_size,
                                     border_scrollbar_padding_start);
          },
          container_builder_.GetWritingDirection(),
          /*is_block_direction=*/is_for_columns, effective_stacking_axis_size,
          border_scrollbar_padding_start));
    }
    container_builder_.MoveChildrenInDirection(
        align_content_offset, /*is_block_direction=*/is_for_columns,
        additional_offset_adjustment);
  }
}

void GridLanesLayoutAlgorithm::RunGridLanesPlacementPhase(
    GridItems& grid_items,
    const GridLayoutSubtree* layout_subtree,
    GridLayoutData& layout_data,
    std::optional<SizingConstraint> sizing_constraint,
    LayoutUnit stacking_axis_gap,
    PlacementPhase placement_phase,
    BaselineAccumulator* baseline_accumulator,
    GridLanesRunningPositions& running_positions) {
  const bool is_for_layout = sizing_constraint == SizingConstraint::kLayout;
  const auto& container_space = GetConstraintSpace();
  const auto& style = Style();
  const auto border_scrollbar_padding = BorderScrollbarPadding();
  const auto container_writing_direction =
      container_space.GetWritingDirection();
  const auto container_writing_mode = container_space.GetWritingMode();
  const auto grid_axis_direction = style.GridLanesTrackSizingDirection();
  auto& track_collection = grid_axis_direction == kForColumns
                               ? layout_data.Columns()
                               : layout_data.Rows();
  const bool is_for_columns = grid_axis_direction == kForColumns;
  const wtf_size_t grid_axis_start_offset =
      Node().CachedPlacementData().StartOffset(grid_axis_direction);

  auto* next_subgrid_subtree =
      layout_subtree ? layout_subtree->FirstChild() : nullptr;

  for (auto& grid_lanes_item : grid_items) {
    GridLayoutSubtree* child_layout_subtree = nullptr;
    const bool is_subgrid = grid_lanes_item.IsSubgrid();
    if (is_subgrid) {
      if (layout_subtree) {
        DCHECK(next_subgrid_subtree);
        child_layout_subtree = next_subgrid_subtree;
        next_subgrid_subtree = next_subgrid_subtree->NextSibling();
      } else {
        // During the `kCalculateBaselines` pass, the layout subtree is not yet
        // available. Skip subgrid layout to avoid corrupting the subgrid's
        // cached placement data.
        continue;
      }
    }

    // Get the starting offset of where we want the item placed in the stacking
    // axis.
    LayoutUnit start_offset_in_stacking_axis =
        running_positions.FinalizeItemSpanAndGetMaxPosition(
            grid_axis_start_offset, grid_lanes_item, track_collection);

    // For auto-placed subgrids, the inherited track collection in the
    // subgridded (grid) axis was built from the temporary placement at the
    // beginning of the container during track sizing. Now that the grid lanes
    // placement algorithm has determined the final position, rebuild the
    // subgrid's inherited track collection using the updated range indices so
    // the subgrid layout uses the correct tracks.
    //
    // TODO(almaher): What about nested subgrids? Those won't be updated
    // correctly. Will this require a separate pass, or do we just need to
    // make this update for the rest of its subtree, as well?
    //
    // TODO(almaher): Also, note that this only updates the inherited track
    // collection. For cases where the opposing axis depends on the sizing in
    // the grid axis, we will need to run another pass altogether to ensure
    // accurate sizing. We will likely need a way to store the final position
    // for these subgrids before the second pass.
    if (is_subgrid && grid_lanes_item.is_auto_placed &&
        grid_lanes_item.StartLine(grid_axis_direction) !=
            grid_axis_start_offset) {
      CHECK(child_layout_subtree);
      GridLayoutData* child_layout_data = child_layout_subtree->LayoutData();
      CHECK(child_layout_data->HasSubgriddedAxis(grid_axis_direction));

      // TODO(almaher): `SubgriddedItemData` should incorporate the parent
      // subgrid's info.
      const SubgriddedItemData subgridded_item_data(
          grid_lanes_item, &layout_data, container_writing_mode);
      const ConstraintSpace subgrid_space =
          CreateConstraintSpaceForLayout(subgridded_item_data);
      const FragmentGeometry subgrid_fragment_geometry =
          CalculateInitialFragmentGeometryForSubgrid(grid_lanes_item,
                                                     subgrid_space);

      const GridLayoutAlgorithm subgrid_algorithm(
          {grid_lanes_item.node, subgrid_fragment_geometry, subgrid_space});
      child_layout_data->SetTrackCollection(CreateSubgridTrackCollection(
          subgridded_item_data, grid_lanes_item.node.Style(), subgrid_space,
          subgrid_algorithm.BorderScrollbarPadding(),
          subgrid_algorithm.GetGridAvailableSize(), grid_axis_direction));
    }

    // During track sizing, we may force a specific inline size on an item
    // if the available space in that direction is indefinite, particularly for
    // orthogonal items. In Grid, that constraint is maintained during layout
    // due to the two dimensional nature of Grid tracks. In grid-lanes,
    // recompute this fixed size to guarantee we maintain the same constraint
    // during track sizing and layout. Subgrids don't contribute to track
    // sizing, so they can be skipped.
    std::optional<LayoutUnit> opt_fixed_inline_size;
    if (is_for_layout && !is_subgrid) {
      // TODO(almaher): `SubgriddedItemData` should incorporate the parent
      // subgrid's info.
      const ConstraintSpace space_for_measure =
          CreateConstraintSpaceForMeasure(SubgriddedItemData(
              grid_lanes_item, &layout_data, container_writing_mode));
      if (space_for_measure.AvailableSize().inline_size == kIndefiniteSize) {
        const MinMaxSizes sizes = ComputeMinAndMaxContentContributionForSelf(
                                      grid_lanes_item.node, space_for_measure)
                                      .sizes;
        opt_fixed_inline_size = sizes.max_size;
      }
    }

    // This item is ultimately placed below the maximum running position among
    // its spanned tracks. Account for border, scrollbar, and padding in the
    // offset of the item.
    LogicalRect containing_grid_area;

    // TODO(almaher): `SubgriddedItemData` should incorporate the parent
    // subgrid's info.
    const ConstraintSpace space =
        is_for_layout
            ? CreateConstraintSpaceForLayout(
                  SubgriddedItemData(grid_lanes_item, &layout_data,
                                     container_writing_mode),
                  child_layout_subtree, &containing_grid_area,
                  /*unavailable_block_size=*/LayoutUnit(),
                  /*min_block_size_should_encompass_intrinsic_size=*/false,
                  /*opt_child_block_offset=*/std::nullopt,
                  opt_fixed_inline_size)
            : CreateConstraintSpaceForMeasure(
                  SubgriddedItemData(grid_lanes_item, &layout_data,
                                     container_writing_mode),
                  CalculateItemInlineContribution(
                      grid_lanes_item, track_collection, *sizing_constraint),
                  &track_collection,
                  /*is_for_min_max_sizing=*/true);

    const auto& item_node = grid_lanes_item.node;
    const auto& item_style = item_node.Style();
    const LayoutResult* result =
        is_for_layout ? result = item_node.Layout(space)
                      : LayoutGridItemForMeasure(grid_lanes_item, space,
                                                 *sizing_constraint);

    const auto& physical_fragment =
        To<PhysicalBoxFragment>(result->GetPhysicalFragment());
    const LogicalBoxFragment fragment(container_writing_direction,
                                      physical_fragment);

    // Margins can affect the visual placement of the item, but should not cause
    // the running position to move backwards. If the margin size is greater
    // than the size of the fragment and the stacking axis gap, then the
    // placement of the item should not contribute anything at all to the
    // stacking axis.
    auto margins = ComputeMarginsFor(space, item_style, container_space);
    LayoutUnit fragment_size =
        is_for_columns ? fragment.BlockSize() + margins.BlockSum()
                       : fragment.InlineSize() + margins.InlineSum();
    const LayoutUnit visual_stacking_axis_size =
        fragment_size + stacking_axis_gap;
    const LayoutUnit fragment_stacking_axis_contribution =
        visual_stacking_axis_size.ClampNegativeToZero();

    // If dense packing is set, we need to figure out if the item can possibly
    // fit into any previous track openings. If it can, then we need to adjust
    // `item_span` as well as the offset of `containing_grid_area`, which is
    // sized based on the items within the grid-lanes container. Margins need to
    // be added to the item's size in the stacking axis.
    const bool is_dense_packing = style.IsGridLanesPackDense();
    bool item_moved_to_earlier_opening = false;
    if (is_dense_packing) {
      LayoutUnit updated_item_start_offset =
          running_positions.GetEligibleTrackOpeningAndUpdateGridLanesItemSpan(
              grid_axis_start_offset,
              /*item_stacking_axis_contribution=*/
              fragment_stacking_axis_contribution,
              /*auto_placement_stacking_axis_offset=*/
              start_offset_in_stacking_axis, track_collection, grid_lanes_item);

      // If we have a valid offset for the item in the stacking axis, it means
      // we found an earlier track opening for the item.
      if (updated_item_start_offset != LayoutUnit::Max()) {
        // Because it's possible that we switched the item to a different span,
        // update the offset of where the item should be placed in the grid
        // axis.
        const LayoutUnit grid_lanes_item_start_offset =
            track_collection.GetSetOffset(
                grid_lanes_item.SetIndices(track_collection.Direction()).begin);
        is_for_columns ? containing_grid_area.offset.inline_offset =
                             grid_lanes_item_start_offset
                       : containing_grid_area.offset.block_offset =
                             grid_lanes_item_start_offset;

        item_moved_to_earlier_opening = true;
        start_offset_in_stacking_axis = updated_item_start_offset;
      }
    }

    // In the final placement pass, position items and apply self-alignment.
    // This must be done after calculating track baselines in the first pass,
    // since baseline alignment needs to know the track's baseline to properly
    // align items within their grid areas.
    if (placement_phase == PlacementPhase::kFinalPlacement) {
      // `start_offset_in_stacking_axis` specifies where in the stacking axis
      // the item should be placed, so we need to adjust the `containing_rect`
      // in the stacking axis to accommodate the newly placed item.
      const LayoutUnit border_scrollbar_padding_start =
          is_for_columns ? border_scrollbar_padding.block_start
                         : border_scrollbar_padding.inline_start;

      LayoutUnit final_start_offset_in_stacking_axis =
          start_offset_in_stacking_axis + border_scrollbar_padding_start;

      // For fill-reverse, items stack from the end of the container instead
      // of the start. We compute the base offset from the container's end so
      // that alignment (which adds margins) works correctly without needing
      // to mirror or swap margins after the fact. Columns with an indefinite
      // block size are handled after placement. Only columns can have an
      // indefinite stacking axis size, since rows use the viewport width if no
      // width is defined.
      if (style.IsReverseGridLanesFillDirection()) {
        // `ChildAvailableSize()` returns the content-box size of the container
        // (i.e., the resolved size minus border, scrollbar, and padding). We
        // use it here to determine the stacking-axis offset for fill-reverse.
        const LayoutUnit container_stacking_axis_size =
            is_for_columns ? ChildAvailableSize().block_size
                           : ChildAvailableSize().inline_size;

        // Add back `stacking_axis_gap` when computing fill-reverse offsets;
        // in fill-reverse, gaps appear before each item, so the item's offset
        // must be increased to leave a visual gap in the stacking axis.
        if (container_stacking_axis_size != kIndefiniteSize) {
          final_start_offset_in_stacking_axis =
              container_stacking_axis_size + border_scrollbar_padding_start +
              stacking_axis_gap - start_offset_in_stacking_axis -
              visual_stacking_axis_size;
        }
      }

      is_for_columns ? containing_grid_area.offset.block_offset =
                           final_start_offset_in_stacking_axis
                     : containing_grid_area.offset.inline_offset =
                           final_start_offset_in_stacking_axis;

      // TODO(celestepan): Account for extra margins from sub-grid items.
      //
      // Adjust item's position in the track based on style. We only want offset
      // applied to the grid axis at the moment.
      //
      // TODO(celestepan): Update alignment logic if needed once we resolve on
      // https://github.com/w3c/csswg-drafts/issues/10275.

      const auto inline_alignment = is_for_columns
                                        ? grid_lanes_item.Alignment(kForColumns)
                                        : AxisEdge::kStart;
      const auto block_alignment = is_for_columns
                                       ? AxisEdge::kStart
                                       : grid_lanes_item.Alignment(kForRows);

      const LogicalBoxFragment baseline_fragment(
          grid_lanes_item.BaselineWritingDirection(grid_axis_direction),
          physical_fragment);

      // In grid-lanes, we only have tracks in one dimension, so baseline
      // alignment is only supported in one dimension (the grid axis). Only
      // compute the baseline offset for the direction that applies.
      LayoutUnit inline_baseline_offset;
      LayoutUnit block_baseline_offset;
      if (is_for_columns) {
        inline_baseline_offset = ComputeBaselineOffset(
            grid_lanes_item, layout_data, baseline_fragment, fragment,
            style.GetFontBaseline(), kForColumns,
            containing_grid_area.size.inline_size);
      } else {
        block_baseline_offset = ComputeBaselineOffset(
            grid_lanes_item, layout_data, baseline_fragment, fragment,
            style.GetFontBaseline(), kForRows,
            containing_grid_area.size.block_size);
      }

      containing_grid_area.offset += LogicalOffset(
          AlignmentOffset(
              containing_grid_area.size.inline_size, fragment.InlineSize(),
              margins.inline_start, margins.inline_end, inline_baseline_offset,
              inline_alignment, grid_lanes_item.IsOverflowSafe(kForColumns)),
          AlignmentOffset(
              containing_grid_area.size.block_size, fragment.BlockSize(),
              margins.block_start, margins.block_end, block_baseline_offset,
              block_alignment, grid_lanes_item.IsOverflowSafe(kForRows)));
    }

    // If the item was not placed in an earlier track opening, update
    // `running_positions` of the tracks that the items spans to include the
    // size of the item, the size of the opening in the stacking axis, and the
    // margin.
    if (!item_moved_to_earlier_opening) {
      auto new_running_position = start_offset_in_stacking_axis +
                                  fragment_stacking_axis_contribution;

      // If dense packing is enabled, we need to input the maximum running
      // position of the tracks our items span so that we can account for any
      // new openings that may form.
      running_positions.UpdateRunningPositionsForSpan(
          grid_lanes_item.resolved_position.Span(grid_axis_direction),
          new_running_position,
          is_dense_packing
              ? std::make_optional(
                    /*max_running_position=*/start_offset_in_stacking_axis)
              : std::nullopt);

      // Update auto-placement cursor after we have determined the item's final
      // placement.
      running_positions.UpdateAutoPlacementCursor(
          grid_lanes_item.resolved_position, grid_axis_direction);
    }

    if (placement_phase == PlacementPhase::kCalculateBaselines) {
      // In the baseline calculation pass, skip items without baseline
      // alignment since they don't contribute to track baselines. This check
      // must happen after the `running_positions` updates because all items
      // need to update placement state, regardless of whether they contribute
      // to baselines.
      if (!grid_lanes_item.IsBaselineAligned(grid_axis_direction)) {
        continue;
      }

      // Create `baseline_fragment` with the writing direction appropriate for
      // the `grid_axis_direction`. This may differ from the container's writing
      // direction for items with different writing modes, and ensures baselines
      // are calculated relative to the correct axis.
      const LogicalBoxFragment baseline_fragment(
          grid_lanes_item.BaselineWritingDirection(grid_axis_direction),
          physical_fragment);
      const LayoutUnit extra_margin =
          GetBaselineSideMargin(grid_lanes_item, margins, grid_axis_direction);

      StoreItemBaseline(baseline_fragment, grid_axis_direction,
                        style.GetFontBaseline(), extra_margin, layout_data,
                        grid_lanes_item);
    } else {
      // Items are only added to the container in the final placement pass.
      // During the baseline calculation pass, we only compute and store track
      // baselines without adding items, since baseline information is needed
      // before items can be properly aligned and placed.
      container_builder_.AddResult(*result, containing_grid_area.offset,
                                   margins);
      baseline_accumulator->Accumulate(
          grid_lanes_item, fragment, containing_grid_area.offset.block_offset,
          start_offset_in_stacking_axis, item_moved_to_earlier_opening);
    }
  }
}

void GridLanesLayoutAlgorithm::PlaceOutOfFlowItems(
    const GridLayoutData& layout_data,
    LayoutUnit block_size,
    HeapVector<Member<LayoutBox>>& oof_children) {
  const auto& container_style = Style();
  const auto& node = Node();
  const auto& placement_data = node.CachedPlacementData();
  const LogicalSize total_fragment_size = {container_builder_.InlineSize(),
                                           block_size};
  const auto default_containing_block_size =
      ShrinkLogicalSize(total_fragment_size, BorderScrollbarPadding());

  for (LayoutBox* oof_child : oof_children) {
    GridItemData* out_of_flow_item = MakeGarbageCollected<GridItemData>(
        BlockNode(oof_child), container_style);
    DCHECK(out_of_flow_item->IsOutOfFlow());

    std::optional<LogicalRect> containing_block_rect;
    const auto position = out_of_flow_item->node.Style().GetPosition();

    // If the grid-lanes container is also the containing-block for the
    // OOF-positioned item, pick up the static-position from the grid-area
    // in the grid axis.
    if ((node.IsAbsoluteContainer() && position == EPosition::kAbsolute) ||
        (node.IsFixedContainer() && position == EPosition::kFixed)) {
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

    // TODO(kschmi): Handle fragmentation.
    container_builder_.AddOutOfFlowChildCandidate(out_of_flow_item->node,
                                                  static_pos);
  }
}

LayoutUnit GridLanesLayoutAlgorithm::ComputeSharedBaselineForGroup(
    const GridItems::GridItemDataVector& group_items,
    GridTrackSizingDirection grid_axis_direction,
    SizingConstraint sizing_constraint) const {
  LayoutUnit shared_baseline = LayoutUnit::Min();

  // All items in a group have the same baseline alignment, so if the first
  // item isn't baseline aligned, there is no need to calculate a shared
  // baseline for the group.
  CHECK(!group_items.empty());
  if (!group_items[0]->IsBaselineAligned(grid_axis_direction)) {
    return shared_baseline;
  }

  for (const Member<GridItemData>& group_item : group_items) {
    // Subgrids don't contribute toward the contribution size of tracks. Thus,
    // they also shouldn't contribute toward the shared baseline size used to
    // compute item baseline shims during track sizing.
    if (group_item->MustConsiderGridItemsForSizing(grid_axis_direction)) {
      continue;
    }

    // TODO(almaher): `SubgriddedItemData` should incorporate the parent
    // subgrid's info.
    //
    // TODO(almaher): Plumb the parent grid's `GridLayoutData` here instead of
    // passing nullptr.
    const auto space_for_measure = CreateConstraintSpaceForMeasure(
        SubgriddedItemData(*group_item, /*parent_layout_data=*/nullptr,
                           GetConstraintSpace().GetWritingMode()));
    const BoxStrut margins = ComputeMarginsFor(
        space_for_measure, group_item->node.Style(), GetConstraintSpace());
    const LayoutUnit extra_margin =
        GetBaselineSideMargin(*group_item, margins, grid_axis_direction);

    const LayoutResult* result = LayoutItemForMeasureWithFallback(
        group_item, space_for_measure, sizing_constraint);
    LogicalBoxFragment baseline_fragment(
        group_item->BaselineWritingDirection(grid_axis_direction),
        To<PhysicalBoxFragment>(result->GetPhysicalFragment()));
    const LayoutUnit item_baseline = GetLogicalBaseline(
        baseline_fragment, group_item->parent_grid_font_baseline,
        group_item->IsLastBaselineSpecified(grid_axis_direction));

    const LayoutUnit total_baseline = extra_margin + item_baseline;
    shared_baseline = std::max(shared_baseline, total_baseline);
  }
  return shared_baseline;
}

VirtualItems* GridLanesLayoutAlgorithm::BuildVirtualGridLanesItems(
    const GridLineResolver& line_resolver,
    const GridItems& grid_lanes_items,
    const bool needs_intrinsic_track_size,
    SizingConstraint sizing_constraint,
    const wtf_size_t auto_repetition_count,
    wtf_size_t& start_offset,
    bool& has_baseline_aligned_items) const {
  const auto& style = Style();
  const auto grid_axis_direction = style.GridLanesTrackSizingDirection();
  const bool is_for_columns = grid_axis_direction == kForColumns;

  wtf_size_t max_end_line;
  auto* virtual_item_results = MakeGarbageCollected<VirtualItems>();
  GridItems* virtual_items = virtual_item_results->items.Get();

  // If there is an auto-fit track definition, store what tracks it spans.
  const GridTrackList& track_list =
      is_for_columns ? style.GridTemplateColumns().GetTrackList()
                     : style.GridTemplateRows().GetTrackList();
  GridSpan auto_fit_span = GridSpan::IndefiniteGridSpan();
  if (!needs_intrinsic_track_size && track_list.HasAutoRepeater() &&
      track_list.RepeatType(track_list.AutoRepeatTrackIndex()) ==
          GridTrackRepeater::RepeatType::kAutoFit) {
    auto_fit_span = GridSpan::TranslatedDefiniteGridSpan(
        track_list.TrackCountBeforeAutoRepeat(),
        track_list.TrackCountBeforeAutoRepeat() + auto_repetition_count);
  }

  wtf_size_t unplaced_item_span_count = 0;

  // Store the item groups in `virtual_item_results` so that the virtual item
  // contributions can be computed from those groups after track initialization.
  GridLanesItemGroups& item_groups = virtual_item_results->item_groups =
      Node().CollectItemGroups(line_resolver, grid_lanes_items, max_end_line,
                               start_offset, unplaced_item_span_count);

  for (auto& item_group_ptr : item_groups) {
    GridLanesItemGroup& item_group = *item_group_ptr;
    const auto& group_items = item_group.items;
    const auto& group_properties = item_group.properties;

    auto* virtual_item = MakeGarbageCollected<GridItemData>();

    // Share the same contribution size as that stored on the item group.
    // Each virtual item created from the same group will share the same
    // contribution sizes.
    //
    // TODO(almaher): Move contribution calculation until after track
    // initialization.
    virtual_item->contribution_sizes = item_group.contribution_sizes;

    GridSpan span = group_properties.Span();
    wtf_size_t span_size = span.SpanSize();
    CHECK_GT(span_size, 0u);

    // For each group, iterate all items, compute each item's baseline, and
    // choose the maximum as `shared_baseline` for the group. This value is
    // later used to calculate baseline shims for alignment within the track.
    //
    // The baseline shim added into each item's contribution size below is
    // specific to the `BuildVirtualGridLanesItems` phase. Per the spec,
    // "determine the baselines of the virtual grid item by placing all of
    // its items into a single hypothetical grid track and finding their
    // shared baseline(s) and shims. Increase the group's intrinsic size
    // contributions accordingly." [1]
    //
    // [1] https://www.w3.org/TR/css-grid-3/#track-sizing-performance
    const LayoutUnit shared_baseline = ComputeSharedBaselineForGroup(
        group_items, grid_axis_direction, sizing_constraint);

    // Store the group's shared baseline so copies inherit it for baseline
    // shim computation during track sizing.
    virtual_item->SetSharedBaseline(shared_baseline);

    // Copy baseline alignment properties from the first item in the group,
    // since all items in a group have the same baseline-sharing group.
    if (is_for_columns) {
      virtual_item->column_alignment = group_items[0]->column_alignment;
      virtual_item->column_baseline_group =
          group_items[0]->column_baseline_group;
    } else {
      virtual_item->row_alignment = group_items[0]->row_alignment;
      virtual_item->row_baseline_group = group_items[0]->row_baseline_group;
    }

    for (const Member<GridItemData>& group_item : group_items) {
      GridItemData& item_data = *group_item;

      // Per https://drafts.csswg.org/css-grid-2/#subgrid-size-contribution,
      // "the subgrid itself acts as if it was completely empty for track sizing
      // purposes in the subgridded dimension." Give it a zero contribution so
      // it still provides range coverage but doesn't affect track sizes.
      if (item_data.MustConsiderGridItemsForSizing(grid_axis_direction)) {
        virtual_item->EncompassContributionSize(MinMaxSizes());
        continue;
      }

      has_baseline_aligned_items |=
          item_data.IsBaselineSpecified(grid_axis_direction);

      const BlockNode& item_node = item_data.node;
      // TODO(almaher): `SubgriddedItemData` should incorporate the parent
      // subgrid's info.
      //
      // TODO(almaher): Plumb the parent grid's `GridLayoutData` here instead
      // of passing nullptr.
      const auto space = CreateConstraintSpaceForMeasure(
          SubgriddedItemData(item_data, /*parent_layout_data=*/nullptr,
                             GetConstraintSpace().GetWritingMode()));
      const ComputedStyle& item_style = item_node.Style();

      const bool use_item_inline_contribution =
          is_for_columns == item_data.is_parallel_with_root_grid;

      // TODO(almaher): Subgrids have extra margin to handle unique gap sizes.
      // This requires access to the subgrid track collection, where that extra
      // margin is accumulated.
      const BoxStrut margins =
          ComputeMarginsFor(space, item_style, GetConstraintSpace());
      const LayoutUnit margin_sum =
          is_for_columns ? margins.InlineSum() : margins.BlockSum();

      MinMaxSizes min_max_contribution;
      LayoutUnit baseline_shim;
      if (use_item_inline_contribution) {
        // The min/max contribution may depend on the block-size of the
        // grid-area: <div id="target" style="height: 200px; width: 600px;">
        //   <div style="display: inline-grid-lanes; width: min-content;
        //   grid-template-rows: auto; height: 100%;">
        //     <canvas width=60 height=60 style="height: 100%;"></canvas>
        //   </div>
        // </div>
        // <script>
        //   document.body.offsetTop;
        //   document.getElementById('target').style.height = '100px';
        // </script>
        // Mark the item as dependent on the block size in these cases; if the
        // block size changes, we'll need to re-run min/max calculations to get
        // the correct contribution from this item.
        const MinMaxSizesResult result =
            ComputeMinAndMaxContentContributionForSelf(item_node, space);
        if (result.depends_on_block_constraints) {
          item_data.is_sizing_dependent_on_block_size = true;
        }
        min_max_contribution = result.sizes;

        if (item_data.IsBaselineAligned(grid_axis_direction)) {
          const LayoutUnit extra_margin =
              GetBaselineSideMargin(item_data, margins, grid_axis_direction);

          const LayoutUnit min_shim = CalculateSynthesizedBaselineShim(
              item_data, min_max_contribution.min_size, grid_axis_direction,
              shared_baseline, extra_margin);
          min_max_contribution.min_size += min_shim;

          const LayoutUnit max_shim = CalculateSynthesizedBaselineShim(
              item_data, min_max_contribution.max_size, grid_axis_direction,
              shared_baseline, extra_margin);
          min_max_contribution.max_size += max_shim;

          baseline_shim = std::max(min_shim, max_shim);
        }
      } else {
        LayoutUnit block_contribution = ComputeGridLanesItemBlockContribution(
            grid_axis_direction, sizing_constraint, space, &item_data,
            needs_intrinsic_track_size, margins, shared_baseline,
            baseline_shim);
        min_max_contribution =
            MinMaxSizes(block_contribution, block_contribution);
      }

      // Keep track of special item contributions for intrinsic minimums. This
      // logic can depend on the tracks the item spans, so store three different
      // contributions - one assuming that the items are spanning such tracks,
      // and two assuming they aren't (one that may need to be clamped and one
      // that doesn't), so that later we can choose one or the other depending
      // on the tracks the virtual item spans. If a contribution may need to be
      // clamped, `maybe_clamp` will be set to true. See
      // https://drafts.csswg.org/css-grid/#min-size-auto for more details.
      //
      // TODO(almaher): pass in `subgrid_minmax_sizes` when we support
      // subgrid.
      bool maybe_clamp = false;
      LayoutUnit contribution_assuming_tracks =
          CalculateIntrinsicMinimumContribution(
              use_item_inline_contribution,
              /*special_spanning_criteria=*/true,
              [&]() { return min_max_contribution.min_size; },
              [&]() { return min_max_contribution.max_size; },
              /*subgrid_minmax_sizes=*/[]() { return MinMaxSizesResult(); },
              space, &item_data, maybe_clamp);
      // If we assume we are spanning tracks that force us to use the automatic
      // min size, we will never need to clamp the value returned here. As such,
      // `maybe_clamp` should never be true if `special_spanning_criteria` is
      // true.
      CHECK(!maybe_clamp);

      // It is ok to use the same `maybe_clamp` var here since the previous call
      // will never produce clamping, and the next call is the one we care about
      // potentially clamping.
      LayoutUnit contribution_ignoring_tracks =
          CalculateIntrinsicMinimumContribution(
              use_item_inline_contribution,
              /*special_spanning_criteria=*/false,
              [&]() { return min_max_contribution.min_size; },
              [&]() { return min_max_contribution.max_size; },
              /*subgrid_minmax_sizes=*/[]() { return MinMaxSizesResult(); },
              space, &item_data, maybe_clamp);

      // Add the margin sum to all contribution sizes.
      auto AdjustItemContribution = [&](LayoutUnit& contribution_size) {
        contribution_size += margin_sum;
      };
      AdjustItemContribution(min_max_contribution.min_size);
      AdjustItemContribution(min_max_contribution.max_size);
      AdjustItemContribution(contribution_ignoring_tracks);
      AdjustItemContribution(contribution_assuming_tracks);

      // Store the different contribution sizes on the virtual item to be used
      // later during track sizing.
      virtual_item->EncompassContributionSize(min_max_contribution);
      virtual_item->EncompassIntrinsicMinAssumingTrackPlacement(
          contribution_assuming_tracks);
      if (maybe_clamp) {
        virtual_item->EncompassIntrinsicMinIgnoringTrackPlacement(
            contribution_ignoring_tracks);

        const auto border_padding = ComputeBorders(space, item_node) +
                                    ComputePadding(space, item_style);
        const auto border_padding_sum = use_item_inline_contribution
                                            ? border_padding.InlineSum()
                                            : border_padding.BlockSum();

        virtual_item->EncompassMinClampSize(margin_sum + border_padding_sum +
                                            baseline_shim);
      } else {
        virtual_item->EncompassIntrinsicMinIgnoringTrackPlacementUnclamped(
            contribution_ignoring_tracks);
      }
    }

    // If `needs_intrinsic_track_size` is true, that means we have a repeat()
    // track definition with an intrinsic sized track(s). The current track
    // sizing pass is used to find the track size to apply to the intrinsic
    // sized track(s). Ignore item placement as part of this pass, and apply all
    // items in every position, regardless of explicit placement [1].
    //
    // [1] https://www.w3.org/TR/css-grid-3/#masonry-intrinsic-repeat
    if (span.IsIndefinite() || needs_intrinsic_track_size) {
      auto PlaceItemInEveryPosition = [&](GridSpan& item_span) {
        while (item_span.EndLine() < max_end_line) {
          auto* item_copy = MakeGarbageCollected<GridItemData>(*virtual_item);
          item_copy->resolved_position.SetSpan(item_span, grid_axis_direction);
          virtual_items->Append(item_copy);

          // `Translate` will move the span to the start and end of the next
          // line, allowing us to "slide" over the entire implicit grid.
          item_span.Translate(1);

          // Per the auto-fit heuristic, don't add auto placed items to
          // tracks within the auto-fit range that are greater than the
          // total span count of auto placed items.
          //
          // https://drafts.csswg.org/css-grid-3/#repeat-auto-fit
          if (!auto_fit_span.IsIndefinite()) {
            while (item_span.Intersects(auto_fit_span) &&
                   item_span.EndLine() > unplaced_item_span_count) {
              item_span.Translate(1);
            }
          }
        }
      };

      // If `needs_intrinsic_track_size` is true, that means we have a repeat()
      // track definition with an intrinsic sized track(s). The current track
      // sizing pass is used to find the track size to apply to the intrinsic
      // sized track(s). During this pass, we need to use the growth limit as
      // the track size for intrinsic tracks. However, the growth limit is
      // stored within a Grid set. To enable this look up, we create a
      // single-span virtual item that has zero contribution sizes, and place it
      // in every position. This guarantees we have one track per set, allowing
      // us to look up the growth limit for each track quickly and accurately.
      //
      // [1] https://www.w3.org/TR/css-grid-3/#masonry-intrinsic-repeat
      if (needs_intrinsic_track_size && virtual_items->IsEmpty()) {
        auto* item_copy = MakeGarbageCollected<GridItemData>(*virtual_item);
        GridSpan single_span = GridSpan::TranslatedDefiniteGridSpan(0, 1);
        virtual_item->ResetContributionSizes();
        PlaceItemInEveryPosition(single_span);

        if (single_span.EndLine() <= max_end_line) {
          virtual_item->resolved_position.SetSpan(single_span,
                                                  grid_axis_direction);
          virtual_items->Append(virtual_item);
        }
        virtual_item = item_copy;
      }

      // For groups of items that are auto-placed, we need to create
      // copies of the virtual item and place them at each possible start
      // line. At the end of the loop below, `span` will be located at the
      // last start line, which should be the position of the last copy
      // appended to `virtual_items`.
      span = GridSpan::TranslatedDefiniteGridSpan(0, span.SpanSize());
      PlaceItemInEveryPosition(span);
    }

    DCHECK(span.IsTranslatedDefinite());
    if (span.EndLine() <= max_end_line) {
      virtual_item->resolved_position.SetSpan(span, grid_axis_direction);
      virtual_items->Append(virtual_item);
    }
  }
  return virtual_item_results;
}

LayoutUnit GridLanesLayoutAlgorithm::ContributionSizeForVirtualItem(
    const GridLayoutTrackCollection& track_collection,
    LayoutUnit track_baseline,
    GridItemContributionType contribution_type,
    GridItemData* virtual_item) const {
  DCHECK(virtual_item);
  DCHECK(virtual_item->contribution_sizes);

  const GridTrackSizingDirection track_direction = track_collection.Direction();

  LayoutUnit baseline_shim;
  if (track_baseline != LayoutUnit::Min()) {
    baseline_shim = track_baseline -
                    virtual_item->contribution_sizes->group_shared_baseline;
  }

  switch (contribution_type) {
    case GridItemContributionType::kForContentBasedMinimums:
    case GridItemContributionType::kForIntrinsicMaximums:
      return virtual_item->contribution_sizes->min_max_contribution.min_size +
             baseline_shim;
    case GridItemContributionType::kForIntrinsicMinimums: {
      // See https://drafts.csswg.org/css-grid/#min-size-auto for more details
      // on the special logic applied for intrinsic minimums.
      if (!virtual_item->IsSpanningAutoMinimumTrack(track_direction) ||
          (virtual_item->IsSpanningFlexibleTrack(track_direction) &&
           virtual_item->SpanSize(track_direction) > 1)) {
        // Per the spec, we apply the automatic min when:
        // - it spans at least one track in that axis whose min track sizing
        // function is auto.
        // - if it spans more than one track in that axis, none of those tracks
        // are flexible.
        return virtual_item->contribution_sizes
                   ->intrinsic_min_assuming_track_placement +
               baseline_shim;
      } else {
        // When we aren't spanning tracks that force all items to their
        // automatic minimum, we end up with some items that use the automatic
        // min, and some that use the content minimum. Those that use a content
        // min need to be further clamped by the total track sizes it spans, if
        // those tracks are definite. After clamping, use the max of these two
        // values as the final contribution size.
        const LayoutUnit contribution_unclamped =
            virtual_item->contribution_sizes
                ->intrinsic_min_ignoring_track_placement_unclamped;

        LayoutUnit contribution_to_clamp =
            virtual_item->contribution_sizes
                ->intrinsic_min_ignoring_track_placement;

        const auto& [begin_set_index, end_set_index] =
            virtual_item->SetIndices(track_direction);
        auto spanned_tracks_definite_max_size =
            track_collection.CalculateSetSpanSize(begin_set_index,
                                                  end_set_index);
        if (spanned_tracks_definite_max_size != kIndefiniteSize) {
          contribution_to_clamp = ClampIntrinsicMinSize(
              contribution_to_clamp,
              virtual_item->contribution_sizes->min_clamp_size,
              spanned_tracks_definite_max_size);
        }

        return max(contribution_to_clamp, contribution_unclamped) +
               baseline_shim;
      }
    }
    case GridItemContributionType::kForMaxContentMaximums:
    case GridItemContributionType::kForMaxContentMinimums:
      return virtual_item->contribution_sizes->min_max_contribution.max_size +
             baseline_shim;
    case GridItemContributionType::kForFreeSpace:
      NOTREACHED() << "`kForFreeSpace` should only be used to distribute extra "
                      "space in maximize tracks and stretch auto tracks steps.";
  }
}

LayoutUnit
GridLanesLayoutAlgorithm::ComputeIntrinsicBlockSizeIgnoringChildren() {
  // First check if we've overridden the intrinsic block size.
  LayoutUnit override_intrinsic_block_size =
      Node().OverrideIntrinsicContentBlockSize();
  if (override_intrinsic_block_size == kIndefiniteSize) {
    if (Style().GridLanesTrackSizingDirection() != kForColumns) {
      // If we are in rows, we can use the grid-axis size as our block size.
      const auto grid_axis_direction = Style().GridLanesTrackSizingDirection();
      GridItems* grid_items = nullptr;
      const GridLayoutSubtree* layout_subtree = ComputeGridLanesGeometry(
          SizingConstraint::kLayout,
          /*should_apply_inline_size_containment=*/true, &grid_items);

      const auto& track_collection =
          grid_axis_direction == kForColumns
              ? layout_subtree->LayoutData()->Columns()
              : layout_subtree->LayoutData()->Rows();
      override_intrinsic_block_size = track_collection.CalculateSetSpanSize();
    } else {
      // If we are in columns, the block size should just be the size of an
      // empty container.
      override_intrinsic_block_size = LayoutUnit();
    }
  }
  return override_intrinsic_block_size + BorderScrollbarPadding().BlockSum();
}

const LayoutResult* GridLanesLayoutAlgorithm::LayoutItemForMeasureWithFallback(
    GridItemData* grid_lanes_item,
    const ConstraintSpace& space_for_measure,
    SizingConstraint sizing_constraint) const {
  if (space_for_measure.AvailableSize().inline_size == kIndefiniteSize) {
    // If we are orthogonal virtual item, resolving against an indefinite
    // size, set our inline size to our max-content contribution.
    const MinMaxSizesResult min_max_sizes_result =
        ComputeMinAndMaxContentContributionForSelf(grid_lanes_item->node,
                                                   space_for_measure);
    // The min/max contribution may depend on the block-size of the
    // grid-area: <div id="target" style="height: 200px; width: 600px;">
    //   <div style="display: inline-grid-lanes; width: min-content;
    //   grid-template-clumns: auto; height: 100%;">
    //     <canvas width=60 height=60 style="height: 100%;"></canvas>
    //   </div>
    // </div>
    // <script>
    //   document.body.offsetTop;
    //   document.getElementById('target').style.height = '100px';
    // </script>
    // Mark `grid_lanes_item` as dependent on the block size in these cases; if
    // the block size changes, we'll need to re-run min/max calculations to get
    // the correct contribution from this item.
    if (min_max_sizes_result.depends_on_block_constraints) {
      grid_lanes_item->is_sizing_dependent_on_block_size = true;
    }
    const MinMaxSizes sizes = min_max_sizes_result.sizes;
    // TODO(almaher): `SubgriddedItemData` should incorporate the parent
    // subgrid's info.
    //
    // TODO(almaher): Plumb the parent grid's `GridLayoutData` here instead of
    // passing nullptr.
    const auto fallback_space = CreateConstraintSpaceForMeasure(
        SubgriddedItemData(*grid_lanes_item, /*parent_layout_data=*/nullptr,
                           GetConstraintSpace().GetWritingMode()),
        /*opt_fixed_inline_size=*/sizes.max_size);
    return LayoutGridItemForMeasure(*grid_lanes_item, fallback_space,
                                         sizing_constraint);
  }
  return LayoutGridItemForMeasure(*grid_lanes_item, space_for_measure,
                                  sizing_constraint);
}

// TODO(almaher): Eventually look into consolidating repeated code with
// GridLayoutAlgorithm::ContributionSizeForGridItem().
LayoutUnit GridLanesLayoutAlgorithm::ComputeGridLanesItemBlockContribution(
    GridTrackSizingDirection track_direction,
    SizingConstraint sizing_constraint,
    const ConstraintSpace space_for_measure,
    GridItemData* grid_lanes_item,
    const bool needs_intrinsic_track_size,
    const BoxStrut& margins,
    LayoutUnit shared_baseline,
    LayoutUnit& baseline_shim) const {
  DCHECK(grid_lanes_item);

  // TODO(ikilpatrick): We'll need to record if any child used an indefinite
  // size for its contribution, such that we can then do the 2nd pass on the
  // track-sizing algorithm.

  // TODO(ikilpatrick): This should try and skip layout when possible. Notes:
  //  - We'll need to do a full layout for tables.
  //  - We'll need special logic for replaced elements.
  //  - We'll need to respect the aspect-ratio when appropriate.

  const LayoutResult* result = LayoutItemForMeasureWithFallback(
      grid_lanes_item, space_for_measure, sizing_constraint);

  LogicalBoxFragment baseline_fragment(
      grid_lanes_item->BaselineWritingDirection(track_direction),
      To<PhysicalBoxFragment>(result->GetPhysicalFragment()));

  if (grid_lanes_item->IsBaselineAligned(track_direction)) {
    const LayoutUnit baseline = GetLogicalBaseline(
        baseline_fragment, grid_lanes_item->parent_grid_font_baseline,
        grid_lanes_item->IsLastBaselineSpecified(track_direction));
    const LayoutUnit extra_margin =
        GetBaselineSideMargin(*grid_lanes_item, margins, track_direction);
    baseline_shim = shared_baseline - baseline - extra_margin;
    return baseline_fragment.BlockSize() + baseline_shim;
  }

  return baseline_fragment.BlockSize();
}

GridLayoutSubtree* GridLanesLayoutAlgorithm::ComputeGridLanesGeometry(
    SizingConstraint sizing_constraint,
    bool should_apply_inline_size_containment,
    GridItems** grid_items,
    HeapVector<Member<LayoutBox>>* opt_oof_children) {
  CHECK(grid_items);

  GridSizingTree sizing_tree;

  // TODO(almaher): For subgrid, we don't want to recompute the below and will
  // want to return early here.

  bool needs_intrinsic_track_size = false;
  ComputeSizingTreeInGridAxis(
      sizing_constraint, should_apply_inline_size_containment, &sizing_tree,
      needs_intrinsic_track_size, opt_oof_children);

  // We have a repeat() track definition with an intrinsic sized track(s). The
  // previous track sizing pass was used to find the track size to apply
  // to the intrinsic sized track(s). Retrieve that value(s), and re-run track
  // sizing to get the correct number of automatic repetitions for the
  // repeat() definition.
  //
  // https://www.w3.org/TR/css-grid-3/#masonry-intrinsic-repeat
  if (needs_intrinsic_track_size) {
    CalculateIntrinsicTrackSizes(sizing_tree);
    ComputeSizingTreeInGridAxis(sizing_constraint,
                                should_apply_inline_size_containment,
                                &sizing_tree, needs_intrinsic_track_size);
  }

  const auto& container_style = Style();
  const auto grid_axis_direction =
      container_style.GridLanesTrackSizingDirection();

  const bool applies_auto_min_size =
      !container_style.AspectRatio().IsAuto() &&
      container_style.IsOverflowVisibleOrClip() &&
      container_style.LogicalMinHeight().HasAuto();

  if (grid_axis_direction == kForRows) {
    auto& track_collection =
        sizing_tree.LayoutData().SizingCollection(kForRows);

    // For row grid-lanes, capture `intrinsic_block_size_` before any
    // additional layout pass that may change the track sizes. This preserves
    // the pre-re-run value so the container height is not affected by the
    // re-run, matching grid behavior.
    if (!sizing_tree.GetGridItems().IsEmpty()) {
      intrinsic_block_size_ = track_collection.CalculateSetSpanSize();
    }

    if (grid_lanes_available_size_.block_size == kIndefiniteSize ||
        applies_auto_min_size) {
      const auto& constraint_space = GetConstraintSpace();
      LayoutUnit intrinsic_block_size =
          track_collection.CalculateSetSpanSize() +
          BorderScrollbarPadding().BlockSum();
      intrinsic_block_size = ClampIntrinsicBlockSize(
          constraint_space, Node(), GetBreakToken(), BorderScrollbarPadding(),
          intrinsic_block_size);

      const auto block_size = ComputeBlockSizeForFragment(
          constraint_space, Node(), BorderPadding(), intrinsic_block_size,
          container_builder_.InlineSize());

      grid_lanes_available_size_.block_size =
          grid_lanes_min_available_size_.block_size =
              grid_lanes_max_available_size_.block_size =
                  (block_size - BorderScrollbarPadding().BlockSum())
                      .ClampNegativeToZero();

      if (NeedsAdditionalLayoutPass(container_style, constraint_space, Node(),
                                    BorderPadding(), track_collection,
                                    container_builder_.InlineSize())) {
        // TODO(yanlingwang): The auto-repeat count is preserved from the first
        // pass. Recomputing it here would require re-running
        // `ComputeSizingTreeInGridAxis`, which is expensive and rarely needed,
        // though we could end up with potentially more allowed repetitions
        // after percentages are properly resolved.
        InitializeTrackSizes(&sizing_tree);
        CompleteTrackSizingAlgorithm(sizing_constraint, &sizing_tree,
                                     /*needs_intrinsic_track_size=*/false);
      } else if (container_style.AlignContent() !=
                 ComputedStyleInitialValues::InitialAlignContent()) {
        // After resolving the block-size, if we don't need to rerun the track
        // sizing algorithm, simply apply any content alignment to its rows.
        auto first_set_geometry =
            GridTrackSizingAlgorithm::ComputeFirstSetGeometry(
                track_collection, container_style, grid_lanes_available_size_,
                BorderScrollbarPadding());
        track_collection.FinalizeSetsGeometry(first_set_geometry.start_offset,
                                              first_set_geometry.gutter_size);
      }
    }
  }

  CompleteFinalBaselineAlignment(&sizing_tree);

  auto& sizing_collection =
      sizing_tree.LayoutData().SizingCollection(grid_axis_direction);
  sizing_tree.LayoutData().SetTrackCollection(
      MakeGarbageCollected<GridLayoutTrackCollection>(sizing_collection));

  *grid_items = &sizing_tree.GetGridItems();
  return MakeGarbageCollected<GridLayoutSubtree>((sizing_tree.FinalizeTree()));
}

void GridLanesLayoutAlgorithm::BuildSizingCollection(
    GridTrackSizingDirection track_direction,
    const GridLineResolver& line_resolver,
    GridItems& grid_items,
    GridLayoutData& layout_data,
    SizingConstraint sizing_constraint,
    bool needs_intrinsic_track_size,
    VirtualItems** opt_virtual_items) const {
  CHECK(opt_virtual_items);
  const auto& style = Style();
  const auto grid_axis_direction = style.GridLanesTrackSizingDirection();

  // Grid-lanes only has tracks in the grid axis; the stacking axis is a no-op.
  if (track_direction != grid_axis_direction) {
    return;
  }

  bool has_baseline_aligned_items = false;
  wtf_size_t start_offset = 0;
  *opt_virtual_items = BuildVirtualGridLanesItems(
      line_resolver, grid_items, needs_intrinsic_track_size, sizing_constraint,
      line_resolver.AutoRepetitions(grid_axis_direction), start_offset,
      has_baseline_aligned_items);

  // Cache placement data. This is used for DevTools inspector highlighting and
  // also to access the computed auto repetitions in
  // `CalculateIntrinsicTrackSizes`.
  GridPlacementData placement_data(line_resolver);
  if (grid_axis_direction == kForColumns) {
    placement_data.column_start_offset = start_offset;
  } else {
    placement_data.row_start_offset = start_offset;
  }
  To<LayoutGridLanes>(Node().GetLayoutBox())
      ->SetCachedPlacementData(std::move(placement_data));

  auto BuildRanges = [&]() {
    GridRangeBuilder range_builder(
        style, grid_axis_direction,
        line_resolver.AutoRepetitions(grid_axis_direction), start_offset);

    for (auto& virtual_item : *(*opt_virtual_items)->items) {
      auto& range_indices = virtual_item.RangeIndices(grid_axis_direction);
      const auto& span = virtual_item.Span(grid_axis_direction);

      range_builder.EnsureTrackCoverage(span.StartLine(), span.IntegerSpan(),
                                        &range_indices.begin,
                                        &range_indices.end);
    }
    return range_builder.FinalizeRanges(needs_intrinsic_track_size);
  };

  layout_data.SetTrackCollection(
      MakeGarbageCollected<GridSizingTrackCollection>(
          BuildRanges(), grid_axis_direction,
          /*should_store_collapsed_track_indexes=*/true));
  if (has_baseline_aligned_items) {
    layout_data.CreateBaselines(grid_axis_direction);
  }
}

void GridLanesLayoutAlgorithm::InitializeTrackSizes(
    const GridSizingSubtree& sizing_subtree,
    const SubgriddedItemData& opt_subgrid_data) const {
  const auto& style = Style();
  const auto grid_axis_direction = style.GridLanesTrackSizingDirection();
  auto& layout_data = sizing_subtree.LayoutData();

  InitializeTrackCollection(
      opt_subgrid_data, style, GetConstraintSpace(), BorderScrollbarPadding(),
      grid_lanes_available_size_, grid_axis_direction, &layout_data);

  // TODO(almaher): For grid-lanes subgrids, we will want to get the tracks from
  // the parent.
  auto& track_collection = layout_data.SizingCollection(grid_axis_direction);

  // Allocate the major/minor baseline vectors now that we know the set count.
  if (layout_data.HasBaselines(grid_axis_direction)) {
    layout_data.ResetBaselines(grid_axis_direction,
                               track_collection.GetSetCount());
  }

  if (track_collection.HasNonDefiniteTrack()) {
    GridTrackSizingAlgorithm::CacheGridItemsProperties(
        track_collection, &sizing_subtree.GetVirtualItems());

    track_collection.CacheInitializedSetsGeometry(
        (grid_axis_direction == kForColumns)
            ? BorderScrollbarPadding().inline_start
            : BorderScrollbarPadding().block_start);

    // Build per-track shared baselines from all virtual item copies. Each
    // copy's `group_shared_baseline` is set to the maximum baseline across
    // its item group. Here, we take the max across all copies for each track,
    // so the track ends up with the largest baseline from any group.
    //
    // Note: these track baselines are specific to this track sizing phase —
    // they are derived from virtual items and used to compute baseline shims
    // for intrinsic track sizing. Later, in `ComputeBaselineAlignment`, track
    // baselines are reset and recomputed from actual item placements.
    if (layout_data.HasBaselines(grid_axis_direction)) {
      for (auto& virtual_item : sizing_subtree.GetVirtualItems()) {
        if (!virtual_item.IsBaselineAligned(grid_axis_direction) ||
            !virtual_item.contribution_sizes) {
          continue;
        }
        SetTrackBaseline(virtual_item, grid_axis_direction,
                         virtual_item.contribution_sizes->group_shared_baseline,
                         layout_data);
      }
    }
  } else {
    // If all tracks have a definite size upfront, we can use the current set
    // sizes as the used track sizes (applying alignment, if present).
    auto first_set_geometry = GridTrackSizingAlgorithm::ComputeFirstSetGeometry(
        track_collection, style, grid_lanes_available_size_,
        BorderScrollbarPadding());

    track_collection.FinalizeSetsGeometry(first_set_geometry.start_offset,
                                          first_set_geometry.gutter_size);
  }

  // Compute set indices for subgrid items so that `ForEachSubgrid` can create
  // constraint spaces for them.
  //
  // TODO(almaher): The position for these is not known at this point - for
  // every subgrid with an indefinite position, it will get set to the beginning
  // of the grid lanes container. We will eventually re-run layout if needed
  // to get the correct position.
  for (auto& grid_item : sizing_subtree.GetGridItems()) {
    if (grid_item.IsSubgrid()) {
      Node().ComputeSetIndicesForSubgrid(grid_item, layout_data);
    }
  }

  // Cache track span properties for subgrid items so that we know the track
  // properties for the tracks it spans (when explicitly placed). This is used
  // to determine if extra margin is needed to be added to those tracks.
  GridTrackSizingAlgorithm::CacheSubgridItemsProperties(
      track_collection, &sizing_subtree.GetGridItems(), grid_axis_direction);

  // Pass `nullopt` so that subgrids initialize both axes. A subgrid nested
  // in grid-lanes only subgrids in the grid axis; its other axis is standalone
  // and also needs track initialization.
  //
  // TODO(almaher): We will eventually need to handle this in a different
  // way once we support grid lanes subgrids.
  InitializeTrackSizesForEachSubgrid(sizing_subtree, *this,
                                     /*opt_track_direction=*/std::nullopt);
}

void GridLanesLayoutAlgorithm::InitializeTrackSizes(
    GridSizingTree* sizing_tree) const {
  InitializeTrackSizes(GridSizingSubtree(sizing_tree),
                       /*opt_subgrid_data=*/kNoSubgriddedItemData);
}

void GridLanesLayoutAlgorithm::CompleteTrackSizingAlgorithm(
    const GridSizingSubtree& sizing_subtree,
    SizingConstraint sizing_constraint,
    bool needs_intrinsic_track_size) const {
  const auto& style = Style();
  const auto grid_axis_direction = style.GridLanesTrackSizingDirection();
  auto& track_collection =
      sizing_subtree.LayoutData().SizingCollection(grid_axis_direction);

  if (track_collection.HasNonDefiniteTrack()) {
    // TODO(almaher): We will eventually want to do something with grid lanes
    // subgrids here.

    ComputeUsedTrackSizes(sizing_subtree, sizing_constraint,
                          needs_intrinsic_track_size);

    auto first_set_geometry = GridTrackSizingAlgorithm::ComputeFirstSetGeometry(
        track_collection, style, grid_lanes_available_size_,
        BorderScrollbarPadding());

    track_collection.FinalizeSetsGeometry(first_set_geometry.start_offset,
                                          first_set_geometry.gutter_size);
  }

  // Complete both axes for subgrids. A subgrid nested in grid-lanes only
  // subgrids in the grid axis; its other (standalone) axis also needs track
  // sizing completion. Always complete columns before rows, matching the grid
  // convention since row sizing can depend on resolved column sizes.
  //
  // TODO(almaher): We will eventually need to handle this in a different
  // way once we support grid lanes subgrids.
  CompleteTrackSizingAlgorithmForEachSubgrid(
      sizing_subtree, *this, kForColumns, sizing_constraint,
      /*opt_needs_additional_pass=*/nullptr);
  CompleteTrackSizingAlgorithmForEachSubgrid(
      sizing_subtree, *this, kForRows, sizing_constraint,
      /*opt_needs_additional_pass=*/nullptr);
}

void GridLanesLayoutAlgorithm::CompleteTrackSizingAlgorithm(
    SizingConstraint sizing_constraint,
    GridSizingTree* sizing_tree,
    bool needs_intrinsic_track_size) const {
  const auto sizing_subtree = GridSizingSubtree(sizing_tree);

  ValidateMinMaxSizesCache(Node(), sizing_subtree,
                           Style().GridLanesTrackSizingDirection());

  // TODO(almaher): When a grid subgrid is under grid-lanes, we may need to
  // call `ComputeBaselineAlignment` here for the subgrid's track direction, as
  // `GridLayoutAlgorithm` does. Revisit when testing grid-lanes baselines with
  // subgrid.

  CompleteTrackSizingAlgorithm(sizing_subtree, sizing_constraint,
                               needs_intrinsic_track_size);
}

void GridLanesLayoutAlgorithm::CompleteFinalBaselineAlignment(
    GridSizingTree* sizing_tree) {
  ComputeBaselineAlignment(sizing_tree->FinalizeTree(),
                           GridSizingSubtree(sizing_tree));
}

void GridLanesLayoutAlgorithm::ComputeUsedTrackSizes(
    const GridSizingSubtree& sizing_subtree,
    SizingConstraint sizing_constraint,
    bool needs_intrinsic_track_size) const {
  const auto& style = Style();
  const auto grid_axis_direction = style.GridLanesTrackSizingDirection();
  auto& track_collection =
      sizing_subtree.LayoutData().SizingCollection(grid_axis_direction);

  AccommodateSubgridExtraMargins(sizing_subtree, track_collection,
                                 grid_axis_direction);

  const GridTrackSizingAlgorithm track_sizing_algorithm(
      style, grid_lanes_available_size_, grid_lanes_min_available_size_,
      sizing_constraint);

  const auto& layout_data = sizing_subtree.LayoutData();

  track_sizing_algorithm.ComputeUsedTrackSizes(
      [&](GridItemContributionType contribution_type,
          GridItemData* virtual_item) {
        const LayoutUnit track_baseline =
            virtual_item->IsBaselineAligned(grid_axis_direction)
                ? GetTrackBaseline(*virtual_item, layout_data,
                                   grid_axis_direction)
                : LayoutUnit::Min();
        return ContributionSizeForVirtualItem(track_collection, track_baseline,
                                              contribution_type, virtual_item);
      },
      &track_collection, &sizing_subtree.GetVirtualItems(),
      needs_intrinsic_track_size);
}

void GridLanesLayoutAlgorithm::ComputeBaselineAlignment(
    const GridLayoutTree* layout_tree,
    const GridSizingSubtree& sizing_subtree) {
  const auto& style = Style();
  const auto grid_axis_direction = style.GridLanesTrackSizingDirection();
  auto& layout_data = sizing_subtree.LayoutData();
  auto& track_collection = layout_data.SizingCollection(grid_axis_direction);

  if (!layout_data.HasBaselines(grid_axis_direction)) {
    return;
  }

  // TODO(almaher): We will need special subgrid logic here utilizing
  // `opt_subgrid_data`, similar to grid.

  layout_data.ResetBaselines(grid_axis_direction,
                             track_collection.GetSetCount());

  const bool is_for_columns = grid_axis_direction == kForColumns;
  const auto stacking_axis_gap = GridTrackSizingAlgorithm::CalculateGutterSize(
      style, grid_lanes_available_size_,
      is_for_columns ? kForRows : kForColumns);

  // If an item is baseline aligned, placement is required in two passes because
  // track baselines must be computed before items can be aligned. In the
  // initial placement pass, compute track baselines by placing items to
  // determine their positions and baselines. Note that during this baseline
  // calculation pass, items are not added to the container; only baseline
  // information is computed and stored, since baselines are needed before items
  // can be properly aligned and placed.
  GridLanesRunningPositions running_positions(
      track_collection, style,
      ResolveFlowToleranceForGridLanes(style, grid_lanes_available_size_));

  // Use a dummy baseline accumulator since we only care about storing
  // baselines on the track collection, not accumulating container baselines.
  std::optional<StackingBaselineAccumulator> stacking_baseline_accumulator;
  std::optional<GridBaselineAccumulator> grid_baseline_accumulator;
  BaselineAccumulator* baseline_accumulator;
  if (is_for_columns) {
    stacking_baseline_accumulator.emplace(running_positions,
                                          grid_axis_direction);
    baseline_accumulator = &stacking_baseline_accumulator.value();
  } else {
    grid_baseline_accumulator.emplace(style.GetFontBaseline());
    baseline_accumulator = &grid_baseline_accumulator.value();
  }

  RunGridLanesPlacementPhase(sizing_subtree.GetGridItems(),
                             /*layout_subtree=*/nullptr,
                             sizing_subtree.LayoutData(),
                             SizingConstraint::kLayout, stacking_axis_gap,
                             PlacementPhase::kCalculateBaselines,
                             baseline_accumulator, running_positions);

  // Pass `nullopt` so that subgrids handle baseline alignment for both axes,
  // since a subgrid nested in grid-lanes may have a standalone axis.
  //
  // TODO(almaher): We will eventually need to handle this in a different
  // way once we support grid lanes subgrids.
  ComputeBaselineAlignmentForEachSubgrid(sizing_subtree, *this, layout_tree,
                                         /*opt_track_direction=*/std::nullopt,
                                         SizingConstraint::kLayout);
}

void GridLanesLayoutAlgorithm::ComputeSizingTreeInGridAxis(
    SizingConstraint sizing_constraint,
    const bool should_apply_inline_size_containment,
    GridSizingTree* sizing_tree,
    bool& needs_intrinsic_track_size,
    HeapVector<Member<LayoutBox>>* opt_oof_children) {
  DCHECK(sizing_tree);
  const ComputedStyle& style = Style();

  needs_intrinsic_track_size = false;
  const auto* intrinsic_repeat_track_sizes =
      sizing_tree->Size() > 0
          ? sizing_tree->LayoutData().IntrinsicRepeatTrackSizes()
          : nullptr;
  const GridLineResolver line_resolver(
      style, ComputeAutomaticRepetitions(intrinsic_repeat_track_sizes,
                                         needs_intrinsic_track_size));

  *sizing_tree =
      should_apply_inline_size_containment
          ? BuildGridSizingTreeIgnoringChildren(*this, line_resolver,
                                                sizing_constraint,
                                                needs_intrinsic_track_size)
          : BuildGridSizingTree(*this, line_resolver, opt_oof_children,
                                sizing_constraint, needs_intrinsic_track_size);

  InitializeTrackSizes(sizing_tree);
  CompleteTrackSizingAlgorithm(sizing_constraint, sizing_tree,
                               needs_intrinsic_track_size);
}

void GridLanesLayoutAlgorithm::CalculateIntrinsicTrackSizes(
    GridSizingTree& sizing_tree) const {
  const ComputedStyle& style = Style();
  GridTrackSizingDirection grid_axis_direction =
      style.GridLanesTrackSizingDirection();
  const bool is_for_columns = grid_axis_direction == kForColumns;
  const bool has_items = !sizing_tree.GetGridItems().IsEmpty();
  const auto& track_collection =
      sizing_tree.LayoutData().SizingCollection(grid_axis_direction);

  const GridTrackList& track_list =
      is_for_columns ? style.GridTemplateColumns().GetTrackList()
                     : style.GridTemplateRows().GetTrackList();

  // `repeat_size` is the number of tracks within the auto repeat definition,
  // and `repeat_track_count` is the total number of tracks when the
  // auto repeat is expanded.
  const wtf_size_t repeat_size = track_list.AutoRepeatTrackCount();
  const wtf_size_t repeat_track_count =
      Node().CachedPlacementData().line_resolver.AutoRepeatTrackCount(
          grid_axis_direction);

  const wtf_size_t auto_repeat_index = track_list.AutoRepeatTrackIndex();
  const wtf_size_t track_count_before_auto_repeat =
      track_list.TrackCountBeforeAutoRepeat();

  // Index of the auto repeat definition we are currently processing.
  wtf_size_t intrinsic_repeat_track_index = 0;
  for (wtf_size_t i = track_count_before_auto_repeat;
       i < track_count_before_auto_repeat + repeat_track_count - 1; ++i) {
    LayoutUnit track_size;
    if (has_items) {
      // We guarantee that during the track sizing pass to determine intrinsic
      // repeat track sizes, we have at least one single-span virtual item per
      // track, which guarantees one track per set.
      GridSet current_set = track_collection.GetSetAt(i);
      CHECK_EQ(current_set.track_count, 1U);
      track_size = current_set.GrowthLimit();
    }

    const GridTrackSize& track_definition = track_list.RepeatTrackSize(
        auto_repeat_index, intrinsic_repeat_track_index);

    sizing_tree.LayoutData().AppendIntrinsicRepeatTrackSize(track_definition,
                                                            track_size);

    if (intrinsic_repeat_track_index == repeat_size - 1) {
      // If there are no items, we only need to account for the track
      // definitions in one full repeat() expansion.
      if (!has_items) {
        break;
      }
      intrinsic_repeat_track_index = 0;
    } else {
      ++intrinsic_repeat_track_index;
    }
  }
}

// https://drafts.csswg.org/css-grid-2/#auto-repeat
wtf_size_t GridLanesLayoutAlgorithm::ComputeAutomaticRepetitions(
    const HashMap<GridTrackSize, LayoutUnit>* intrinsic_repeat_track_sizes,
    bool& needs_intrinsic_track_size) const {
  const ComputedStyle& style = Style();
  GridTrackSizingDirection grid_axis_direction =
      style.GridLanesTrackSizingDirection();
  const bool is_for_columns = grid_axis_direction == kForColumns;

  const GridTrackList& track_list =
      is_for_columns ? style.GridTemplateColumns().GetTrackList()
                     : style.GridTemplateRows().GetTrackList();

  if (!track_list.HasAutoRepeater()) {
    return 0;
  }

  // To determine an intrinsic track size within a repeat, we need to expand
  // them out to capture all possible automatic placements of each item, and
  // run track sizing to get the actual size [1]. Then we will run this again
  // with the actual intrinsic track size within a final track sizing pass
  // based on this size.
  //
  // According to the spec [1], the total number of tracks to expand to is
  // '2 + (largest span - 2)/(number of tracks in repeat())' rounded down.
  //
  // [1] https://www.w3.org/TR/css-grid-3/#masonry-intrinsic-repeat
  if (track_list.HasIntrinsicSizedRepeater() && !intrinsic_repeat_track_sizes) {
    CHECK(!needs_intrinsic_track_size);
    CHECK_GT(track_list.AutoRepeatTrackCount(), 0u);
    needs_intrinsic_track_size = true;

    const wtf_size_t largest_span = Node().ComputeLargestChildSpanSize();
    return 2 + (largest_span > 2
                    ? (largest_span - 2) / track_list.AutoRepeatTrackCount()
                    : 0);
  }

  // TODO(almaher): We will need special computation of automatic repetitions
  // for subgrid (see ComputeAutomaticRepetitionsForSubgrid()). Once this is
  // supported, we can move more of this method to the helper in
  // grid_layout_utils.cc.

  const LayoutUnit gutter_size = GridTrackSizingAlgorithm::CalculateGutterSize(
      style, grid_lanes_available_size_, grid_axis_direction);

  return CalculateAutomaticRepetitions(
      track_list, gutter_size,
      is_for_columns ? grid_lanes_available_size_.inline_size
                     : grid_lanes_available_size_.block_size,
      is_for_columns ? grid_lanes_min_available_size_.inline_size
                     : grid_lanes_min_available_size_.block_size,
      is_for_columns ? grid_lanes_max_available_size_.inline_size
                     : grid_lanes_max_available_size_.block_size,
      intrinsic_repeat_track_sizes);
}

ConstraintSpace GridLanesLayoutAlgorithm::CreateConstraintSpace(
    const GridItemData& grid_lanes_item,
    const LogicalSize& containing_size,
    const LogicalSize& fixed_available_size,
    LayoutResultCacheSlot result_cache_slot,
    const GridLayoutSubtree* opt_layout_subtree) const {
  ConstraintSpaceBuilder builder(
      GetConstraintSpace(), grid_lanes_item.node.Style().GetWritingDirection(),
      /*is_new_fc=*/true, /*adjust_inline_size_if_needed=*/false);

  builder.SetCacheSlot(result_cache_slot);
  builder.SetIsPaintedAtomically(true);

  {
    LogicalSize available_size = containing_size;
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
    DCHECK(grid_lanes_item.IsSubgrid());
    DCHECK(!opt_layout_subtree->HasUnresolvedGeometry());
    builder.SetGridLayoutSubtree(opt_layout_subtree);
  }

  builder.SetPercentageResolutionSize(containing_size);
  builder.SetInlineAutoBehavior(grid_lanes_item.column_auto_behavior);
  builder.SetBlockAutoBehavior(grid_lanes_item.row_auto_behavior);
  return builder.ToConstraintSpace();
}

// TODO(almaher): `opt_child_block_offset` and `unavailable_block_size` aren't
// used yet, but they will likely be needed for fragmentatation support.
//
// TODO(almaher): Should we do something with
// `min_block_size_should_encompass_intrinsic_size`?
ConstraintSpace GridLanesLayoutAlgorithm::CreateConstraintSpaceForLayout(
    const SubgriddedItemData& subgridded_item,
    const GridLayoutSubtree* opt_layout_subtree,
    LogicalRect* containing_grid_area,
    LayoutUnit unavailable_block_size,
    bool min_block_size_should_encompass_intrinsic_size,
    std::optional<LayoutUnit> opt_child_block_offset,
    std::optional<LayoutUnit> opt_fixed_inline_size) const {
  const auto writing_mode = GetConstraintSpace().GetWritingMode();
  const bool is_for_columns =
      Style().GridLanesTrackSizingDirection() == kForColumns;
  const auto& track_collection = is_for_columns
                                     ? subgridded_item.Columns(writing_mode)
                                     : subgridded_item.Rows(writing_mode);

  auto containing_size = grid_lanes_available_size_;
  auto& grid_axis_size =
      is_for_columns ? containing_size.inline_size : containing_size.block_size;

  LayoutUnit start_offset;
  grid_axis_size =
      subgridded_item->CalculateAvailableSize(track_collection, &start_offset);

  if (containing_grid_area) {
    is_for_columns ? containing_grid_area->offset.inline_offset = start_offset
                   : containing_grid_area->offset.block_offset = start_offset;
    containing_grid_area->size = containing_size;
  }

  // Unlike grid, in grid-lanes, we are only constrained by the final track
  // sizing in one dimension. However, at track sizing, we may force a
  // block/inline constraint for orthogonal items. This logic ensures we enforce
  // the same constraint at layout, as well. Otherwise, we can end up with odd
  // layout and overflow of items that we don't get in grid.
  LogicalSize fixed_available_size = kIndefiniteLogicalSize;
  if (opt_fixed_inline_size) {
    const auto item_writing_mode =
        subgridded_item->node.Style().GetWritingMode();
    const bool is_parallel =
        IsParallelWritingMode(item_writing_mode, writing_mode);
    const bool used_block_constraint_at_track_sizing =
        is_for_columns ? !is_parallel : is_parallel;
    if (used_block_constraint_at_track_sizing) {
      if (is_parallel) {
        if (containing_size.inline_size == kIndefiniteSize) {
          CHECK_NE(containing_size.block_size, kIndefiniteSize);
          fixed_available_size.inline_size = *opt_fixed_inline_size;
        }
      } else {
        if (containing_size.block_size == kIndefiniteSize) {
          CHECK_NE(containing_size.inline_size, kIndefiniteSize);
          fixed_available_size.block_size = *opt_fixed_inline_size;
        }
      }
    }
  }

  // For subgrid items, fix the available size along the subgridded axis to
  // the size carved out by the parent grid-lanes (minus the subgrid's own
  // margins). This ensures the subgrid's tracks line up with the parent's
  // and that the subgrid doesn't size itself independently in that
  // dimension.
  //
  // TODO(almaher): For the standalone axis of a subgrid, we should also
  // derive `containing_size` from the parent subgrid's track collection
  // (rather than the grid-lanes' own available size) so subgridded
  // descendants are measured against the correct subgrid-relative
  // containing block. See `GridLayoutAlgorithm::CreateConstraintSpaceFor*`
  // for the analogous handling in regular grid.
  if (subgridded_item.IsSubgrid()) {
    const auto subgrid_margins = ComputeMarginsFor(
        subgridded_item->node.Style(), containing_size.inline_size,
        GetConstraintSpace().GetWritingDirection());
    const auto fixed_size = ShrinkLogicalSize(containing_size, subgrid_margins);
    if (subgridded_item->has_subgridded_columns) {
      fixed_available_size.inline_size = fixed_size.inline_size;
    } else if (subgridded_item->has_subgridded_rows) {
      fixed_available_size.block_size = fixed_size.block_size;
    }
  }

  return CreateConstraintSpace(*subgridded_item, containing_size,
                               fixed_available_size,
                               LayoutResultCacheSlot::kLayout,
                               opt_layout_subtree);
}

ConstraintSpace GridLanesLayoutAlgorithm::CreateConstraintSpaceForMeasure(
    const SubgriddedItemData& subgridded_item,
    std::optional<LayoutUnit> opt_fixed_inline_size,
    const GridLayoutTrackCollection* track_collection,
    bool is_for_min_max_sizing) const {
  LogicalSize containing_size = grid_lanes_available_size_;
  const auto writing_mode = GetConstraintSpace().GetWritingMode();
  const auto grid_axis_direction = Style().GridLanesTrackSizingDirection();
  const bool is_parallel_with_root_grid =
      subgridded_item->is_parallel_with_root_grid;

  // Check against columns, as opposed to whether the item is parallel, because
  // the ConstraintSpaceBuilder takes care of handling orthogonal items.
  if (grid_axis_direction == kForColumns) {
    containing_size.inline_size = kIndefiniteSize;

    // Only set a definite inline size if the item is orthogonal because the
    // block and inline constraints get swapped for such items later on, and
    // unlike the inline constraint, the block constraint can be definite in
    // a measure pass.
    if (track_collection && !is_parallel_with_root_grid) {
      LayoutUnit start_offset;
      containing_size.inline_size = subgridded_item->CalculateAvailableSize(
          *track_collection, &start_offset);
    }
  } else {
    if (is_for_min_max_sizing) {
      // In the row direction, we use this method to create a space for
      // measuring the min/max-content of the item, so we have to set the inline
      // size as indefinite to allow for text flow.
      containing_size.inline_size = kIndefiniteSize;
    }
    containing_size.block_size = kIndefiniteSize;

    // Don't set a definite block size if the item is orthogonal because the
    // block and inline constraints get swapped later on for such items, and the
    // inline constraint should always be indefinite in a measure pass.
    if (track_collection && is_parallel_with_root_grid) {
      LayoutUnit start_offset;
      containing_size.block_size = subgridded_item->CalculateAvailableSize(
          *track_collection, &start_offset);
    }
  }

  // TODO(almaher): For the standalone axis of a subgrid, we should also
  // derive `containing_size` from the parent subgrid's track collection
  // (rather than the grid-lanes' own available size) so subgridded
  // descendants are measured against the correct subgrid-relative
  // containing block. See `GridLayoutAlgorithm::CreateConstraintSpaceFor*`
  // for the analogous handling in regular grid.
  LogicalSize fixed_available_size = kIndefiniteLogicalSize;
  if (subgridded_item.IsSubgrid()) {
    const auto subgrid_margins = ComputeMarginsFor(
        subgridded_item->node.Style(), containing_size.inline_size,
        GetConstraintSpace().GetWritingDirection());
    const auto fixed_size = ShrinkLogicalSize(containing_size, subgrid_margins);
    if (subgridded_item->has_subgridded_columns) {
      fixed_available_size.inline_size = fixed_size.inline_size;
    } else if (subgridded_item->has_subgridded_rows) {
      fixed_available_size.block_size = fixed_size.block_size;
    }
  }

  if (opt_fixed_inline_size) {
    const auto item_writing_mode =
        subgridded_item->node.Style().GetWritingMode();
    if (IsParallelWritingMode(item_writing_mode, writing_mode)) {
      DCHECK_EQ(fixed_available_size.inline_size, kIndefiniteSize);
      fixed_available_size.inline_size = *opt_fixed_inline_size;
    } else {
      DCHECK_EQ(fixed_available_size.block_size, kIndefiniteSize);
      fixed_available_size.block_size = *opt_fixed_inline_size;
    }
  }

  return CreateConstraintSpace(*subgridded_item, containing_size,
                               fixed_available_size,
                               LayoutResultCacheSlot::kMeasure);
}

// static
LogicalRect GridLanesLayoutAlgorithm::ComputeOutOfFlowItemContainingRect(
    const GridPlacementData& placement_data,
    const GridLayoutData& layout_data,
    const ComputedStyle& grid_lanes_style,
    const BoxStrut& borders,
    const LogicalSize& border_box_size,
    GridItemData* out_of_flow_item) {
  DCHECK(out_of_flow_item && out_of_flow_item->IsOutOfFlow());
  const bool is_for_columns =
      grid_lanes_style.GridLanesTrackSizingDirection() == kForColumns;

  out_of_flow_item->ComputeOutOfFlowItemPlacement(
      is_for_columns ? layout_data.Columns() : layout_data.Rows(),
      placement_data, grid_lanes_style);
  LogicalRect containing_rect;
  const auto& track_collection =
      is_for_columns ? layout_data.Columns() : layout_data.Rows();

  // Compute the containing rect for out-of-flow items in grid-lanes:
  // - Grid axis: Use normal grid placement
  // - Stacking axis: Ignore grid placement and use the full container size,
  // since items flow and stack naturally in this direction and OOF items should
  // have access to the entire space.
  ComputeOutOfFlowOffsetAndSize(
      *out_of_flow_item, track_collection, borders, border_box_size,
      &containing_rect.offset.inline_offset, &containing_rect.size.inline_size,
      /*is_grid_lanes_axis=*/!is_for_columns);

  ComputeOutOfFlowOffsetAndSize(
      *out_of_flow_item, track_collection, borders, border_box_size,
      &containing_rect.offset.block_offset, &containing_rect.size.block_size,
      /*is_grid_lanes_axis=*/is_for_columns);

  return containing_rect;
}

}  // namespace blink
