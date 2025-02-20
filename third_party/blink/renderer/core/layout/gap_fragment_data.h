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
  };
};

}  // namespace blink

WTF_ALLOW_CLEAR_UNUSED_SLOTS_WITH_MEM_FUNCTIONS(
    blink::GapFragmentData::GapBoundary)
#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_GAP_FRAGMENT_DATA_H_
