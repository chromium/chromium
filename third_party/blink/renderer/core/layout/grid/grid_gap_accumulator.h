// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_GRID_GRID_GAP_ACCUMULATOR_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_GRID_GRID_GAP_ACCUMULATOR_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/gap/gap_utils.h"
#include "third_party/blink/renderer/core/layout/grid/grid_layout_utils.h"
#include "third_party/blink/renderer/platform/geometry/layout_unit.h"

namespace blink {

class GapGeometry;
class GridLayoutData;
class GridLayoutTrackCollection;
struct GridItemData;

class CORE_EXPORT GridGapAccumulator {
  STACK_ALLOCATED();

 public:
  GridGapAccumulator();

  void BuildGapGeometry(const GridLayoutData& layout_data);

  // Aggregates the intervals of gaps blocked by a `grid_item`. This identifies
  // which gaps are intersected by a spanning item and records the track ranges
  // within those gaps that are blocked.
  //
  // For example:
  // - If a grid item spans columns [0, 3] and rows [3, 5]:
  //     - It crosses column gaps at indices [0, 1]. For each of these column
  //     gaps, the blocked row range is [3, 5].
  //     - It crosses row gaps at index [3]. For this row gap, the blocked
  //     column range is [0, 3].
  //
  // For an item spanning tracks [start_line, end_line], the gap indices it
  // crosses are [start_line, end_line - 1).
  void AggregateCellStates(const GridItemData& grid_item);

  // Returns a mapping from row gap indices to their corresponding set indices.
  // The returned vector represents the mapping where the index in the vector
  // corresponds to the row gap index, and the value at that index is the
  // corresponding set index. This follows a similar implementation as
  // `LayoutGrid::ComputeExpandedPositions` and
  // `LayoutGrid::CollectTrackSizesForComputedStyle` but adapted to row gaps.
  Vector<wtf_size_t> GetRowGapToSetIndicesMap(
      const GridLayoutData& layout_data);

  const GapGeometry* FinalizeGapGeometry(
      const GridLayoutTrackCollection& rows,
      const GridLayoutTrackCollection& columns);

 private:
  // Builds the list of "main" gaps for Grid. In the MC (Main-Cross)
  // gap geometry model, we pick rows as the main axis (an arbitrary but
  // consistent choice) and columns as cross axis. This approach avoids
  // duplication and keeps storage minimal since intersections are computed
  // on-demand during paint.
  //
  // See third_party/blink/renderer/core/layout/gap/README.md for more.
  void BuildMainGaps(const GridLayoutData& layout_data);
  void BuildCrossGaps(const GridLayoutData& layout_data);

  // Finalizes each main/cross gap's `GapSegmentStateRanges` using the adjacent
  // track index as the key, adjusting for collapsed tracks.
  void FinalizeMainGapRanges(const GridLayoutTrackCollection& rows);
  void FinalizeCrossGapRanges(const GridLayoutTrackCollection& columns);

  GapGeometry* gap_geometry_ = nullptr;

  GridTrackGapData row_gap_data_;
  GridTrackGapData column_gap_data_;

  LayoutUnit col_gutter_size_;
  LayoutUnit row_gutter_size_;

  GapSegmentStateAggregator main_gaps_aggregator_;
  GapSegmentStateAggregator cross_gaps_aggregator_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_GRID_GRID_GAP_ACCUMULATOR_H_
