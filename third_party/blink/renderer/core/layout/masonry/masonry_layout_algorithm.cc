// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#include "third_party/blink/renderer/core/layout/masonry/masonry_layout_algorithm.h"

#include "base/notreached.h"
#include "third_party/blink/renderer/core/layout/disable_layout_side_effects_scope.h"
#include "third_party/blink/renderer/core/layout/grid/grid_baseline_accumulator.h"
#include "third_party/blink/renderer/core/layout/grid/grid_item.h"
#include "third_party/blink/renderer/core/layout/grid/grid_layout_utils.h"
#include "third_party/blink/renderer/core/layout/grid/grid_track_collection.h"
#include "third_party/blink/renderer/core/layout/grid/grid_track_sizing_algorithm.h"
#include "third_party/blink/renderer/core/layout/layout_utils.h"
#include "third_party/blink/renderer/core/layout/logical_box_fragment.h"
#include "third_party/blink/renderer/core/layout/masonry/masonry_running_positions.h"

namespace blink {

MasonryLayoutAlgorithm::MasonryLayoutAlgorithm(
    const LayoutAlgorithmParams& params)
    : LayoutAlgorithm(params) {
  DCHECK(params.space.IsNewFormattingContext());

  // At various stages of the algorithm we need to know the masonry
  // available-size. If it's initially indefinite, we need to know the min/max
  // sizes as well. Initialize all these to the same value.
  masonry_available_size_ = masonry_min_available_size_ =
      masonry_max_available_size_ = ChildAvailableSize();
  ComputeAvailableSizes(BorderScrollbarPadding(), Node(), GetConstraintSpace(),
                        container_builder_, masonry_available_size_,
                        masonry_min_available_size_,
                        masonry_max_available_size_);

  // TODO(almaher): Apply block-size containment.
}

MinMaxSizesResult MasonryLayoutAlgorithm::ComputeMinMaxSizes(
    const MinMaxSizesFloatInput&) {
  auto ComputeIntrinsicInlineSize = [&](SizingConstraint sizing_constraint) {
    bool needs_auto_track_size = false;
    std::optional<LayoutUnit> auto_repeat_track_size = std::nullopt;
    wtf_size_t start_offset;
    GridItems masonry_items;
    Vector<wtf_size_t> collapsed_track_indexes;
    const bool is_for_columns =
        Style().MasonryTrackSizingDirection() == kForColumns;

    GridSizingTrackCollection track_collection = ComputeGridAxisTracks(
        sizing_constraint, auto_repeat_track_size, masonry_items,
        collapsed_track_indexes, start_offset, needs_auto_track_size);

    // We have a repeat() track definition with an auto sized track(s). The
    // previous track sizing pass was used to find the track size to apply
    // to the auto sized track(s). Retrieve that value, and re-run track
    // sizing to get the correct number of automatic repetitions for the
    // repeat() definition.
    //
    // https://www.w3.org/TR/css-grid-3/#masonry-intrinsic-repeat
    if (needs_auto_track_size) {
      CHECK_NE(track_collection.GetAutoSizedRepeaterTrackIndex(), kNotFound);
      CHECK(collapsed_track_indexes.empty());
      // Note that when `needs_auto_track_size` is true, we skip the steps to
      // distribute free space during track sizing. This means that the base
      // track size at this point represents the size of the intrinsic track
      // without free space distribution.
      auto_repeat_track_size =
          track_collection
              .GetSetAt(track_collection.GetAutoSizedRepeaterTrackIndex())
              .BaseSize();

      track_collection = ComputeGridAxisTracks(
          sizing_constraint, auto_repeat_track_size, masonry_items,
          collapsed_track_indexes, start_offset, needs_auto_track_size);
    }

    if (is_for_columns) {
      // Track sizing is done during the guess placement step, which happens in
      // `BuildGridAxisTracks`, so at this point, getting the width of all of
      // the columns should correctly give us the intrinsic inline size.
      return track_collection.CalculateSetSpanSize();
    } else {
      if (masonry_items.IsEmpty()) {
        // If there are no masonry items, the intrinsic inline size is only
        // border, scrollbar, and padding.
        return BorderScrollbarPadding().InlineSum();
      }

      MasonryRunningPositions running_positions(
          track_collection.EndLineOfImplicitGrid(), LayoutUnit(),
          ResolveItemToleranceForMasonry(Style(), masonry_available_size_),
          collapsed_track_indexes);
      PlaceMasonryItems(track_collection, masonry_items, start_offset,
                        running_positions, sizing_constraint);
      // `stacking_axis_gap` represents the space between each of the items
      // in the row. We need to subtract this as it is always added to
      // `running_positions` whenever an item is placed, but the very last
      // addition should be deleted as there is no item after it.
      const auto stacking_axis_gap =
          GridTrackSizingAlgorithm::CalculateGutterSize(
              Style(), masonry_available_size_, kForColumns);
      return running_positions.GetMaxPositionForSpan(
                 GridSpan::TranslatedDefiniteGridSpan(
                     /*start_line=*/0,
                     /*end_line=*/track_collection.EndLineOfImplicitGrid())) -
             stacking_axis_gap;
    }
  };

  MinMaxSizes intrinsic_sizes{
      ComputeIntrinsicInlineSize(SizingConstraint::kMinContent),
      ComputeIntrinsicInlineSize(SizingConstraint::kMaxContent)};
  intrinsic_sizes += BorderScrollbarPadding().InlineSum();

  // TODO(ethavar): Compute `depends_on_block_constraints` by checking if any
  // masonry item has `is_sizing_dependent_on_block_size` set to true.
  return {intrinsic_sizes, /*depends_on_block_constraints=*/false};
}

const LayoutResult* MasonryLayoutAlgorithm::Layout() {
  bool needs_auto_track_size = false;
  std::optional<LayoutUnit> auto_repeat_track_size = std::nullopt;
  wtf_size_t start_offset;
  GridItems masonry_items;
  HeapVector<Member<LayoutBox>> oof_children;
  Vector<wtf_size_t> collapsed_track_indexes;

  GridSizingTrackCollection track_collection =
      ComputeGridAxisTracks(SizingConstraint::kLayout, auto_repeat_track_size,
                            masonry_items, collapsed_track_indexes,
                            start_offset, needs_auto_track_size, &oof_children);

  // We have a repeat() track definition with an auto sized track(s). The
  // previous track sizing pass was used to find the track size to apply
  // to the auto sized track(s). Retrieve that value, and re-run track
  // sizing to get the correct number of automatic repetitions for the
  // repeat() definition.
  //
  // https://www.w3.org/TR/css-grid-3/#masonry-intrinsic-repeat
  if (needs_auto_track_size) {
    CHECK_NE(track_collection.GetAutoSizedRepeaterTrackIndex(), kNotFound);
    CHECK(collapsed_track_indexes.empty());
    // Note that when `needs_auto_track_size` is true, we skip the steps to
    // distribute free space during track sizing. This means that the base track
    // size at this point represents the size of the intrinsic track without
    // free space distribution.
    auto_repeat_track_size =
        track_collection
            .GetSetAt(track_collection.GetAutoSizedRepeaterTrackIndex())
            .BaseSize();

    track_collection = ComputeGridAxisTracks(
        SizingConstraint::kLayout, auto_repeat_track_size, masonry_items,
        collapsed_track_indexes, start_offset, needs_auto_track_size);
  }

  if (!masonry_items.IsEmpty()) {
    MasonryRunningPositions running_positions(
        /*track_count=*/track_collection.EndLineOfImplicitGrid(),
        /*initial_running_position=*/LayoutUnit(),
        ResolveItemToleranceForMasonry(Style(), masonry_available_size_),
        collapsed_track_indexes);
    PlaceMasonryItems(track_collection, masonry_items, start_offset,
                      running_positions, SizingConstraint::kLayout);
  }

  if (!oof_children.empty()) {
    PlaceOutOfFlowItems(oof_children);
  }

  // Transfer track layout data to support masonry overlay in DevTools.
  GridLayoutData layout_data;
  layout_data.SetTrackCollection(
      std::make_unique<GridLayoutTrackCollection>(track_collection));
  container_builder_.TransferGridLayoutData(
      std::make_unique<GridLayoutData>(layout_data));

  // Account for border, scrollbar, and padding in the intrinsic block size.
  intrinsic_block_size_ += BorderScrollbarPadding().BlockSum();

  container_builder_.SetFragmentsTotalBlockSize(ComputeBlockSizeForFragment(
      GetConstraintSpace(), Node(), BorderPadding(), intrinsic_block_size_,
      container_builder_.InlineSize()));
  container_builder_.SetIntrinsicBlockSize(intrinsic_block_size_);
  container_builder_.HandleOofsAndSpecialDescendants();
  return container_builder_.ToBoxFragment();
}

namespace {

// TODO(almaher): Should we consolidate this with LayoutGridItemForMeasure()?
const LayoutResult* LayoutMasonryItemForMeasure(
    const GridItemData& masonry_item,
    const ConstraintSpace& constraint_space,
    SizingConstraint sizing_constraint) {
  const auto& node = masonry_item.node;

  // Disable side effects during MinMax computation to avoid potential "MinMax
  // after layout" crashes. This is not necessary during the layout pass, and
  // would have a negative impact on performance if used there.
  //
  // TODO(ikilpatrick): For subgrid, ideally we don't want to disable side
  // effects as it may impact performance significantly; this issue can be
  // avoided by introducing additional cache slots (see crbug.com/1272533).
  //
  // TODO(almaher): Handle submasonry here.
  std::optional<DisableLayoutSideEffectsScope> disable_side_effects;
  if (!node.GetLayoutBox()->NeedsLayout() &&
      sizing_constraint != SizingConstraint::kLayout) {
    disable_side_effects.emplace();
  }
  return node.Layout(constraint_space);
}

}  // namespace

// TODO(almaher): Item margins aren't being taken into account for placement.
void MasonryLayoutAlgorithm::PlaceMasonryItems(
    const GridLayoutTrackCollection& track_collection,
    GridItems& masonry_items,
    wtf_size_t start_offset,
    MasonryRunningPositions& running_positions,
    std::optional<SizingConstraint> sizing_constraint) {
  const auto& border_scrollbar_padding = BorderScrollbarPadding();
  const auto& container_space = GetConstraintSpace();
  const auto& style = Style();
  const bool is_for_layout = sizing_constraint == SizingConstraint::kLayout;

  const auto container_writing_direction =
      container_space.GetWritingDirection();
  const auto grid_axis_direction = track_collection.Direction();
  const bool is_for_columns = grid_axis_direction == kForColumns;
  const auto stacking_axis_gap = GridTrackSizingAlgorithm::CalculateGutterSize(
      style, masonry_available_size_, is_for_columns ? kForRows : kForColumns);

  // TODO(kschmi): Handle baselines in the stacking direction, depending on the
  // resolution for https://github.com/w3c/csswg-drafts/issues/9530.
  GridBaselineAccumulator baseline_accumulator(style.GetFontBaseline());

  for (auto& masonry_item : masonry_items) {
    // Find the definite span that the masonry items should be placed in.
    LayoutUnit max_position;
    GridSpan item_span =
        masonry_item.MaybeTranslateSpan(start_offset, grid_axis_direction);

    // Determine final placement for remaining indefinite spans.
    if (item_span.IsIndefinite()) {
      item_span = running_positions.GetFirstEligibleLine(
          item_span.IndefiniteSpanSize(), max_position);
      masonry_item.resolved_position.SetSpan(item_span, grid_axis_direction);
    } else {
      max_position = running_positions.GetMaxPositionForSpan(item_span);
    }

    masonry_item.ComputeSetIndices(track_collection);
    running_positions.UpdateAutoPlacementCursor(item_span.EndLine());

    // This item is ultimately placed below the maximum running position among
    // its spanned tracks. Account for border, scrollbar, and padding in the
    // offset of the item.
    LogicalRect containing_rect;
    is_for_columns ? containing_rect.offset.block_offset =
                         max_position + border_scrollbar_padding.block_start
                   : containing_rect.offset.inline_offset =
                         max_position + border_scrollbar_padding.inline_start;

    std::optional<LayoutUnit> fixed_inline_size = ([&]() {
      if (is_for_layout) {
        return std::optional<LayoutUnit>(std::nullopt);
      }

      // We need to compute the available space for the item if we are using it
      // to compute min/max content sizes.
      const ConstraintSpace space_for_measure =
          CreateConstraintSpaceForMeasure(masonry_item);
      const MinMaxSizes sizes = ComputeMinAndMaxContentContributionForSelf(
                                    masonry_item.node, space_for_measure)
                                    .sizes;

      return std::optional<LayoutUnit>(
          (sizing_constraint == SizingConstraint::kMinContent)
              ? sizes.min_size
              : sizes.max_size);
    })();

    const ConstraintSpace space =
        is_for_layout ? CreateConstraintSpaceForLayout(
                            masonry_item, track_collection, &containing_rect)
                      : CreateConstraintSpaceForMeasure(
                            masonry_item, /*needs_auto_track_size=*/false,
                            fixed_inline_size,
                            /*is_for_min_max_sizing=*/true);

    const auto& item_node = masonry_item.node;
    const auto& item_style = item_node.Style();
    const LayoutResult* result =
        is_for_layout ? result = item_node.Layout(space)
                      : LayoutMasonryItemForMeasure(masonry_item, space,
                                                    *sizing_constraint);

    const auto& physical_fragment =
        To<PhysicalBoxFragment>(result->GetPhysicalFragment());
    const LogicalBoxFragment fragment(container_writing_direction,
                                      physical_fragment);

    // TODO(celestepan): Account for extra margins from sub-masonry items.
    //
    // Adjust item's position in the track based on style. We only want offset
    // applied to the grid axis at the moment.
    //
    // TODO(celestepan): Update alignment logic if needed once we resolve on
    // https://github.com/w3c/csswg-drafts/issues/10275.
    const auto margins = ComputeMarginsFor(space, item_style, container_space);
    const auto inline_alignment =
        is_for_columns ? masonry_item.Alignment(kForColumns) : AxisEdge::kStart;
    const auto block_alignment =
        is_for_columns ? AxisEdge::kStart : masonry_item.Alignment(kForRows);
    containing_rect.offset += LogicalOffset(
        AlignmentOffset(containing_rect.size.inline_size, fragment.InlineSize(),
                        margins.inline_start, margins.inline_end,
                        /*baseline_offset=*/LayoutUnit(), inline_alignment,
                        masonry_item.IsOverflowSafe(kForColumns)),
        AlignmentOffset(containing_rect.size.block_size, fragment.BlockSize(),
                        margins.block_start, margins.block_end,
                        /*baseline_offset=*/LayoutUnit(), block_alignment,
                        masonry_item.IsOverflowSafe(kForRows)));

    // Update `running_positions` of the tracks that the items spans to include
    // the size of the item, the size of the gap in the stacking axis, and the
    // margin.
    auto new_running_position =
        max_position + stacking_axis_gap +
        (is_for_columns ? fragment.BlockSize() + margins.BlockSum()
                        : fragment.InlineSize() + margins.InlineSum());
    running_positions.UpdateRunningPositionsForSpan(item_span,
                                                    new_running_position);

    container_builder_.AddResult(*result, containing_rect.offset, margins);
    baseline_accumulator.Accumulate(masonry_item, fragment,
                                    containing_rect.offset.block_offset);
  }
  if (is_for_columns) {
    // Remove last gap that was added, since there is no item after it.
    intrinsic_block_size_ =
        running_positions.GetMaxPositionForSpan(
            GridSpan::TranslatedDefiniteGridSpan(
                /*start_line=*/0,
                /*end_line=*/track_collection.EndLineOfImplicitGrid())) -
        stacking_axis_gap;
  } else {
    // If the stacking axis is the inline axis, add the size of the tracks to
    // `intrinsic_block_size_`.
    intrinsic_block_size_ = track_collection.CalculateSetSpanSize();
  }

  // Propagate the baselines to the container.
  if (auto first_baseline = baseline_accumulator.FirstBaseline()) {
    container_builder_.SetFirstBaseline(*first_baseline);
  }
  if (auto last_baseline = baseline_accumulator.LastBaseline()) {
    container_builder_.SetLastBaseline(*last_baseline);
  }
}

void MasonryLayoutAlgorithm::PlaceOutOfFlowItems(
    HeapVector<Member<LayoutBox>>& oof_children) {
  const auto& container_style = Style();

  // TODO(kschmi): This doesn't match grid, which passes in the block size.
  const LogicalSize total_fragment_size = {container_builder_.InlineSize(),
                                           LayoutUnit()};

  for (LayoutBox* oof_child : oof_children) {
    GridItemData* out_of_flow_item = MakeGarbageCollected<GridItemData>(
        BlockNode(oof_child), container_style);
    DCHECK(out_of_flow_item->IsOutOfFlow());

    // TODO(kschmi): Apply grid-area containing rect.
    auto child_offset = BorderScrollbarPadding().StartOffset();

    // TODO(kschmi): Apply actual alignment.
    LogicalStaticPosition::InlineEdge inline_edge =
        LogicalStaticPosition::kInlineStart;
    LogicalStaticPosition::BlockEdge block_edge =
        LogicalStaticPosition::kBlockStart;

    // TODO(kschmi): Handle fragmentation.
    container_builder_.AddOutOfFlowChildCandidate(
        out_of_flow_item->node, child_offset, inline_edge, block_edge);
  }
}

GridItems MasonryLayoutAlgorithm::BuildVirtualMasonryItems(
    const GridLineResolver& line_resolver,
    const GridItems& masonry_items,
    const bool needs_auto_track_size,
    SizingConstraint sizing_constraint,
    const wtf_size_t auto_repetition_count,
    wtf_size_t& start_offset) const {
  const auto& style = Style();
  const auto grid_axis_direction = style.MasonryTrackSizingDirection();
  const bool is_for_columns = grid_axis_direction == kForColumns;

  const LayoutUnit grid_axis_gap =
      GridTrackSizingAlgorithm::CalculateGutterSize(
          style, masonry_available_size_,
          is_for_columns ? kForColumns : kForRows);

  wtf_size_t max_end_line;
  GridItems virtual_items;

  // If there is an auto-fit track definition, store what tracks it spans.
  const GridTrackList& track_list =
      is_for_columns ? style.GridTemplateColumns().GetTrackList()
                     : style.GridTemplateRows().GetTrackList();
  GridSpan auto_fit_span = GridSpan::IndefiniteGridSpan();
  if (!needs_auto_track_size && track_list.HasAutoRepeater() &&
      track_list.RepeatType(track_list.AutoRepeatTrackIndex()) ==
          GridTrackRepeater::RepeatType::kAutoFit) {
    auto_fit_span = GridSpan::TranslatedDefiniteGridSpan(
        track_list.TrackCountBeforeAutoRepeat(),
        track_list.TrackCountBeforeAutoRepeat() + auto_repetition_count);
  }

  wtf_size_t unplaced_item_span_count = 0;

  for (const auto& [group_items, group_properties] :
       Node().CollectItemGroups(line_resolver, masonry_items, max_end_line,
                                start_offset, unplaced_item_span_count)) {
    auto* virtual_item = MakeGarbageCollected<GridItemData>();

    GridSpan span = group_properties.Span();
    wtf_size_t span_size = span.SpanSize();
    CHECK_GT(span_size, 0u);

    for (const Member<GridItemData>& group_item : group_items) {
      const GridItemData& item_data = *group_item;
      const BlockNode& item_node = item_data.node;
      const auto space =
          CreateConstraintSpaceForMeasure(item_data, needs_auto_track_size);
      const ComputedStyle& item_style = item_node.Style();

      bool is_parallel = IsParallelWritingMode(
          item_style.GetWritingMode(), GetConstraintSpace().GetWritingMode());
      bool use_item_inline_contribution =
          is_for_columns ? is_parallel : !is_parallel;
      // TODO(almaher): Subgrids have extra margin to handle unique gap sizes.
      // This requires access to the subgrid track collection, where that extra
      // margin is accumulated.
      const BoxStrut margins =
          ComputeMarginsFor(space, item_style, GetConstraintSpace());
      const LayoutUnit margins_sum =
          is_for_columns ? margins.InlineSum() : margins.BlockSum();

      if (use_item_inline_contribution) {
        MinMaxSizes min_max_sizes =
            ComputeMinAndMaxContentContributionForSelf(item_node, space).sizes;
        min_max_sizes += margins_sum;

        // We have a repeat() track definition with an auto sized track(s). The
        // current track sizing pass is used to find the track size to apply
        // to the auto sized track(s). If the current item spans more than
        // one track, treat it as if it spans one track per the intrinsic
        // tracks and repeat algorithm [1].
        //
        // [1] https://www.w3.org/TR/css-grid-3/#masonry-intrinsic-repeat
        if (needs_auto_track_size && span_size > 1) {
          LayoutUnit total_gap_spanned = grid_axis_gap * (span_size - 1);
          min_max_sizes -= total_gap_spanned;
          min_max_sizes /= LayoutUnit(span_size);
        }

        virtual_item->EncompassContributionSize(min_max_sizes);
      } else {
        LayoutUnit block_contribution =
            ComputeMasonryItemBlockContribution(
                grid_axis_direction, sizing_constraint, space, &item_data,
                needs_auto_track_size) +
            margins_sum;

        // We have a repeat() track definition with an auto sized track(s). The
        // current track sizing pass is used to find the track size to apply
        // to the auto sized track(s). If the current item spans more than
        // one track, treat it as if it spans one track per the intrinsic
        // tracks and repeat algorithm [1].
        //
        // [1] https://www.w3.org/TR/css-grid-3/#masonry-intrinsic-repeat
        if (needs_auto_track_size && span_size > 1) {
          LayoutUnit total_gap_spanned = grid_axis_gap * (span_size - 1);
          block_contribution -= total_gap_spanned;
          block_contribution /= span_size;
        }

        virtual_item->EncompassContributionSize(block_contribution);
      }
    }

    // If `needs_auto_track_size` is true, that means we have a repeat() track
    // definition with an auto sized track(s). The current track sizing pass is
    // used to find the track size to apply to the auto sized track(s). Ignore
    // item placement as part of this pass, and apply all items in every
    // position, regardless of explicit placement [1].
    //
    // [1] https://www.w3.org/TR/css-grid-3/#masonry-intrinsic-repeat
    if (span.IsIndefinite() || needs_auto_track_size) {
      // For groups of items that are auto-placed, we need to create copies of
      // the virtual item and place them at each possible start line. At the end
      // of the loop below, `span` will be located at the last start line, which
      // should be the position of the last copy appended to `virtual_items`.
      if (needs_auto_track_size) {
        span = GridSpan::TranslatedDefiniteGridSpan(0, 1);
      } else {
        span =
            GridSpan::TranslatedDefiniteGridSpan(0, span.IndefiniteSpanSize());
      }

      while (span.EndLine() < max_end_line) {
        auto* item_copy = MakeGarbageCollected<GridItemData>(*virtual_item);
        item_copy->resolved_position.SetSpan(span, grid_axis_direction);
        virtual_items.Append(std::move(item_copy));

        // `Translate` will move the span to the start and end of the next line,
        // allowing us to "slide" over the entire implicit grid.
        span.Translate(1);

        // Per the auto-fit heuristic, don't add auto placed items to tracks
        // within the auto-fit range that are greater than the total span count
        // of auto placed items.
        //
        // https://drafts.csswg.org/css-grid-3/#repeat-auto-fit
        if (!auto_fit_span.IsIndefinite()) {
          while (span.Intersects(auto_fit_span) &&
                 span.EndLine() > unplaced_item_span_count) {
            span.Translate(1);
          }
        }
      }
    }

    DCHECK(span.IsTranslatedDefinite());
    if (span.EndLine() <= max_end_line) {
      virtual_item->resolved_position.SetSpan(span, grid_axis_direction);
      virtual_items.Append(virtual_item);
    }
  }
  return virtual_items;
}

namespace {

// TODO(almaher): Eventually look into consolidating repeated code with
// GridLayoutAlgorithm::ContributionSizeForGridItem().
LayoutUnit ContributionSizeForVirtualItem(
    GridItemContributionType contribution_type,
    GridItemData* virtual_item) {
  DCHECK(virtual_item);
  DCHECK(virtual_item->contribution_sizes);

  switch (contribution_type) {
    // TODO(almaher): Do we need to do something special for
    // kForIntrinsicMinimums (see
    // GridLayoutAlgorithm::ContributionSizeForGridItem())?
    case GridItemContributionType::kForContentBasedMinimums:
    case GridItemContributionType::kForIntrinsicMaximums:
    case GridItemContributionType::kForIntrinsicMinimums:
      return virtual_item->contribution_sizes->min_size;
    case GridItemContributionType::kForMaxContentMaximums:
    case GridItemContributionType::kForMaxContentMinimums:
      return virtual_item->contribution_sizes->max_size;
    case GridItemContributionType::kForFreeSpace:
      NOTREACHED() << "`kForFreeSpace` should only be used to distribute extra "
                      "space in maximize tracks and stretch auto tracks steps.";
  }
}

}  // namespace

// TODO(almaher): Eventually look into consolidating repeated code with
// GridLayoutAlgorithm::ContributionSizeForGridItem().
LayoutUnit MasonryLayoutAlgorithm::ComputeMasonryItemBlockContribution(
    GridTrackSizingDirection track_direction,
    SizingConstraint sizing_constraint,
    const ConstraintSpace space_for_measure,
    const GridItemData* masonry_item,
    const bool needs_auto_track_size) const {
  DCHECK(masonry_item);

  // TODO(ikilpatrick): We'll need to record if any child used an indefinite
  // size for its contribution, such that we can then do the 2nd pass on the
  // track-sizing algorithm.

  // TODO(almaher): Handle baseline logic here.

  // TODO(ikilpatrick): This should try and skip layout when possible. Notes:
  //  - We'll need to do a full layout for tables.
  //  - We'll need special logic for replaced elements.
  //  - We'll need to respect the aspect-ratio when appropriate.

  // TODO(almaher): Properly handle submasonry here.

  const LayoutResult* result = nullptr;
  if (space_for_measure.AvailableSize().inline_size == kIndefiniteSize) {
    // If we are orthogonal virtual item, resolving against an indefinite
    // size, set our inline size to our min-content or max-content contribution
    // size depending on the `sizing_contraint`.
    const MinMaxSizes sizes = ComputeMinAndMaxContentContributionForSelf(
                                  masonry_item->node, space_for_measure)
                                  .sizes;
    const auto fallback_space = CreateConstraintSpaceForMeasure(
        *masonry_item, needs_auto_track_size,
        /*opt_fixed_inline_size=*/sizing_constraint ==
                SizingConstraint::kMinContent
            ? sizes.min_size
            : sizes.max_size);

    result = LayoutMasonryItemForMeasure(*masonry_item, fallback_space,
                                         sizing_constraint);
  } else {
    result = LayoutMasonryItemForMeasure(*masonry_item, space_for_measure,
                                         sizing_constraint);
  }

  LogicalBoxFragment baseline_fragment(
      masonry_item->BaselineWritingDirection(track_direction),
      To<PhysicalBoxFragment>(result->GetPhysicalFragment()));

  // TODO(almaher): Properly handle baselines here.

  return baseline_fragment.BlockSize();
}

GridSizingTrackCollection MasonryLayoutAlgorithm::ComputeGridAxisTracks(
    const SizingConstraint sizing_constraint,
    std::optional<LayoutUnit> auto_repeat_track_size,
    GridItems& masonry_items,
    Vector<wtf_size_t>& collapsed_track_indexes,
    wtf_size_t& start_offset,
    bool& needs_auto_track_size,
    HeapVector<Member<LayoutBox>>* opt_oof_children) const {
  start_offset = 0;
  needs_auto_track_size = false;

  const GridLineResolver line_resolver(
      Style(), ComputeAutomaticRepetitions(auto_repeat_track_size,
                                           needs_auto_track_size));
  const auto& node = Node();
  if (masonry_items.IsEmpty()) {
    masonry_items = node.ConstructMasonryItems(line_resolver, opt_oof_children);
  } else {
    // If `masonry_items` is not empty, that means that we are in
    // a second track sizing pass required for intrinsic tracks within
    // a repeat() track definition. Don't construct the masonry items
    // from scratch. Rather, adjust their spans based on the updated
    // `line_resolver`.
    node.AdjustMasonryItemSpans(masonry_items, line_resolver);
  }

  return BuildGridAxisTracks(line_resolver, masonry_items, sizing_constraint,
                             needs_auto_track_size, collapsed_track_indexes,
                             start_offset);
}

GridSizingTrackCollection MasonryLayoutAlgorithm::BuildGridAxisTracks(
    const GridLineResolver& line_resolver,
    const GridItems& masonry_items,
    SizingConstraint sizing_constraint,
    bool& needs_auto_track_size,
    Vector<wtf_size_t>& collapsed_track_indexes,
    wtf_size_t& start_offset) const {
  const auto& style = Style();
  const auto grid_axis_direction = style.MasonryTrackSizingDirection();
  GridItems virtual_items = BuildVirtualMasonryItems(
      line_resolver, masonry_items, needs_auto_track_size, sizing_constraint,
      line_resolver.AutoRepetitions(grid_axis_direction), start_offset);

  auto BuildRanges = [&]() {
    GridRangeBuilder range_builder(
        style, grid_axis_direction,
        line_resolver.AutoRepetitions(grid_axis_direction), start_offset);

    for (auto& virtual_item : virtual_items) {
      auto& range_indices = virtual_item.RangeIndices(grid_axis_direction);
      const auto& span = virtual_item.Span(grid_axis_direction);

      range_builder.EnsureTrackCoverage(span.StartLine(), span.IntegerSpan(),
                                        &range_indices.begin,
                                        &range_indices.end);
    }
    return range_builder.FinalizeRanges(needs_auto_track_size,
                                        &collapsed_track_indexes);
  };

  GridSizingTrackCollection track_collection(BuildRanges(),
                                             grid_axis_direction);
  track_collection.BuildSets(style, masonry_available_size_);

  if (track_collection.HasNonDefiniteTrack()) {
    GridTrackSizingAlgorithm::CacheGridItemsProperties(track_collection,
                                                       &virtual_items);

    const GridTrackSizingAlgorithm track_sizing_algorithm(
        style, masonry_available_size_, masonry_min_available_size_,
        sizing_constraint);

    track_sizing_algorithm.ComputeUsedTrackSizes(
        ContributionSizeForVirtualItem, &track_collection, &virtual_items,
        needs_auto_track_size);
  }

  auto first_set_geometry = GridTrackSizingAlgorithm::ComputeFirstSetGeometry(
      track_collection, style, masonry_available_size_,
      BorderScrollbarPadding());

  track_collection.FinalizeSetsGeometry(first_set_geometry.start_offset,
                                        first_set_geometry.gutter_size);
  return track_collection;
}

// https://drafts.csswg.org/css-grid-2/#auto-repeat
wtf_size_t MasonryLayoutAlgorithm::ComputeAutomaticRepetitions(
    std::optional<LayoutUnit> auto_repeat_track_size,
    bool& needs_auto_track_size) const {
  const ComputedStyle& style = Style();
  GridTrackSizingDirection masonry_track_sizing_direction =
      style.MasonryTrackSizingDirection();
  const bool is_for_columns = masonry_track_sizing_direction == kForColumns;

  const GridTrackList& track_list =
      is_for_columns ? style.GridTemplateColumns().GetTrackList()
                     : style.GridTemplateRows().GetTrackList();

  if (!track_list.HasAutoRepeater()) {
    return 0;
  }

  // To determine the auto track size within a repeat, we need to expand
  // them out once, and run track sizing to get the actual size [1]. Then we
  // will run this again with the actual auto track size within a final track
  // sizing pass based on this size.
  //
  // https://www.w3.org/TR/css-grid-3/#masonry-intrinsic-repeat
  if (track_list.HasAutoSizedRepeater() && !auto_repeat_track_size) {
    CHECK(!needs_auto_track_size);
    needs_auto_track_size = true;
    return 1;
  }

  // TODO(almaher): We will need special computation of automatic repetitions
  // for submasonry (see ComputeAutomaticRepetitionsForSubgrid()). Once this is
  // supported, we can move more of this method to the helper in
  // grid_layout_utils.cc.

  const LayoutUnit gutter_size = GridTrackSizingAlgorithm::CalculateGutterSize(
      style, masonry_available_size_, masonry_track_sizing_direction);

  return CalculateAutomaticRepetitions(
      track_list, gutter_size,
      is_for_columns ? masonry_available_size_.inline_size
                     : masonry_available_size_.block_size,
      is_for_columns ? masonry_min_available_size_.inline_size
                     : masonry_min_available_size_.block_size,
      is_for_columns ? masonry_max_available_size_.inline_size
                     : masonry_max_available_size_.block_size,
      auto_repeat_track_size);
}

ConstraintSpace MasonryLayoutAlgorithm::CreateConstraintSpace(
    const GridItemData& masonry_item,
    const LogicalSize& containing_size,
    const LogicalSize& fixed_available_size,
    LayoutResultCacheSlot result_cache_slot,
    const std::optional<LogicalSize>& opt_percentage_resolution_size) const {
  ConstraintSpaceBuilder builder(
      GetConstraintSpace(), masonry_item.node.Style().GetWritingDirection(),
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

  builder.SetPercentageResolutionSize(
      opt_percentage_resolution_size ? opt_percentage_resolution_size.value()
                                     : containing_size);
  builder.SetInlineAutoBehavior(masonry_item.column_auto_behavior);
  builder.SetBlockAutoBehavior(masonry_item.row_auto_behavior);
  return builder.ToConstraintSpace();
}

// TODO(celestepan): If item-direction is row, we should not be returning an
// indefinite inline size. Discussions are still ongoing on if we want to always
// return min/max-content or inherit from the parent.
ConstraintSpace MasonryLayoutAlgorithm::CreateConstraintSpaceForLayout(
    const GridItemData& masonry_item,
    const GridLayoutTrackCollection& track_collection,
    LogicalRect* containing_rect) const {
  const bool is_for_columns = track_collection.Direction() == kForColumns;

  auto containing_size = masonry_available_size_;
  auto& grid_axis_size =
      is_for_columns ? containing_size.inline_size : containing_size.block_size;

  LayoutUnit start_offset;
  grid_axis_size =
      masonry_item.CalculateAvailableSize(track_collection, &start_offset);

  if (containing_rect) {
    is_for_columns ? containing_rect->offset.inline_offset = start_offset
                   : containing_rect->offset.block_offset = start_offset;
    containing_rect->size = containing_size;
  }

  // TODO(almaher): Will likely need special fixed available size handling for
  // submasonry.
  return CreateConstraintSpace(masonry_item, containing_size,
                               /*fixed_available_size=*/kIndefiniteLogicalSize,
                               LayoutResultCacheSlot::kLayout);
}

ConstraintSpace MasonryLayoutAlgorithm::CreateConstraintSpaceForMeasure(
    const GridItemData& masonry_item,
    const bool needs_auto_track_size,
    std::optional<LayoutUnit> opt_fixed_inline_size,
    bool is_for_min_max_sizing) const {
  LogicalSize containing_size = masonry_available_size_;
  const auto writing_mode = GetConstraintSpace().GetWritingMode();
  const auto grid_axis_direction = Style().MasonryTrackSizingDirection();

  // Check against columns, as opposed to whether the item is parallel, because
  // the ConstraintSpaceBuilder takes care of handling orthogonal items.
  if (grid_axis_direction == kForColumns) {
    containing_size.inline_size = kIndefiniteSize;
  } else {
    if (is_for_min_max_sizing) {
      // In the row direction, we use this method to create a space for
      // measuring the min/max-content of the item, so we have to set the inline
      // size as indefinite to allow for text flow.
      containing_size.inline_size = kIndefiniteSize;
    }
    containing_size.block_size = kIndefiniteSize;
  }

  // TODO(almaher): Do we need to do something special here for subgrid like
  // GridLayoutAlgorithm::CreateConstraintSpaceForMeasure()?
  LogicalSize fixed_available_size = kIndefiniteLogicalSize;

  if (opt_fixed_inline_size) {
    const auto item_writing_mode = masonry_item.node.Style().GetWritingMode();
    if (IsParallelWritingMode(item_writing_mode, writing_mode)) {
      DCHECK_EQ(fixed_available_size.inline_size, kIndefiniteSize);
      fixed_available_size.inline_size = *opt_fixed_inline_size;
    } else {
      DCHECK_EQ(fixed_available_size.block_size, kIndefiniteSize);
      fixed_available_size.block_size = *opt_fixed_inline_size;
    }
  }

  // If we are determining the track size of an auto track within an auto
  // repeat(), we resolve percentages against the container.
  std::optional<LogicalSize> percentage_resolution_size =
      needs_auto_track_size
          ? std::optional<LogicalSize>(masonry_available_size_)
          : std::nullopt;

  return CreateConstraintSpace(
      masonry_item, containing_size, fixed_available_size,
      LayoutResultCacheSlot::kMeasure, percentage_resolution_size);
}

}  // namespace blink
