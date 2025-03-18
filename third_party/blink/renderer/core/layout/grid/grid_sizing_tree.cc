// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#include "third_party/blink/renderer/core/layout/grid/grid_sizing_tree.h"

namespace blink {

void GridSizingTree::AddToPreorderTraversal(const BlockNode& grid_node) {
  DCHECK(grid_node.IsGrid());

  const auto* grid_layout_box = grid_node.GetLayoutBox();
  DCHECK(!subgrid_index_lookup_map_.Contains(grid_layout_box));
  subgrid_index_lookup_map_.insert(grid_layout_box, tree_data_.size());

  tree_data_.emplace_back();
}

void GridSizingTree::SetSizingNodeData(const BlockNode& grid_node,
                                       GridItems&& grid_items,
                                       GridLayoutData&& layout_data) {
  DCHECK(grid_node.IsGrid());

  const bool has_standalone_columns =
      !layout_data.HasSubgriddedAxis(kForColumns);
  const bool has_standalone_rows = !layout_data.HasSubgriddedAxis(kForRows);

  const auto grid_node_index = LookupSubgridIndex(grid_node);
  auto child_subgrid_index = grid_node_index + 1;
  auto& tree_node = At(grid_node_index);

  for (wtf_size_t current_item_index = 0; const auto& grid_item : grid_items) {
    // If this grid item is a subgrid, we need to add its subtree size to this
    // grid's subtree size and move to the next `child_subgrid_index`.
    if (grid_item.IsSubgrid()) {
      DCHECK_EQ(child_subgrid_index, LookupSubgridIndex(grid_item.node));
      const auto subtree_size = SubtreeSize(child_subgrid_index);
      tree_node.subtree_size += subtree_size;
      child_subgrid_index += subtree_size;
    }

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
                                                        grid_node_index);

    DCHECK(!subgridded_item_data_lookup_map_.Contains(item_layout_box));
    subgridded_item_data_lookup_map_.insert(item_layout_box,
                                            subgridded_item_indices);
  }

  tree_node.grid_items = std::move(grid_items);
  tree_node.layout_data = std::move(layout_data);
  tree_node.writing_mode = grid_node.Style().GetWritingMode();
}

GridLayoutTreePtr GridSizingTree::FinalizeTree() const {
  const auto tree_size = tree_data_.size();

  Vector<GridLayoutTree::GridTreeNode, 16> layout_tree_data;
  layout_tree_data.ReserveInitialCapacity(tree_size);

  for (const auto& grid_tree_node : tree_data_) {
    layout_tree_data.emplace_back(grid_tree_node.layout_data,
                                  grid_tree_node.subtree_size);
  }

  for (wtf_size_t i = tree_size; i; --i) {
    auto& subtree_data = layout_tree_data[i - 1];

    if (subtree_data.has_unresolved_geometry &&
        subtree_data.subtree_size == 1) {
      continue;
    }

    const wtf_size_t next_subtree_index = i + subtree_data.subtree_size - 1;
    for (wtf_size_t j = i;
         j < next_subtree_index && !subtree_data.has_unresolved_geometry;
         j += layout_tree_data[j].subtree_size) {
      DCHECK_LT(j, tree_size);
      subtree_data.has_unresolved_geometry =
          layout_tree_data[j].has_unresolved_geometry;
    }
  }
  return base::MakeRefCounted<GridLayoutTree>(std::move(layout_tree_data));
}

SubgriddedItemData GridSizingTree::LookupSubgriddedItemData(
    const GridItemData& grid_item) const {
  const auto* item_layout_box = grid_item.node.GetLayoutBox();

  DCHECK(subgridded_item_data_lookup_map_.Contains(item_layout_box));
  const auto [item_index_in_parent, parent_grid_index] =
      subgridded_item_data_lookup_map_.at(item_layout_box);

  const auto& subgrid_tree_node = At(parent_grid_index);
  return SubgriddedItemData(
      subgrid_tree_node.grid_items.At(item_index_in_parent),
      subgrid_tree_node.layout_data, subgrid_tree_node.writing_mode);
}

wtf_size_t GridSizingTree::LookupSubgridIndex(
    const BlockNode& grid_node) const {
  const auto* grid_layout_box = grid_node.GetLayoutBox();

  DCHECK(subgrid_index_lookup_map_.Contains(grid_layout_box));
  return subgrid_index_lookup_map_.at(grid_layout_box);
}

}  // namespace blink
