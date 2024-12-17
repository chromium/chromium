// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_GAP_FRAGMENT_DATA_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_GAP_FRAGMENT_DATA_H_

#include <optional>

#include "third_party/blink/renderer/platform/geometry/layout_unit.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_vector.h"

namespace blink {

class GapFragmentData {
 public:
  // GapBoundary represents the start and end offsets of a single gap.
  // A fragment can contain the start offset, the end offset, or both,
  // indicating the boundaries of the gap. When a grid container is fragmented,
  // it is possible to have only the start/end offsets for a given row gap in a
  // fragment. Example: Consider a grid container fragmented into two parts. The
  // gap between rows might be represented as follows:
  //
  // Fragment 1:                     Fragment 2:
  // +-----------------+          +-------------------+
  // | Item 1| |Item 2 |          |                   |
  // |       | |       |          | Gap End           |
  // |_______| |_______|          |________   ________|
  // | Gap Start       |          | Item 3 | | Item 4 |
  // |                 |          |        | |        |
  // +-----------------+          +-------------------+
  struct GapBoundary {
    DISALLOW_NEW();

   public:
    GapBoundary(wtf_size_t index,
                std::optional<LayoutUnit> start_offset = std::nullopt,
                std::optional<LayoutUnit> end_offset = std::nullopt)
        : index(index), start_offset(start_offset), end_offset(end_offset) {}

    wtf_size_t index;
    std::optional<LayoutUnit> start_offset;
    std::optional<LayoutUnit> end_offset;
  };

  using GapBoundaries = HeapVector<GapBoundary>;

  // Gap locations are used for painting gap decorations.
  struct GapGeometry {
    USING_FAST_MALLOC(GapGeometry);

   public:
    GapBoundaries columns;
    GapBoundaries rows;

    void Trace(Visitor* visitor) const {
      visitor->Trace(rows);
      visitor->Trace(columns);
    }
  };
};

}  // namespace blink
#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_GAP_DECORATIONS_FRAGMENT_GEOMETRY_H_
