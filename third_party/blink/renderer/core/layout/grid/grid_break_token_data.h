// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_GRID_GRID_BREAK_TOKEN_DATA_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_GRID_GRID_BREAK_TOKEN_DATA_H_

#include "third_party/blink/renderer/core/layout/break_token_algorithm_data.h"
#include "third_party/blink/renderer/core/layout/gap/gap_geometry.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

struct GridItemPlacementData {
  GridItemPlacementData(
      LogicalOffset offset,
      LogicalOffset relative_offset,
      bool has_descendant_that_depends_on_percentage_block_size)
      : offset(offset),
        relative_offset(relative_offset),
        has_descendant_that_depends_on_percentage_block_size(
            has_descendant_that_depends_on_percentage_block_size) {}

  LogicalOffset offset;
  LogicalOffset relative_offset;
  bool has_descendant_that_depends_on_percentage_block_size;
};

struct GridBreakTokenData final : BreakTokenAlgorithmData {
  GridBreakTokenData(
      GridItems&& grid_items,
      GridLayoutSubtree grid_layout_subtree,
      LayoutUnit intrinsic_block_size,
      LayoutUnit offset_in_stitched_container,
      const Vector<GridItemPlacementData>& grid_items_placement_data,
      const Vector<LayoutUnit>& row_offset_adjustments,
      const Vector<EBreakBetween>& row_break_between,
      const HeapVector<Member<LayoutBox>>& oof_children,
      const GapGeometry* full_gap_geometry,
      Vector<wtf_size_t>& track_idx_to_set_idx,
      Vector<wtf_size_t>& column_gaps_segment_ranges_start_indices,
      LayoutUnit cumulative_gap_offset_adjustment,
      wtf_size_t first_unprocessed_row_gap_idx)
      : BreakTokenAlgorithmData(kGridData),
        grid_items(std::move(grid_items)),
        grid_layout_subtree(std::move(grid_layout_subtree)),
        intrinsic_block_size(intrinsic_block_size),
        offset_in_stitched_container(offset_in_stitched_container),
        grid_items_placement_data(grid_items_placement_data),
        row_offset_adjustments(row_offset_adjustments),
        row_break_between(row_break_between),
        oof_children(oof_children),
        full_gap_geometry(full_gap_geometry),
        track_idx_to_set_idx(track_idx_to_set_idx),
        column_gaps_segment_ranges_start_indices(
            column_gaps_segment_ranges_start_indices),
        cumulative_gap_offset_adjustment(cumulative_gap_offset_adjustment),
        first_unprocessed_row_gap_idx(first_unprocessed_row_gap_idx) {}

  void Trace(Visitor* visitor) const override {
    visitor->Trace(grid_items);
    visitor->Trace(oof_children);
    visitor->Trace(full_gap_geometry);
    BreakTokenAlgorithmData::Trace(visitor);
  }

  GridItems grid_items;
  GridLayoutSubtree grid_layout_subtree;
  LayoutUnit intrinsic_block_size;

  // This is similar to |BreakTokenAlgorithmData::consumed_block_size|, however
  // it isn't used for determining the final block-size of the fragment and
  // won't include any block-end padding (this prevents saturation bugs).
  // It also won't include any cloned box decorations.
  LayoutUnit offset_in_stitched_container;

  Vector<GridItemPlacementData> grid_items_placement_data;
  Vector<LayoutUnit> row_offset_adjustments;
  Vector<EBreakBetween> row_break_between;
  HeapVector<Member<LayoutBox>> oof_children;

  // Holds the gap geometry for the unfragmented grid pass before fragmentation
  // is applied. This helps retain the original gap positions, which is
  // important for suppressing row gaps that cross fragmentainer boundaries, or
  // when they are the last content in a fragmentainer, and for building
  // fragment-specific gap geometry.
  Member<const GapGeometry> full_gap_geometry;

  // Represents the mapping where the index in the vector corresponds to the row
  // gap index, and the value at that index is the corresponding set index. It
  // is used to determine the appropriate row offset adjustments for each track.
  Vector<wtf_size_t> track_idx_to_set_idx;

  // Stores the indices for the first range of column gaps segment ranges that
  // should be processed within the current fragmentainer. Each column gap has
  // different segment ranges, hence the need for a Vector to represent each
  // column gap. This is used to make the column gap segment ranges fragment
  // relative. By storing these indices in the break token data, we can begin
  // from the current range in the fragment instead of searching through all the
  // segment ranges to locate the current one in a specific fragment.
  Vector<wtf_size_t> column_gaps_segment_ranges_start_indices;

  // Cumulative offset that tracks how much row gap offsets have been adjusted
  // across fragments as a result of gap suppression. Decreases as further
  // suppression happens due to fragmentation.
  LayoutUnit cumulative_gap_offset_adjustment;

  // The index of the first unprocessed row gap. This is used to keep track of
  // which row gaps have already been processed in previous fragments, so that
  // we can continue processing from this index in subsequent fragments.
  wtf_size_t first_unprocessed_row_gap_idx;
};

template <>
struct DowncastTraits<GridBreakTokenData> {
  static bool AllowFrom(const BreakTokenAlgorithmData& token_data) {
    return token_data.IsGridType();
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_GRID_GRID_BREAK_TOKEN_DATA_H_
