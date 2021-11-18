// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/grid/ng_grid_node.h"

#include "third_party/blink/renderer/core/layout/ng/grid/layout_ng_grid.h"
#include "third_party/blink/renderer/core/layout/ng/grid/ng_grid_placement.h"

namespace blink {

absl::optional<const wtf_size_t> NGGridNode::CachedGridItemCount() const {
  LayoutNGGrid* layout_grid = To<LayoutNGGrid>(box_.Get());
  if (layout_grid->HasCachedPlacementData())
    return layout_grid->CachedPlacementData().grid_item_positions.size();
  return absl::nullopt;
}

const NGGridPlacementData& NGGridNode::CachedPlacementData() const {
  LayoutNGGrid* layout_grid = To<LayoutNGGrid>(box_.Get());
  return layout_grid->CachedPlacementData();
}

const Vector<GridArea>& NGGridNode::ResolveGridItemPositions(
    const GridItems& grid_items,
    NGGridPlacement* grid_placement) const {
  LayoutNGGrid* layout_grid = To<LayoutNGGrid>(box_.Get());

  if (layout_grid->HasCachedPlacementData() &&
      RuntimeEnabledFeatures::LayoutNGGridCachingEnabled()) {
    const auto& cached_data = layout_grid->CachedPlacementData();

    if (cached_data.column_auto_repetitions ==
            grid_placement->AutoRepetitions(kForColumns) &&
        cached_data.row_auto_repetitions ==
            grid_placement->AutoRepetitions(kForRows)) {
#if DCHECK_IS_ON()
      auto duplicate_data =
          grid_placement->RunAutoPlacementAlgorithm(grid_items);
      DCHECK(cached_data == duplicate_data);
#endif
      grid_placement->SetPlacementData(cached_data);
      return cached_data.grid_item_positions;
    }
  }

  layout_grid->SetCachedPlacementData(
      grid_placement->RunAutoPlacementAlgorithm(grid_items));
  return layout_grid->CachedPlacementData().grid_item_positions;
}

}  // namespace blink
