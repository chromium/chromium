// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#include "third_party/blink/renderer/core/layout/masonry/masonry_layout_algorithm.h"

#include "third_party/blink/renderer/core/layout/grid/grid_item.h"
#include "third_party/blink/renderer/core/layout/grid/grid_track_collection.h"
#include "third_party/blink/renderer/core/layout/grid/grid_track_sizing_algorithm.h"

namespace blink {

const LayoutResult* MasonryLayoutAlgorithm::Layout() {
  for (auto child = Node().FirstChild(); child; child = child.NextSibling()) {
    To<BlockNode>(child).Layout(CreateConstraintSpaceForMeasure(
        GridItemData(To<BlockNode>(child), Style(), Style())));
  }

  // TODO(ethavar): Compute the actual block size.
  container_builder_.SetFragmentsTotalBlockSize(LayoutUnit());
  return container_builder_.ToBoxFragment();
}

MinMaxSizesResult MasonryLayoutAlgorithm::ComputeMinMaxSizes(
    const MinMaxSizesFloatInput&) {
  return {MinMaxSizes(), /*depends_on_block_constraints=*/false};
}

MasonryLayoutAlgorithm::MasonryLayoutAlgorithm(
    const LayoutAlgorithmParams& params)
    : LayoutAlgorithm(params) {
  DCHECK(params.space.IsNewFormattingContext());
}

GridSizingTrackCollection MasonryLayoutAlgorithm::BuildGridAxisTracks() const {
  const auto& style = Style();
  const auto& available_size = ChildAvailableSize();
  const auto grid_direction = style.MasonryTrackSizingDirection();

  GridRangeBuilder range_builder(style, grid_direction,
                                 ComputeAutomaticRepetitions(),
                                 /*start_offset=*/0);

  GridSizingTrackCollection track_collection(range_builder.FinalizeRanges(),
                                             grid_direction);
  track_collection.BuildSets(style, available_size);

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
