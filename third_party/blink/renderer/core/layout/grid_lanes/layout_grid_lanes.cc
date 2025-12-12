// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#include "third_party/blink/renderer/core/layout/grid_lanes/layout_grid_lanes.h"

#include "third_party/blink/renderer/core/layout/grid/layout_grid.h"

namespace blink {

LayoutGridLanes::LayoutGridLanes(Element* element) : LayoutBlock(element) {}

const GridLayoutData* LayoutGridLanes::LayoutData() const {
  return LayoutGrid::GetGridLayoutDataFromFragments(this);
}

Vector<LayoutUnit> LayoutGridLanes::GridTrackPositions(
    GridTrackSizingDirection track_direction) const {
  NOT_DESTROYED();
  if (track_direction != StyleRef().GridLanesTrackSizingDirection()) {
    return {};
  }
  return LayoutGrid::ComputeExpandedPositions(track_direction == kForColumns
                                                  ? LayoutData()->Columns()
                                                  : LayoutData()->Rows());
}

LayoutUnit LayoutGridLanes::GridGap(
    GridTrackSizingDirection track_direction) const {
  NOT_DESTROYED();
  return LayoutGrid::ComputeGridGap(LayoutData(), track_direction);
}

LayoutUnit LayoutGridLanes::GridLanesItemOffset(
    GridTrackSizingDirection track_direction) const {
  NOT_DESTROYED();
  // Distribution offset is baked into the `gutter_size` in Grid Lanes.
  return LayoutUnit();
}

bool LayoutGridLanes::HasCachedPlacementData() const {
  // TODO(almaher): Check for !IsGridPlacementDirty() similar to
  // LayoutGrid.
  return !!cached_placement_data_;
}

const GridPlacementData& LayoutGridLanes::CachedPlacementData() const {
  DCHECK(cached_placement_data_);
  return *cached_placement_data_;
}

void LayoutGridLanes::SetCachedPlacementData(
    GridPlacementData&& placement_data) {
  cached_placement_data_ = std::move(placement_data);
}

wtf_size_t LayoutGridLanes::AutoRepeatCountForDirection(
    GridTrackSizingDirection track_direction) const {
  NOT_DESTROYED();
  if (!cached_placement_data_) {
    return 0;
  }
  return cached_placement_data_->AutoRepeatTrackCount(track_direction);
}

wtf_size_t LayoutGridLanes::ExplicitGridStartForDirection(
    GridTrackSizingDirection track_direction) const {
  NOT_DESTROYED();
  if (!cached_placement_data_) {
    return 0;
  }
  return cached_placement_data_->StartOffset(track_direction);
}

wtf_size_t LayoutGridLanes::ExplicitGridEndForDirection(
    GridTrackSizingDirection track_direction) const {
  NOT_DESTROYED();
  if (!cached_placement_data_) {
    return 0;
  }

  return base::checked_cast<wtf_size_t>(
      ExplicitGridStartForDirection(track_direction) +
      cached_placement_data_->ExplicitGridTrackCount(track_direction));
}

Vector<LayoutUnit, 1> LayoutGridLanes::TrackSizesForComputedStyle(
    GridTrackSizingDirection track_direction) const {
  NOT_DESTROYED();
  if (track_direction != StyleRef().GridLanesTrackSizingDirection()) {
    return {};
  }
  return LayoutGrid::CollectTrackSizesForComputedStyle(LayoutData(),
                                                       track_direction);
}

}  // namespace blink
