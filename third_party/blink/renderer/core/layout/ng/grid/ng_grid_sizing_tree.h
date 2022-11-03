// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_GRID_NG_GRID_SIZING_TREE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_GRID_NG_GRID_SIZING_TREE_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/ng/grid/ng_grid_data.h"
#include "third_party/blink/renderer/core/layout/ng/grid/ng_grid_item.h"
#include "third_party/blink/renderer/core/layout/ng/grid/ng_grid_node.h"

namespace blink {

struct NGGridSizingData : public GarbageCollected<NGGridSizingData> {
  NGGridSizingData() = delete;

  NGGridSizingData(const NGGridSizingData* parent_sizing_data,
                   const GridItemData* subgrid_data_in_parent)
      : parent_sizing_data(parent_sizing_data),
        subgrid_data_in_parent(subgrid_data_in_parent) {}

  bool MustBuildLayoutData(GridTrackSizingDirection track_direction) const;

  void Trace(Visitor* visitor) const {
    visitor->Trace(grid_items);
    visitor->Trace(parent_sizing_data);
    visitor->Trace(subgrid_data_in_parent);
  }

  GridItems grid_items;
  NGGridLayoutData layout_data;

  Member<const NGGridSizingData> parent_sizing_data;
  Member<const GridItemData> subgrid_data_in_parent;
  wtf_size_t subtree_size{1};
};

class CORE_EXPORT NGGridSizingTree {
  STACK_ALLOCATED();

 public:
  using GridSizingDataLookupMap =
      HeapHashMap<Member<const LayoutBox>, Member<NGGridSizingData>>;
  using GridSizingDataVector = HeapVector<Member<NGGridSizingData>, 16>;

  NGGridSizingData& CreateSizingData(
      const NGGridNode& grid,
      const NGGridSizingData* parent_sizing_data,
      const GridItemData* subgrid_data_in_parent);

  wtf_size_t Size() const { return sizing_data_.size(); }

  NGGridSizingData& operator[](wtf_size_t index);

 private:
  GridSizingDataLookupMap data_lookup_map_;
  GridSizingDataVector sizing_data_;
};

}  // namespace blink

WTF_ALLOW_CLEAR_UNUSED_SLOTS_WITH_MEM_FUNCTIONS(blink::NGGridSizingData)

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_GRID_NG_GRID_SIZING_TREE_H_
