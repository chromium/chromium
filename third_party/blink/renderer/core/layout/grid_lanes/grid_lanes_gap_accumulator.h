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

class ComputedStyle;
class GapGeometry;
class GridLayoutTrackCollection;

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
class CORE_EXPORT GridLanesGapAccumulator {
  STACK_ALLOCATED();

 public:
  explicit GridLanesGapAccumulator(const ComputedStyle& style);

  // Builds `MainGap` geometry for gutters between grid-axis tracks, parallel to
  // the stacking axis. See
  // `third_party/blink/renderer/core/layout/gap/README.md` for Blink's
  // Main/Cross model.
  void BuildMainGaps(const GridLayoutTrackCollection& grid_axis_tracks);

  // Builds `CrossGap` geometry from the placed-item lane graph.
  const GapGeometry* FinalizeGapGeometry(const GridLanesDataVector& grid_lanes,
                                         const GridLanesGapGeometryState& state,
                                         LayoutUnit stacking_content_start,
                                         LayoutUnit stacking_content_end);

 private:
  // Final stacking-axis offset of the gutter preceding `item`.
  LayoutUnit FinalGutterCenter(const GridLanesItemData& item,
                               const GridLanesGapGeometryState& state) const;

  wtf_size_t UncollapsedTrackCount() const;

  // Records the item and adds its `CrossGap` if needed.
  void RecordLaneEntry(const GridLanesItemData& item,
                       const GridLanesItemData* first_item_in_track,
                       wtf_size_t compact_track_index,
                       const GridLanesGapGeometryState& state,
                       Vector<wtf_size_t>& lane_occupant_ids);

  // Adds blocked ranges between the previous and current lanes.
  void MarkBlockedMainGapSegments(
      wtf_size_t main_gap_index,
      const Vector<wtf_size_t>& previous_lane_occupant_ids,
      const Vector<wtf_size_t>& current_lane_occupant_ids);

  // Sorts and adds the densely packed items above `item_below`.
  void AddCrossGapsForPackedItems(const GridLanesItemData& item_below,
                                  const GridLanesItemData* first_item_in_track,
                                  wtf_size_t compact_track_index,
                                  const GridLanesGapGeometryState& state,
                                  Vector<wtf_size_t>& lane_occupant_ids);

  // Builds the `CrossGap`s grouped by track in ascending track order.
  void BuildCrossGaps(const GridLanesDataVector& grid_lanes,
                      const GridLanesGapGeometryState& state);

  GapGeometry* gap_geometry_;

  // The track collection owns this sorted list and outlives this stack-local
  // accumulator.
  const Vector<wtf_size_t>* collapsed_track_indexes_ = nullptr;
  wtf_size_t raw_track_count_ = 0;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_GRID_LANES_GRID_LANES_GAP_ACCUMULATOR_H_
