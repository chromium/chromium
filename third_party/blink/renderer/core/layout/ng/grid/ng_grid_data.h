// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_GRID_NG_GRID_DATA_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_GRID_NG_GRID_DATA_H_

#include "third_party/blink/renderer/platform/geometry/layout_unit.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

// Given grid-template-columns: repeat(5, 30px 50px) 25px;
// Recall that the first set represents the grid item offset.
// If there are no grid items to break things up, we would expect the set and
// range data to be as follows:
//  Vector<SetData>: [{0,1}, {150,5}, {250, 5}, {25, 1}]
//  Vector<RangeData>: [{10, 1, 2}, {1, 3, 1}]
// This means that although splitting the sets by their track count would not
// give the correct result.
struct NGGridData {
  wtf_size_t row_start;
  wtf_size_t column_start;
  wtf_size_t row_auto_repeat_count;
  wtf_size_t column_auto_repeat_count;
  struct SetData {
    SetData(LayoutUnit offset, wtf_size_t track_count)
        : offset(offset), track_count(track_count) {}
    LayoutUnit offset;
    // This is derived from other data in layout_ng_grid, can be removed if not
    // useful for out of flow elements.
    wtf_size_t track_count;
  };
  struct RangeData {
    RangeData(wtf_size_t track_count,
              wtf_size_t starting_set_index,
              wtf_size_t set_count)
        : track_count(track_count),
          starting_set_index(starting_set_index),
          set_count(set_count) {}
    wtf_size_t track_count;
    // This is derived from other data in layout_ng_grid, can be removed if not
    // useful for out of flow elements.
    wtf_size_t starting_set_index;
    wtf_size_t set_count;
  };
  struct TrackCollectionGeometry {
    Vector<SetData> sets;
    Vector<RangeData> ranges;

    LayoutUnit gutter_size;
    wtf_size_t total_track_count;
  };
  TrackCollectionGeometry row_geometry;
  TrackCollectionGeometry column_geometry;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_GRID_NG_GRID_DATA_H_
