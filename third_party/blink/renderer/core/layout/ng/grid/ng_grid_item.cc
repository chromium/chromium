// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/grid/ng_grid_item.h"

#include "third_party/blink/renderer/core/layout/ng/grid/ng_grid_placement.h"

namespace blink {

void GridItemData::SetAlignmentFallback(
    const GridTrackSizingDirection track_direction,
    const ComputedStyle& container_style,
    const bool has_synthesized_baseline) {
  // Alignment fallback is only possible when baseline alignment is specified.
  if (!IsBaselineSpecifiedForDirection(track_direction))
    return;

  auto CanParticipateInBaselineAlignment =
      [&](const ComputedStyle& container_style,
          const GridTrackSizingDirection track_direction) -> bool {
    // "If baseline alignment is specified on a grid item whose size in that
    // axis depends on the size of an intrinsically-sized track (whose size is
    // therefore dependent on both the item’s size and baseline alignment,
    // creating a cyclic dependency), that item does not participate in
    // baseline alignment, and instead uses its fallback alignment as if that
    // were originally specified. For this purpose, <flex> track sizes count
    // as “intrinsically-sized” when the grid container has an indefinite size
    // in the relevant axis."
    // https://drafts.csswg.org/css-grid-2/#row-align
    if (has_synthesized_baseline &&
        (IsSpanningIntrinsicTrack(track_direction) ||
         IsSpanningFlexibleTrack(track_direction))) {
      // Parallel grid items with a synthesized baseline support baseline
      // alignment only of the height doesn't depend on the track size.
      const auto& item_style = node.Style();
      const bool is_parallel_to_baseline_axis =
          (track_direction == kForRows) ==
          IsParallelWritingMode(container_style.GetWritingMode(),
                                item_style.GetWritingMode());
      if (is_parallel_to_baseline_axis) {
        const bool logical_height_depends_on_container =
            item_style.LogicalHeight().IsPercentOrCalc() ||
            item_style.LogicalMinHeight().IsPercentOrCalc() ||
            item_style.LogicalMaxHeight().IsPercentOrCalc() ||
            item_style.LogicalHeight().IsAuto();
        return !logical_height_depends_on_container;
      } else {
        // Orthogonal items with synthesized baselines never support baseline
        // alignment when they span intrinsic or flex tracks.
        return false;
      }
    }
    return true;
  };

  // Set fallback alignment to start edges if an item requests baseline
  // alignment but does not meet requirements for it.
  if (!CanParticipateInBaselineAlignment(container_style, track_direction)) {
    if (track_direction == kForColumns &&
        inline_axis_alignment == AxisEdge::kBaseline) {
      inline_axis_alignment_fallback = AxisEdge::kStart;
    } else if (track_direction == kForRows &&
               block_axis_alignment == AxisEdge::kBaseline) {
      block_axis_alignment_fallback = AxisEdge::kStart;
    }
  } else {
    // Reset the alignment fallback if eligibility has changed.
    if (track_direction == kForColumns &&
        inline_axis_alignment_fallback.has_value()) {
      inline_axis_alignment_fallback.reset();
    } else if (track_direction == kForRows &&
               block_axis_alignment_fallback.has_value()) {
      block_axis_alignment_fallback.reset();
    }
  }
}

void GridItemData::ComputeSetIndices(
    const NGGridLayoutAlgorithmTrackCollection& track_collection) {
  DCHECK(!IsOutOfFlow());
  GridItemIndices range_indices = RangeIndices(track_collection.Direction());

#if DCHECK_IS_ON()
  const wtf_size_t start_line = StartLine(track_collection.Direction());
  const wtf_size_t end_line = EndLine(track_collection.Direction());
  DCHECK_LE(end_line, track_collection.EndLineOfImplicitGrid());
  DCHECK_LT(start_line, end_line);

  // Check the range index caching was correct by running a binary search.
  DCHECK_EQ(track_collection.RangeIndexFromTrackNumber(start_line),
            range_indices.begin);
  DCHECK_EQ(track_collection.RangeIndexFromTrackNumber(end_line - 1),
            range_indices.end);
#endif

  auto& set_indices =
      track_collection.IsForColumns() ? column_set_indices : row_set_indices;
  set_indices.begin =
      track_collection.RangeStartingSetIndex(range_indices.begin);
  set_indices.end = track_collection.RangeStartingSetIndex(range_indices.end) +
                    track_collection.RangeSetCount(range_indices.end);

  DCHECK_LE(set_indices.end, track_collection.SetCount());
  DCHECK_LT(set_indices.begin, set_indices.end);
}

void GridItemData::ComputeOutOfFlowItemPlacement(
    const NGGridLayoutAlgorithmTrackCollection& track_collection,
    const NGGridPlacement& grid_placement) {
  DCHECK(IsOutOfFlow());

  auto& start_offset = track_collection.IsForColumns()
                           ? column_placement.offset_in_range.begin
                           : row_placement.offset_in_range.begin;
  auto& end_offset = track_collection.IsForColumns()
                         ? column_placement.offset_in_range.end
                         : row_placement.offset_in_range.end;

  if (IsGridContainingBlock()) {
    grid_placement.ResolveOutOfFlowItemGridLines(track_collection, node.Style(),
                                                 &start_offset, &end_offset);
  } else {
    start_offset = kNotFound;
    end_offset = kNotFound;
  }

#if DCHECK_IS_ON()
  if (start_offset != kNotFound && end_offset != kNotFound) {
    DCHECK_LE(end_offset, track_collection.EndLineOfImplicitGrid());
    DCHECK_LT(start_offset, end_offset);
  } else if (start_offset != kNotFound) {
    DCHECK_LE(start_offset, track_collection.EndLineOfImplicitGrid());
  } else if (end_offset != kNotFound) {
    DCHECK_LE(end_offset, track_collection.EndLineOfImplicitGrid());
  }
#endif

  // We only calculate the range placement if the line was not defined as 'auto'
  // and it is within the bounds of the grid, since an out of flow item cannot
  // create grid lines.
  const wtf_size_t range_count = track_collection.RangeCount();
  auto& start_range_index = track_collection.IsForColumns()
                                ? column_placement.range_index.begin
                                : row_placement.range_index.begin;
  if (start_offset != kNotFound) {
    if (!range_count) {
      // An undefined and empty grid has a single start/end grid line and no
      // ranges. Therefore, if the start offset isn't 'auto', the only valid
      // offset is zero.
      DCHECK_EQ(start_offset, 0u);
      start_range_index = 0;
    } else {
      // If the start line of an out of flow item is the last line of the grid,
      // we can just subtract one unit to the range count.
      start_range_index =
          (start_offset < track_collection.EndLineOfImplicitGrid())
              ? track_collection.RangeIndexFromTrackNumber(start_offset)
              : range_count - 1;
      start_offset -= track_collection.RangeTrackNumber(start_range_index);
    }
  }

  auto& end_range_index = track_collection.IsForColumns()
                              ? column_placement.range_index.end
                              : row_placement.range_index.end;
  if (end_offset != kNotFound) {
    if (!range_count) {
      // Similarly to the start offset, if we have an undefined, empty grid and
      // the end offset isn't 'auto', the only valid offset is zero.
      DCHECK_EQ(end_offset, 0u);
      end_range_index = 0;
    } else {
      // If the end line of an out of flow item is the first line of the grid,
      // then |last_spanned_range| is set to zero.
      end_range_index =
          end_offset
              ? track_collection.RangeIndexFromTrackNumber(end_offset - 1)
              : 0;
      end_offset -= track_collection.RangeTrackNumber(end_range_index);
    }
  }
}

}  // namespace blink
