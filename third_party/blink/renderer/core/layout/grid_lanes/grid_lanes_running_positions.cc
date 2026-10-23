// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#include "third_party/blink/renderer/core/layout/grid_lanes/grid_lanes_running_positions.h"

#include "third_party/blink/renderer/core/layout/grid/layout_grid.h"
#include "third_party/blink/renderer/core/style/grid_area.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"

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
  // the direction in which we iterate through the vector. This can iterate over
  // a 1D vector. We use this in the case where we need access to the values
  // within the vector, such as when we are working with `max_running_positions`
  // in `GetFirstEligibleLine`.
  RunningPositionsIterator(bool is_reverse_direction,
                           wtf_size_t auto_placement_cursor,
                           wtf_size_t span_size,
                           Vector<LayoutUnit>& running_positions)
      : is_reverse_track_direction_(is_reverse_direction),
        max_index_(running_positions.size() - span_size),
        running_positions_(running_positions) {
    InitializeIterator(auto_placement_cursor, span_size);
  }

  // Constructor for 2D vector, which allows `RunningPositionsIterator` to
  // iterate over the first dimension of a 2D vector without accessing values
  // within the vector. We currently use this in the case where we are iterating
  // through `track_collection_openings_` in both forward and reverse
  // directions.
  RunningPositionsIterator(
      bool is_reverse_direction,
      wtf_size_t auto_placement_cursor,
      wtf_size_t span_size,
      HeapVector<HeapVector<TrackOpening>>& track_collection_openings)
      : is_reverse_track_direction_(is_reverse_direction),
        max_index_(track_collection_openings.size() - span_size) {
    InitializeIterator(auto_placement_cursor, span_size);
  }

  void InitializeIterator(wtf_size_t auto_placement_cursor,
                          wtf_size_t span_size) {
    if (is_reverse_track_direction_) {
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
    is_reverse_track_direction_ ? Decrement() : Increment();
    return prev_position;
  }

  bool end() { return current_index_ == end_index_; }

  wtf_size_t CurrentIndex() { return current_index_; }

  LayoutUnit CurrentRunningPosition() {
    CHECK(!running_positions_.empty());
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

  bool is_reverse_track_direction_{false};
  // `end_index_` is the last index the iterator should access before it returns
  // to the starting index we accessed.
  wtf_size_t end_index_;
  wtf_size_t current_index_;
  // `max_index` is the last index in `running_positions` that we can access
  // safely.
  wtf_size_t max_index_;
  Vector<LayoutUnit> running_positions_;
};

ItemIndexPath NewItemIndexPath(const ItemIndexPath& item_below_index_path,
                               wtf_size_t lane_index,
                               const GridLanesDataVector& grid_lanes) {
  CHECK_LT(lane_index, grid_lanes.size());

  ItemIndexPath item_index_path_result = item_below_index_path;
  GridLaneData* lane_data = grid_lanes[lane_index];

  // Append the position where the new item will be stored. With no item below
  // the opening, it becomes the next lane root. Otherwise, it becomes the next
  // densely packed child of the item below.
  if (item_below_index_path.empty()) {
    item_index_path_result.push_back(lane_data ? lane_data->item_data.size()
                                               : 0);
  } else {
    CHECK(lane_data);
    const GridLanesItemData* parent =
        GridLanesItemDataFromPath(*lane_data, item_below_index_path);
    CHECK(parent);
    item_index_path_result.push_back(parent->items_densely_packed_above.size());
  }
  return item_index_path_result;
}

}  // namespace

// TODO(celestepan): Depending on how
// https://github.com/w3c/csswg-drafts/issues/12803 resolves, we may want to
// update how we place explicitly-placed items when we are performing reverse
// placement.
GridSpan GridLanesRunningPositions::GetFirstEligibleLine(
    wtf_size_t span_size,
    LayoutUnit& max_running_position) const {
  DCHECK_LE(span_size, track_collection_openings_.size());
  DCHECK_LE(auto_placement_cursor_, track_collection_openings_.size());

  // TODO(celestepan): Possibly add optimization here which directly iterates
  // through `track_collection_openings_` instead of calling
  // `GetMaxPositionsForAllTracks` for single-spanning items.
  //
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
  RunningPositionsIterator iterator(is_reverse_track_direction_,
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

void GridLanesRunningPositions::UpdateRunningPositionsForSpan(
    GridItemData& grid_lanes_item,
    LayoutUnit new_running_position,
    LayoutUnit original_stacking_size,
    LayoutUnit max_running_position_for_span,
    wtf_size_t item_index,
    GridLayoutSubtree* layout_subtree,
    const GridLanesDataVector* grid_lanes) {
  const auto& span = grid_lanes_item.Span(grid_axis_direction_);
  const auto end_line = span.EndLine();

  CHECK_LE(end_line, track_collection_openings_.size());
  CHECK(!grid_lanes || end_line <= grid_lanes->size());

  // When item placement data is collected for gap decorations or
  // fragmentation, it is stored in a tree for each lane. Alignment candidates
  // are created before the item is added to the tree. Calculate its future
  // start-lane path so that an alignment candidate can later find its
  // corresponding item.
  ItemIndexPath item_index_path_in_start_lane;
  if (grid_lanes && is_stacking_axis_alignment_set_) {
    // Densely packed items are children of the item below their opening, while
    // normally placed items are roots. This method handles normal placement,
    // which is why we use an empty `item_below_index_path`.
    item_index_path_in_start_lane =
        NewItemIndexPath(/*item_below_index_path=*/ItemIndexPath(),
                         span.StartLine(), *grid_lanes);
  }

  // Share one alignment space object across all lanes spanned by this item.
  AlignmentSpace* item_alignment_space =
      IsTrackingAlignmentCandidates() ? MakeGarbageCollected<AlignmentSpace>()
                                      : nullptr;

  for (auto track_idx = span.StartLine(); track_idx < end_line; ++track_idx) {
    TrackOpening& last_track_opening = GetLastTrackOpening(track_idx);
    CHECK_EQ(last_track_opening.coordinates.end_position, LayoutUnit::Max());
    const LayoutUnit current_running_position =
        GetRunningPositionForTrack(track_idx);
    // If the current running position is less than the new running position,
    // account for a new opening after placement. We should only be creating new
    // track openings when openings are tracked.
    if (IsTrackingOpenings() &&
        current_running_position < max_running_position_for_span) {
      CHECK_LT(track_idx, track_collection_openings_.size());
      last_track_opening.coordinates.start_position = current_running_position;
      last_track_opening.coordinates.end_position =
          max_running_position_for_span;

      // The opening now has its size, so set up the initial available alignment
      // space in the stacking axis for the item above the opening.
      if (last_track_opening.alignment_candidate.IsValid()) {
        last_track_opening.alignment_candidate.alignment_space
            ->AccommodateNewOpeningSize(last_track_opening.Size());
      }

      // This opening is directly above the current item. Record where the item
      // will be stored so a later densely packed item can refer to it. Only
      // dense packing reads this path.
      if (grid_lanes && is_dense_packing_) {
        // Densely packed items are children of the item below their opening,
        // while normally placed items are roots. This method handles normal
        // placement, which is why we use an empty `item_below_index_path`.
        last_track_opening.item_below_index_path = NewItemIndexPath(
            /*item_below_index_path=*/ItemIndexPath(), track_idx, *grid_lanes);
      }

      // Create a new track opening to account for the open end of the track
      // after placing the item.
      track_collection_openings_[track_idx].emplace_back(
          TrackOpening(new_running_position, LayoutUnit::Max()));
    } else {
      // The previous item is now directly above the placed item, with no
      // opening between them. This means that the previous alignment
      // candidate has no further available alignment space in any track it
      // spans.
      if (last_track_opening.alignment_candidate.IsValid()) {
        last_track_opening.alignment_candidate.alignment_space
            ->AccommodateNewOpeningSize(LayoutUnit());
      }
      last_track_opening.coordinates.start_position = new_running_position;
    }

    // The placed item is now directly above the last unbounded opening in this
    // track. That is either the opening created above, or the existing one
    // whose start moved below the item.
    SetItemAboveOpening(track_collection_openings_[track_idx].back(),
                        grid_lanes_item, item_alignment_space,
                        original_stacking_size, item_index, layout_subtree,
                        grid_lanes ? &item_index_path_in_start_lane : nullptr);
  }
}

void GridLanesRunningPositions::UpdateAutoPlacementCursor(
    const GridArea& resolved_position,
    const GridTrackSizingDirection grid_axis_direction) {
  auto_placement_cursor_ =
      is_reverse_track_direction_
          ? resolved_position.StartLine(grid_axis_direction)
          : resolved_position.EndLine(grid_axis_direction);
}

void GridLanesRunningPositions::SetItemAboveOpening(
    TrackOpening& opening,
    GridItemData& item,
    AlignmentSpace* alignment_space,
    LayoutUnit original_stacking_size,
    wtf_size_t item_index,
    GridLayoutSubtree* layout_subtree,
    const ItemIndexPath* item_index_path) {
  if (is_stacking_axis_alignment_set_) {
    opening.alignment_candidate =
        AlignmentCandidate(&item, alignment_space, original_stacking_size,
                           item_index, layout_subtree, item_index_path);
  } else if (has_main_gap_decorations_) {
    // Gap decorations only read the item and its alignment space. With a lane
    // graph there is no item index or path to build a full candidate from.
    opening.alignment_candidate.item = &item;
    opening.alignment_candidate.alignment_space = alignment_space;
  }
}

void GridLanesRunningPositions::FinalizeTrackOpenings(
    LayoutUnit stacking_axis_size,
    LayoutUnit stacking_axis_gap) {
  DCHECK(IsTrackingAlignmentCandidates());
  for (auto& track_openings : track_collection_openings_) {
    TrackOpening& last_opening = track_openings.back();
    DCHECK_EQ(last_opening.coordinates.end_position, LayoutUnit::Max());
    last_opening.coordinates.start_position -= stacking_axis_gap;
    last_opening.coordinates.end_position = stacking_axis_size;
    if (last_opening.alignment_candidate.IsValid()) {
      last_opening.alignment_candidate.alignment_space
          ->AccommodateNewOpeningSize(last_opening.Size());
    }
  }
}

GridLanesRunningPositions::AlignmentCandidateIterator::
    AlignmentCandidateIterator(
        const GridLanesRunningPositions& running_positions)
    : running_positions_(running_positions) {
  DCHECK(running_positions_.is_stacking_axis_alignment_set_);
}

std::optional<AlignmentCandidate>
GridLanesRunningPositions::AlignmentCandidateIterator::Next() {
  const auto& openings = running_positions_.track_collection_openings_;

  while (track_index_ < openings.size()) {
    while (opening_index_ < openings[track_index_].size()) {
      const auto& opening = openings[track_index_][opening_index_];
      ++opening_index_;

      // Skip invalid openings and already-seen multi-span items.
      if (!opening.alignment_candidate.IsValid() ||
          !processed_alignment_candidates_
               .insert(opening.alignment_candidate.item.Get())
               .is_new_entry) {
        continue;
      }

      if (opening.alignment_candidate.HasAvailableAlignmentSpace()) {
        return opening.alignment_candidate;
      }
    }

    ++track_index_;
    opening_index_ = 0;
  }

  return std::nullopt;
}

LayoutUnit GridLanesRunningPositions::GetMaxPositionForSpan(
    const GridSpan& span) const {
  return ComputeMaxPositionForSpan(span, /*exclude_collapsed_tracks=*/false);
}

LayoutUnit GridLanesRunningPositions::GetStackingAxisSizeForSpan(
    const GridSpan& span) const {
  return ComputeMaxPositionForSpan(span, /*exclude_collapsed_tracks=*/true);
}

LayoutUnit GridLanesRunningPositions::ComputeMaxPositionForSpan(
    const GridSpan& span,
    bool exclude_collapsed_tracks) const {
  DCHECK_LE(span.EndLine(), track_collection_openings_.size());
  const wtf_size_t span_size = span.IntegerSpan();

  const auto start_line = span.StartLine();
  LayoutUnit max_running_position_for_span = LayoutUnit::Min();
  for (wtf_size_t offset = 0; offset < span_size; ++offset) {
    const LayoutUnit running_position_for_track =
        GetRunningPositionForTrack(start_line + offset);

    // Collapsed `auto-fit` tracks carry a `LayoutUnit::Max()` sentinel running
    // position. Skip them if required.
    if (exclude_collapsed_tracks &&
        running_position_for_track == LayoutUnit::Max()) {
      continue;
    }
    if (running_position_for_track > max_running_position_for_span) {
      max_running_position_for_span = running_position_for_track;
    }
  }

  // If every track in the span was a collapsed track that we excluded, the span
  // contributes no size to the stacking axis.
  if (exclude_collapsed_tracks &&
      max_running_position_for_span == LayoutUnit::Min()) {
    return LayoutUnit();
  }

  return max_running_position_for_span;
}

LayoutUnit GridLanesRunningPositions::CalculateUsedTrackSize(
    const GridSpan& span) const {
  LayoutUnit used_track_size;
  const auto end_line = span.EndLine();
  CHECK_LE(end_line, track_data_.size());
  for (wtf_size_t start_line = span.StartLine(); start_line < end_line;
       ++start_line) {
    used_track_size += track_data_[start_line].size;
  }
  return used_track_size;
}

bool GridLanesRunningPositions::AccumulateTrackOpeningsToAccommodateItem(
    LayoutUnit item_stacking_axis_contribution,
    LayoutUnit previous_track_opening_start_position,
    LayoutUnit previous_track_opening_end_position,
    wtf_size_t num_tracks_remaining,
    wtf_size_t track_to_check_for_openings,
    EligibleTrackOpeningPath& eligible_track_opening_result) {
  // Iterate through the track's openings to search for opening overlaps.
  const HeapVector<TrackOpening>& current_track_openings =
      track_collection_openings_[track_to_check_for_openings];
  for (wtf_size_t i = 0; i < current_track_openings.size(); ++i) {
    const TrackOpening& current_track_opening = current_track_openings[i];

    // Calculate the overlap between the previous track's eligible opening and
    // the current opening. We need to ensure that the item we are placing into
    // the track opening does not layout on top of already laid out items, which
    // means that we have to always choose the lowest start position and the
    // highest end position.
    const LayoutUnit overlap_start_position =
        std::max(previous_track_opening_start_position,
                 current_track_opening.coordinates.start_position);
    const LayoutUnit overlap_end_position =
        std::min(previous_track_opening_end_position,
                 current_track_opening.coordinates.end_position);
    const LayoutUnit overlap_range_size =
        overlap_start_position > overlap_end_position
            ? LayoutUnit::Min()
            : overlap_end_position - overlap_start_position;

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
          eligible_track_opening_result.end_position = overlap_end_position;
        }
        eligible_track_opening_result.track_opening_indices.emplace_back(i);
        eligible_track_opening_result.starting_track_index =
            track_to_check_for_openings;
        break;
      }
    }
  }
  return eligible_track_opening_result.IsValid();
}

std::optional<wtf_size_t>
GridLanesRunningPositions::ChooseFirstEligibleTrackOpeningPath(
    const Vector<EligibleTrackOpeningPath>& eligible_track_opening_results,
    LayoutUnit minimum_track_opening_running_position,
    LayoutUnit auto_placement_stacking_axis_offset,
    wtf_size_t initial_span_start_line) const {
  if (eligible_track_opening_results.empty()) {
    return std::nullopt;
  }

  const LayoutUnit largest_start_position_allowed =
      minimum_track_opening_running_position + tie_threshold_;
  for (wtf_size_t i = 0; i < eligible_track_opening_results.size(); ++i) {
    if (eligible_track_opening_results[i].start_position >
        largest_start_position_allowed) {
      continue;
    }

    const EligibleTrackOpeningPath& eligible_track_opening_result =
        eligible_track_opening_results[i];

    // Per the spec, if there exists skipped spaces in the layout where the item
    // could have fit if it were placed earlier, we should densely-pack the item
    // into that space; if the running position of the eligible opening is less
    // than the auto-placement's running position, that qualifies as a skipped
    // space.
    if (eligible_track_opening_result.start_position <
        auto_placement_stacking_axis_offset) {
      return i;
    }

    // If track opening has the same start position and is earlier in the
    // flow-order than auto-placement, this opening qualifies as a skipped
    // space, so the item should be densely-packed into that position.
    if (eligible_track_opening_result.start_position ==
            auto_placement_stacking_axis_offset &&
        (is_reverse_track_direction_
             ? eligible_track_opening_result.starting_track_index >
                   initial_span_start_line
             : eligible_track_opening_result.starting_track_index <
                   initial_span_start_line)) {
      return i;
    }
  }
  return std::nullopt;
}

std::optional<LayoutUnit>
GridLanesRunningPositions::GetEligibleTrackOpeningAndUpdateGridLanesItemSpan(
    wtf_size_t start_offset,
    const LayoutUnit item_stacking_axis_contribution,
    LayoutUnit original_stacking_size,
    const LayoutUnit auto_placement_stacking_axis_offset,
    const GridLayoutTrackCollection& track_collection,
    GridItemData& grid_lanes_item,
    wtf_size_t item_index,
    GridLayoutSubtree* layout_subtree,
    const GridLanesDataVector* grid_lanes,
    Vector<ItemIndexPath>* parent_item_index_path_per_lane) {
  DCHECK(is_dense_packing_);
  CHECK_EQ(!!grid_lanes, !!parent_item_index_path_per_lane);
  CHECK(!parent_item_index_path_per_lane ||
        parent_item_index_path_per_lane->empty());

  const auto grid_axis_direction = track_collection.Direction();
  const GridSpan& initial_span =
      grid_lanes_item.resolved_position.Span(grid_axis_direction);
  const wtf_size_t span_size = initial_span.SpanSize();
  const LayoutUnit used_track_size = CalculateUsedTrackSize(initial_span);

  Vector<EligibleTrackOpeningPath> eligible_track_opening_results;
  LayoutUnit minimum_track_opening_running_position = LayoutUnit::Max();
  RunningPositionsIterator iterator(
      is_reverse_track_direction_,
      /*auto_placement_cursor=*/
      is_reverse_track_direction_ ? track_collection_openings_.size() : 0,
      span_size, track_collection_openings_);
  do {
    GridSpan item_span =
        grid_lanes_item.is_auto_placed
            ? GridSpan::TranslatedDefiniteGridSpan(
                  iterator.CurrentIndex(), iterator.CurrentIndex() + span_size)
            : initial_span;
    // If the item we are attempting to place has a user-specified
    // position that doesn't match the current span, there is no reason to
    // continue iterating through the rest of the spans.
    if (!grid_lanes_item.is_auto_placed && item_span != initial_span) {
      break;
    }

    // If the used track size of the item doesn't match the total track size of
    // the span, move on to the next span.
    if (CalculateUsedTrackSize(item_span) != used_track_size) {
      continue;
    }

    wtf_size_t current_track = item_span.StartLine();

    EligibleTrackOpeningPath eligible_track_opening_result;
    AccumulateTrackOpeningsToAccommodateItem(
        item_stacking_axis_contribution,
        /*previous_track_opening_start_position=*/LayoutUnit(),
        /*previous_track_opening_end_position=*/LayoutUnit::Max(),
        /*num_tracks_remaining=*/span_size - 1,
        /*track_to_check_for_openings=*/current_track,
        eligible_track_opening_result);
    if (!eligible_track_opening_result.ContainsTrackOpening()) {
      continue;
    }

    eligible_track_opening_result.starting_track_index = current_track;
    eligible_track_opening_results.emplace_back(eligible_track_opening_result);
    minimum_track_opening_running_position =
        std::min(minimum_track_opening_running_position,
                 eligible_track_opening_result.start_position);
  } while (!iterator++.end());

  const std::optional<wtf_size_t> chosen_opening_index =
      ChooseFirstEligibleTrackOpeningPath(
          eligible_track_opening_results,
          minimum_track_opening_running_position,
          auto_placement_stacking_axis_offset, initial_span.StartLine());
  if (!chosen_opening_index) {
    return std::nullopt;
  }

  const EligibleTrackOpeningPath& first_eligible_track_opening_result =
      eligible_track_opening_results[*chosen_opening_index];

  // Share one alignment space object across all lanes spanned by this item.
  AlignmentSpace* item_alignment_space =
      IsTrackingAlignmentCandidates() ? MakeGarbageCollected<AlignmentSpace>()
                                      : nullptr;

  ItemIndexPath item_index_path_in_start_lane;
  if (parent_item_index_path_per_lane) {
    parent_item_index_path_per_lane->resize(span_size);

    // Each occupied lane may have a different item below its selected opening.
    // The opening indices are stored in reverse track order, with the start
    // lane's opening last. Iterate through the lanes in forward order and read
    // the corresponding opening indices from the end of that vector.
    for (wtf_size_t span_index = 0; span_index < span_size; ++span_index) {
      const wtf_size_t track_index =
          first_eligible_track_opening_result.starting_track_index + span_index;
      const wtf_size_t opening_index =
          first_eligible_track_opening_result
              .track_opening_indices[span_size - span_index - 1];

      parent_item_index_path_per_lane->at(span_index) =
          track_collection_openings_[track_index][opening_index]
              .item_below_index_path;
    }

    // Alignment candidates locate the item through its start lane.
    if (is_stacking_axis_alignment_set_) {
      item_index_path_in_start_lane = NewItemIndexPath(
          parent_item_index_path_per_lane->front(),
          first_eligible_track_opening_result.starting_track_index,
          *grid_lanes);
    }
  }

  // TODO(celestepan): Determine if we need a faster data structure for
  // erasing items.
  //
  // The indices of the track openings are stored in reverse order due to the
  // recursive nature of `AccumulateTrackOpeningsToAccommodateItem`, so we need
  // to iterate through the tracks in reverse order.
  const LayoutUnit dense_packing_start_position =
      first_eligible_track_opening_result.start_position;
  const LayoutUnit dense_packing_end_position =
      dense_packing_start_position + item_stacking_axis_contribution;

  wtf_size_t current_track_index =
      first_eligible_track_opening_result.starting_track_index + span_size;
  for (wtf_size_t track_opening_index :
       first_eligible_track_opening_result.track_opening_indices) {
    // Perform subtraction here to avoid underflow.
    --current_track_index;
    const TrackOpening current_track_opening =
        track_collection_openings_[current_track_index][track_opening_index];

    // `has_opening_above` is true if after dense-packing the item, there is
    // space above the densely-packed item. `has_opening_below` is true if
    // after dense-packing the item, there is space below the item.
    const bool has_opening_above =
        current_track_opening.coordinates.start_position <
        dense_packing_start_position;
    const bool has_opening_below =
        dense_packing_end_position <
        current_track_opening.coordinates.end_position;

    ItemIndexPath current_item_index_path;
    if (parent_item_index_path_per_lane && has_opening_above) {
      current_item_index_path =
          NewItemIndexPath(current_track_opening.item_below_index_path,
                           current_track_index, *grid_lanes);
    }

    // The alignment space of the item that was directly above the opening
    // before this placement.
    AlignmentSpace* previous_item_alignment_space =
        current_track_opening.alignment_candidate.alignment_space.Get();

    // If there is no space above the densely packed item, the previous item is
    // directly above it. If there is no space below the densely packed item,
    // it is directly above the next item. In both cases, the upper of the two
    // items has no available alignment space in this lane.
    if (IsTrackingAlignmentCandidates()) {
      if (previous_item_alignment_space && !has_opening_above) {
        previous_item_alignment_space->AccommodateNewOpeningSize(LayoutUnit());
      }
      if (!has_opening_below) {
        item_alignment_space->AccommodateNewOpeningSize(LayoutUnit());
      }
    }

    if (has_opening_above) {
      if (has_opening_below) {
        // With space both above and below, split the opening into upper and
        // lower remainders. The sizing changes for the lower opening are
        // calculated later with the `lower_opening` reference.
        TrackOpening new_opening_above_item(
            current_track_opening.coordinates.start_position,
            dense_packing_start_position);
        if (parent_item_index_path_per_lane) {
          // The densely packed item is now directly below the upper remainder.
          // Store its path so another item packed there becomes its child.
          new_opening_above_item.item_below_index_path =
              current_item_index_path;
        }
        // The upper opening should take the alignment candidate of the original
        // opening.
        if (IsTrackingAlignmentCandidates()) {
          new_opening_above_item.alignment_candidate =
              current_track_opening.alignment_candidate;
          if (previous_item_alignment_space) {
            previous_item_alignment_space->AccommodateNewOpeningSize(
                new_opening_above_item.Size());
          }
        }
        track_collection_openings_[current_track_index].insert(
            track_opening_index, new_opening_above_item);
        ++track_opening_index;
      } else {
        // If there is only space above the densely-packed item, update the
        // current opening to be that space.
        TrackOpening& opening_above_item =
            track_collection_openings_[current_track_index]
                                      [track_opening_index];
        opening_above_item.coordinates.end_position =
            dense_packing_start_position;

        if (parent_item_index_path_per_lane) {
          opening_above_item.item_below_index_path = current_item_index_path;
        }

        if (IsTrackingAlignmentCandidates() && previous_item_alignment_space) {
          previous_item_alignment_space->AccommodateNewOpeningSize(
              opening_above_item.Size());
        }
        continue;
      }
    } else if (!has_opening_below) {
      // The item exactly fills the opening.
      track_collection_openings_[current_track_index].EraseAt(
          track_opening_index);
      continue;
    }

    TrackOpening& lower_opening =
        track_collection_openings_[current_track_index][track_opening_index];
    lower_opening.coordinates.start_position = dense_packing_end_position;

    // The dense item is directly above the lower remainder.
    SetItemAboveOpening(lower_opening, grid_lanes_item, item_alignment_space,
                        original_stacking_size, item_index, layout_subtree,
                        parent_item_index_path_per_lane
                            ? &item_index_path_in_start_lane
                            : nullptr);
    // If the lower opening is still unbounded, its size is recorded when the
    // track openings are finalized.
    if (item_alignment_space &&
        lower_opening.coordinates.end_position != LayoutUnit::Max()) {
      item_alignment_space->AccommodateNewOpeningSize(lower_opening.Size());
    }
  }

  const GridSpan first_eligible_opening_span =
      GridSpan::TranslatedDefiniteGridSpan(
          first_eligible_track_opening_result.starting_track_index,
          first_eligible_track_opening_result.starting_track_index + span_size);
  DCHECK_EQ(grid_lanes_item.resolved_position.SpanSize(grid_axis_direction),
            first_eligible_opening_span.SpanSize());
  grid_lanes_item.UpdateSpan(first_eligible_opening_span, grid_axis_direction,
                             start_offset, track_collection);

  return first_eligible_track_opening_result.start_position;
}

void GridLanesRunningPositions::CalculateAndCacheTrackSizes(
    const GridLayoutTrackCollection& track_collection) {
  Vector<LayoutUnit> line_positions =
      LayoutGrid::ComputeExpandedPositions(track_collection);
  // The number of lines should be one more than the number of tracks.
  CHECK_EQ(line_positions.size(), TrackCount() + 1);

  const auto track_collection_gutter_size = track_collection.GutterSize();

  // `line_positions` contains the offset of each line; the space between the
  // adjacent lines is equivalent to the size of the tracks.
  for (wtf_size_t i = 0; i < TrackCount(); ++i) {
    LayoutUnit track_size = line_positions[i + 1] - line_positions[i];
    // There is no gutter after the last track.
    if (i < TrackCount() - 1) {
      track_size -= track_collection_gutter_size;
    }
    track_data_[i].size = track_size;
  }
}

Vector<LayoutUnit> GridLanesRunningPositions::GetMaxPositionsForAllTracks(
    wtf_size_t span_size) const {
  // For each track, if the item fits into the grid axis' span starting at that
  // track, calculate and store the max-position for that track span.
  const wtf_size_t first_non_fit_start_line =
      (track_collection_openings_.size() - span_size) + 1;
  Vector<LayoutUnit> max_running_positions;
  max_running_positions.ReserveInitialCapacity(
      track_collection_openings_.size() - span_size);

  for (auto span = GridSpan::TranslatedDefiniteGridSpan(0, span_size);
       span.StartLine() < first_non_fit_start_line; ++span) {
    max_running_positions.emplace_back(GetMaxPositionForSpan(span));
  }

  // The last `span_size` tracks will all have the same max-position.
  LayoutUnit max_running_position_for_last_span =
      max_running_positions[first_non_fit_start_line - 1];
  for (wtf_size_t idx = first_non_fit_start_line;
       idx < track_collection_openings_.size(); idx++) {
    max_running_positions.emplace_back(max_running_position_for_last_span);
  }

  return max_running_positions;
}

LayoutUnit GridLanesRunningPositions::FinalizeItemSpanAndGetMaxPosition(
    wtf_size_t start_offset,
    GridItemData& grid_lanes_item,
    const GridLayoutTrackCollection& track_collection) {
  LayoutUnit max_running_position;
  const auto grid_axis_direction = track_collection.Direction();

  // Auto-placed subgrids have their span temporarily translated to the
  // beginning of the grid-lanes container during track sizing (see
  // `GridLanesNode::ComputeSetIndicesForSubgrid`). Reset the span back to
  // indefinite here so the grid lanes placement algorithm places it per the
  // auto-placement rules. When laying out a grid-lanes subgrid, an auto-placed
  // item that is itself a subgrid may already have an indefinite span and needs
  // no reset.
  if (grid_lanes_item.IsSubgrid() && grid_lanes_item.is_auto_placed &&
      !grid_lanes_item.Span(grid_axis_direction).IsIndefinite()) {
    grid_lanes_item.resolved_position.SetSpan(
        GridSpan::IndefiniteGridSpan(
            grid_lanes_item.SpanSize(grid_axis_direction)),
        grid_axis_direction);
    grid_lanes_item.ResetPlacementIndices();
  }

  const GridSpan item_span =
      grid_lanes_item.MaybeTranslateSpan(start_offset, grid_axis_direction);
  if (item_span.IsIndefinite()) {
    grid_lanes_item.resolved_position.SetSpan(
        GetFirstEligibleLine(item_span.IndefiniteSpanSize(),
                             max_running_position),
        grid_axis_direction);
  } else {
    max_running_position = GetMaxPositionForSpan(item_span);
  }

  grid_lanes_item.ComputeSetIndices(track_collection);

  return max_running_position;
}

}  // namespace blink
