// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#include "third_party/blink/renderer/core/layout/masonry/masonry_layout_algorithm.h"

#include "third_party/blink/renderer/core/layout/grid/grid_track_collection.h"

namespace blink {

MasonryLayoutAlgorithm::MasonryLayoutAlgorithm(
    const LayoutAlgorithmParams& params)
    : LayoutAlgorithm(params) {
  DCHECK(params.space.IsNewFormattingContext());
}

namespace {

// Auto-placed masonry items can be placed at every cross axis track that fits
// its span size, this implies that `masonry-template-tracks` will be expanded
// to include all possible track starts, mapping 1:1 tracks to ranges.
GridRangeVector ExpandRangesFromTemplateTracks(
    const NGGridTrackList& template_tracks,
    wtf_size_t auto_repetitions) {
  GridRangeVector ranges;
  wtf_size_t current_set_index = 0;
  const auto repeater_count = template_tracks.RepeaterCount();

  for (wtf_size_t i = 0; i < repeater_count; ++i) {
    const auto repetitions = template_tracks.RepeatCount(i, auto_repetitions);
    const auto repeat_size = template_tracks.RepeatSize(i);

    // Expand this repeater `repetitions` times, create a `GridRange` of a
    // single track and set for each definition in the repeater.
    for (wtf_size_t j = 0; j < repetitions; ++j) {
      for (wtf_size_t k = 0; k < repeat_size; ++k) {
        GridRange range;
        range.begin_set_index = range.start_line = current_set_index++;
        range.repeater_index = i;
        range.repeater_offset = k;
        range.set_count = range.track_count = 1;
        ranges.emplace_back(std::move(range));
      }
    }
  }
  return ranges;
}

}  // namespace

GridSizingTrackCollection MasonryLayoutAlgorithm::ComputeCrossAxisTrackSizes()
    const {
  GridSizingTrackCollection cross_axis_tracks(
      ExpandRangesFromTemplateTracks(Style().MasonryTemplateTracks().track_list,
                                     ComputeAutomaticRepetitions()));
  return cross_axis_tracks;
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
