// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#include "third_party/blink/renderer/core/layout/masonry/layout_masonry.h"

#include "third_party/blink/renderer/core/layout/grid/layout_grid.h"

namespace blink {

LayoutMasonry::LayoutMasonry(Element* element) : LayoutBlock(element) {}

const GridLayoutData* LayoutMasonry::LayoutData() const {
  return LayoutGrid::GetGridLayoutDataFromFragments(this);
}

Vector<LayoutUnit> LayoutMasonry::GridTrackPositions(
    GridTrackSizingDirection track_direction) const {
  NOT_DESTROYED();
  if (track_direction != StyleRef().MasonryTrackSizingDirection()) {
    return {};
  }
  return LayoutGrid::ComputeExpandedPositions(track_direction == kForColumns
                                                  ? LayoutData()->Columns()
                                                  : LayoutData()->Rows());
}

LayoutUnit LayoutMasonry::GridGap(
    GridTrackSizingDirection track_direction) const {
  NOT_DESTROYED();
  return LayoutGrid::ComputeGridGap(LayoutData(), track_direction);
}

LayoutUnit LayoutMasonry::MasonryItemOffset(
    GridTrackSizingDirection track_direction) const {
  NOT_DESTROYED();
  // Distribution offset is baked into the `gutter_size` in Masonry.
  return LayoutUnit();
}

bool LayoutMasonry::HasCachedPlacementData() const {
  // TODO(almaher): Check for !IsGridPlacementDirty() similar to
  // LayoutGrid.
  return !!cached_placement_data_;
}

const GridPlacementData& LayoutMasonry::CachedPlacementData() const {
  DCHECK(cached_placement_data_);
  return *cached_placement_data_;
}

void LayoutMasonry::SetCachedPlacementData(GridPlacementData&& placement_data) {
  cached_placement_data_ = std::move(placement_data);
}

wtf_size_t LayoutMasonry::AutoRepeatCountForDirection(
    GridTrackSizingDirection track_direction) const {
  NOT_DESTROYED();
  if (!cached_placement_data_) {
    return 0;
  }
  return cached_placement_data_->AutoRepeatTrackCount(track_direction);
}

wtf_size_t LayoutMasonry::ExplicitGridStartForDirection(
    GridTrackSizingDirection track_direction) const {
  NOT_DESTROYED();
  if (!cached_placement_data_) {
    return 0;
  }
  return cached_placement_data_->StartOffset(track_direction);
}

wtf_size_t LayoutMasonry::ExplicitGridEndForDirection(
    GridTrackSizingDirection track_direction) const {
  NOT_DESTROYED();
  if (!cached_placement_data_) {
    return 0;
  }

  return base::checked_cast<wtf_size_t>(
      ExplicitGridStartForDirection(track_direction) +
      cached_placement_data_->ExplicitGridTrackCount(track_direction));
}

Vector<LayoutUnit, 1> LayoutMasonry::TrackSizesForComputedStyle(
    GridTrackSizingDirection track_direction) const {
  NOT_DESTROYED();
  if (track_direction != StyleRef().MasonryTrackSizingDirection()) {
    return {};
  }
  return LayoutGrid::CollectTrackSizesForComputedStyle(LayoutData(),
                                                       track_direction);
}

}  // namespace blink
