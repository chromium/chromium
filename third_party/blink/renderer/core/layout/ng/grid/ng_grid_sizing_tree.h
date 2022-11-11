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

class CORE_EXPORT NGGridSizingTree {
  STACK_ALLOCATED();

 public:
  using GridSizingDataVector = Vector<std::unique_ptr<NGGridSizingData>, 16>;

  wtf_size_t Size() const { return sizing_data_.size(); }

  NGGridSizingData& CreateSizingData() {
    return *sizing_data_.emplace_back(std::make_unique<NGGridSizingData>());
  }

  NGGridSizingData& operator[](wtf_size_t index) {
    DCHECK_LT(index, sizing_data_.size());
    return *sizing_data_[index];
  }

 private:
  GridSizingDataVector sizing_data_;
};

}  // namespace blink

WTF_ALLOW_CLEAR_UNUSED_SLOTS_WITH_MEM_FUNCTIONS(blink::NGGridSizingData)

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_GRID_NG_GRID_SIZING_TREE_H_
