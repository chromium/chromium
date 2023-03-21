// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_GRID_NG_GRID_SIZING_TREE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_GRID_NG_GRID_SIZING_TREE_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/ng/grid/ng_grid_data.h"
#include "third_party/blink/renderer/core/layout/ng/grid/ng_grid_item.h"

namespace blink {

struct NGGridSizingData {
  USING_FAST_MALLOC(NGGridSizingData);

 public:
  GridItems grid_items;
  NGGridLayoutData layout_data;
  wtf_size_t subtree_size{1};
};

// In subgrid, we allow "subgridded items" to be considered by the sizing
// algorithm of an ancestor grid that may not be its parent grid.
//
// For a given subgridded item, this class encapsulates a pointer to its
// |GridItemData| in the context of its parent grid (i.e., its properties are
// relative to its parent's area and writing mode) and a pointer to the actual
// |NGGridLayoutData| of the grid that directly contains the subgridded item.
class NGSubgriddedItemData {
  STACK_ALLOCATED();

 public:
  NGSubgriddedItemData() = default;

  NGSubgriddedItemData(const GridItemData& item_data_in_parent,
                       const NGGridLayoutData& parent_layout_data)
      : item_data_in_parent_(&item_data_in_parent),
        parent_layout_data_(&parent_layout_data) {}

  explicit operator bool() const { return item_data_in_parent_ != nullptr; }
  const GridItemData* operator->() const { return item_data_in_parent_; }

  const GridItemData& operator*() const {
    DCHECK(item_data_in_parent_);
    return *item_data_in_parent_;
  }

  std::unique_ptr<NGGridLayoutTrackCollection> CreateSubgridCollection(
      GridTrackSizingDirection track_direction) const;

  const NGGridLayoutData& ParentLayoutData() const {
    DCHECK(parent_layout_data_);
    return *parent_layout_data_;
  }

 private:
  const GridItemData* item_data_in_parent_{nullptr};
  const NGGridLayoutData* parent_layout_data_{nullptr};
};

constexpr NGSubgriddedItemData kNoSubgriddedItemData;

class CORE_EXPORT NGGridSizingTree {
  DISALLOW_NEW();

 public:
  NGGridSizingTree() = default;
  NGGridSizingTree(NGGridSizingTree&&) = default;
  NGGridSizingTree(const NGGridSizingTree&) = delete;
  NGGridSizingTree& operator=(NGGridSizingTree&&) = default;
  NGGridSizingTree& operator=(const NGGridSizingTree&) = delete;

  NGGridSizingData& CreateSizingData() {
    return *sizing_data_.emplace_back(std::make_unique<NGGridSizingData>());
  }

  NGGridSizingData& At(wtf_size_t index) {
    DCHECK_LT(index, sizing_data_.size());
    return *sizing_data_[index];
  }

  NGGridSizingData& operator[](wtf_size_t index) { return At(index); }

  wtf_size_t SubtreeSize(wtf_size_t index) const {
    DCHECK_LT(index, sizing_data_.size());
    return sizing_data_[index]->subtree_size;
  }

  // Creates a copy of the current grid geometry for the entire tree in a new
  // `NGGridLayoutTree` instance, which doesn't hold the grid items and its
  // stored in a `scoped_refptr` to be shared by multiple subtrees.
  scoped_refptr<const NGGridLayoutTree> FinalizeTree() const;

  wtf_size_t Size() const { return sizing_data_.size(); }

 private:
  Vector<std::unique_ptr<NGGridSizingData>, 16> sizing_data_;
};

}  // namespace blink

WTF_ALLOW_CLEAR_UNUSED_SLOTS_WITH_MEM_FUNCTIONS(blink::NGGridSizingData)

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_GRID_NG_GRID_SIZING_TREE_H_
