// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/grid/grid_gap_accumulator.h"

#include "third_party/blink/renderer/core/layout/gap/gap_geometry.h"
#include "third_party/blink/renderer/core/layout/grid/grid_data.h"
#include "third_party/blink/renderer/core/layout/grid/grid_item.h"
#include "third_party/blink/renderer/core/layout/grid/grid_track_collection.h"

namespace blink {

GridGapAccumulator::GridGapAccumulator()
    : gap_geometry_(MakeGarbageCollected<GapGeometry>(
          GapGeometry::ContainerType::kGrid)) {}

void GridGapAccumulator::BuildMainGaps(const GridLayoutData& layout_data) {
  const auto& rows = layout_data.Rows();
  row_gap_data_ =
      BuildGridTrackGapData(rows, GridTrackGapType::kMain, *gap_geometry_);
  row_gutter_size_ = rows.GutterSize();

  // Initialize `cross_gaps_aggregator_` to track cell states along the cross
  // axis (columns). We pass in the number of row tracks because when we
  // aggregate column cell states, they are aggregated along the column for
  // each row in the grid.
  cross_gaps_aggregator_ =
      GapSegmentStateAggregator(/*cell_count=*/row_gap_data_.track_count);
}

void GridGapAccumulator::BuildCrossGaps(const GridLayoutData& layout_data) {
  const auto& columns = layout_data.Columns();
  column_gap_data_ =
      BuildGridTrackGapData(columns, GridTrackGapType::kCross, *gap_geometry_);
  col_gutter_size_ = columns.GutterSize();

  // Initialize `main_gaps_aggregator_` to track cell states along the main
  // axis (rows). We pass in the number of column tracks because when we
  // aggregate row cell states, they are aggregated along the row for
  // each column in the grid.
  main_gaps_aggregator_ =
      GapSegmentStateAggregator(/*cell_count=*/column_gap_data_.track_count);
}

void GridGapAccumulator::BuildGapGeometry(const GridLayoutData& layout_data) {
  BuildMainGaps(layout_data);
  BuildCrossGaps(layout_data);
}

void GridGapAccumulator::AggregateCellStates(const GridItemData& grid_item) {
  main_gaps_aggregator_.ProcessItem(grid_item.Span(kForRows),
                                    grid_item.Span(kForColumns));
  cross_gaps_aggregator_.ProcessItem(grid_item.Span(kForColumns),
                                     grid_item.Span(kForRows));
}

Vector<wtf_size_t> GridGapAccumulator::GetRowGapToSetIndicesMap(
    const GridLayoutData& layout_data) {
  const auto& rows = layout_data.Rows();

  const wtf_size_t range_count = rows.RangeCount();
  Vector<wtf_size_t> gap_idx_to_set_idx;

  for (wtf_size_t range_idx = 0; range_idx < range_count; ++range_idx) {
    const wtf_size_t range_set_count = rows.RangeSetCount(range_idx);
    const wtf_size_t begin_set_index = rows.RangeBeginSetIndex(range_idx);
    const wtf_size_t tracks_in_range = rows.RangeTrackCount(range_idx);

    for (wtf_size_t track_idx_in_range = 0;
         track_idx_in_range < tracks_in_range; ++track_idx_in_range) {
      // Skip the last track in the last range since there's no gap after
      // the final track.
      if (range_idx == range_count - 1 &&
          track_idx_in_range == tracks_in_range - 1) {
        break;
      }

      // Determine which set this track belongs to by using the
      // `begin_set_index` plus the track's set position within this range.
      // The set position is determined using the modulo operator since sets
      // preserve the order in which track definitions appear in their range.
      // If a range has no sets, we exclude it from the list because gaps
      // are not emitted for collapsed tracks.
      if (range_set_count) {
        wtf_size_t set_idx =
            begin_set_index + (track_idx_in_range % range_set_count);
        gap_idx_to_set_idx.emplace_back(set_idx);
      }
      CHECK_LE(gap_idx_to_set_idx.size(),
               static_cast<wtf_size_t>(kGridMaxTracks));
      if (gap_idx_to_set_idx.size() == kGridMaxTracks) {
        // Return early to prevent exceeding the maximum allowed grid tracks
        // limit.
        return gap_idx_to_set_idx;
      }
    }
  }

  return gap_idx_to_set_idx;
}

const GapGeometry* GridGapAccumulator::FinalizeGapGeometry(
    const GridLayoutTrackCollection& rows,
    const GridLayoutTrackCollection& columns) {
  // `GapGeometry` requires both row(main) and column(cross) gaps to be valid.
  if (gap_geometry_->MainGapCount() == 0 &&
      gap_geometry_->CrossGapCount() == 0) {
    return nullptr;
  }

  gap_geometry_->SetInlineGapSize(col_gutter_size_);
  gap_geometry_->SetBlockGapSize(row_gutter_size_);

  // Finalize the `GapSegmentStateRanges` for each gap using the aggregated
  // cell states collected during `AggregateCellStates`.
  if (main_gaps_aggregator_.GetCellCount() > 0 &&
      gap_geometry_->MainGapCount() > 0) {
    FinalizeMainGapRanges(rows);
  }

  if (cross_gaps_aggregator_.GetCellCount() > 0 &&
      gap_geometry_->CrossGapCount() > 0) {
    FinalizeCrossGapRanges(columns);
  }

  gap_geometry_->SetContentInlineOffsets(column_gap_data_.content_start,
                                         column_gap_data_.content_end);
  gap_geometry_->SetContentBlockOffsets(row_gap_data_.content_start,
                                        row_gap_data_.content_end);

  return gap_geometry_;
}

void GridGapAccumulator::FinalizeMainGapRanges(
    const GridLayoutTrackCollection& rows) {
  CHECK_EQ(rows.Direction(), kForRows);
  wtf_size_t gap_index = 0;
  for (const auto& gap : row_gap_data_.gaps) {
    main_gaps_aggregator_.FinalizeGapSegmentStateRangesFor(
        gap_geometry_->MainGapAt(gap_index), gap.line_index - 1);
    ++gap_index;
  }
  CHECK_EQ(gap_index, gap_geometry_->MainGapCount());
}

void GridGapAccumulator::FinalizeCrossGapRanges(
    const GridLayoutTrackCollection& columns) {
  CHECK_EQ(columns.Direction(), kForColumns);
  wtf_size_t gap_index = 0;
  for (const auto& gap : column_gap_data_.gaps) {
    cross_gaps_aggregator_.FinalizeGapSegmentStateRangesFor(
        gap_geometry_->CrossGapAt(gap_index), gap.line_index - 1);
    ++gap_index;
  }
  CHECK_EQ(gap_index, gap_geometry_->CrossGapCount());
}

}  // namespace blink
