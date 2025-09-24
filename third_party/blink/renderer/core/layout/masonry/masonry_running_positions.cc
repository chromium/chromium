// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#include "third_party/blink/renderer/core/layout/masonry/masonry_running_positions.h"

#include "third_party/blink/renderer/core/style/grid_area.h"

namespace blink {


GridSpan MasonryRunningPositions::GetFirstEligibleLine(
    wtf_size_t span_size,
    LayoutUnit& max_running_position) const {
  DCHECK_LE(span_size, running_positions_.size());
  DCHECK_LE(auto_placement_cursor_, running_positions_.size());

  // Find the minimum max-position and calculate the largest max-position that's
  // within the tie threshold of that minimum. Lines that span running positions
  // less than or equal to `largest_max_running_position_allowed` are possible
  // lines as defined in
  // https://drafts.csswg.org/css-grid-3/#masonry-layout-algorithm.
  const auto max_running_positions = GetMaxPositionsForAllTracks(span_size);
  const auto largest_max_running_position_allowed =
      *(std::min_element(max_running_positions.begin(),
                         max_running_positions.end())) +
      tie_threshold_;

  // From https://drafts.csswg.org/css-grid-3/#masonry-layout-algorithm:
  // "Choose the first line in possible lines greater than or equal to the
  // auto-placement cursor as the item’s position in the grid axis; or if there
  // are none such, choose the first one."
  auto FindPositionWithinThreshold = [&](wtf_size_t begin_index) {
    for (auto i = begin_index; i < max_running_positions.size(); ++i) {
      if (max_running_positions[i] <= largest_max_running_position_allowed) {
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
  max_running_position = max_running_positions[first_eligible_line];
  return GridSpan::TranslatedDefiniteGridSpan(first_eligible_line,
                                              first_eligible_line + span_size);
}

void MasonryRunningPositions::UpdateRunningPositionsForSpan(
    const GridSpan& span,
    LayoutUnit new_running_position,
    std::optional<LayoutUnit> max_running_position_for_span) {
  const auto end_line = span.EndLine();

  CHECK_LE(end_line, running_positions_.size());

  for (auto track_idx = span.StartLine(); track_idx < end_line; ++track_idx) {
    const LayoutUnit current_running_position = running_positions_[track_idx];
    DCHECK_GE(new_running_position, current_running_position);
    // If the current running position is less than the new running position, it
    // means that a opening will be formed after placement. We should only ever
    // be accounting for track openings in the case of dense packing.
    if (max_running_position_for_span &&
        (current_running_position < *max_running_position_for_span)) {
      DCHECK(is_dense_packing_);
      CHECK_LT(track_idx, track_collection_openings_.size());
      track_collection_openings_[track_idx].emplace_back(TrackOpening{
          current_running_position, *max_running_position_for_span});
    }
    running_positions_[track_idx] = new_running_position;
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

LayoutUnit MasonryRunningPositions::CalculateUsedTrackSize(
    const GridSpan& span) const {
  LayoutUnit used_track_size;
  const auto end_line = span.EndLine();
  CHECK_LE(end_line, track_collection_sizes_.size());
  for (wtf_size_t start_line = span.StartLine(); start_line < end_line;
       ++start_line) {
    used_track_size += track_collection_sizes_[start_line];
  }
  return used_track_size;
}

bool MasonryRunningPositions::AccumulateTrackOpeningsToAccomodateItem(
    LayoutUnit item_stacking_axis_contribution,
    LayoutUnit previous_track_opening_start_position,
    LayoutUnit previous_track_opening_end_position,
    wtf_size_t num_tracks_remaining,
    wtf_size_t track_to_check_for_openings,
    EligibleTrackOpeningPath& eligible_track_opening_result) {
  // Iterate through the track's openings to search for opening overlaps.
  const Vector<TrackOpening>& current_track_openings =
      track_collection_openings_[track_to_check_for_openings];
  for (wtf_size_t i = 0; i < current_track_openings.size(); ++i) {
    TrackOpening current_track_opening = current_track_openings[i];

    // Calculate the overlap between the previous track's eligible opening and
    // the current opening. We need to ensure that the item we are placing into
    // the track opening does not layout on top of already laid out items, which
    // means that we have to always choose the greatest start position and the
    // smallest end position.
    const LayoutUnit overlap_start_position =
        std::max(previous_track_opening_start_position,
                 current_track_opening.start_position);
    const LayoutUnit overlap_end_position =
        std::min(previous_track_opening_end_position,
                 current_track_opening.end_position);
    const LayoutUnit overlap_range_size =
        overlap_end_position - overlap_start_position;

    if (overlap_range_size >= item_stacking_axis_contribution) {
      // If this is the last track we needed to check, we can return the current
      // start position as the final position we want to place the item in.
      // Otherwise, check to see if the next n-1 tracks have openings that can
      // align to accomodate the current item. If they do, we can return.
      if (num_tracks_remaining == 0 ||
          AccumulateTrackOpeningsToAccomodateItem(
              item_stacking_axis_contribution,
              /*previous_track_opening_start_position=*/
              overlap_start_position,
              /*previous_track_opening_end_position=*/overlap_end_position,
              num_tracks_remaining - 1, track_to_check_for_openings + 1,
              eligible_track_opening_result)) {
        eligible_track_opening_result.track_opening_indices.emplace_back(i);
        eligible_track_opening_result.start_position = overlap_start_position;
        break;
      }
    }
  }
  return eligible_track_opening_result.IsValid();
}

LayoutUnit
MasonryRunningPositions::GetEligibleTrackOpeningAndUpdateMasonryItemSpan(
    wtf_size_t start_offset,
    GridItemData& masonry_item,
    const LayoutUnit item_stacking_axis_contribution,
    const GridLayoutTrackCollection& track_collection) {
  DCHECK(is_dense_packing_);

  const auto grid_axis_direction = track_collection.Direction();
  const GridSpan& initial_span =
      masonry_item.resolved_position.Span(grid_axis_direction);
  const wtf_size_t span_size = initial_span.SpanSize();
  const LayoutUnit used_track_size = CalculateUsedTrackSize(initial_span);

  EligibleTrackOpeningPath highest_eligible_track_opening_result;

  // Find the highest eligible opening iterating from the auto-placement cursor
  // to the end of the tracks, then looping around from the first track to the
  // auto-placement cursor. This gives priority to openings right after the
  // auto-placement cursor.
  GridSpan item_span = GridSpan::TranslatedDefiniteGridSpan(
      auto_placement_cursor_, auto_placement_cursor_ + span_size);

  // TODO(celestepan): Start iterating through tracks starting from the
  // beginning of the track collection instead of after the auto-placement
  // cursor.
  //
  // `max_iterations` is the maximum number of iterations we should need to
  // perform to check all possible track spans of size `span_size`.
  wtf_size_t iterations = 0;
  wtf_size_t max_iterations = running_positions_.size() - span_size + 1;
  do {
    ++iterations;
    if (item_span.EndLine() > running_positions_.size()) {
      item_span = GridSpan::TranslatedDefiniteGridSpan(0, span_size);
    }

    // If the used track size of the item doesn't match the total track size of
    // the span OR if the item we are attempting to place has a user-specified
    // position that doesn't match the current span, move onto the next span.
    if (CalculateUsedTrackSize(item_span) != used_track_size ||
        (!masonry_item.is_auto_placed && item_span != initial_span)) {
      ++item_span;
      continue;
    }

    wtf_size_t current_track = item_span.StartLine();
    // If the current track does not have any openings OR the first track
    // opening is already greater than the highest eligible opening found so
    // far, we won't end up finding any better results that start with this
    // track.
    if (track_collection_openings_[current_track].empty() ||
        (highest_eligible_track_opening_result.IsValid() &&
         track_collection_openings_[current_track][0].start_position >=
             highest_eligible_track_opening_result.start_position)) {
      ++item_span;
      continue;
    }

    EligibleTrackOpeningPath eligible_track_opening_result;
    AccumulateTrackOpeningsToAccomodateItem(
        item_stacking_axis_contribution,
        /*previous_track_opening_start_position=*/LayoutUnit(),
        /*previous_track_opening_end_position=*/LayoutUnit::Max(),
        /*num_tracks_remaining=*/span_size - 1,
        /*track_to_check_for_openings=*/current_track,
        eligible_track_opening_result);

    // Starting at `current_track`, find a series of adjacent track openings
    // that the item could be placed into starting at this line.  If there is
    // not previous result for the highest eligible path of openings OR the
    // series of adjacent track openings is higher than the previous highest
    // series of adjacent track openings found, store the result in
    // `highest_eligible_track_opening_result`.
    if (eligible_track_opening_result.IsValid() &&
        (!highest_eligible_track_opening_result.IsValid() ||
         eligible_track_opening_result.start_position <
             highest_eligible_track_opening_result.start_position)) {
      highest_eligible_track_opening_result = eligible_track_opening_result;
      highest_eligible_track_opening_result.starting_track_index =
          current_track;
    }

    ++item_span;
  } while (iterations <= max_iterations);

  // TODO(celestepan): Determine if we need a faster data structure for
  // erasing items.
  //
  // The indices of the track openings are stored in reverse order due to the
  // recursive nature of `AccumulateTrackOpeningsToAccomodateItem`, so we need
  // to iterate through the tracks in reverse order.
  if (highest_eligible_track_opening_result.IsValid()) {
    wtf_size_t current_track_index =
        highest_eligible_track_opening_result.starting_track_index + span_size;
    for (wtf_size_t track_opening_index :
         highest_eligible_track_opening_result.track_opening_indices) {
      // Perform subtraction here to avoid underflow.
      --current_track_index;
      // If an eligible opening was found, we should place the item into it
      // and remove or adjust the opening as needed.
      const TrackOpening current_track_opening =
          track_collection_openings_[current_track_index][track_opening_index];
      // If the item completely fills the opening, remove the opening.
      if (item_stacking_axis_contribution == current_track_opening.Size()) {
        track_collection_openings_[current_track_index].EraseAt(
            track_opening_index);
      } else {
        // If the item causes an opening to split, create a new track
        // opening above the item.
        if (current_track_opening.start_position !=
            highest_eligible_track_opening_result.start_position) {
          track_collection_openings_[current_track_index].insert(
              track_opening_index,
              TrackOpening{
                  current_track_opening.start_position,
                  highest_eligible_track_opening_result.start_position});
          ++track_opening_index;
        }
        // If the item doesn't fill the opening, adjust the size of the track
        // opening.
        track_collection_openings_[current_track_index][track_opening_index]
            .start_position = current_track_opening.start_position +
                              item_stacking_axis_contribution;
      }
    }

    // Set the span of `masonry_item` to the span of the highest eligible
    // opening found.
    GridSpan highest_eligible_opening_span =
        GridSpan::TranslatedDefiniteGridSpan(
            highest_eligible_track_opening_result.starting_track_index,
            highest_eligible_track_opening_result.starting_track_index +
                span_size);
    DCHECK_EQ(masonry_item.resolved_position.SpanSize(grid_axis_direction),
              highest_eligible_opening_span.SpanSize());
    masonry_item.UpdateSpan(highest_eligible_opening_span, grid_axis_direction,
                            start_offset, track_collection);
  }

  return highest_eligible_track_opening_result.start_position;
}

// TODO(celestepan): Add method GridLayoutTrackCollection to query for
// individual track sizes and call that here instead; that should allow us to
// avoid the creation of a temporary `GridItemData`, as this is not good
// performance-wise.
void MasonryRunningPositions::CalculateAndCacheTrackSizes(
    const GridLayoutTrackCollection& track_collection) {
  track_collection_openings_.resize(track_collection.EndLineOfImplicitGrid());
  auto* item = MakeGarbageCollected<GridItemData>();
  LayoutUnit start_offset;
  GridSpan span = GridSpan::TranslatedDefiniteGridSpan(0, 1);

  for (; span.StartLine() < running_positions_.size(); ++span) {
    item->resolved_position.SetSpan(span, track_collection.Direction());
    item->ComputeSetIndices(track_collection);
    track_collection_sizes_.emplace_back(
        item->CalculateAvailableSize(track_collection, &start_offset));
    item->ResetPlacementIndices();
  }
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
  Vector<LayoutUnit> max_running_positions;
  max_running_positions.ReserveInitialCapacity(first_non_fit_start_line);

  for (auto span = GridSpan::TranslatedDefiniteGridSpan(0, span_size);
       span.StartLine() < first_non_fit_start_line; ++span) {
    max_running_positions.emplace_back(GetMaxPositionForSpan(span));
  }

  return max_running_positions;
}

LayoutUnit MasonryRunningPositions::FinalizeItemSpanAndGetMaxPosition(
    wtf_size_t start_offset,
    GridItemData& masonry_item,
    const GridLayoutTrackCollection& track_collection) {
  LayoutUnit max_running_position;
  const auto grid_axis_direction = track_collection.Direction();
  const GridSpan item_span =
      masonry_item.MaybeTranslateSpan(start_offset, grid_axis_direction);
  if (item_span.IsIndefinite()) {
    masonry_item.resolved_position.SetSpan(
        GetFirstEligibleLine(item_span.IndefiniteSpanSize(),
                             max_running_position),
        grid_axis_direction);
  } else {
    max_running_position = GetMaxPositionForSpan(item_span);
  }

  masonry_item.ComputeSetIndices(track_collection);

  return max_running_position;
}

}  // namespace blink
