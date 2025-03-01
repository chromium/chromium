// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_MASONRY_MASONRY_RUNNING_POSITIONS_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_MASONRY_MASONRY_RUNNING_POSITIONS_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/geometry/layout_unit.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"
#include "third_party/blink/renderer/platform/wtf/wtf_size_t.h"

namespace blink {

struct GridSpan;

// This class holds a list of running positions for each track. This will be
// used to calculate the next position that an item should be placed.
class CORE_EXPORT MasonryRunningPositions {
 public:
  explicit MasonryRunningPositions(const wtf_size_t size)
      : running_positions_(size) {}

  // Return the first span with the minimum max-position that comes after
  // the auto-placement cursor in masonry's flow.
  GridSpan DetermineMinMaxPositionSpan(wtf_size_t span_size) const;

  // Update all the running positions for the tracks within the given lines to
  // have the inputted `running_position`.
  void UpdateRunningPositionsForSpan(const GridSpan& span,
                                     LayoutUnit running_position);

 private:
  friend class MasonryLayoutAlgorithmTest;

  // Struct to keep track of a span of tracks' start lines and their
  // max-positions, where the max-position of a span represents the maximum
  // running position of all tracks in a span. This will always be used in
  // conjunction with a span size, so we can calculate the ending line using
  // `start_line` and a given span size.
  struct MaxPositionSpan {
    wtf_size_t start_line;
    LayoutUnit max_pos;
  };

  // Given span size, returns a list of spans that have a max-position within
  // the tie-threshold of the minimum max-position.
  Vector<MaxPositionSpan> GetAllMaxPositionSpans(wtf_size_t span_size) const;

  // The index of the `running_positions_` vector corresponds to the track
  // number, while the value of the vector item corresponds to the current
  // running position of the track. Note that the tracks are 0-indexed.
  Vector<LayoutUnit> running_positions_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_MASONRY_MASONRY_RUNNING_POSITIONS_H_
