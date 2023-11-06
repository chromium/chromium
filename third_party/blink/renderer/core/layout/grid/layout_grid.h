// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_GRID_LAYOUT_GRID_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_GRID_LAYOUT_GRID_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/layout_block.h"
#include "third_party/blink/renderer/core/layout/grid/grid_data.h"

namespace blink {

class CORE_EXPORT LayoutGrid : public LayoutBlock {
 public:
  explicit LayoutGrid(Element*);

  const char* GetName() const override {
    NOT_DESTROYED();
    // This string can affect a production behavior.
    // See tool_highlight.ts in devtools-frontend.
    return "LayoutGrid";
  }

  bool HasCachedPlacementData() const;
  const GridPlacementData& CachedPlacementData() const;
  void SetCachedPlacementData(GridPlacementData&& placement_data);

  wtf_size_t AutoRepeatCountForDirection(
      const GridTrackSizingDirection track_direction) const;
  wtf_size_t ExplicitGridStartForDirection(
      const GridTrackSizingDirection track_direction) const;
  wtf_size_t ExplicitGridEndForDirection(
      const GridTrackSizingDirection track_direction) const;
  LayoutUnit GridGap(const GridTrackSizingDirection track_direction) const;
  LayoutUnit GridItemOffset(
      const GridTrackSizingDirection track_direction) const;
  Vector<LayoutUnit, 1> TrackSizesForComputedStyle(
      const GridTrackSizingDirection track_direction) const;

  Vector<LayoutUnit> RowPositions() const;
  Vector<LayoutUnit> ColumnPositions() const;

  const GridLayoutData* LayoutData() const;

 protected:
  bool IsOfType(LayoutObjectType type) const override {
    NOT_DESTROYED();
    return type == kLayoutObjectGrid || LayoutBlock::IsOfType(type);
  }

 private:
  Vector<LayoutUnit> ComputeTrackSizeRepeaterForRange(
      const GridLayoutTrackCollection& track_collection,
      wtf_size_t range_index) const;
  Vector<LayoutUnit> ComputeExpandedPositions(
      const GridTrackSizingDirection track_direction) const;

  void AddChild(LayoutObject* new_child,
                LayoutObject* before_child = nullptr) override;
  void RemoveChild(LayoutObject* child) override;
  void StyleDidChange(StyleDifference diff,
                      const ComputedStyle* old_style) override;

  std::unique_ptr<GridPlacementData> cached_placement_data_;
};

// wtf/casting.h helper.
template <>
struct DowncastTraits<LayoutGrid> {
  static bool AllowFrom(const LayoutObject& object) {
    return object.IsLayoutGrid();
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_GRID_LAYOUT_GRID_H_
