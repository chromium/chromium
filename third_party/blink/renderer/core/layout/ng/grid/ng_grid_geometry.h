// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_GRID_NG_GRID_GEOMETRY_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_GRID_NG_GRID_GEOMETRY_H_

#include "third_party/blink/renderer/core/style/grid_positions_resolver.h"
#include "third_party/blink/renderer/platform/geometry/layout_unit.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

struct SetOffsetData {
  SetOffsetData(LayoutUnit offset, wtf_size_t track_count)
      : offset(offset), track_count(track_count) {}

  LayoutUnit offset;
  wtf_size_t track_count;
};

// Contains the information about the start offset of the tracks, as well as
// the gutter size between them, for a given direction.
struct TrackGeometry {
  LayoutUnit start_offset;
  LayoutUnit gutter_size;
};

// Represents the offsets for the sets, and the gutter-size.
//
// Initially we only know some of the set sizes - others will be indefinite. To
// represent this we store both the offset for the set, and a vector of all
// last indefinite indices (or kNotFound if everything so far has been
// definite). This allows us to get the appropriate size if a grid item spans
// only fixed tracks, but will allow us to return an indefinite size if it
// spans any indefinite set.
//
// As an example:
//   grid-template-rows: auto auto 100px 100px auto 100px;
//
// Results in:
//                  |  auto |  auto |   100   |   100   |   auto  |   100 |
//   [{0, kNotFound}, {0, 0}, {0, 1}, {100, 1}, {200, 1}, {200, 4}, {300, 4}]
//
// Various queries (start/end refer to the grid lines):
//  start: 0, end: 1 -> indefinite as:
//    "start <= sets[end].last_indefinite_index"
//  start: 1, end: 3 -> indefinite as:
//    "start <= sets[end].last_indefinite_index"
//  start: 2, end: 4 -> 200px
//  start: 5, end: 6 -> 100px
//  start: 3, end: 5 -> indefinite as:
//    "start <= sets[end].last_indefinite_index"
struct SetGeometry {
  SetGeometry() = default;

  SetGeometry(const TrackGeometry track_alignment_geometry,
              const wtf_size_t set_count)
      : gutter_size(track_alignment_geometry.gutter_size) {
    sets.ReserveInitialCapacity(set_count);
    sets.emplace_back(track_alignment_geometry.start_offset,
                      /* track_count */ 0);
  }

  SetGeometry(const Vector<SetOffsetData>& sets, const LayoutUnit gutter_size)
      : sets(sets), gutter_size(gutter_size) {}

  LayoutUnit FinalGutterSize() const {
    DCHECK_GT(sets.size(), 0u);
    return (sets.size() == 1) ? LayoutUnit() : gutter_size;
  }

  Vector<wtf_size_t> last_indefinite_indices;
  Vector<SetOffsetData> sets;
  LayoutUnit gutter_size;
};

// Contains all the geometry information relevant to the final grid. This
// should have all the grid information needed to size and position items.
struct NGGridGeometry {
  NGGridGeometry(SetGeometry&& column_geometry, SetGeometry&& row_geometry)
      : column_geometry(column_geometry),
        row_geometry(row_geometry),
        major_inline_baselines(column_geometry.sets.size(), LayoutUnit::Min()),
        minor_inline_baselines(column_geometry.sets.size(), LayoutUnit::Min()),
        major_block_baselines(row_geometry.sets.size(), LayoutUnit::Min()),
        minor_block_baselines(row_geometry.sets.size(), LayoutUnit::Min()) {}

  NGGridGeometry() = default;

  const SetGeometry& Geometry(
      const GridTrackSizingDirection track_direction) const {
    return (track_direction == kForColumns) ? column_geometry : row_geometry;
  }

  SetGeometry column_geometry;
  SetGeometry row_geometry;

  Vector<LayoutUnit> major_inline_baselines;
  Vector<LayoutUnit> minor_inline_baselines;
  Vector<LayoutUnit> major_block_baselines;
  Vector<LayoutUnit> minor_block_baselines;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_GRID_NG_GRID_GEOMETRY_H_
