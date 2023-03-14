// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/grid/ng_grid_sizing_tree.h"

namespace blink {

std::unique_ptr<NGGridLayoutTrackCollection>
NGSubgriddedItemData::CreateSubgridCollection(
    GridTrackSizingDirection track_direction) const {
  DCHECK(item_data_in_parent_->IsSubgrid());

  const bool is_for_columns_in_parent =
      item_data_in_parent_->is_parallel_with_root_grid
          ? track_direction == kForColumns
          : track_direction == kForRows;

  const auto& parent_track_collection = is_for_columns_in_parent
                                            ? parent_layout_data_->Columns()
                                            : parent_layout_data_->Rows();
  const auto& range_indices = is_for_columns_in_parent
                                  ? item_data_in_parent_->column_range_indices
                                  : item_data_in_parent_->row_range_indices;

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
