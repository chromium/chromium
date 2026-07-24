// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_GRID_LANES_GRID_LANES_GAP_ACCUMULATOR_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_GRID_LANES_GRID_LANES_GAP_ACCUMULATOR_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/geometry/layout_unit.h"

namespace blink {

class GapGeometry;
class GridLayoutTrackCollection;

// Builds gap-decoration geometry and related state for grid-lanes.
class CORE_EXPORT GridLanesGapAccumulator {
  STACK_ALLOCATED();

 public:
  GridLanesGapAccumulator();

  // Builds `MainGap` geometry for gutters between grid-axis tracks, parallel to
  // the stacking axis. See
  // third_party/blink/renderer/core/layout/gap/README.md` for Blink's
  // Main/Cross model.
  void BuildMainGaps(const GridLayoutTrackCollection& grid_axis_tracks);

  const GapGeometry* FinalizeGapGeometry(LayoutUnit stacking_content_start,
                                         LayoutUnit stacking_content_end);

 private:
  GapGeometry* gap_geometry_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_GRID_LANES_GRID_LANES_GAP_ACCUMULATOR_H_
