// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/grid/ng_grid_sizing_tree.h"

namespace blink {

bool NGGridSizingData::MustBuildLayoutData(
    GridTrackSizingDirection track_direction) const {
  return !subgrid_data_in_parent ||
         ((track_direction == kForColumns)
              ? !subgrid_data_in_parent->has_subgridded_columns
              : !subgrid_data_in_parent->has_subgridded_rows);
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
