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

scoped_refptr<const NGGridLayoutTree> NGGridSizingTree::FinalizeTree() const {
  auto layout_tree =
      base::MakeRefCounted<NGGridLayoutTree>(sizing_data_.size());
  for (const auto& sizing_data : sizing_data_) {
    layout_tree->Append(sizing_data->layout_data, sizing_data->subtree_size);
  }
  return layout_tree;
}

}  // namespace blink
