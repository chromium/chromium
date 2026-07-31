// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#include "third_party/blink/renderer/core/layout/grid_lanes/layout_grid_lanes.h"

#include "third_party/blink/renderer/core/layout/grid/layout_grid.h"
#include "third_party/blink/renderer/platform/text/writing_mode_utils.h"

namespace blink {

namespace {

// Returns which physical edges have overflow for a grid-lanes container.
// `fill-reverse` flips the stacking-axis overflow and `track-reverse` flips
// the grid-axis overflow.
LogicalToPhysical<bool> GetOverflowConverter(const ComputedStyle& style) {
  const bool is_fill_reverse = style.IsReverseGridLanesFillDirection();
  const bool is_track_reverse = style.IsReverseGridLanesTrackDirection();
  const bool is_for_columns =
      style.GridLanesTrackSizingDirection() == kForColumns;

  bool inline_start = false;
  bool inline_end = true;
  bool block_start = false;
  bool block_end = true;

  // `fill-reverse` places items from the end edge during layout, so its
  // overflow is always on the start edge.
  if (is_fill_reverse) {
    if (is_for_columns) {
      std::swap(block_start, block_end);
    } else {
      std::swap(inline_start, inline_end);
    }
  }

  // `track-reverse` reverses the grid axis, so scrollable overflow originates
  // from the start edge.
  if (is_track_reverse) {
    if (is_for_columns) {
      std::swap(inline_start, inline_end);
    } else {
      std::swap(block_start, block_end);
    }
  }

  return LogicalToPhysical(style.GetWritingDirection(), inline_start,
                           inline_end, block_start, block_end);
}

}  // namespace

LayoutGridLanes::LayoutGridLanes(Element* element) : LayoutBlock(element) {}

void LayoutGridLanes::AddChild(LayoutObject* new_child,
                               LayoutObject* before_child) {
  NOT_DESTROYED();
  LayoutBlock::AddChild(new_child, before_child);
  SetGridPlacementDirty(true);
}

void LayoutGridLanes::RemoveChild(LayoutObject* child) {
  NOT_DESTROYED();
  LayoutBlock::RemoveChild(child);
  SetGridPlacementDirty(true);
}

void LayoutGridLanes::StyleDidChange(
    StyleDifference diff,
    const ComputedStyle* old_style,
    const StyleChangeContext& style_change_context) {
  NOT_DESTROYED();
  LayoutBlock::StyleDidChange(diff, old_style, style_change_context);
  if (!old_style) {
    return;
  }

  const ComputedStyle& new_style = StyleRef();

  // The full direction captures the orientation and the fill/track reverse
  // flags, and the packing mode changes how auto-placed items fill the lanes;
  // both change where items are placed.
  if (new_style.GetGridLanesDirection() != old_style->GetGridLanesDirection() ||
      new_style.GridLanesPack() != old_style->GridLanesPack()) {
    SetGridPlacementDirty(true);
    return;
  }

  // The resolved grid axis can flip even when the `grid-lanes-direction`
  // property itself is unchanged.
  const GridTrackSizingDirection track_direction =
      new_style.GridLanesTrackSizingDirection();
  if (track_direction != old_style->GridLanesTrackSizingDirection() ||
      LayoutGrid::GridPlacementInputsDidChange(new_style, *old_style, diff,
                                               track_direction)) {
    SetGridPlacementDirty(true);
  }
}

bool LayoutGridLanes::HasTopOverflow() const {
  NOT_DESTROYED();
  return GetOverflowConverter(StyleRef()).Top();
}

bool LayoutGridLanes::HasLeftOverflow() const {
  NOT_DESTROYED();
  return GetOverflowConverter(StyleRef()).Left();
}

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
  return !!cached_placement_data_;
}

const GridPlacementData& LayoutGridLanes::CachedPlacementData() const {
  DCHECK(cached_placement_data_);
  return *cached_placement_data_;
}

void LayoutGridLanes::SetCachedPlacementData(
    GridPlacementData&& placement_data) {
  cached_placement_data_ = std::move(placement_data);
  SetGridPlacementDirty(false);
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
