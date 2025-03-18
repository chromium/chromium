// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#include "third_party/blink/renderer/core/layout/masonry/masonry_layout_algorithm.h"

#include "third_party/blink/renderer/core/layout/grid/grid_item.h"
#include "third_party/blink/renderer/core/layout/grid/grid_track_collection.h"
#include "third_party/blink/renderer/core/layout/grid/grid_track_sizing_algorithm.h"
#include "third_party/blink/renderer/core/layout/logical_box_fragment.h"

namespace blink {

MasonryLayoutAlgorithm::MasonryLayoutAlgorithm(
    const LayoutAlgorithmParams& params)
    : LayoutAlgorithm(params) {
  DCHECK(params.space.IsNewFormattingContext());
}

MinMaxSizesResult MasonryLayoutAlgorithm::ComputeMinMaxSizes(
    const MinMaxSizesFloatInput&) {
  return {MinMaxSizes(), /*depends_on_block_constraints=*/false};
}

const LayoutResult* MasonryLayoutAlgorithm::Layout() {
  const GridLineResolver line_resolver(Style(), ComputeAutomaticRepetitions());

  wtf_size_t start_offset;
  const auto track_collection = BuildGridAxisTracks(
      line_resolver, SizingConstraint::kLayout, start_offset);
  const auto masonry_items =
      BuildMasonryItems(line_resolver, track_collection, start_offset);

  LayoutUnit intrinsic_block_size;
  PlaceMasonryItems(masonry_items, track_collection, &intrinsic_block_size);

  // TODO(ethavar): Compute the actual block size for the fragment.
  container_builder_.SetFragmentsTotalBlockSize(intrinsic_block_size);
  return container_builder_.ToBoxFragment();
}

void MasonryLayoutAlgorithm::PlaceMasonryItems(
    const GridItems& masonry_items,
    const GridLayoutTrackCollection& track_collection,
    LayoutUnit* intrinsic_block_size) {
  DCHECK(intrinsic_block_size);

  const auto& container_space = GetConstraintSpace();
  const auto container_writing_direction =
      container_space.GetWritingDirection();

  for (const auto& masonry_item : masonry_items) {
    const auto& item_node = masonry_item.node;

    LogicalRect containing_rect;
    const auto space = CreateConstraintSpaceForLayout(
        masonry_item, track_collection, &containing_rect);

    const auto* result = item_node.Layout(space);
    const auto& physical_fragment =
        To<PhysicalBoxFragment>(result->GetPhysicalFragment());
    const LogicalBoxFragment fragment(container_writing_direction,
                                      physical_fragment);

    *intrinsic_block_size =
        std::max(*intrinsic_block_size, fragment.BlockSize());

    container_builder_.AddResult(
        *result, containing_rect.offset,
        ComputeMarginsFor(space, item_node.Style(), container_space));
  }
  *intrinsic_block_size += BorderScrollbarPadding().BlockSum();
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

    if (span.IsTranslatedDefinite()) {
      // For groups of items that are explicitly placed, we only need to add a
      // single virtual masonry item within the specified span.
      virtual_item->resolved_position.SetSpan(span, grid_axis_direction);
      virtual_items.Append(virtual_item);
      continue;
    }

    DCHECK(span.IsIndefinite());
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
        style, ChildAvailableSize(), ChildAvailableSize(), sizing_constraint);

    track_sizing_algorithm.ComputeUsedTrackSizes(
        ContributionSizeForVirtualItem, &track_collection, &virtual_items);
  }

  auto first_set_geometry = GridTrackSizingAlgorithm::ComputeFirstSetGeometry(
      track_collection, style, available_size, BorderScrollbarPadding());

  track_collection.FinalizeSetsGeometry(first_set_geometry.start_offset,
                                        first_set_geometry.gutter_size);
  return track_collection;
}

GridItems MasonryLayoutAlgorithm::BuildMasonryItems(
    const GridLineResolver& line_resolver,
    const GridLayoutTrackCollection& track_collection,
    wtf_size_t start_offset) const {
  auto masonry_items =
      Node().ConstructMasonryItems(line_resolver, start_offset);

  // TODO(celestepan): Implement placement algorithm here.

  for (auto& masonry_item : masonry_items) {
    if (masonry_item.Span(track_collection.Direction()).IsIndefinite()) {
      // TODO(ethavar): Currently we need to skip over auto-placed items and
      // force their indices to the first set in the track collection.
      masonry_item.column_set_indices = masonry_item.row_set_indices = {0, 0};
      continue;
    }
    masonry_item.ComputeSetIndices(track_collection);
  }
  return masonry_items;
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
    // TODO(ethavar): This is a placeholder since we need to compute the actual
    // offset in the stacking axis via the placement algorithm.
    is_for_columns ? containing_rect->offset.block_offset =
                         BorderScrollbarPadding().block_start
                   : containing_rect->offset.block_offset =
                         BorderScrollbarPadding().inline_start;

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

  (Style().MasonryTrackSizingDirection() == kForColumns)
      ? containing_size.inline_size = kIndefiniteSize
      : containing_size.block_size = kIndefiniteSize;

  return CreateConstraintSpace(masonry_item, containing_size,
                               LayoutResultCacheSlot::kMeasure);
}

}  // namespace blink
