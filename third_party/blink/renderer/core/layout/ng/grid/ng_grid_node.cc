// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/grid/ng_grid_node.h"

#include "third_party/blink/renderer/core/layout/ng/grid/layout_ng_grid.h"
#include "third_party/blink/renderer/core/layout/ng/grid/ng_grid_placement.h"
#include "third_party/blink/renderer/core/layout/ng/grid/ng_grid_properties.h"

namespace blink {

absl::optional<wtf_size_t>
NGGridNode::GetPreviousGridItemsSizeForReserveCapacity() const {
  LayoutNGGrid* layout_grid = To<LayoutNGGrid>(box_.Get());
  return layout_grid->GetPreviousGridItemsSizeForReserveCapacity();
}

const NGGridPlacementProperties& NGGridNode::GetPositions(
    const NGGridPlacement& grid_placement,
    const GridItems& grid_items,
    wtf_size_t column_auto_repetitions,
    wtf_size_t row_auto_repititions) const {
  LayoutNGGrid* layout_grid = To<LayoutNGGrid>(box_.Get());

  // Always re-run placement if |grid_items| is empty, as this method also
  // gets called for CSS Contains, where there won't be any children. In that
  // case, we don't want to use cached placements even if the cache is clean.
  if (!RuntimeEnabledFeatures::LayoutNGGridCachingEnabled() ||
      !layout_grid->HasCachedPlacements(column_auto_repetitions,
                                        row_auto_repititions) ||
      grid_items.IsEmpty()) {
    auto properties = grid_placement.RunAutoPlacementAlgorithm(grid_items);
    layout_grid->SetCachedPlacementProperties(
        std::move(properties), column_auto_repetitions, row_auto_repititions);
  } else {
#if DCHECK_IS_ON()
    auto duplicate_properties =
        grid_placement.RunAutoPlacementAlgorithm(grid_items);
    DCHECK(duplicate_properties == layout_grid->GetCachedPlacementProperties());
#endif
  }
  return layout_grid->GetCachedPlacementProperties();
}

}  // namespace blink
