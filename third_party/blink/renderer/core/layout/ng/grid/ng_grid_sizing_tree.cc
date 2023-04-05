// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/grid/ng_grid_sizing_tree.h"

namespace blink {

std::unique_ptr<NGGridLayoutTrackCollection>
NGSubgriddedItemData::CreateSubgridCollection(
    GridTrackSizingDirection track_direction) const {
  DCHECK(item_data_in_parent_ && item_data_in_parent_->IsSubgrid());

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
  auto layout_tree = base::MakeRefCounted<NGGridLayoutTree>(tree_data_.size());
  for (const auto& grid_tree_node : tree_data_) {
    layout_tree->Append(grid_tree_node->layout_data,
                        grid_tree_node->subtree_size);
  }
  return layout_tree;
}

NGGridSizingTree::GridTreeNode& NGGridSizingTree::CreateSizingData(
    const NGSubgriddedItemData& subgrid_data) {
  // Don't add this subgrid's index to the lookup map if it doesn't have a
  // standalone axis, as we are not going to compute its contribution size.
  if (subgrid_data && subgrid_data->HasStandaloneAndSubgriddedAxis()) {
    const auto* subgrid_layout_box = subgrid_data->node.GetLayoutBox();

    if (!subgrid_index_lookup_map_) {
      subgrid_index_lookup_map_ = MakeGarbageCollected<SubgridIndexLookupMap>();
    }

    DCHECK(!subgrid_index_lookup_map_->Contains(subgrid_layout_box));
    subgrid_index_lookup_map_->insert(subgrid_layout_box, tree_data_.size());
  }
  return *tree_data_.emplace_back(std::make_unique<GridTreeNode>());
}

void NGGridSizingTree::AddSubgriddedItemLookupData(
    NGSubgriddedItemData&& subgridded_item_data) {
  const auto* item_layout_box = subgridded_item_data->node.GetLayoutBox();

  if (!subgridded_item_data_lookup_map_) {
    subgridded_item_data_lookup_map_ =
        MakeGarbageCollected<SubgriddedItemDataLookupMap>();
  }

  DCHECK(!subgridded_item_data_lookup_map_->Contains(item_layout_box));
  subgridded_item_data_lookup_map_->insert(item_layout_box,
                                           std::move(subgridded_item_data));
}

NGSubgriddedItemData NGGridSizingTree::LookupSubgriddedItemData(
    const GridItemData& grid_item) const {
  const auto* item_layout_box = grid_item.node.GetLayoutBox();

  DCHECK(subgridded_item_data_lookup_map_ &&
         subgridded_item_data_lookup_map_->Contains(item_layout_box));
  return subgridded_item_data_lookup_map_->at(item_layout_box);
}

wtf_size_t NGGridSizingTree::LookupSubgridIndex(
    const GridItemData& subgrid_data) const {
  const auto* subgrid_layout_box = subgrid_data.node.GetLayoutBox();

  DCHECK(subgrid_index_lookup_map_ &&
         subgrid_index_lookup_map_->Contains(subgrid_layout_box));
  return subgrid_index_lookup_map_->at(subgrid_layout_box);
}

}  // namespace blink
