// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_GRID_LANES_GRID_LANES_GAP_ACCUMULATOR_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_GRID_LANES_GRID_LANES_GAP_ACCUMULATOR_H_

#include <optional>

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/grid_lanes/grid_lane_data.h"
#include "third_party/blink/renderer/platform/geometry/layout_unit.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"
#include "third_party/blink/renderer/platform/wtf/wtf_size_t.h"

namespace blink {

class BoxFragmentBuilder;
class ComputedStyle;
class CrossGap;
class GapGeometry;
class GridLanesGapGeometry;
class GridLanesRunningPositions;
class GridLayoutTrackCollection;
struct LogicalRect;

// Placement-derived inputs used to build grid-lanes gap geometry.
// TODO(layout-dev): Investigate if making this Oilpan-allocated improves
// performance.
struct GridLanesGapGeometryState {
  struct StackingContentBounds {
    LayoutUnit start;
    LayoutUnit end;
  };

  LayoutUnit stacking_gap;
  LayoutUnit border_scrollbar_padding_start;
  LayoutUnit effective_stacking_size;
  LayoutUnit content_alignment_translation;
  bool is_fill_reverse = false;
  std::optional<StackingContentBounds> explicit_content_bounds;
};

// Builds gap-decoration geometry and related state for grid-lanes.
// Real cross gaps are stacking-axis gutters based on placement before item
// self-alignment. Artificial cross gaps record final item occupancy and
// positions one stacking gap before and after spanners.
class CORE_EXPORT GridLanesGapAccumulator {
  STACK_ALLOCATED();

 public:
  explicit GridLanesGapAccumulator(const ComputedStyle& style);

  // Builds `MainGap` geometry for gutters between grid-axis tracks, parallel to
  // the stacking axis. See
  // `third_party/blink/renderer/core/layout/gap/README.md` for Blink's
  // Main/Cross model.
  void BuildMainGaps(const GridLayoutTrackCollection& grid_axis_tracks);

  bool HasMainGaps() const;

  // Stores placement openings and items with available alignment space in
  // every non-collapsed lane.
  // We consider an opening empty if no item is above it, or if the item above
  // it has no available alignment space across its whole span. Otherwise the
  // whole opening is occupied.
  void CaptureEmptyOpenings(const GridLanesRunningPositions& running_positions,
                            const GridLanesGapGeometryState& state);

  // Builds `CrossGap` geometry from the placed-item lane graph.
  const GapGeometry* FinalizeGapGeometry(const GridLanesDataVector& grid_lanes,
                                         const GridLanesGapGeometryState& state,
                                         LayoutUnit stacking_content_start,
                                         LayoutUnit stacking_content_end,
                                         const BoxFragmentBuilder& builder);

 private:
  using LaneItems = HeapVector<Member<const GridLanesItemData>>;
  using AlignmentItems = HeapVector<Member<const GridItemData>>;

  // Final stacking-axis offset of the gutter preceding `item`.
  LayoutUnit FinalGutterCenter(const GridLanesItemData& item,
                               const GridLanesGapGeometryState& state) const;

  wtf_size_t UncollapsedTrackCount() const;

  // Reads final child border-box bounds in both logical axes, including
  // stretch.
  LogicalRect FinalItemBounds(const GridLanesItemData& item,
                              const BoxFragmentBuilder& builder) const;

  // Builds ordered cross gaps for final item bounds and positions one stacking
  // gap before and after spanners. Item-boundary types require available
  // alignment space.
  Vector<CrossGap> BuildArtificialCrossGaps(
      const LaneItems& lane_items,
      wtf_size_t compact_track_index,
      const GridLanesGapGeometryState& state,
      const BoxFragmentBuilder& builder);

  // Keeps item and spanner boundaries only where at least one adjacent main
  // gap needs them. Item boundaries covered by gutters or outside the content
  // bounds become occupancy-only cross gaps.
  void FilterArtificialCrossGaps(Vector<CrossGap>& boundaries,
                                 wtf_size_t compact_track_index,
                                 const Vector<Vector<CrossGap>>& lane_gutters,
                                 LayoutUnit stacking_gap) const;

  // Stores an ordered lane run and associates it with adjacent main gaps.
  void AppendLaneCrossGaps(const Vector<CrossGap>& lane_cross_gaps,
                           wtf_size_t compact_track_index);

  // Records the item and adds its real cross gap when the lane traversal
  // identifies a preceding stacking-axis gutter.
  void RecordLaneEntry(const GridLanesItemData& item,
                       bool has_preceding_gap,
                       wtf_size_t compact_track_index,
                       const GridLanesGapGeometryState& state,
                       LaneItems& lane_items,
                       Vector<CrossGap>& lane_cross_gaps);

  // Records every item in a dense-packing tree in placement-opening order,
  // after fill-reverse. Final aligned box edges may have a different order.
  // `contains_last_item_in_lane` is true for each subtree on the path to the
  // lane's final item.
  void RecordLaneItemTree(GridLanesItemData& item,
                          bool contains_last_item_in_lane,
                          wtf_size_t compact_track_index,
                          const GridLanesGapGeometryState& state,
                          LaneItems& lane_items,
                          Vector<CrossGap>& lane_cross_gaps);

  // Records spanner-blocked ranges between two lanes. Entries that share
  // placement data identify the same spanning item.
  void RecordMainGapSegmentStates(wtf_size_t main_gap_index,
                                  const LaneItems& before_items,
                                  const LaneItems& after_items);

  // Flattens the lane graph and collects ordered placement gutters by lane.
  void CollectLaneEntries(const GridLanesDataVector& grid_lanes,
                          const GridLanesGapGeometryState& state,
                          HeapVector<LaneItems>& lane_items,
                          Vector<Vector<CrossGap>>& lane_gutters);

  // Builds real `CrossGap`s grouped by track in ascending track order, and
  // records spanner-blocked ranges between neighboring lanes.
  void BuildCrossGaps(const GridLanesDataVector& grid_lanes,
                      const GridLanesGapGeometryState& state,
                      const BoxFragmentBuilder& builder);

  GridLanesGapGeometry* gap_geometry_;

  // Items with available alignment space, in each lane's placement-opening
  // order. Their final bounds supply additional item-boundary cross gaps.
  HeapVector<AlignmentItems> items_with_alignment_space_;

  // The track collection owns this sorted list and outlives this stack-local
  // accumulator.
  const Vector<wtf_size_t>* collapsed_track_indexes_ = nullptr;
  wtf_size_t raw_track_count_ = 0;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_GRID_LANES_GRID_LANES_GAP_ACCUMULATOR_H_
