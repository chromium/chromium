// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/grid/ng_grid_sizing_tree.h"

namespace blink {

bool NGGridProperties::HasBaseline(
    const GridTrackSizingDirection track_direction) const {
  return (track_direction == kForColumns)
             ? (has_baseline_column ||
                (has_orthogonal_item && has_baseline_row))
             : (has_baseline_row ||
                (has_orthogonal_item && has_baseline_column));
}

bool NGGridProperties::HasFlexibleTrack(
    const GridTrackSizingDirection track_direction) const {
  return (track_direction == kForColumns)
             ? column_properties.HasProperty(
                   TrackSpanProperties::kHasFlexibleTrack)
             : row_properties.HasProperty(
                   TrackSpanProperties::kHasFlexibleTrack);
}

bool NGGridProperties::HasIntrinsicTrack(
    const GridTrackSizingDirection track_direction) const {
  return (track_direction == kForColumns)
             ? column_properties.HasProperty(
                   TrackSpanProperties::kHasIntrinsicTrack)
             : row_properties.HasProperty(
                   TrackSpanProperties::kHasIntrinsicTrack);
}

bool NGGridProperties::IsDependentOnAvailableSize(
    const GridTrackSizingDirection track_direction) const {
  return (track_direction == kForColumns)
             ? column_properties.HasProperty(
                   TrackSpanProperties::kIsDependentOnAvailableSize)
             : row_properties.HasProperty(
                   TrackSpanProperties::kIsDependentOnAvailableSize);
}

bool NGGridProperties::IsSpanningOnlyDefiniteTracks(
    const GridTrackSizingDirection track_direction) const {
  return (track_direction == kForColumns)
             ? !column_properties.HasProperty(
                   TrackSpanProperties::kHasNonDefiniteTrack)
             : !row_properties.HasProperty(
                   TrackSpanProperties::kHasNonDefiniteTrack);
}

NGGridSizingData& NGGridSizingTree::CreateSizingData(
    const NGGridNode& grid,
    const NGGridSizingData* parent_sizing_data,
    const GridItemData* subgrid_data_in_parent) {
  auto* new_sizing_data = MakeGarbageCollected<NGGridSizingData>(
      parent_sizing_data, subgrid_data_in_parent);

  data_lookup_map_.insert(grid.GetLayoutBox(), new_sizing_data);
  sizing_data_.emplace_back(new_sizing_data);
  return *new_sizing_data;
}

NGGridSizingData& NGGridSizingTree::operator[](wtf_size_t index) {
  DCHECK_LT(index, sizing_data_.size());
  return *sizing_data_[index];
}

}  // namespace blink
