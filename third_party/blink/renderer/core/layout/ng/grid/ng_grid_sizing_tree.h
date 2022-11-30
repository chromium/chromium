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

// This class stores various grid properties. Some of these properties
// depend on grid items and some depend on tracks, hence the need for a
// separate class to consolidate them. These properties can then be used
// to skip certain parts of the grid algorithm for better performance.
//
// TODO(ethavar): We can probably merge this struct with the sizing data.
struct CORE_EXPORT NGGridProperties {
  NGGridProperties()
      : has_baseline_column(false),
        has_baseline_row(false),
        has_orthogonal_item(false) {}

  bool HasBaseline(const GridTrackSizingDirection track_direction) const;
  bool HasFlexibleTrack(const GridTrackSizingDirection track_direction) const;
  bool HasIntrinsicTrack(const GridTrackSizingDirection track_direction) const;
  bool IsDependentOnAvailableSize(
      const GridTrackSizingDirection track_direction) const;
  bool IsSpanningOnlyDefiniteTracks(
      const GridTrackSizingDirection track_direction) const;

  // TODO(layout-dev) Initialize these with {false} and remove the constructor
  // when the codebase moves to C++20 (this syntax isn't allowed in bitfields
  // prior to C++20).
  bool has_baseline_column : 1;
  bool has_baseline_row : 1;
  bool has_orthogonal_item : 1;

  TrackSpanProperties column_properties;
  TrackSpanProperties row_properties;
};

struct NGGridSizingData : public GarbageCollected<NGGridSizingData> {
  NGGridSizingData() = delete;

  NGGridSizingData(const NGGridSizingData* parent_sizing_data,
                   const GridItemData* subgrid_data_in_parent)
      : parent_sizing_data(parent_sizing_data),
        subgrid_data_in_parent(subgrid_data_in_parent) {}

  void Trace(Visitor* visitor) const {
    visitor->Trace(grid_items);
    visitor->Trace(parent_sizing_data);
    visitor->Trace(subgrid_data_in_parent);
  }

  GridItems grid_items;
  NGGridProperties grid_properties;
  NGGridLayoutData layout_data;

  std::unique_ptr<NGGridBlockTrackCollection> column_builder_collection;
  std::unique_ptr<NGGridBlockTrackCollection> row_builder_collection;

  Member<const NGGridSizingData> parent_sizing_data;
  Member<const GridItemData> subgrid_data_in_parent;
  wtf_size_t subtree_size{1};
};

class NGGridSizingTree {
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
