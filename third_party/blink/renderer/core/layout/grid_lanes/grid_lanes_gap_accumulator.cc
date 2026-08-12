// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/grid_lanes/grid_lanes_gap_accumulator.h"

#include <algorithm>

#include "third_party/blink/renderer/core/layout/gap/gap_geometry.h"
#include "third_party/blink/renderer/core/layout/grid/grid_layout_utils.h"
#include "third_party/blink/renderer/core/layout/grid/grid_track_collection.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_vector.h"

namespace blink {

namespace {

const GridLanesItemData* FirstItemInForwardStackingOrder(
    const GridLaneData* lane_data) {
  if (!lane_data || lane_data->item_data.empty()) {
    return nullptr;
  }

  const GridLanesItemData* first = lane_data->item_data.front();
  for (const GridLanesItemData* packed : first->items_densely_packed_above) {
    if (packed->ForwardStackingStart() < first->ForwardStackingStart() ||
        (packed->ForwardStackingStart() == first->ForwardStackingStart() &&
         packed->PlacementSequence() < first->PlacementSequence())) {
      first = packed;
    }
  }
  return first;
}

}  // namespace

GridLanesGapAccumulator::GridLanesGapAccumulator()
    : gap_geometry_(MakeGarbageCollected<GapGeometry>(
          GapGeometry::ContainerType::kGridLanes)) {}

void GridLanesGapAccumulator::BuildMainGaps(
    const GridLayoutTrackCollection& grid_axis_tracks) {
  collapsed_track_indexes_ = &grid_axis_tracks.CollapsedTrackIndexes();
  raw_track_count_ = grid_axis_tracks.EndLineOfImplicitGrid();
  const GridTrackGapData gap_data = BuildGridTrackGapData(
      grid_axis_tracks, GridTrackGapType::kMain, *gap_geometry_);
  const wtf_size_t track_count = UncollapsedTrackCount();
  if (track_count > 0) {
    CHECK_EQ(gap_geometry_->MainGapCount(), track_count - 1);
  }

  const LayoutUnit gutter_size = grid_axis_tracks.GutterSize();

  const GridTrackSizingDirection grid_axis_direction =
      grid_axis_tracks.Direction();
  gap_geometry_->SetMainDirection(grid_axis_direction);
  if (grid_axis_direction == kForColumns) {
    gap_geometry_->SetInlineGapSize(gutter_size);
    gap_geometry_->SetContentInlineOffsets(gap_data.content_start,
                                           gap_data.content_end);
  } else {
    gap_geometry_->SetBlockGapSize(gutter_size);
    gap_geometry_->SetContentBlockOffsets(gap_data.content_start,
                                          gap_data.content_end);
  }
}

LayoutUnit GridLanesGapAccumulator::FinalGutterCenter(
    const GridLanesItemData& item,
    const GridLanesGapGeometryState& state) const {
  const LayoutUnit half_stacking_gap = state.stacking_gap / 2;
  const LayoutUnit forward_center =
      item.ForwardStackingStart() - half_stacking_gap;
  return state.is_fill_reverse
             ? state.border_scrollbar_padding_start +
                   state.effective_stacking_size - forward_center +
                   state.content_alignment_translation
             : state.border_scrollbar_padding_start + forward_center +
                   state.content_alignment_translation;
}

wtf_size_t GridLanesGapAccumulator::UncollapsedTrackCount() const {
  CHECK(collapsed_track_indexes_);
  CHECK_LE(collapsed_track_indexes_->size(), raw_track_count_);
  return raw_track_count_ - collapsed_track_indexes_->size();
}

const GapGeometry* GridLanesGapAccumulator::FinalizeGapGeometry(
    const GridLanesDataVector& grid_lanes,
    const GridLanesGapGeometryState& state,
    LayoutUnit stacking_content_start,
    LayoutUnit stacking_content_end) {
  const bool is_for_columns = gap_geometry_->GetMainDirection() == kForColumns;

  if (is_for_columns) {
    gap_geometry_->SetContentBlockOffsets(stacking_content_start,
                                          stacking_content_end);
    gap_geometry_->SetBlockGapSize(state.stacking_gap);
  } else {
    gap_geometry_->SetContentInlineOffsets(stacking_content_start,
                                           stacking_content_end);
    gap_geometry_->SetInlineGapSize(state.stacking_gap);
  }

  BuildCrossGaps(grid_lanes, state);

  if (gap_geometry_->MainGapCount() == 0 &&
      gap_geometry_->CrossGapCount() == 0) {
    return nullptr;
  }

  return gap_geometry_;
}

void GridLanesGapAccumulator::MaybeAddCrossGapForTrackEntry(
    const GridLanesItemData& item,
    const GridLanesItemData* first_item_in_track,
    wtf_size_t compact_track_index,
    const GridLanesGapGeometryState& state) {
  if (&item == first_item_in_track) {
    return;
  }

  const LayoutUnit center = FinalGutterCenter(item, state);
  const LayoutUnit grid_axis_offset =
      gap_geometry_->GridAxisOffsetForLaneBoundary(compact_track_index);
  const bool is_for_columns = gap_geometry_->GetMainDirection() == kForColumns;
  const LogicalOffset offset = is_for_columns
                                   ? LogicalOffset(grid_axis_offset, center)
                                   : LogicalOffset(center, grid_axis_offset);
  const wtf_size_t cross_gap_index = gap_geometry_->CrossGapCount();
  gap_geometry_->AddCrossGap(offset);

  // Associate this `CrossGap` with its neighboring `MainGap`s. Track
  // `compact_track_index` sits before `MainGap[compact_track_index]` and after
  // `MainGap[compact_track_index - 1]`.
  const wtf_size_t main_gap_count = gap_geometry_->MainGapCount();
  if (compact_track_index < main_gap_count) {
    gap_geometry_->MainGapAt(compact_track_index)
        .IncrementRangeOfCrossGapsBefore(cross_gap_index);
  }
  if (compact_track_index > 0) {
    gap_geometry_->MainGapAt(compact_track_index - 1)
        .IncrementRangeOfCrossGapsAfter(cross_gap_index);
  }
}

void GridLanesGapAccumulator::AddCrossGapsForPackedItems(
    const GridLanesItemData& item_below,
    const GridLanesItemData* first_item_in_track,
    wtf_size_t compact_track_index,
    const GridLanesGapGeometryState& state) {
  if (item_below.items_densely_packed_above.empty()) {
    return;
  }

  HeapVector<Member<GridLanesItemData>> packed_items =
      item_below.items_densely_packed_above;
  std::sort(packed_items.begin(), packed_items.end(),
            [&](const GridLanesItemData* a, const GridLanesItemData* b) {
              const LayoutUnit center_a = FinalGutterCenter(*a, state);
              const LayoutUnit center_b = FinalGutterCenter(*b, state);
              if (center_a != center_b) {
                return center_a < center_b;
              }
              // Placement order breaks ties between equal final gutter centers.
              return a->PlacementSequence() < b->PlacementSequence();
            });
  for (const GridLanesItemData* packed : packed_items) {
    MaybeAddCrossGapForTrackEntry(*packed, first_item_in_track,
                                  compact_track_index, state);
  }
}

void GridLanesGapAccumulator::BuildCrossGaps(
    const GridLanesDataVector& grid_lanes,
    const GridLanesGapGeometryState& state) {
  // Placement is skipped when the container has no in-flow items. Main gaps
  // still exist, but there is no lane graph from which to build cross gaps.
  if (grid_lanes.empty()) {
    return;
  }

  CHECK_EQ(grid_lanes.size(), raw_track_count_);
  CHECK(collapsed_track_indexes_);

  wtf_size_t compact_track_index = 0;
  wtf_size_t collapsed_track_index = 0;
  for (wtf_size_t raw_track_index = 0; raw_track_index < grid_lanes.size();
       ++raw_track_index) {
    if (collapsed_track_index < collapsed_track_indexes_->size() &&
        collapsed_track_indexes_->at(collapsed_track_index) ==
            raw_track_index) {
      ++collapsed_track_index;
      continue;
    }
    const GridLaneData* lane_data = grid_lanes[raw_track_index];
    const GridLanesItemData* first_item_in_track =
        FirstItemInForwardStackingOrder(lane_data);
    if (lane_data) {
      const auto& item_data = lane_data->item_data;
      if (!state.is_fill_reverse) {
        // Visit packed entries before the direct entry below them.
        for (const GridLanesItemData* direct : item_data) {
          AddCrossGapsForPackedItems(*direct, first_item_in_track,
                                     compact_track_index, state);
          MaybeAddCrossGapForTrackEntry(*direct, first_item_in_track,
                                        compact_track_index, state);
        }
      } else {
        // Fill-reverse reflects offsets, so walk direct entries backward and
        // visit each direct entry before its packed group. This keeps each
        // track run in increasing final coordinate order.
        for (const GridLanesItemData* direct : base::Reversed(item_data)) {
          MaybeAddCrossGapForTrackEntry(*direct, first_item_in_track,
                                        compact_track_index, state);
          AddCrossGapsForPackedItems(*direct, first_item_in_track,
                                     compact_track_index, state);
        }
      }
    }
    ++compact_track_index;
  }
  CHECK_EQ(compact_track_index, UncollapsedTrackCount());
  CHECK_EQ(collapsed_track_index, collapsed_track_indexes_->size());
}

}  // namespace blink
