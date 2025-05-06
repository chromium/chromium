// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#include "third_party/blink/renderer/core/layout/masonry/masonry_layout_algorithm.h"

#include "base/notreached.h"
#include "third_party/blink/renderer/core/layout/grid/grid_item.h"
#include "third_party/blink/renderer/core/layout/grid/grid_track_collection.h"
#include "third_party/blink/renderer/core/layout/grid/grid_track_sizing_algorithm.h"
#include "third_party/blink/renderer/core/layout/logical_box_fragment.h"
#include "third_party/blink/renderer/core/layout/masonry/masonry_running_positions.h"

namespace blink {

MasonryLayoutAlgorithm::MasonryLayoutAlgorithm(
    const LayoutAlgorithmParams& params)
    : LayoutAlgorithm(params) {
  DCHECK(params.space.IsNewFormattingContext());
}

MinMaxSizesResult MasonryLayoutAlgorithm::ComputeMinMaxSizes(
    const MinMaxSizesFloatInput&) {
  const GridLineResolver line_resolver(Style(), ComputeAutomaticRepetitions());

  auto ComputeIntrinsicInlineSize = [&](SizingConstraint sizing_constraint) {
    wtf_size_t start_offset;
    const auto track_collection =
        BuildGridAxisTracks(line_resolver, sizing_constraint, start_offset);
    return track_collection.CalculateSetSpanSize();
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
  const GridLineResolver line_resolver(Style(), ComputeAutomaticRepetitions());

  wtf_size_t start_offset;
  const auto track_collection = BuildGridAxisTracks(
      line_resolver, SizingConstraint::kLayout, start_offset);

  const auto& node = Node();
  auto masonry_items = node.ConstructMasonryItems(line_resolver, start_offset);
  PlaceMasonryItems(track_collection, masonry_items);

  container_builder_.SetFragmentsTotalBlockSize(ComputeBlockSizeForFragment(
      GetConstraintSpace(), node, BorderPadding(), intrinsic_block_size_,
      container_builder_.InlineSize()));
  container_builder_.SetIntrinsicBlockSize(intrinsic_block_size_);
  return container_builder_.ToBoxFragment();
}

namespace {

LayoutUnit CalculateAlignmentOffset(AxisEdge alignment, LayoutUnit free_space) {
  if (!free_space) {
    return LayoutUnit();
  }

  switch (alignment) {
    case AxisEdge::kCenter:
      return free_space / 2;
    case AxisEdge::kEnd:
      return free_space;
    case AxisEdge::kStart:
      return LayoutUnit();
    default:
      NOTREACHED();
  }
}

}  // namespace

void MasonryLayoutAlgorithm::PlaceMasonryItems(
    const GridLayoutTrackCollection& track_collection,
    GridItems& masonry_items) {
  const auto& available_size = ChildAvailableSize();
  const auto& border_scrollbar_padding = BorderScrollbarPadding();
  const auto& container_space = GetConstraintSpace();
  const auto& style = Style();

  const auto container_writing_direction =
      container_space.GetWritingDirection();
  const auto grid_axis_direction = track_collection.Direction();
  const bool is_for_columns = grid_axis_direction == kForColumns;

  // Initialize data structure to keep track of running positions, where the
  // initial running positions are set to border, scrollbar, and padding.
  MasonryRunningPositions running_positions(
      /*track_count=*/track_collection.EndLineOfImplicitGrid(),
      /*initial_running_position=*/
      is_for_columns ? border_scrollbar_padding.block_start
                     : border_scrollbar_padding.inline_start,
      ResolveItemToleranceForMasonry(style, available_size));

  const auto stacking_axis_gap = GridTrackSizingAlgorithm::CalculateGutterSize(
      style, available_size, is_for_columns ? kForRows : kForColumns);

  for (auto& masonry_item : masonry_items) {
    // Find the definite span that the masonry items should be placed in.
    auto item_span = masonry_item.Span(grid_axis_direction);
    LayoutUnit max_position;

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
    // its spanned tracks.
    LogicalRect containing_rect;
    is_for_columns ? containing_rect.offset.block_offset = max_position
                   : containing_rect.offset.inline_offset = max_position;

    const auto space = CreateConstraintSpaceForLayout(
        masonry_item, track_collection, &containing_rect);

    const auto& item_node = masonry_item.node;
    const auto* result = item_node.Layout(space);
    const auto& physical_fragment =
        To<PhysicalBoxFragment>(result->GetPhysicalFragment());
    const LogicalBoxFragment fragment(container_writing_direction,
                                      physical_fragment);

    // Adjust item's position in the track based on style.
    auto FreeSpace = [&]() -> LayoutUnit {
      const auto free_space =
          is_for_columns
              ? containing_rect.size.inline_size - fragment.InlineSize()
              : containing_rect.size.block_size - fragment.BlockSize();

      // If overflow is 'safe', make sure we don't overflow the 'start' edge
      // (potentially causing some data loss as the overflow is unreachable).
      return masonry_item.IsOverflowSafe(grid_axis_direction)
                 ? free_space.ClampNegativeToZero()
                 : free_space;
    };
    const auto offset = CalculateAlignmentOffset(
        masonry_item.Alignment(grid_axis_direction), FreeSpace());
    (is_for_columns ? containing_rect.offset.inline_offset
                    : containing_rect.offset.block_offset) += offset;

    // Update `running_positions` of the tracks that the items spans to include
    // the size of the item + the size of the gap in the stacking axis.
    auto new_running_position =
        max_position + stacking_axis_gap +
        (is_for_columns ? fragment.BlockSize() : fragment.InlineSize());
    running_positions.UpdateRunningPositionsForSpan(item_span,
                                                    new_running_position);

    container_builder_.AddResult(
        *result, containing_rect.offset,
        ComputeMarginsFor(space, item_node.Style(), container_space));
  }
  if (is_for_columns) {
    // Remove last gap that was added, since there is no item after it.
    intrinsic_block_size_ =
        running_positions.GetMaxPositionForSpan(
            GridSpan::TranslatedDefiniteGridSpan(
                /*start_line=*/0,
                /*end_line=*/track_collection.EndLineOfImplicitGrid())) +
        border_scrollbar_padding.block_end - stacking_axis_gap;
  } else {
    // Set `intrinsic_block_size_` to be the size of the tracks.
    intrinsic_block_size_ = track_collection.CalculateSetSpanSize() +
                            border_scrollbar_padding.BlockSum();
  }
}

GridItems MasonryLayoutAlgorithm::BuildVirtualMasonryItems(
    const GridLineResolver& line_resolver,
    wtf_size_t& start_offset) const {
  const auto& style = Style();
  const auto grid_axis_direction = style.MasonryTrackSizingDirection();

  wtf_size_t max_end_line;
  GridItems virtual_items;

  for (const auto& [group_items, group_properties] :
       Node().CollectItemGroups(line_resolver, max_end_line, start_offset)) {
    auto* virtual_item = MakeGarbageCollected<GridItemData>();
    auto span = group_properties.Span();

    for (const auto& item_node : group_items) {
      const auto space =
          CreateConstraintSpaceForMeasure(GridItemData(item_node, style));
      virtual_item->EncompassContributionSizes(
          ComputeMinAndMaxContentContributionForSelf(item_node, space).sizes);
    }

    if (span.IsIndefinite()) {
      // For groups of items that are auto-placed, we need to create copies of
      // the virtual item and place them at each possible start line. At the end
      // of the loop below, `span` will be located at the last start line, which
      // should be the position of the last copy appended to `virtual_items`.
      span = GridSpan::TranslatedDefiniteGridSpan(0, span.IndefiniteSpanSize());

      while (span.EndLine() < max_end_line) {
        auto* item_copy = MakeGarbageCollected<GridItemData>(*virtual_item);
        item_copy->resolved_position.SetSpan(span, grid_axis_direction);
        virtual_items.Append(std::move(item_copy));

        // `Translate` will move the span to the start and end of the next line,
        // allowing us to "slide" over the entire implicit grid.
        span.Translate(1);
      }
    }

    DCHECK(span.IsTranslatedDefinite());
    virtual_item->resolved_position.SetSpan(span, grid_axis_direction);
    virtual_items.Append(virtual_item);
  }
  return virtual_items;
}

namespace {

LayoutUnit ContributionSizeForVirtualItem(
    GridItemContributionType contribution_type,
    GridItemData* virtual_item) {
  DCHECK(virtual_item);
  DCHECK(virtual_item->contribution_sizes);

  switch (contribution_type) {
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

GridSizingTrackCollection MasonryLayoutAlgorithm::BuildGridAxisTracks(
    const GridLineResolver& line_resolver,
    SizingConstraint sizing_constraint,
    wtf_size_t& start_offset) const {
  const auto& style = Style();
  const auto grid_axis_direction = style.MasonryTrackSizingDirection();
  auto virtual_items = BuildVirtualMasonryItems(line_resolver, start_offset);

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
    return range_builder.FinalizeRanges();
  };

  const auto& available_size = ChildAvailableSize();
  GridSizingTrackCollection track_collection(BuildRanges(),
                                             grid_axis_direction);
  track_collection.BuildSets(style, available_size);

  if (track_collection.HasNonDefiniteTrack()) {
    GridTrackSizingAlgorithm::CacheGridItemsProperties(track_collection,
                                                       &virtual_items);

    // TODO(ethavar): Compute the min available size and use it here.
    const GridTrackSizingAlgorithm track_sizing_algorithm(
        style, available_size, /*container_min_available_size=*/LogicalSize(),
        sizing_constraint);

    track_sizing_algorithm.ComputeUsedTrackSizes(
        ContributionSizeForVirtualItem, &track_collection, &virtual_items);
  }

  auto first_set_geometry = GridTrackSizingAlgorithm::ComputeFirstSetGeometry(
      track_collection, style, available_size, BorderScrollbarPadding());

  track_collection.FinalizeSetsGeometry(first_set_geometry.start_offset,
                                        first_set_geometry.gutter_size);
  return track_collection;
}

wtf_size_t MasonryLayoutAlgorithm::ComputeAutomaticRepetitions() const {
  // TODO(ethavar): Compute the actual number of automatic repetitions.
  return 1;
}

ConstraintSpace MasonryLayoutAlgorithm::CreateConstraintSpace(
    const GridItemData& masonry_item,
    const LogicalSize& containing_size,
    LayoutResultCacheSlot result_cache_slot) const {
  ConstraintSpaceBuilder builder(
      GetConstraintSpace(), masonry_item.node.Style().GetWritingDirection(),
      /*is_new_fc=*/true, /*adjust_inline_size_if_needed=*/false);

  builder.SetCacheSlot(result_cache_slot);
  builder.SetIsPaintedAtomically(true);

  builder.SetAvailableSize(containing_size);
  builder.SetPercentageResolutionSize(containing_size);
  builder.SetInlineAutoBehavior(masonry_item.column_auto_behavior);
  builder.SetBlockAutoBehavior(masonry_item.row_auto_behavior);
  return builder.ToConstraintSpace();
}

ConstraintSpace MasonryLayoutAlgorithm::CreateConstraintSpaceForLayout(
    const GridItemData& masonry_item,
    const GridLayoutTrackCollection& track_collection,
    LogicalRect* containing_rect) const {
  const bool is_for_columns = track_collection.Direction() == kForColumns;

  auto containing_size = ChildAvailableSize();
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

  return CreateConstraintSpace(masonry_item, containing_size,
                               LayoutResultCacheSlot::kLayout);
}

ConstraintSpace MasonryLayoutAlgorithm::CreateConstraintSpaceForMeasure(
    const GridItemData& masonry_item) const {
  auto containing_size = ChildAvailableSize();
  // This method is only used for passing a constraint space into
  // `ComputeMinAndMaxContentContributionForSelf`, and the inline size should
  // always be indefinite in that case to allow for text flow.
  containing_size.inline_size = kIndefiniteSize;
  return CreateConstraintSpace(masonry_item, containing_size,
                               LayoutResultCacheSlot::kMeasure);
}

}  // namespace blink
