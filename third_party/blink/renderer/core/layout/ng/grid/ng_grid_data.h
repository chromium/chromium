// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_GRID_NG_GRID_DATA_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_GRID_NG_GRID_DATA_H_

#include "third_party/blink/renderer/core/layout/ng/grid/ng_grid_geometry.h"
#include "third_party/blink/renderer/core/layout/ng/grid/ng_grid_track_collection.h"
#include "third_party/blink/renderer/platform/geometry/layout_unit.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

struct CORE_EXPORT NGGridPlacementData {
  explicit NGGridPlacementData(Vector<GridArea>&& grid_item_positions)
      : grid_item_positions(grid_item_positions) {}

  bool operator==(const NGGridPlacementData& other) const {
    return grid_item_positions == other.grid_item_positions &&
           column_auto_repetitions == other.column_auto_repetitions &&
           row_auto_repetitions == other.row_auto_repetitions &&
           column_start_offset == other.column_start_offset &&
           row_start_offset == other.row_start_offset;
  }

  Vector<GridArea> grid_item_positions;

  wtf_size_t column_auto_repetitions;
  wtf_size_t row_auto_repetitions;
  wtf_size_t column_start_offset;
  wtf_size_t row_start_offset;
};

// This struct contains a bundle of the ranges from the track collection and the
// resolved used sizes of the sets in a given track direction.
struct CORE_EXPORT NGGridLayoutData {
  USING_FAST_MALLOC(NGGridLayoutData);

 public:
  using RangeData = NGGridLayoutAlgorithmTrackCollection::Range;

  struct TrackCollectionGeometry {
    Vector<RangeData> ranges;
    Vector<SetOffsetData> sets;

    LayoutUnit gutter_size;
    wtf_size_t track_count;
  };

  TrackCollectionGeometry column_geometry;
  TrackCollectionGeometry row_geometry;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_GRID_NG_GRID_DATA_H_
