// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#include "third_party/blink/renderer/core/layout/masonry/masonry_running_positions.h"

#include "third_party/blink/renderer/core/style/grid_area.h"

namespace blink {

GridSpan MasonryRunningPositions::GetFirstEligibleLine(
    wtf_size_t span_size,
    LayoutUnit& max_position) const {
  DCHECK_LE(span_size, running_positions_.size());
  DCHECK_LE(auto_placement_cursor_, running_positions_.size());

  // Find the minimum max-position and calculate the largest max-position that's
  // within the tie threshold of that minimum. Lines that span running positions
  // less than or equal to `largest_max_position_allowed` are possible lines as
  // defined in https://drafts.csswg.org/css-grid-3/#masonry-layout-algorithm.
  const auto max_positions = GetMaxPositionsForAllTracks(span_size);
  const auto largest_max_position_allowed =
      *(std::min_element(max_positions.begin(), max_positions.end())) +
      tie_threshold_;

  // From https://drafts.csswg.org/css-grid-3/#masonry-layout-algorithm:
  // "Choose the first line in possible lines greater than or equal to the
  // auto-placement cursor as the item’s position in the grid axis; or if there
  // are none such, choose the first one."
  auto FindPositionWithinThreshold = [&](wtf_size_t begin_index) {
    for (auto i = begin_index; i < max_positions.size(); ++i) {
      if (max_positions[i] <= largest_max_position_allowed) {
        return i;
      }
    }
    return kNotFound;
  };

  auto first_eligible_line =
      FindPositionWithinThreshold(auto_placement_cursor_);
  if (first_eligible_line == kNotFound) {
    first_eligible_line = FindPositionWithinThreshold(0);
  }

  DCHECK_NE(first_eligible_line, kNotFound);
  max_position = max_positions[first_eligible_line];
  return GridSpan::TranslatedDefiniteGridSpan(first_eligible_line,
                                              first_eligible_line + span_size);
}

void MasonryRunningPositions::UpdateRunningPositionsForSpan(
    const GridSpan& span,
    LayoutUnit running_position) {
  const auto end_line = span.EndLine();

  DCHECK_GE(running_positions_.size(), end_line);

  for (auto track_idx = span.StartLine(); track_idx < end_line; ++track_idx) {
    DCHECK_GE(running_position, running_positions_[track_idx]);
    running_positions_[track_idx] = running_position;
  }
}

LayoutUnit MasonryRunningPositions::GetMaxPositionForSpan(
    const GridSpan& span) const {
  DCHECK_LE(span.EndLine(), running_positions_.size());

  const auto running_positions_for_span =
      base::span(running_positions_)
          .subspan(span.StartLine(), span.IntegerSpan());
  return *(std::max_element(running_positions_for_span.begin(),
                            running_positions_for_span.end()));
}

Vector<LayoutUnit> MasonryRunningPositions::GetMaxPositionsForAllTracks(
    wtf_size_t span_size) const {
  if (span_size == 1) {
    return running_positions_;
  }

  // For each track, if the item fits into the grid axis' span starting at that
  // track, calculate and store the max-position for that track span.
  const wtf_size_t first_non_fit_start_line =
      (running_positions_.size() - span_size) + 1;
  Vector<LayoutUnit> max_positions;
  max_positions.ReserveInitialCapacity(first_non_fit_start_line);

  for (auto span = GridSpan::TranslatedDefiniteGridSpan(0, span_size);
       span.StartLine() < first_non_fit_start_line; span.Translate(1)) {
    max_positions.emplace_back(GetMaxPositionForSpan(span));
  }

  return max_positions;
}

}  // namespace blink
