// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_GRID_LANES_LAYOUT_GRID_LANES_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_GRID_LANES_LAYOUT_GRID_LANES_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/grid/grid_data.h"
#include "third_party/blink/renderer/core/layout/layout_block.h"
#include "third_party/blink/renderer/platform/geometry/layout_unit.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

class CORE_EXPORT LayoutGridLanes : public LayoutBlock {
 public:
  explicit LayoutGridLanes(Element* element);

  const char* GetName() const override {
    NOT_DESTROYED();
    // This string can affect a production behavior.
    // See tool_highlight.ts in devtools-frontend.
    return "LayoutGridLanes";
  }

  bool HasCachedPlacementData() const;
  const GridPlacementData& CachedPlacementData() const;
  void SetCachedPlacementData(GridPlacementData&& placement_data);

  // TODO(almaher): We are missing subgrid methods, similar to LayoutGrid.

  // Helper methods for DevTools inspector highlighting.
  wtf_size_t AutoRepeatCountForDirection(
      GridTrackSizingDirection direction) const;
  wtf_size_t ExplicitGridStartForDirection(
      GridTrackSizingDirection direction) const;
  wtf_size_t ExplicitGridEndForDirection(
      GridTrackSizingDirection direction) const;
  LayoutUnit GridGap(GridTrackSizingDirection track_direction) const;
  LayoutUnit GridLanesItemOffset(
      GridTrackSizingDirection track_direction) const;
  Vector<LayoutUnit, 1> TrackSizesForComputedStyle(
      GridTrackSizingDirection track_direction) const;

  Vector<LayoutUnit> GridTrackPositions(
      GridTrackSizingDirection track_direction) const;

  const GridLayoutData* LayoutData() const;

 private:
  bool IsLayoutGridLanes() const final {
    NOT_DESTROYED();
    return true;
  }

  // TODO(almaher): Do we need special overrides of AddChild(),
  // RemoveChild(), StyleDidChange(), MarkGridDirty() etc?

  // Caches grid-lanes placement data for DevTools inspector highlighting.
  // This avoids recomputing during inspector queries.
  std::optional<GridPlacementData> cached_placement_data_;
};

// wtf/casting.h helper.
template <>
struct DowncastTraits<LayoutGridLanes> {
  static bool AllowFrom(const LayoutObject& object) {
    return object.IsLayoutGridLanes();
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_GRID_LANES_LAYOUT_GRID_LANES_H_
