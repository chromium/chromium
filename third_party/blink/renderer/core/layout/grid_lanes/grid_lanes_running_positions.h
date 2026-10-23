// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_GRID_LANES_GRID_LANES_RUNNING_POSITIONS_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_GRID_LANES_GRID_LANES_RUNNING_POSITIONS_H_

#include <optional>

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/grid/grid_item.h"
#include "third_party/blink/renderer/core/layout/grid_lanes/grid_lane_data.h"
#include "third_party/blink/renderer/core/layout/grid_lanes/track_opening.h"
#include "third_party/blink/renderer/core/style/grid_area.h"
#include "third_party/blink/renderer/platform/geometry/layout_unit.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"
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
class CORE_EXPORT GridLanesRunningPositions {
  STACK_ALLOCATED();

 public:
  GridLanesRunningPositions(const GridLayoutTrackCollection& track_collection,
                            const ComputedStyle& style,
                            LayoutUnit tie_threshold,
                            bool is_stacking_axis_alignment_set = false,
                            bool has_main_gap_decorations = false)
      : track_collection_openings_(
            /*size=*/track_collection.EndLineOfImplicitGrid(),
            HeapVector<TrackOpening>(
                /*size=*/1,
                TrackOpening{LayoutUnit(), LayoutUnit::Max()})),
        track_data_(track_collection.EndLineOfImplicitGrid()),
        auto_placement_cursor_(style.IsReverseGridLanesTrackDirection()
                                   ? track_collection.EndLineOfImplicitGrid()
                                   : 0),
        tie_threshold_(tie_threshold),
        grid_axis_direction_(style.GridLanesTrackSizingDirection()),
        is_dense_packing_(style.IsGridLanesPackDense()),
        is_reverse_track_direction_(style.IsReverseGridLanesTrackDirection()),
        is_stacking_axis_alignment_set_(is_stacking_axis_alignment_set),
        has_main_gap_decorations_(has_main_gap_decorations) {
    // To avoid placing items in collapsed tracks, set such tracks to the max
    // size.
    for (wtf_size_t index : track_collection.CollapsedTrackIndexes()) {
      track_collection_openings_[index].back().coordinates.start_position =
          LayoutUnit::Max();
    }

    if (is_dense_packing_) {
      CalculateAndCacheTrackSizes(track_collection);
    }
  }

  // Return the first span within `tie_threshold_` of the minimum max-position
  // that comes after the auto-placement cursor in grid-lanes' flow.
  GridSpan GetFirstEligibleLine(wtf_size_t span_size,
                                LayoutUnit& max_running_position) const;

  // Update all the running positions for the tracks spanned by
  // `grid_lanes_item` to have the inputted `new_running_position`.
  // `new_running_position` is the new running position of all the tracks the
  // item is placed across. The new running position accounts for the gap
  // between items if the user has specified one.
  //
  // If stacking axis alignment or gap decoration tracking is enabled,
  // `grid_lanes_item` will be set as the item above the last unbounded opening
  // in the track.
  //
  // `max_running_position_for_span` is the current maximum running position of
  // the tracks the item spans. This does not include the size of the item that
  // we are laying out and placing, and is used to determine if a opening will
  // be formed once the item is placed. It is only used when
  // `IsTrackingOpenings()` is true.
  //
  // Without a lane graph, `item_index` indexes the item's fragment in the
  // container builder. With a lane graph (gap decorations or fragmentation
  // collection), complete item paths are inferred from `grid_lanes` instead.
  // `layout_subtree` is the item's layout subtree (only non-null for subgrids).
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
      GridItemData& grid_lanes_item,
      LayoutUnit new_running_position,
      LayoutUnit original_stacking_size,
      LayoutUnit max_running_position_for_span,
      wtf_size_t item_index = kNotFound,
      GridLayoutSubtree* layout_subtree = nullptr,
      const GridLanesDataVector* grid_lanes = nullptr);

  // Returns the max-position for a given span.
  LayoutUnit GetMaxPositionForSpan(const GridSpan& span) const;

  // Returns the extent of real content within a span for computing the
  // container's stacking-axis size. Unlike `GetMaxPositionForSpan`, this skips
  // collapsed `auto-fit` tracks.
  LayoutUnit GetStackingAxisSizeForSpan(const GridSpan& span) const;

  void UpdateAutoPlacementCursor(
      const GridArea& resolved_position,
      const GridTrackSizingDirection grid_axis_direction);

  // If we can find an eligible track opening that is higher than
  // `auto_placement_stacking_axis_offset`, or at the same running position but
  // earlier in track-flow order, set `grid_lanes_item` to have the updated span
  // location, adjust the track opening as needed (either erasing it or reducing
  // the size), and return the running position at which the item will be
  // placed. This method returns `std::nullopt` if no eligible track opening was
  // found.
  //
  // This method is only used when dense-packing is set. In the case where a
  // multi-span item is densely-packed across the open ending of a track after
  // the current running position, the running position of that track will be
  // updated in this method. For an example, see the comment for
  // `AccumulateTrackOpeningsToAccommodateItem`.
  //
  // For a densely packed item, `parent_item_index_path_per_lane` receives one
  // path per occupied lane. Each path identifies the item below the selected
  // opening in that lane. An empty path means there is no item below in that
  // lane, so the item is inserted at the root.
  // `parent_item_index_path_per_lane` is empty for a non-densely-packed item.
  //
  // Without a lane graph, `item_index` identifies the item's fragment in the
  // container builder. With a lane graph, the item's complete path is inferred
  // from its parent path and `grid_lanes`.
  std::optional<LayoutUnit> GetEligibleTrackOpeningAndUpdateGridLanesItemSpan(
      wtf_size_t start_offset,
      const LayoutUnit item_stacking_axis_contribution,
      LayoutUnit original_stacking_size,
      const LayoutUnit auto_placement_stacking_axis_offset,
      const GridLayoutTrackCollection& track_collection,
      GridItemData& grid_lanes_item,
      wtf_size_t item_index = kNotFound,
      GridLayoutSubtree* layout_subtree = nullptr,
      const GridLanesDataVector* grid_lanes = nullptr,
      Vector<ItemIndexPath>* parent_item_index_path_per_lane = nullptr);

  // If the span of `grid_lanes_item` is indefinite this method will find and
  // set the span where the item should be placed. Then, this method will return
  // the maximum running position of the span where the item will be placed.
  LayoutUnit FinalizeItemSpanAndGetMaxPosition(
      wtf_size_t start_offset,
      GridItemData& grid_lanes_item,
      const GridLayoutTrackCollection& track_collection);

  // Per-track data combining sizing and baseline information.
  struct TrackData {
    DISALLOW_NEW();

    // Track size in the grid axis.
    LayoutUnit size;

    // Baseline tracking for grid-lanes layout on the stacking axis:
    // - `first_item_stacking_position`: Position of the first item in this
    //   track, used to decide whether this is the first item for baseline
    //   calculation
    // - `last_item_stacking_position`: Position of the last item in this track,
    //   used to decide whether this is the last item for baseline calculation
    // - `first_baseline`: The first baseline value for this track
    // - `last_baseline`: The last baseline value for this track
    std::optional<LayoutUnit> first_item_stacking_position;
    std::optional<LayoutUnit> last_item_stacking_position;
    std::optional<LayoutUnit> first_baseline;
    std::optional<LayoutUnit> last_baseline;
  };

  wtf_size_t TrackCount() const { return track_data_.size(); }

  const TrackData& GetTrackDataAt(wtf_size_t index) const {
    DCHECK_LT(index, track_data_.size());
    return track_data_[index];
  }

  void SetFirstBaseline(wtf_size_t track_index,
                        LayoutUnit stacking_position,
                        LayoutUnit baseline) {
    DCHECK_LT(track_index, track_data_.size());
    track_data_[track_index].first_item_stacking_position = stacking_position;
    track_data_[track_index].first_baseline = baseline;
  }

  void SetLastBaseline(wtf_size_t track_index,
                       LayoutUnit stacking_position,
                       LayoutUnit baseline) {
    DCHECK_LT(track_index, track_data_.size());
    track_data_[track_index].last_item_stacking_position = stacking_position;
    track_data_[track_index].last_baseline = baseline;
  }

  bool IsStackingAxisAlignmentSet() const {
    return is_stacking_axis_alignment_set_;
  }

  // Whether alignment candidates and their alignment spaces are recorded. Gap
  // decorations use them to find empty openings, even when no item aligns.
  bool IsTrackingAlignmentCandidates() const {
    return is_stacking_axis_alignment_set_ || has_main_gap_decorations_;
  }

  // Clamp the end position of the last (unbounded) `TrackOpening` in each
  // track to the actual stacking axis size, and subtract the trailing gap
  // from the start position in the last opening of each track, since we account
  // for an extra gap behind each item when we update track openings. This also
  // records the size of the last (unbounded) `TrackOpening` in the alignment
  // space of the item above it.
  void FinalizeTrackOpenings(LayoutUnit stacking_axis_size,
                             LayoutUnit stacking_axis_gap);

  const HeapVector<TrackOpening>& GetTrackOpenings(
      wtf_size_t track_index) const {
    return track_collection_openings_[track_index];
  }

  // Iterates over track openings one at a time, returning an item if it's the
  // last item above a track opening or if it's the last item in its track, and
  // if the item has positive alignment space below it. More on stacking axis
  // alignment here:
  // https://drafts.csswg.org/css-grid-3/#stacking-self-alignment.
  class AlignmentCandidateIterator {
    STACK_ALLOCATED();

   public:
    explicit AlignmentCandidateIterator(
        const GridLanesRunningPositions& running_positions);

    // Returns the next alignment candidate which meets the criteria described
    // in this class's header, or `std::nullopt` when all track openings have
    // been exhausted.
    std::optional<AlignmentCandidate> Next();

   private:
    const GridLanesRunningPositions& running_positions_;
    wtf_size_t track_index_ = 0;
    wtf_size_t opening_index_ = 0;
    // Multi-span items may appear in multiple track openings; track which
    // items have already been yielded to avoid returning duplicate candidates.
    HashSet<const void*> processed_alignment_candidates_;
  };

  AlignmentCandidateIterator GetAlignmentCandidateIterator() const {
    return AlignmentCandidateIterator(*this);
  }

  // Calculate the total size of the tracks across the given span.
  LayoutUnit CalculateUsedTrackSize(const GridSpan& span) const;

 private:
  friend class GridLanesLayoutAlgorithmTest;

  bool IsTrackingOpenings() const {
    return is_dense_packing_ || IsTrackingAlignmentCandidates();
  }

  // Records `item` as the item directly above `opening`. If stacking-axis
  // alignment is set, the full alignment candidate is built. Otherwise, only
  // the item and its alignment space are stored, which is all gap decorations
  // need.
  void SetItemAboveOpening(TrackOpening& opening,
                           GridItemData& item,
                           AlignmentSpace* alignment_space,
                           LayoutUnit original_stacking_size,
                           wtf_size_t item_index,
                           GridLayoutSubtree* layout_subtree,
                           const ItemIndexPath* item_index_path);

  // This struct is used to hold a path of eligible track openings.
  // `starting_track_index` refers to the first track index in the path, and
  // corresponds to the first dimension of `track_collection_openings_`. Each
  // element in `track_opening_indices` is the specific index within a track's
  // vector of openings. `start_position` refers to the highest possible
  // position that an item can be placed; this would be the lowest running
  // position of all the openings in the path. `end_position` is the end of the
  // intersection of all openings in the path.
  struct EligibleTrackOpeningPath {
    bool IsValid() const { return !track_opening_indices.empty(); }

    // If the track opening consists only of unbounded openings at the end of
    // the track, placing an item in this path would not actually densely-pack
    // it.
    bool ContainsTrackOpening() const {
      return IsValid() && end_position != LayoutUnit::Max();
    }

    wtf_size_t starting_track_index{0};
    Vector<wtf_size_t> track_opening_indices;
    LayoutUnit start_position{LayoutUnit::Max()};
    LayoutUnit end_position{LayoutUnit::Max()};
  };

  // For testing only.
  GridLanesRunningPositions(const Vector<LayoutUnit>& running_positions,
                            LayoutUnit tie_threshold,
                            const Vector<wtf_size_t>& collapsed_track_indexes)
      : tie_threshold_(tie_threshold) {
    track_collection_openings_.resize(running_positions.size());
    for (wtf_size_t index = 0; index < running_positions.size(); ++index) {
      track_collection_openings_[index].emplace_back(
          TrackOpening(running_positions[index], LayoutUnit::Max()));
    }
    // To avoid placing items in collapsed tracks, set such tracks to the max
    // size.
    for (wtf_size_t index : collapsed_track_indexes) {
      track_collection_openings_[index].back().coordinates.start_position =
          LayoutUnit::Max();
    }
  }

  void SetAutoPlacementCursorForTesting(wtf_size_t cursor) {
    auto_placement_cursor_ = cursor;
  }

  // Populate track sizes in `track_data_` from `track_collection`.
  void CalculateAndCacheTrackSizes(
      const GridLayoutTrackCollection& track_collection);

  // For each track span of size `span_size`, compute its max-position and
  // return a vector where the index corresponds to the track number and the
  // value corresponds to the max-position for that track.
  Vector<LayoutUnit> GetMaxPositionsForAllTracks(wtf_size_t span_size) const;

  // Recursive method that uses backtracking to find a path of
  // track openings which align to accomodate an item with a contribution size
  // in the stacking axis of `item_stacking_axis_contribution`. This method
  // returns whether or not a path of eligible track openings were found.
  // Because of the recursive nature of this method, the `track_opening_indices`
  // in `eligible_track_opening_result` will be in reverse order.
  //
  // This method accounts for laying multi-span items out into the open ending
  // of each track, which spans from the track's running position to infinity.
  // Example case, where the numbers represent the running positions of items
  // within the tracks and "--" represents occupied tracks:
  //
  // | Track 1       | Track 2       | Track 3       |
  // | <---0px---->  | <---0px---->  |               |
  // | <---50px--->  | <---50px--->  | <---50px--->  |
  // |               |               | <---------->  |
  // |               | <---80px--->  | <---------->  |
  //
  // If we are placing a 2-span item with a block size of 30px and an inline
  // size of 50px, then we should be able to lay the item out across Track 1 and
  // Track 2, ending at the track opening in Track 2.
  bool AccumulateTrackOpeningsToAccommodateItem(
      LayoutUnit item_stacking_axis_contribution,
      LayoutUnit previous_track_opening_start_position,
      LayoutUnit previous_track_opening_end_position,
      wtf_size_t num_tracks_remaining,
      wtf_size_t track_to_check_for_openings,
      EligibleTrackOpeningPath& eligible_track_opening_result);

  // Chooses the first eligible track opening path in track-flow order within
  // `tie_threshold_` of `minimum_track_opening_running_position` that is higher
  // than auto-placement, or at the same running position but earlier in
  // track-flow order. Returns `std::nullopt` when auto-placement should be
  // retained.
  std::optional<wtf_size_t> ChooseFirstEligibleTrackOpeningPath(
      const Vector<EligibleTrackOpeningPath>& eligible_track_opening_results,
      LayoutUnit minimum_track_opening_running_position,
      LayoutUnit auto_placement_stacking_axis_offset,
      wtf_size_t initial_span_start_line) const;

  // The current running position for a given track is the start position of the
  // final opening.
  LayoutUnit GetRunningPositionForTrack(wtf_size_t track_index) const {
    return track_collection_openings_[track_index]
        .back()
        .coordinates.start_position;
  }

  // Computes the max running position for the given `span`. When
  // `exclude_collapsed_tracks` is true, any collapsed tracks are skipped in
  // the calculation.
  LayoutUnit ComputeMaxPositionForSpan(const GridSpan& span,
                                       bool exclude_collapsed_tracks) const;

  TrackOpening& GetLastTrackOpening(wtf_size_t track_index) {
    return track_collection_openings_[track_index].back();
  }

  // The indices in the first dimension of vectors corresponds to the track
  // number, while each corresponding vector contains the openings for that
  // track. This is used for determining possible alternative placement
  // locations for dense packing. Within each vector of the 2nd dimension, the
  // last `TrackOpening` represents the open space at the end of the track; the
  // `start_position` of this `TrackOpening` is equivalent to the current
  // running position of the track, and the `end_position` is unbounded
  // (LayoutUnit::Max()).
  HeapVector<HeapVector<TrackOpening>> track_collection_openings_;

  // Per-track data (sizes and baselines), indexed by track line number.
  Vector<TrackData> track_data_;

  wtf_size_t auto_placement_cursor_;
  LayoutUnit tie_threshold_;
  GridTrackSizingDirection grid_axis_direction_;

  bool is_dense_packing_{false};
  bool is_reverse_track_direction_{false};

  // This is true whenever the container has alignment in the stacking
  // axis or any individual item requires alignment in the stacking
  // axis.
  bool is_stacking_axis_alignment_set_{false};

  // TODO(javiercon): Look into consolidating how alignment candidates are
  // recorded for stacking-axis alignment and gap decorations, for example by
  // always building the full alignment candidate and marking whether it needs
  // to be aligned after placement.
  //
  // True when the container paints main gap decorations. Every opening is then
  // kept with the item above it, so gap decorations can tell which openings are
  // empty after placement.
  bool has_main_gap_decorations_{false};
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_GRID_LANES_GRID_LANES_RUNNING_POSITIONS_H_
