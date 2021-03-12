// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/grid/layout_ng_grid.h"
#include "third_party/blink/renderer/core/layout/ng/ng_layout_result.h"

namespace blink {

LayoutNGGrid::LayoutNGGrid(Element* element)
    : LayoutNGMixin<LayoutBlock>(element) {}

void LayoutNGGrid::UpdateBlockLayout(bool relayout_children) {
  if (IsOutOfFlowPositioned()) {
    UpdateOutOfFlowBlockLayout();
    return;
  }

  UpdateInFlowBlockLayout();
}

const LayoutNGGridInterface* LayoutNGGrid::ToLayoutNGGridInterface() const {
  NOT_DESTROYED();
  return this;
}

size_t LayoutNGGrid::ExplicitGridStartForDirection(
    GridTrackSizingDirection direction) const {
  NOT_DESTROYED();
  const auto* grid_data = GetGridData();
  if (!grid_data)
    return 0;
  return (direction == kForRows) ? grid_data->row_start
                                 : grid_data->column_start;
}

size_t LayoutNGGrid::ExplicitGridEndForDirection(
    GridTrackSizingDirection direction) const {
  NOT_DESTROYED();
  const auto* grid_data = GetGridData();
  if (!grid_data)
    return 0;

  const size_t explicit_grid_start = ExplicitGridStartForDirection(direction);
  const auto& geometry = (direction == kForRows) ? grid_data->row_geometry
                                                 : grid_data->column_geometry;

  return explicit_grid_start + geometry.total_track_count;
}

size_t LayoutNGGrid::AutoRepeatCountForDirection(
    GridTrackSizingDirection direction) const {
  NOT_DESTROYED();
  const auto* grid_data = GetGridData();
  if (!grid_data)
    return 0;
  return (direction == kForRows) ? grid_data->row_auto_repeat_count
                                 : grid_data->column_auto_repeat_count;
}

LayoutUnit LayoutNGGrid::GridGap(GridTrackSizingDirection direction) const {
  NOT_DESTROYED();
  const auto* grid_data = GetGridData();
  if (!grid_data)
    return LayoutUnit();
  return (direction == kForRows) ? grid_data->row_geometry.gutter_size
                                 : grid_data->column_geometry.gutter_size;
}

LayoutUnit LayoutNGGrid::GridItemOffset(
    GridTrackSizingDirection direction) const {
  NOT_DESTROYED();
  const auto* grid_data = GetGridData();
  const auto& geometry = (direction == kForRows) ? grid_data->row_geometry
                                                 : grid_data->column_geometry;
  DCHECK(geometry.sets.size());
  return geometry.sets[0].offset;
}

Vector<LayoutUnit> LayoutNGGrid::TrackSizesForComputedStyle(
    GridTrackSizingDirection direction) const {
  NOT_DESTROYED();
  Vector<LayoutUnit> tracks;
  const auto* grid_data = GetGridData();
  if (!grid_data)
    return tracks;
  const auto& geometry = (direction == kForRows) ? grid_data->row_geometry
                                                 : grid_data->column_geometry;

  const LayoutUnit gutter_size = geometry.gutter_size;
  tracks.ReserveInitialCapacity(
      std::min<wtf_size_t>(geometry.total_track_count, kGridMaxTracks));
  for (const auto& range : geometry.ranges) {
    Vector<LayoutUnit> track_sizes =
        ComputeTrackSizesInRange(direction, range, gutter_size);
    for (wtf_size_t track_in_range = 0; track_in_range < range.track_count;
         ++track_in_range) {
      tracks.emplace_back(track_sizes[track_in_range % range.set_count]);
      // Respect track count limit.
      if (tracks.size() > kGridMaxTracks)
        return tracks;
    }
  }
  // TODO(janewman): Handle collapsed tracks when we have auto repetitions.

  return tracks;
}

Vector<LayoutUnit> LayoutNGGrid::RowPositions() const {
  NOT_DESTROYED();
  return ComputeExpandedPositions(kForRows);
}

Vector<LayoutUnit> LayoutNGGrid::ColumnPositions() const {
  NOT_DESTROYED();
  return ComputeExpandedPositions(kForColumns);
}

Vector<LayoutUnit> LayoutNGGrid::ComputeExpandedPositions(
    GridTrackSizingDirection direction) const {
  Vector<LayoutUnit> expanded_positions;
  const auto* grid_data = GetGridData();
  if (!grid_data)
    return expanded_positions;

  const NGGridData::TrackCollectionGeometry& geometry =
      (direction == kForRows) ? grid_data->row_geometry
                              : grid_data->column_geometry;

  const Vector<LayoutUnit> track_sizes = TrackSizesForComputedStyle(direction);
  const LayoutUnit gutter = geometry.gutter_size;
  expanded_positions.ReserveInitialCapacity(track_sizes.size() + 1);
  LayoutUnit current_offset = geometry.sets[0].offset;
  expanded_positions.emplace_back(current_offset);
  for (LayoutUnit track_size : track_sizes) {
    current_offset += track_size;
    // Don't add gap to the last offset.
    if (expanded_positions.size() < track_sizes.size())
      current_offset += gutter;
    expanded_positions.emplace_back(current_offset);
  }
  return expanded_positions;
}

const NGGridData* LayoutNGGrid::GetGridData() const {
  const NGLayoutResult* cached_layout_result = GetCachedLayoutResult();
  return cached_layout_result ? cached_layout_result->GridData() : nullptr;
}

// See comment above |NGGridData| for explanation on why we can't just divide
// the set sizes by their track count.
Vector<LayoutUnit> LayoutNGGrid::ComputeTrackSizesInRange(
    GridTrackSizingDirection direction,
    const NGGridData::RangeData range,
    LayoutUnit gutter_size) const {
  Vector<LayoutUnit> track_sizes;
  const auto* grid_data = GetGridData();
  if (!grid_data)
    return track_sizes;

  const NGGridData::TrackCollectionGeometry& geometry =
      (direction == kForRows) ? grid_data->row_geometry
                              : grid_data->column_geometry;
  const wtf_size_t ending_set_index =
      range.starting_set_index + range.set_count;
  track_sizes.ReserveInitialCapacity(range.set_count);
  for (wtf_size_t set_index = range.starting_set_index;
       set_index < ending_set_index; ++set_index) {
    // Set information is stored as offsets. To determine the size of a single
    // track in a givent set, first determine the total size the set takes up by
    // finding the difference between the offsets.
    const wtf_size_t set_track_count = geometry.sets[set_index + 1].track_count;
    LayoutUnit set_size =
        (geometry.sets[set_index + 1].offset - geometry.sets[set_index].offset);
    DCHECK_GE(set_track_count, 0u);
    // Once we have determined the size of the set, we can find the size of a
    // given track by dividing the |set_size| by the |set_track_count|.
    // In some situations, this will leave a remainder, but rather than try to
    // distribute the space unequally between tracks, discard it to prefer equal
    // length tracks.
    LayoutUnit track_size = (set_size / set_track_count) - gutter_size;
    track_sizes.emplace_back(track_size);
  }
  return track_sizes;
}

}  // namespace blink
