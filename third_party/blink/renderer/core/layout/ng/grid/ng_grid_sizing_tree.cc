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
            parent_layout_data.Columns().GetSetCount());
  DCHECK_LE(item_data_in_parent.row_set_indices.end,
            parent_layout_data.Rows().GetSetCount());
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
                                            ? parent_layout_data->Columns()
                                            : parent_layout_data->Rows();
  const auto& range_indices = is_for_columns_in_parent
                                  ? item_data_in_parent->column_range_indices
                                  : item_data_in_parent->row_range_indices;

  return std::make_unique<NGGridLayoutTrackCollection>(
      parent_track_collection.CreateSubgridCollection(
          range_indices.begin, range_indices.end, track_direction));
}

NGGridSizingTree NGGridSizingTree::CopySubtree(wtf_size_t subtree_root) const {
  DCHECK_LT(subtree_root, sizing_data_.size());

  const wtf_size_t subtree_size = sizing_data_[subtree_root]->subtree_size;
  DCHECK_LE(subtree_root + subtree_size, sizing_data_.size());

  NGGridSizingTree subtree_copy(subtree_size);
  for (wtf_size_t i = 0; i < subtree_size; ++i) {
    auto& copy_data = subtree_copy.CreateSizingData();
    const auto& original_data = *sizing_data_[subtree_root + i];

    copy_data.subtree_size = original_data.subtree_size;
    copy_data.layout_data = original_data.layout_data;
  }
  return subtree_copy;
}

}  // namespace blink
