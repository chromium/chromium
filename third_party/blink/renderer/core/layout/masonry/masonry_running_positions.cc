// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#include "third_party/blink/renderer/core/layout/masonry/masonry_running_positions.h"

#include "third_party/blink/renderer/core/style/grid_area.h"

namespace blink {

void MasonryRunningPositions::UpdateRunningPositionsForSpan(
    const GridSpan& span,
    LayoutUnit running_position) {
  const auto end_line = span.EndLine();

  DCHECK_GE(running_positions_.size(), end_line);

  for (auto track_idx = span.StartLine(); track_idx < end_line; ++track_idx) {
    DCHECK_GE(running_position, running_positions_[track_idx]);
    running_positions_[track_idx] = running_position;
  }
}

}  // namespace blink
