// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/grid/ng_grid_sizing_tree.h"

namespace blink {

NGGridItemSizingData::NGGridItemSizingData(
    const GridItemData& item_data_in_parent,
    const NGGridLayoutData& parent_layout_data)
    : item_data_in_parent(&item_data_in_parent),
      parent_layout_data(&parent_layout_data) {
  DCHECK_LE(item_data_in_parent.column_set_indices.end,
            parent_layout_data.Columns()->GetSetCount());
  DCHECK_LE(item_data_in_parent.row_set_indices.end,
            parent_layout_data.Rows()->GetSetCount());
}

std::unique_ptr<NGGridLayoutTrackCollection>
NGGridItemSizingData::CreateSubgridCollection(
    GridTrackSizingDirection track_direction) const {
  DCHECK(item_data_in_parent->IsSubgrid());

  const bool is_for_columns_in_parent =
      item_data_in_parent->is_parallel_with_root_grid
          ? track_direction == kForColumns
          : track_direction == kForRows;

  const auto& parent_track_collection = is_for_columns_in_parent
                                            ? *parent_layout_data->Columns()
                                            : *parent_layout_data->Rows();
  const auto& range_indices = is_for_columns_in_parent
                                  ? item_data_in_parent->column_range_indices
                                  : item_data_in_parent->row_range_indices;

  return std::make_unique<NGGridLayoutTrackCollection>(
      parent_track_collection.CreateSubgridCollection(
          range_indices.begin, range_indices.end, track_direction));
}

}  // namespace blink
