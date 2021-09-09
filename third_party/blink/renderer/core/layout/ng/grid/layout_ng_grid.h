// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_GRID_LAYOUT_NG_GRID_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_GRID_LAYOUT_NG_GRID_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/ng/grid/layout_ng_grid_interface.h"
#include "third_party/blink/renderer/core/layout/ng/grid/ng_grid_data.h"
#include "third_party/blink/renderer/core/layout/ng/grid/ng_grid_properties.h"
#include "third_party/blink/renderer/core/layout/ng/layout_ng_block.h"
#include "third_party/blink/renderer/core/layout/ng/layout_ng_mixin.h"

namespace blink {

class CORE_EXPORT LayoutNGGrid : public LayoutNGBlock,
                                 public LayoutNGGridInterface {
 public:
  explicit LayoutNGGrid(Element*);

  void UpdateBlockLayout(bool relayout_children) override;

  const char* GetName() const override { return "LayoutNGGrid"; }

  const LayoutNGGridInterface* ToLayoutNGGridInterface() const final;

  bool HasCachedPlacements(wtf_size_t column_auto_repititions,
                           wtf_size_t row_auto_reptitions) const;
  const NGGridPlacementProperties& GetCachedPlacementProperties();
  void SetCachedPlacementProperties(NGGridPlacementProperties&& properties,
                                    wtf_size_t column_auto_repititions,
                                    wtf_size_t row_auto_reptitions);

  wtf_size_t ExplicitGridStartForDirection(
      GridTrackSizingDirection direction) const final;
  wtf_size_t ExplicitGridEndForDirection(
      GridTrackSizingDirection direction) const final;
  wtf_size_t AutoRepeatCountForDirection(
      GridTrackSizingDirection direction) const final;
  LayoutUnit GridGap(GridTrackSizingDirection) const final;
  LayoutUnit GridItemOffset(GridTrackSizingDirection) const final;
  Vector<LayoutUnit, 1> TrackSizesForComputedStyle(
      GridTrackSizingDirection direction) const final;
  Vector<LayoutUnit> RowPositions() const final;
  Vector<LayoutUnit> ColumnPositions() const final;

  absl::optional<wtf_size_t> GetPreviousGridItemsSizeForReserveCapacity();

 protected:
  bool IsOfType(LayoutObjectType type) const override {
    return type == kLayoutObjectNGGrid ||
           LayoutNGMixin<LayoutBlock>::IsOfType(type);
  }

 private:
  const NGGridData* GetGridData() const;
  Vector<LayoutUnit> ComputeTrackSizesInRange(
      const NGGridLayoutAlgorithmTrackCollection::Range& range,
      GridTrackSizingDirection direction) const;
  Vector<LayoutUnit> ComputeExpandedPositions(
      GridTrackSizingDirection direction) const;

  void AddChild(LayoutObject* new_child,
                LayoutObject* before_child = nullptr) override;
  void RemoveChild(LayoutObject*) override;
  void StyleDidChange(StyleDifference, const ComputedStyle*) override;

  NGGridPlacementProperties cached_placement_properties_;
  wtf_size_t cached_row_auto_repititions_;
  wtf_size_t cached_column_auto_repititions_;
};

// wtf/casting.h helper.
template <>
struct DowncastTraits<LayoutNGGrid> {
  static bool AllowFrom(const LayoutObject& object) {
    return object.IsLayoutNGGrid();
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_GRID_LAYOUT_NG_GRID_H_
