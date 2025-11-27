// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_MASONRY_MASONRY_RUNNING_POSITIONS_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_MASONRY_MASONRY_RUNNING_POSITIONS_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/grid/grid_item.h"
#include "third_party/blink/renderer/core/style/grid_area.h"
#include "third_party/blink/renderer/platform/geometry/layout_unit.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"
#include "third_party/blink/renderer/platform/wtf/wtf_size_t.h"

namespace blink {

struct GridSpan;
class GridLayoutTrackCollection;

// TODO(celestepan): Based on how
// https://github.com/w3c/csswg-drafts/issues/12803 resolves, we may want to
// change the keyword that triggers reversed placement. Currently
// column/row-reverse triggers reversed placement.
//
// This class holds a list of running positions for each track. This will be
// used to calculate the next position that an item should be placed.
class CORE_EXPORT MasonryRunningPositions {
 public:
  MasonryRunningPositions(const GridLayoutTrackCollection& track_collection,
                          const ComputedStyle& style,
                          LayoutUnit tie_threshold,
                          const Vector<wtf_size_t>& collapsed_track_indexes)
      : running_positions_(/*size=*/track_collection.EndLineOfImplicitGrid(),
                           LayoutUnit()),
        auto_placement_cursor_(style.IsReverseGridLanesDirection()
                                   ? track_collection.EndLineOfImplicitGrid()
                                   : 0),
        tie_threshold_(tie_threshold),
        is_dense_packing_(style.IsGridAutoFlowAlgorithmDense()),
        is_reverse_direction_(style.IsReverseGridLanesDirection()) {
    // To avoid placing items in collapsed tracks, set such tracks to the max
    // size.
    for (wtf_size_t index : collapsed_track_indexes) {
      running_positions_[index] = LayoutUnit::Max();
    }

    if (is_dense_packing_) {
      track_collection_openings_.resize(
          track_collection.EndLineOfImplicitGrid());
      // If dense packing is enabled, we need to keep track of the track sizes
      // and initialize the data structure that will be used to keep track of
      // any openings.
      CalculateAndCacheTrackSizes(track_collection);
    }
  }

  // Struct used to represent openings that occur in the tracks as a result of
  // layouts with items of varying span sizes.
  struct TrackOpening {
    TrackOpening() = default;
    TrackOpening(LayoutUnit start_position, LayoutUnit end_position)
        : start_position(start_position), end_position(end_position) {}

    LayoutUnit Size() const { return end_position - start_position; }

    // `start_position` and `end_position` the start and end of the opening in
    // the stacking axis.
    LayoutUnit start_position;
    LayoutUnit end_position;
  };

  // Return the first span within `tie_threshold_` of the minimum max-position
  // that comes after the auto-placement cursor in masonry's flow.
  GridSpan GetFirstEligibleLine(wtf_size_t span_size,
                                LayoutUnit& max_running_position) const;

  // Update all the running positions for the tracks within the given `span` to
  // have the inputted `new_running_position`. `new_running_position` is the
  // new running position of all the tracks the item is placed across. The new
  // running position accounts for the gap between items if the user has
  // specified one.
  //
  // `max_running_position_for_span` should only be used in the case of
  // dense-packing, and it is the current maximum running position of the tracks
  // the item spans. This does not include the size of the item that we are
  // laying out and placing, and is used to determine if a opening will be
  // formed once the item is placed.
  //
  // Example of how `max_running_position_for_span` is used when dense-packing
  // is enabled: |Track 1|Track 2|Track 3|
  // |-------|#######|-------|
  // |       |#######|       |
  // |       |#######|<------|---30px (max_running_position_for_span)
  // |ooooooo|ooooooo|       |
  // |ooooooo|ooooooo|<------|---50px (max_running_position_for_span)
  // |       |       |       |
  // |-------|-------|-------|
  //
  // ###: Item 1
  // ooo: Item 2
  // When we place Item 2, the running position of Track 1 is 0, which is less
  // than `max_running_position_for_span`; this means a track opening will be
  // formed in track 1. Track 2's running position is equal to
  // `max_running_position_for_span`, so no new track openings will be formed in
  // Track 2.
  void UpdateRunningPositionsForSpan(
      const GridSpan& span,
      LayoutUnit new_running_position,
      std::optional<LayoutUnit> max_running_position_for_span = std::nullopt);

  // Returns the max-position for a given span.
  LayoutUnit GetMaxPositionForSpan(const GridSpan& span) const;

  void UpdateAutoPlacementCursor(
      const GridArea& resolved_position,
      const GridTrackSizingDirection grid_axis_direction);

  // If we can find an eligible track opening to fit the item, set
  // `masonry_item` to have the updated span location, adjust the track opening
  // as needed (either erasing it or reducing the size), and return the running
  // position at which the item will be placed. This method is only used when
  // dense-packing is set.
  LayoutUnit GetEligibleTrackOpeningAndUpdateMasonryItemSpan(
      wtf_size_t start_offset,
      GridItemData& masonry_item,
      const LayoutUnit item_height,
      const GridLayoutTrackCollection& track_collection);

  // If the span of `masonry_item` is indefinite this method will find and set
  // the span where the item should be placed. Then, this method will return the
  // maximum running position of the span where the item will be placed.
  LayoutUnit FinalizeItemSpanAndGetMaxPosition(
      wtf_size_t start_offset,
      GridItemData& masonry_item,
      const GridLayoutTrackCollection& track_collection);

 private:
  friend class MasonryLayoutAlgorithmTest;

  // Struct to keep track of a span of tracks' start lines and their
  // max-positions, where the max-position of a span represents the maximum
  // running position of all tracks in a span. This will always be used in
  // conjunction with a span size, so we can calculate the ending line using
  // `start_line` and a given span size.
  struct MaxPositionSpan {
    bool operator==(const MaxPositionSpan& other) const {
      return (start_line == other.start_line) && (max_pos == other.max_pos);
    }

    wtf_size_t start_line;
    LayoutUnit max_pos;
  };

  // This struct is used to hold a path of eligible track openings.
  // `starting_track_index` refers to the first track index in the path, and
  // corresponds to the first dimension of `track_collection_openings_`. Each
  // element in `track_opening_indices` is the specific index within a track's
  // vector of openings. `start_position` refers to the highest possible
  // position that an item can be placed; this would be the lowest running
  // position of all the openings in the path.
  struct EligibleTrackOpeningPath {
    bool IsValid() const { return start_position != LayoutUnit::Max(); }

    wtf_size_t starting_track_index{0};
    Vector<wtf_size_t> track_opening_indices;
    LayoutUnit start_position{LayoutUnit::Max()};
  };

  // For testing only.
  MasonryRunningPositions(const Vector<LayoutUnit>& running_positions,
                          LayoutUnit tie_threshold,
                          const Vector<wtf_size_t>& collapsed_track_indexes)
      : running_positions_(running_positions), tie_threshold_(tie_threshold) {
    // To avoid placing items in collapsed tracks, set such tracks to the max
    // size.
    for (wtf_size_t index : collapsed_track_indexes) {
      running_positions_[index] = LayoutUnit::Max();
    }
  }

  void SetAutoPlacementCursorForTesting(wtf_size_t cursor) {
    auto_placement_cursor_ = cursor;
  }

  // Populate `track_collection_sizes_` with the size of each track in
  // `track_collection`.
  void CalculateAndCacheTrackSizes(
      const GridLayoutTrackCollection& track_collection);

  // For each track span of size `span_size` in `running_positions_`, compute
  // its max-position and return a vector where the index corresponds to the
  // track number and the value corresponds to the max-position for that track.
  Vector<LayoutUnit> GetMaxPositionsForAllTracks(wtf_size_t span_size) const;

  // Calculate the total size of the tracks across the given span.
  LayoutUnit CalculateUsedTrackSize(const GridSpan& span) const;

  // Recursive method that uses backtracking to find a path of
  // track openings which align to accomodate an item with a contribution size
  // in the stacking axis of `item_stacking_axis_contribution`. This method
  // returns whether or not a path of eligible track openings were found.
  // Because of the recursive nature of this method, the `track_opening_indices`
  // in `eligible_track_opening_result` will be in reverse order.
  bool AccumulateTrackOpeningsToAccommodateItem(
      LayoutUnit item_stacking_axis_contribution,
      LayoutUnit previous_track_opening_start_position,
      LayoutUnit previous_track_opening_end_position,
      wtf_size_t num_tracks_remaining,
      wtf_size_t track_to_check_for_openings,
      EligibleTrackOpeningPath& eligible_track_opening_result);

  // The index of the `running_positions_` vector corresponds to the track
  // number, while the value of the vector item corresponds to the current
  // running position of the track. Note that the tracks are 0-indexed.
  Vector<LayoutUnit> running_positions_;

  // The indices in the first dimension of vectors corresponds to the track
  // number, while each corresponding vector contains the openings for that
  // track. This is used for determining possible alternative placement
  // locations for dense packing. It will only be populated in such cases.
  Vector<Vector<TrackOpening>> track_collection_openings_;

  // The index of `track_collection_sizes_` corresponds to the track number, and
  // each element represents the size of the track at that index.
  Vector<LayoutUnit> track_collection_sizes_;

  wtf_size_t auto_placement_cursor_;
  LayoutUnit tie_threshold_;

  bool is_dense_packing_{false};
  bool is_reverse_direction_{false};
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_MASONRY_MASONRY_RUNNING_POSITIONS_H_
