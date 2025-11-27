// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#include "third_party/blink/renderer/core/layout/masonry/masonry_running_positions.h"

#include "third_party/blink/renderer/core/layout/grid/layout_grid.h"
#include "third_party/blink/renderer/core/style/grid_area.h"

namespace blink {

namespace {

// Iterator class that allows us to loop through a vector in forward and
// backward directions. The iterator will loop around once hitting the start/end
// of the vector and will keep track of whether it has completed a full loop of
// the vector.
class RunningPositionsIterator {
 public:
  // This uses `auto_placement_cursor` and `span_size` to determine which index
  // to begin iteration through a vector for an eligible line that an item with
  // `span_size` could be placed. `is_reverse_direction` is used to determine
  // the direction in which we iterate through the vector.
  RunningPositionsIterator(bool is_reverse_direction,
                           wtf_size_t auto_placement_cursor,
                           wtf_size_t span_size,
                           Vector<LayoutUnit>& running_positions)
      : is_reverse_direction_(is_reverse_direction),
        max_index_(running_positions.size() - span_size),
        running_positions_(running_positions) {
    if (is_reverse_direction_) {
      // If the auto placement cursor is less than the span size in the reverse
      // direction, we can't place an item there, and need to loop back to the
      // end of the vector.
      current_index_ = (auto_placement_cursor < span_size)
                           ? max_index_
                           : auto_placement_cursor - span_size;
      end_index_ = (current_index_ < max_index_) ? current_index_ + 1 : 0;
    } else {
      // If while iterating forward the auto placement cursor is greater than
      // the greatest index we can safely access, we need to loop back to the
      // start of the vector.
      current_index_ =
          (auto_placement_cursor > max_index_) ? 0 : auto_placement_cursor;
      end_index_ = (current_index_ > 0) ? current_index_ - 1 : max_index_;
    }
  }

  // Post-increment operator.
  RunningPositionsIterator operator++(int) {
    RunningPositionsIterator prev_position(*this);
    is_reverse_direction_ ? Decrement() : Increment();
    return prev_position;
  }

  bool end() { return current_index_ == end_index_; }

  wtf_size_t CurrentIndex() { return current_index_; }

  LayoutUnit CurrentRunningPosition() {
    return running_positions_[current_index_];
  }

 private:
  void Decrement() {
    if (current_index_ == 0) {
      current_index_ = max_index_;
    } else {
      --current_index_;
    }
  }

  void Increment() {
    if (current_index_ == max_index_) {
      current_index_ = 0;
    } else {
      ++current_index_;
    }
  }

  bool is_reverse_direction_{false};
  // `end_index_` is the last index the iterator should access before it returns
  // to the starting index we accessed.
  wtf_size_t end_index_;
  wtf_size_t current_index_;
  // `max_index` is the last index in `running_positions` that we can access
  // safely.
  wtf_size_t max_index_;
  Vector<LayoutUnit> running_positions_;
};

}  // namespace

// TODO(celestepan): Depending on how
// https://github.com/w3c/csswg-drafts/issues/12803 resolves, we may want to
// update how we place explicitly-placed items when we are performing reverse
// placement.
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
  auto max_running_positions = GetMaxPositionsForAllTracks(span_size);
  const auto largest_max_running_position_allowed =
      *(std::min_element(max_running_positions.begin(),
                         max_running_positions.end())) +
      tie_threshold_;

  // From https://drafts.csswg.org/css-grid-3/#masonry-layout-algorithm:
  // "Choose the first line in possible lines greater than or equal to the
  // auto-placement cursor as the item's position in the grid axis; or if there
  // are none such, choose the first one."
  wtf_size_t first_eligible_line = kNotFound;
  RunningPositionsIterator iterator(is_reverse_direction_,
                                    auto_placement_cursor_, span_size,
                                    max_running_positions);
  do {
    if (iterator.CurrentRunningPosition() <=
        largest_max_running_position_allowed) {
      first_eligible_line = iterator.CurrentIndex();
      break;
    }
  } while (!iterator++.end());

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
    // TODO(celestepan): Consider setting the running position of the track to
    // be the maximum between the current and the new, depending on how
    // https://github.com/w3c/csswg-drafts/issues/12918 resolves.
    running_positions_[track_idx] = new_running_position;
  }
}

void MasonryRunningPositions::UpdateAutoPlacementCursor(
    const GridArea& resolved_position,
    const GridTrackSizingDirection grid_axis_direction) {
  auto_placement_cursor_ =
      is_reverse_direction_ ? resolved_position.StartLine(grid_axis_direction)
                            : resolved_position.EndLine(grid_axis_direction);
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

// TODO(celestepan): Account for fully available tracks as track openings when
// placing multi-span items.
// Example case:
// | Track 1       | Track 2       | Track 3       |
// | <-- 30px--->  | <-- 30px--->  |               |
// | <---50px--->  | <---50px--->  | <---------->  |
// |               |               | <---------->  |
// |               | <---80px--->  | <---------->  |
// If we are placing a 2-span item with inline size of 30px, then we should be
// able to place the item laid out across Track 1 and Track 2, even though Track
// 1 doesn't technically have any track openings.
bool MasonryRunningPositions::AccumulateTrackOpeningsToAccommodateItem(
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
    // means that we have to always choose the lowest start position and the
    // highest end position.
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
          AccumulateTrackOpeningsToAccommodateItem(
              item_stacking_axis_contribution,
              /*previous_track_opening_start_position=*/
              overlap_start_position,
              /*previous_track_opening_end_position=*/overlap_end_position,
              num_tracks_remaining - 1, track_to_check_for_openings + 1,
              eligible_track_opening_result)) {
        // The first time we encounter this conditional should be when
        // `num_tracks_remaining` is 0, which is when we're at the end of the
        // path of adjacent track openings. At that point,
        // `overlap_start_position` will hold the lowest start position amongst
        // the path of eligible tracks.
        if (!eligible_track_opening_result.IsValid()) {
          DCHECK_EQ(num_tracks_remaining, 0u);
          eligible_track_opening_result.start_position = overlap_start_position;
        }
        eligible_track_opening_result.track_opening_indices.emplace_back(i);
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

  // Find the highest eligible opening iterating from the start of the tracks if
  // the item is auto-placed (if item placement direction is reversed, the
  // "start" should be the last track), otherwise within the author-specified
  // track(s).
  RunningPositionsIterator iterator(
      is_reverse_direction_,
      /*auto_placement_cursor=*/
      is_reverse_direction_ ? running_positions_.size() : 0, span_size,
      running_positions_);
  do {
    GridSpan item_span =
        masonry_item.is_auto_placed
            ? GridSpan::TranslatedDefiniteGridSpan(
                  iterator.CurrentIndex(), iterator.CurrentIndex() + span_size)
            : initial_span;
    // If the item we are attempting to place has a user-specified
    // position that doesn't match the current span, there is no reason to
    // continue iterating through the rest of the spans.
    if (!masonry_item.is_auto_placed && item_span != initial_span) {
      break;
    }

    // If the used track size of the item doesn't match the total track size of
    // the span, move on to the next span.
    if (CalculateUsedTrackSize(item_span) != used_track_size) {
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
      continue;
    }

    EligibleTrackOpeningPath eligible_track_opening_result;
    AccumulateTrackOpeningsToAccommodateItem(
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

  } while (!iterator++.end());

  // TODO(celestepan): Determine if we need a faster data structure for
  // erasing items.
  //
  // The indices of the track openings are stored in reverse order due to the
  // recursive nature of `AccumulateTrackOpeningsToAccommodateItem`, so we need
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

void MasonryRunningPositions::CalculateAndCacheTrackSizes(
    const GridLayoutTrackCollection& track_collection) {
  Vector<LayoutUnit> line_positions =
      LayoutGrid::ComputeExpandedPositions(track_collection);
  track_collection_sizes_.resize(track_collection.EndLineOfImplicitGrid());
  // The number of lines should be one more than the number of tracks.
  CHECK_EQ(line_positions.size(), track_collection_sizes_.size() + 1);

  const auto track_collection_size = track_collection_sizes_.size();
  const auto track_collection_gutter_size = track_collection.GutterSize();

  // `line_positions` contains the offset of each line; the space between the
  // adjacent lines is equivalent to the size of the tracks.
  for (wtf_size_t i = 0; i < track_collection_size; ++i) {
    LayoutUnit track_size = line_positions[i + 1] - line_positions[i];
    // There is no gutter after the last track.
    if (i < track_collection_size - 1) {
      track_size -= track_collection_gutter_size;
    }
    track_collection_sizes_[i] = track_size;
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
  max_running_positions.ReserveInitialCapacity(running_positions_.size());

  for (auto span = GridSpan::TranslatedDefiniteGridSpan(0, span_size);
       span.StartLine() < first_non_fit_start_line; ++span) {
    max_running_positions.emplace_back(GetMaxPositionForSpan(span));
  }

  // The last `span_size` tracks will all have the same max-position.
  LayoutUnit max_running_position_for_last_span =
      max_running_positions[first_non_fit_start_line - 1];
  for (wtf_size_t idx = first_non_fit_start_line;
       idx < running_positions_.size(); idx++) {
    max_running_positions.emplace_back(max_running_position_for_last_span);
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
