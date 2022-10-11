// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_GRID_LAYOUT_NG_GRID_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_GRID_LAYOUT_NG_GRID_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/ng/grid/layout_ng_grid_interface.h"
#include "third_party/blink/renderer/core/layout/ng/grid/ng_grid_data.h"
#include "third_party/blink/renderer/core/layout/ng/layout_ng_block.h"
#include "third_party/blink/renderer/core/layout/ng/layout_ng_mixin.h"

namespace blink {

class CORE_EXPORT LayoutNGGrid : public LayoutNGBlock,
                                 public LayoutNGGridInterface {
 public:
  explicit LayoutNGGrid(Element*);

  void UpdateBlockLayout(bool relayout_children) override;

  const char* GetName() const override {
    NOT_DESTROYED();
    return "LayoutNGGrid";
  }

  const LayoutNGGridInterface* ToLayoutNGGridInterface() const final;

  bool HasCachedPlacementData() const;
  const NGGridPlacementData& CachedPlacementData() const;
  void SetCachedPlacementData(NGGridPlacementData&& placement_data);

  wtf_size_t AutoRepeatCountForDirection(
      const GridTrackSizingDirection track_direction) const final;
  wtf_size_t ExplicitGridStartForDirection(
      const GridTrackSizingDirection track_direction) const final;
  wtf_size_t ExplicitGridEndForDirection(
      const GridTrackSizingDirection track_direction) const final;
  LayoutUnit GridGap(
      const GridTrackSizingDirection track_direction) const final;
  LayoutUnit GridItemOffset(
      const GridTrackSizingDirection track_direction) const final;
  Vector<LayoutUnit, 1> TrackSizesForComputedStyle(
      const GridTrackSizingDirection track_direction) const final;

  Vector<LayoutUnit> RowPositions() const final;
  Vector<LayoutUnit> ColumnPositions() const final;

  const NGGridLayoutData* GridLayoutData() const;

 protected:
  bool IsOfType(LayoutObjectType type) const override {
    NOT_DESTROYED();
    return type == kLayoutObjectNGGrid ||
           LayoutNGMixin<LayoutBlock>::IsOfType(type);
  }

 private:
  Vector<LayoutUnit> ComputeTrackSizeRepeaterForRange(
      const NGGridLayoutTrackCollection& track_collection,
      wtf_size_t range_index) const;
  Vector<LayoutUnit> ComputeExpandedPositions(
      const GridTrackSizingDirection track_direction) const;

  void AddChild(LayoutObject* new_child,
                LayoutObject* before_child = nullptr) override;
  void RemoveChild(LayoutObject* child) override;
  void StyleDidChange(StyleDifference diff,
                      const ComputedStyle* old_style) override;

  std::unique_ptr<NGGridPlacementData> cached_placement_data_;
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
