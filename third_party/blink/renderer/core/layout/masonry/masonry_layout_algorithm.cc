// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#include "third_party/blink/renderer/core/layout/masonry/masonry_layout_algorithm.h"

#include "third_party/blink/renderer/core/layout/grid/grid_track_collection.h"
#include "third_party/blink/renderer/core/layout/grid/grid_track_sizing_algorithm.h"

namespace blink {

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

const LayoutResult* MasonryLayoutAlgorithm::Layout() {
  // TODO(ethavar): Compute the actual block size.
  container_builder_.SetFragmentsTotalBlockSize(LayoutUnit());
  return container_builder_.ToBoxFragment();
}

MinMaxSizesResult MasonryLayoutAlgorithm::ComputeMinMaxSizes(
    const MinMaxSizesFloatInput&) {
  return {MinMaxSizes(), /*depends_on_block_constraints=*/false};
}

}  // namespace blink
