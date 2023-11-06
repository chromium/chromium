// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#include "third_party/blink/renderer/core/layout/grid/grid_sizing_tree.h"

namespace blink {

GridSizingTree GridSizingTree::CopyForFragmentation() const {
  GridSizingTree tree_copy;
  tree_copy.tree_data_.ReserveInitialCapacity(tree_data_.size());

  for (const auto& sizing_data : tree_data_) {
    DCHECK(sizing_data);
    auto& sizing_data_copy = tree_copy.CreateSizingData();
    sizing_data_copy = *sizing_data;
  }
  return tree_copy;
}

scoped_refptr<const GridLayoutTree> GridSizingTree::FinalizeTree() const {
  auto layout_tree = base::MakeRefCounted<GridLayoutTree>(tree_data_.size());

  for (const auto& grid_tree_node : tree_data_) {
    layout_tree->Append(grid_tree_node->layout_data,
                        grid_tree_node->subtree_size);
  }
  return layout_tree;
}

GridSizingTree::GridTreeNode& GridSizingTree::CreateSizingData(
    const SubgriddedItemData& subgrid_data) {
  if (subgrid_data) {
    const auto* subgrid_layout_box = subgrid_data->node.GetLayoutBox();

    DCHECK(!subgrid_index_lookup_map_.Contains(subgrid_layout_box));
    subgrid_index_lookup_map_.insert(subgrid_layout_box, tree_data_.size());
  }
  return *tree_data_.emplace_back(std::make_unique<GridTreeNode>());
}

void GridSizingTree::AddSubgriddedItemLookupData(
    SubgriddedItemData&& subgridded_item_data) {
  const auto* item_layout_box = subgridded_item_data->node.GetLayoutBox();

  DCHECK(!subgridded_item_data_lookup_map_.Contains(item_layout_box));
  subgridded_item_data_lookup_map_.insert(item_layout_box,
                                          std::move(subgridded_item_data));
}

SubgriddedItemData GridSizingTree::LookupSubgriddedItemData(
    const GridItemData& grid_item) const {
  const auto* item_layout_box = grid_item.node.GetLayoutBox();

  DCHECK(subgridded_item_data_lookup_map_.Contains(item_layout_box));
  return subgridded_item_data_lookup_map_.at(item_layout_box);
}

wtf_size_t GridSizingTree::LookupSubgridIndex(
    const GridItemData& subgrid_data) const {
  const auto* subgrid_layout_box = subgrid_data.node.GetLayoutBox();

  DCHECK(subgrid_index_lookup_map_.Contains(subgrid_layout_box));
  return subgrid_index_lookup_map_.at(subgrid_layout_box);
}

}  // namespace blink
