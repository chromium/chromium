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
    tree_copy.tree_data_.emplace_back(
        MakeGarbageCollected<GridTreeNode>(*sizing_data));
  }
  return tree_copy;
}

GridLayoutTreePtr GridSizingTree::FinalizeTree() const {
  Vector<GridLayoutTree::GridTreeNode, 16> layout_tree_data;

  layout_tree_data.ReserveInitialCapacity(tree_data_.size());
  for (const auto& grid_tree_node : tree_data_) {
    layout_tree_data.emplace_back(grid_tree_node->layout_data,
                                  grid_tree_node->subtree_size);
  }

  for (wtf_size_t i = layout_tree_data.size(); i; --i) {
    auto& subtree_data = layout_tree_data[i - 1];

    if (subtree_data.has_unresolved_geometry) {
      continue;
    }

    const wtf_size_t next_subtree_index = i + subtree_data.subtree_size - 1;
    for (wtf_size_t j = i;
         !subtree_data.has_unresolved_geometry && j < next_subtree_index;
         j += layout_tree_data[j].subtree_size) {
      subtree_data.has_unresolved_geometry =
          layout_tree_data[j].has_unresolved_geometry;
    }
  }
  return base::MakeRefCounted<GridLayoutTree>(std::move(layout_tree_data));
}

GridSizingTree::GridTreeNode& GridSizingTree::CreateSizingTreeNode(
    const BlockNode& grid_node,
    GridItems* non_subgridded_items,
    bool has_standalone_columns,
    bool has_standalone_rows) {
#if DCHECK_IS_ON()
  // In debug mode, we want to insert the root grid node into the lookup map
  // since it will be queried by `GridSizingSubtree::HasValidRootFor`.
  const bool needs_to_insert_root_grid_for_lookup = true;
#else
  const bool needs_to_insert_root_grid_for_lookup = !tree_data_.empty();
#endif

  const auto current_subgrid_index = tree_data_.size();
  if (needs_to_insert_root_grid_for_lookup) {
    const auto* grid_layout_box = grid_node.GetLayoutBox();

    DCHECK(!subgrid_index_lookup_map_.Contains(grid_layout_box));
    subgrid_index_lookup_map_.insert(grid_layout_box, current_subgrid_index);
  }

  auto* tree_node = MakeGarbageCollected<GridTreeNode>();
  tree_node->writing_mode = grid_node.Style().GetWritingMode();
  tree_node->SetGridItems(non_subgridded_items);

  for (wtf_size_t current_item_index = 0;
       const auto& grid_item : tree_node->GetGridItems()) {
    // We don't want to add lookup data for grid items that are not going to be
    // subgridded to the parent grid. We need to check for both axes:
    //   - If it's standalone, then this subgrid's items won't be subgridded.
    //   - Otherwise, if the grid item is a subgrid itself and its respective
    //   axis is also subgridded, we won't need its lookup data.
    if ((has_standalone_columns || grid_item.has_subgridded_columns) &&
        (has_standalone_rows || grid_item.has_subgridded_rows)) {
      ++current_item_index;
      continue;
    }

    const auto* item_layout_box = grid_item.node.GetLayoutBox();
    const SubgriddedItemIndices subgridded_item_indices(current_item_index++,
                                                        current_subgrid_index);

    DCHECK(!subgridded_item_data_lookup_map_.Contains(item_layout_box));
    subgridded_item_data_lookup_map_.insert(item_layout_box,
                                            subgridded_item_indices);
  }
  return *tree_data_.emplace_back(tree_node);
}

SubgriddedItemData GridSizingTree::LookupSubgriddedItemData(
    const GridItemData& grid_item) const {
  const auto* item_layout_box = grid_item.node.GetLayoutBox();

  DCHECK(subgridded_item_data_lookup_map_.Contains(item_layout_box));
  const auto subgridded_item_indices =
      subgridded_item_data_lookup_map_.at(item_layout_box);

  const auto& subgrid_tree_node = At(subgridded_item_indices.parent_grid_index);
  return SubgriddedItemData(subgrid_tree_node.GetGridItems().At(
                                subgridded_item_indices.item_index_in_parent),
                            subgrid_tree_node.layout_data,
                            subgrid_tree_node.writing_mode);
}

wtf_size_t GridSizingTree::LookupSubgridIndex(
    const BlockNode& grid_node) const {
  const auto* grid_layout_box = grid_node.GetLayoutBox();

  DCHECK(subgrid_index_lookup_map_.Contains(grid_layout_box));
  return subgrid_index_lookup_map_.at(grid_layout_box);
}

}  // namespace blink
