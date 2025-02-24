// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#include "third_party/blink/renderer/core/layout/masonry/masonry_layout_algorithm.h"

#include "third_party/blink/renderer/core/layout/grid/grid_item.h"
#include "third_party/blink/renderer/core/layout/grid/grid_track_collection.h"
#include "third_party/blink/renderer/core/layout/grid/grid_track_sizing_algorithm.h"

namespace blink {

MasonryLayoutAlgorithm::MasonryLayoutAlgorithm(
    const LayoutAlgorithmParams& params)
    : LayoutAlgorithm(params) {
  DCHECK(params.space.IsNewFormattingContext());
}

const LayoutResult* MasonryLayoutAlgorithm::Layout() {
  for (auto child = Node().FirstChild(); child; child = child.NextSibling()) {
    To<BlockNode>(child).Layout(CreateConstraintSpaceForMeasure(
        GridItemData(To<BlockNode>(child), Style())));
  }

  // TODO(ethavar): Compute the actual block size.
  container_builder_.SetFragmentsTotalBlockSize(LayoutUnit());
  return container_builder_.ToBoxFragment();
}

MinMaxSizesResult MasonryLayoutAlgorithm::ComputeMinMaxSizes(
    const MinMaxSizesFloatInput&) {
  return {MinMaxSizes(), /*depends_on_block_constraints=*/false};
}

GridItems MasonryLayoutAlgorithm::VirtualMasonryItems(
    const GridLineResolver& line_resolver,
    wtf_size_t* start_offset) const {
  DCHECK(start_offset);

  const auto item_groups =
      Node().CollectItemGroups(line_resolver, start_offset);
  DCHECK_GE(*start_offset, 0u);

  GridItems virtual_items;
  const auto& style = Style();
  const auto grid_axis_direction = style.MasonryTrackSizingDirection();

  for (const auto& [group_properties, group_items] : item_groups) {
    auto virtual_item = std::make_unique<GridItemData>();
    auto span = group_properties.Span();

    for (const auto& item_node : group_items) {
      const auto space =
          CreateConstraintSpaceForMeasure(GridItemData(item_node, style));
      virtual_item->EncompassContributionSizes(
          ComputeMinAndMaxContentContributionForSelf(item_node, space).sizes);
    }

    if (span.IsUntranslatedDefinite()) {
      // For groups of items that are explicitly placed, we only need to add a
      // single virtual masonry item within the specified span.
      span.Translate(*start_offset);
      virtual_item->resolved_position.SetSpan(span, grid_axis_direction);
      virtual_items.Append(std::move(virtual_item));
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
    wtf_size_t* start_offset) const {
  const auto& style = Style();
  const auto& available_size = ChildAvailableSize();
  const auto grid_direction = style.MasonryTrackSizingDirection();

  GridRangeBuilder range_builder(style, grid_direction,
                                 ComputeAutomaticRepetitions(),
                                 /*start_offset=*/0);

  GridSizingTrackCollection track_collection(range_builder.FinalizeRanges(),
                                             grid_direction);
  track_collection.BuildSets(style, available_size);

  if (track_collection.HasNonDefiniteTrack()) {
    auto virtual_items = VirtualMasonryItems(line_resolver, start_offset);
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

ConstraintSpace MasonryLayoutAlgorithm::CreateConstraintSpaceForMeasure(
    const GridItemData& masonry_item) const {
  auto containing_size = ChildAvailableSize();

  if (Style().MasonryTrackSizingDirection() == kForColumns) {
    containing_size.inline_size = kIndefiniteSize;
  } else {
    containing_size.block_size = kIndefiniteSize;
  }
  return CreateConstraintSpace(masonry_item, containing_size,
                               LayoutResultCacheSlot::kMeasure);
}

}  // namespace blink
