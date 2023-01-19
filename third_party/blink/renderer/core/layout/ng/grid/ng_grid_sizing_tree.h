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

class NGGridItemSizingData {
  STACK_ALLOCATED();

 public:
  NGGridItemSizingData(const GridItemData& item_data_in_parent,
                       const NGGridLayoutData& parent_layout_data);

  std::unique_ptr<NGGridLayoutTrackCollection> CreateSubgridCollection(
      GridTrackSizingDirection track_direction) const;

 private:
  const GridItemData* item_data_in_parent;
  const NGGridLayoutData* parent_layout_data;
};

using NGSubgridSizingData = absl::optional<NGGridItemSizingData>;

class CORE_EXPORT NGGridSizingTree {
  DISALLOW_NEW();

 public:
  using GridSizingDataVector = Vector<std::unique_ptr<NGGridSizingData>, 16>;

  NGGridSizingTree(NGGridSizingTree&&) = default;
  NGGridSizingTree(const NGGridSizingTree&) = delete;
  NGGridSizingTree& operator=(NGGridSizingTree&&) = default;
  NGGridSizingTree& operator=(const NGGridSizingTree&) = delete;

  explicit NGGridSizingTree(wtf_size_t tree_size = 1) {
    sizing_data_.ReserveInitialCapacity(tree_size);
  }

  NGGridSizingData& CreateSizingData() {
    return *sizing_data_.emplace_back(std::make_unique<NGGridSizingData>());
  }

  NGGridSizingData& operator[](wtf_size_t index) {
    DCHECK_LT(index, sizing_data_.size());
    return *sizing_data_[index];
  }

  NGGridSizingTree CopySubtree(wtf_size_t subtree_root) const;
  wtf_size_t Size() const { return sizing_data_.size(); }

 private:
  GridSizingDataVector sizing_data_;
};

}  // namespace blink

WTF_ALLOW_CLEAR_UNUSED_SLOTS_WITH_MEM_FUNCTIONS(blink::NGGridSizingData)

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_GRID_NG_GRID_SIZING_TREE_H_
