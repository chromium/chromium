// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_GRID_GRID_SIZING_TREE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_GRID_GRID_SIZING_TREE_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/grid/grid_data.h"
#include "third_party/blink/renderer/core/layout/grid/grid_item.h"

namespace blink {

// In subgrid, we allow "subgridded items" to be considered by the sizing
// algorithm of an ancestor grid that may not be its parent grid.
//
// For a given subgridded item, this class encapsulates a pointer to its
// `GridItemData` in the context of its parent grid (i.e., its properties are
// relative to its parent's area and writing mode) and a pointer to the actual
// `GridLayoutData` of the grid that directly contains the subgridded item.
class SubgriddedItemData {
  DISALLOW_NEW();

 public:
  SubgriddedItemData() = default;

  SubgriddedItemData(const GridItemData& item_data_in_parent,
                     const GridLayoutData& parent_layout_data,
                     WritingMode parent_writing_mode)
      : item_data_in_parent_(&item_data_in_parent),
        parent_layout_data_(&parent_layout_data),
        parent_writing_mode_(parent_writing_mode) {}

  explicit operator bool() const { return item_data_in_parent_ != nullptr; }

  const GridItemData* operator->() const {
    DCHECK(item_data_in_parent_);
    return item_data_in_parent_;
  }

  const GridItemData& operator*() const { return *operator->(); }

  bool IsSubgrid() const {
    return item_data_in_parent_ && item_data_in_parent_->IsSubgrid();
  }

  const GridLayoutTrackCollection& Columns(
      std::optional<WritingMode> container_writing_mode = std::nullopt) const {
    return (!container_writing_mode ||
            IsParallelWritingMode(*container_writing_mode,
                                  parent_writing_mode_))
               ? parent_layout_data_->Columns()
               : parent_layout_data_->Rows();
  }

  const GridLayoutTrackCollection& Rows(
      std::optional<WritingMode> container_writing_mode = std::nullopt) const {
    return (!container_writing_mode ||
            IsParallelWritingMode(*container_writing_mode,
                                  parent_writing_mode_))
               ? parent_layout_data_->Rows()
               : parent_layout_data_->Columns();
  }

  const GridLayoutData& ParentLayoutData() const {
    DCHECK(parent_layout_data_);
    return *parent_layout_data_;
  }

 private:
  const GridItemData* item_data_in_parent_{nullptr};
  const GridLayoutData* parent_layout_data_{nullptr};
  WritingMode parent_writing_mode_{WritingMode::kHorizontalTb};
};

constexpr SubgriddedItemData kNoSubgriddedItemData;

// This class represents a grid tree (see `grid_subtree.h`) and contains the
// necessary data to perform the track sizing algorithm of its nested subgrids.
class CORE_EXPORT GridSizingTree {
  DISALLOW_NEW();

 public:
  struct GridTreeNode {
    USING_FAST_MALLOC(GridTreeNode);

   public:
    GridItems grid_items;
    GridLayoutData layout_data;
    wtf_size_t subtree_size{1};
  };

  GridSizingTree() = default;
  GridSizingTree(GridSizingTree&&) = default;
  GridSizingTree(const GridSizingTree&) = delete;
  GridSizingTree& operator=(GridSizingTree&&) = default;
  GridSizingTree& operator=(const GridSizingTree&) = delete;

  GridTreeNode& CreateSizingData(const BlockNode& grid_node);

  GridTreeNode& At(wtf_size_t index) const {
    DCHECK_LT(index, tree_data_.size());
    return *tree_data_[index];
  }

  GridSizingTree CopyForFragmentation() const;

  // Creates a copy of the current grid geometry for the entire tree in a new
  // `GridLayoutTree` instance, which doesn't hold the grid items and its
  // stored in a `scoped_refptr` to be shared by multiple subtrees.
  scoped_refptr<const GridLayoutTree> FinalizeTree() const;

  wtf_size_t Size() const { return tree_data_.size(); }
  GridTreeNode& TreeRootData() const { return At(0); }

  wtf_size_t SubtreeSize(wtf_size_t index) const {
    DCHECK_LT(index, tree_data_.size());
    return tree_data_[index]->subtree_size;
  }

  void AddSubgriddedItemLookupData(SubgriddedItemData&& subgridded_item_data);

  SubgriddedItemData LookupSubgriddedItemData(
      const GridItemData& grid_item) const;

  wtf_size_t LookupSubgridIndex(const BlockNode& grid_node) const;

  void Trace(Visitor* visitor) const {
    visitor->Trace(subgrid_index_lookup_map_);
    visitor->Trace(subgridded_item_data_lookup_map_);
  }

 private:
  // Stores a subgrid's index in the grid sizing tree; this is useful when we
  // want to create a `GridSizingSubtree` for an arbitrary subgrid.
  HeapHashMap<Member<const LayoutBox>, wtf_size_t> subgrid_index_lookup_map_;

  // In order to correctly determine the available space of a subgridded item,
  // which might be measured by a different grid than its parent grid, this map
  // stores the item's `SubgriddedItemData`, whose layout data should be used
  // to compute its span size within its parent grid's tracks.
  HeapHashMap<Member<const LayoutBox>, SubgriddedItemData>
      subgridded_item_data_lookup_map_;

  Vector<std::unique_ptr<GridTreeNode>, 16> tree_data_;
};

// This class represents a subtree in a `GridSizingTree` and provides seamless
// traversal over the tree and access to the sizing tree's lookup methods.
class GridSizingSubtree
    : public GridSubtree<GridSizingSubtree, const GridSizingTree*> {
  STACK_ALLOCATED();

 public:
  GridSizingSubtree() = default;

  explicit GridSizingSubtree(const GridSizingTree& sizing_tree,
                             wtf_size_t subtree_root = 0)
      : GridSubtree(&sizing_tree, subtree_root) {}

  GridSizingSubtree(const GridSizingTree* sizing_tree,
                    wtf_size_t parent_end_index,
                    wtf_size_t subtree_root)
      : GridSubtree(sizing_tree, parent_end_index, subtree_root) {}

  SubgriddedItemData LookupSubgriddedItemData(
      const GridItemData& grid_item) const {
    DCHECK(grid_tree_);
    return grid_tree_->LookupSubgriddedItemData(grid_item);
  }

  wtf_size_t LookupSubgridIndex(const GridItemData& subgrid_data) const {
    DCHECK(grid_tree_);
    DCHECK(subgrid_data.IsSubgrid());
    return grid_tree_->LookupSubgridIndex(subgrid_data.node);
  }

  GridSizingSubtree SubgridSizingSubtree(
      const GridItemData& subgrid_data) const {
    DCHECK(grid_tree_);
    DCHECK(subgrid_data.IsSubgrid());

    return GridSizingSubtree(
        *grid_tree_,
        /*subtree_root=*/grid_tree_->LookupSubgridIndex(subgrid_data.node));
  }

  // This method is only intended to be used to validate that the given grid
  // node is the respective root of the current subtree.
  bool HasValidRootFor(const BlockNode& grid_node) const {
    return grid_tree_ &&
           grid_tree_->LookupSubgridIndex(grid_node) == subtree_root_;
  }

  GridItems& GetGridItems() const {
    DCHECK(grid_tree_);
    return grid_tree_->At(subtree_root_).grid_items;
  }

  GridLayoutData& LayoutData() const {
    DCHECK(grid_tree_);
    return grid_tree_->At(subtree_root_).layout_data;
  }

  GridSizingTrackCollection& SizingCollection(
      GridTrackSizingDirection track_direction) const {
    DCHECK(grid_tree_);
    return grid_tree_->At(subtree_root_)
        .layout_data.SizingCollection(track_direction);
  }
};

constexpr GridSizingSubtree kNoGridSizingSubtree;

}  // namespace blink

WTF_ALLOW_CLEAR_UNUSED_SLOTS_WITH_MEM_FUNCTIONS(
    blink::GridSizingTree::GridTreeNode)

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_GRID_GRID_SIZING_TREE_H_
