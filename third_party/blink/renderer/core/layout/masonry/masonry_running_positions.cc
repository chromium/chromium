// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#include "third_party/blink/renderer/core/layout/masonry/masonry_running_positions.h"

#include "third_party/blink/renderer/core/style/grid_area.h"

namespace blink {

namespace {
struct EligibleTrackOpening {
  MasonryRunningPositions::TrackOpening opening{LayoutUnit::Max(),
                                                LayoutUnit()};
  GridSpan span{GridSpan::IndefiniteGridSpan(1)};
  wtf_size_t track_index{0};
  wtf_size_t opening_index{0};

  EligibleTrackOpening() = default;

  void SetOpening(
      const MasonryRunningPositions::TrackOpening& other_track_opening,
      wtf_size_t other_track_index,
      wtf_size_t other_opening_index,
      const GridSpan other_span) {
    opening = other_track_opening;
    track_index = other_track_index;
    opening_index = other_opening_index;
    span = other_span;
  }

  bool HasValidOpening() const {
    return opening.start_position != LayoutUnit::Max();
  }
};
}  // namespace

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
    // TODO(celestepan): Account for openings with span > 1.
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

LayoutUnit
MasonryRunningPositions::GetEligibleTrackOpeningAndUpdateMasonryItemSpan(
    wtf_size_t start_offset,
    GridItemData& masonry_item,
    const LayoutUnit item_stacking_axis_contribution,
    const GridLayoutTrackCollection& track_collection) {
  DCHECK(is_dense_packing_);

  const auto grid_axis_direction = track_collection.Direction();
  GridSpan item_span =
      masonry_item.MaybeTranslateSpan(start_offset, grid_axis_direction);
  const wtf_size_t span_size = item_span.SpanSize();
  const wtf_size_t start_line = item_span.StartLine();
  const LayoutUnit used_track_size = track_collection_sizes_[start_line];

  EligibleTrackOpening highest_eligible_opening;
  // TODO(celestepan): Remove this check once we support multi-spanning items.
  if (span_size > 1) {
    return highest_eligible_opening.opening.start_position;
  }

  auto GetHighestEligibleTrackOpeningForTrack =
      [&](Vector<TrackOpening>& current_track_openings,
          const wtf_size_t current_line) {
        // TODO (celestepan): account for openings between items when
        // checking if the item can fit into the space. This affects the
        // conditional below when we're adjusting/removing the opening we
        // place the item into.
        //
        // Check if the item can fit into any of the current track's
        // openings and if the opening is higher than the current highest
        // existing opening; if it is, then it should be our newest highest
        // opening.
        for (wtf_size_t i = 0; i < current_track_openings.size(); ++i) {
          TrackOpening& current_opening = current_track_openings[i];
          if ((current_opening.Size() >= item_stacking_axis_contribution) &&
              (current_opening.start_position <
               highest_eligible_opening.opening.start_position)) {
            GridSpan span = GridSpan::TranslatedDefiniteGridSpan(
                current_line, current_line + 1);
            span.Translate(start_offset);
            highest_eligible_opening.SetOpening(
                current_opening, /*other_track_idx=*/current_line,
                /*other_opening_idx=*/i, span);
            // Since openings are sorted by start position, if we find an
            // opening in the curent track, there's no reason to iterate
            // through the rest of the openings in the track.
            break;
          }
        }
      };

  // Iterate through the openings in each track to find the highest eligible
  // opening that can fit `masonry_item`. `start_line` is the first line that
  // we will begin checking for openings at.
  auto SetHighestEligibleTrackOpening = [&](wtf_size_t start_line,
                                            wtf_size_t end_line) {
    CHECK_LE(end_line, track_collection_openings_.size());
    CHECK_LE(end_line, track_collection_sizes_.size());
    // TODO(celestepan): Account for items with a span size greater than 1;
    // we should iterate through the tracks in spans.
    for (wtf_size_t current_line = start_line; current_line < end_line;
         ++current_line) {
      // Only iterate through tracks with the same size as the track the
      // item was laid out into.
      if ((track_collection_sizes_[current_line] == used_track_size)) {
        Vector<TrackOpening>& current_track_openings =
            track_collection_openings_[current_line];
        // If there are no openings or the highest opening in the track is
        // lower than the current highest position, skip iterating through
        // the rest of the openings in the track.
        if (current_track_openings.empty() ||
            (highest_eligible_opening.HasValidOpening() &&
             current_track_openings[0].start_position >=
                 highest_eligible_opening.opening.start_position)) {
          continue;
        }
        GetHighestEligibleTrackOpeningForTrack(current_track_openings,
                                               current_line);
      }
    }
  };

  // TODO(celestepan): If the item has a specified track, only check the
  // openings within that track.
  //
  // Find the highest eligible opening iterating from the auto-placement cursor
  // to the end of the tracks, then looping around from the first track to the
  // auto-placement cursor. This gives priority to openings right after the
  // auto-placement cursor.
  SetHighestEligibleTrackOpening(
      /*start_line=*/auto_placement_cursor_,
      /*end_line=*/running_positions_.size());

  SetHighestEligibleTrackOpening(
      /*start_line=*/0,
      /*end_line=*/auto_placement_cursor_);

  // If an eligible opening was found, we should place the item into it
  // and remove or adjust the opening as needed.
  if (highest_eligible_opening.HasValidOpening()) {
    // TODO(celestepan): Determine if we need a faster data structure for
    // erasing items.
    //
    // If the item completely fills the opening, remove the opening.
    if (item_stacking_axis_contribution ==
        highest_eligible_opening.opening.Size()) {
      track_collection_openings_[highest_eligible_opening.track_index].EraseAt(
          highest_eligible_opening.opening_index);
    } else {
      // Otherwise, adjust the opening size.
      track_collection_openings_[highest_eligible_opening.track_index]
                                [highest_eligible_opening.opening_index]
                                    .start_position =
          highest_eligible_opening.opening.start_position +
          item_stacking_axis_contribution;
    }
    DCHECK_EQ(masonry_item.resolved_position.SpanSize(grid_axis_direction),
              highest_eligible_opening.span.SpanSize());
    masonry_item.UpdateSpan(highest_eligible_opening.span, grid_axis_direction,
                            start_offset, track_collection);
  }

  return highest_eligible_opening.opening.start_position;
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

  for (; span.StartLine() < running_positions_.size(); span.Translate(1)) {
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
       span.StartLine() < first_non_fit_start_line; span.Translate(1)) {
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
