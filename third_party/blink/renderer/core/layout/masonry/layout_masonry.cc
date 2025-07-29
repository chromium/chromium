// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#include "third_party/blink/renderer/core/layout/masonry/layout_masonry.h"

#include "third_party/blink/renderer/core/layout/grid/grid_data.h"
#include "third_party/blink/renderer/core/layout/grid/layout_grid.h"

namespace blink {

LayoutMasonry::LayoutMasonry(Element* element) : LayoutBlock(element) {}

const GridLayoutData* LayoutMasonry::LayoutData() const {
  return LayoutGrid::GetGridLayoutDataFromFragments(this);
}

Vector<LayoutUnit> LayoutMasonry::GridTrackPositions(
    GridTrackSizingDirection track_direction) const {
  NOT_DESTROYED();
  return LayoutGrid::ComputeExpandedPositions(LayoutData(), track_direction);
}

LayoutUnit LayoutMasonry::GridGap(
    GridTrackSizingDirection track_direction) const {
  NOT_DESTROYED();
  return LayoutGrid::ComputeGridGap(LayoutData(), track_direction);
}

LayoutUnit LayoutMasonry::MasonryItemOffset(
    GridTrackSizingDirection track_direction) const {
  NOT_DESTROYED();
  // Distribution offset is baked into the `gutter_size` in Masonry.
  return LayoutUnit();
}

}  // namespace blink
