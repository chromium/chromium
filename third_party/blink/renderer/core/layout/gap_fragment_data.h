// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_GAP_FRAGMENT_DATA_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_GAP_FRAGMENT_DATA_H_

#include "third_party/blink/renderer/core/style/grid_enums.h"
#include "third_party/blink/renderer/platform/geometry/layout_unit.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_vector.h"

namespace blink {

class GapFragmentData {
 public:
  // GapIntersection points are used to paint gap decorations. An
  // intersection point occurs:
  // 1. At the center of an intersection between a gap and the container edge.
  // 2. At the center of an intersection between gaps in different directions.
  // https://drafts.csswg.org/css-gaps-1/#layout-painting
  class GapIntersection {
   public:
    GapIntersection() = default;
    GapIntersection(LayoutUnit column_offset, LayoutUnit row_offset)
        : column_offset(column_offset), row_offset(row_offset) {}

    LayoutUnit column_offset;
    LayoutUnit row_offset;

    // Represents whether the intersection point is blocked before or after
    // due to the presence of a spanning item.
    bool is_blocked_before = false;
    bool is_blocked_after = false;
  };

  using GapIntersectionList = Vector<GapIntersection>;

  // TODO(samomekarajr): Take this out when done with the new implementation.
  // GapBoundary represents the start and end offsets of a single gap.
  struct GapBoundary {
    DISALLOW_NEW();

   public:
    GapBoundary(wtf_size_t index,
                LayoutUnit start_offset,
                LayoutUnit end_offset)
        : index(index), start_offset(start_offset), end_offset(end_offset) {}

    void Trace(Visitor* visitor) const { visitor->Trace(intersection_points); }

    wtf_size_t index;
    LayoutUnit start_offset;
    LayoutUnit end_offset;
    HeapVector<LayoutUnit> intersection_points;
  };

  using GapBoundaries = HeapVector<GapBoundary>;

  // Gap locations are used for painting gap decorations.
  struct GapGeometry : public GarbageCollected<GapGeometry> {
   public:
    GapBoundaries columns;
    GapBoundaries rows;

    void AddGapBoundary(GridTrackSizingDirection track_direction,
                        GapBoundary gap) {
      (track_direction == kForColumns) ? columns.push_back(gap)
                                       : rows.push_back(gap);
    }

    GapBoundaries& GetGapBoundaries(GridTrackSizingDirection track_direction) {
      return track_direction == kForColumns ? columns : rows;
    }

    void Trace(Visitor* visitor) const {
      visitor->Trace(rows);
      visitor->Trace(columns);
    }

    void SetGapIntersections(GridTrackSizingDirection track_direction,
                             Vector<GapIntersectionList>&& intersection_list) {
      track_direction == kForColumns ? column_intersections_ = intersection_list
                                     : row_intersections_ = intersection_list;
    }

    const Vector<GapIntersectionList>& GetGapIntersections(
        GridTrackSizingDirection track_direction) const {
      return track_direction == kForColumns ? column_intersections_
                                            : row_intersections_;
    }

   private:
    // TODO(samomekarajr): Potential optimization. This can be a single
    // Vector<GapIntersection> if we exclude intersection points at the edge of
    // the container. We can check the "blocked" status of edge intersection
    // points to determine if we should draw from edge of the container to that
    // intersection.
    Vector<GapIntersectionList> column_intersections_;
    Vector<GapIntersectionList> row_intersections_;
  };
};

}  // namespace blink

WTF_ALLOW_CLEAR_UNUSED_SLOTS_WITH_MEM_FUNCTIONS(
    blink::GapFragmentData::GapBoundary)
#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_GAP_FRAGMENT_DATA_H_
