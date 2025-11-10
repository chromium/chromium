// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/gap/cross_gap.h"

namespace blink {

void CrossGap::UpdateCrossGapRangeEdgeState(
    Vector<CrossGap>& cross_gaps,
    wtf_size_t start_index,
    wtf_size_t end_index,
    CrossGap::EdgeIntersectionState new_state) {
  if (cross_gaps.empty() || start_index > cross_gaps.size() - 1 ||
      start_index > end_index) {
    return;
  }

  for (wtf_size_t i = start_index; i <= end_index; ++i) {
    CrossGap& gap = cross_gaps[i];
    CrossGap::EdgeIntersectionState old_state = gap.GetEdgeIntersectionState();

    if (new_state == old_state) {
      // No update needed.
      continue;
    }

    switch (new_state) {
      case CrossGap::EdgeIntersectionState::kBoth:
        gap.SetEdgeIntersectionState(CrossGap::EdgeIntersectionState::kBoth);
        break;
      case CrossGap::EdgeIntersectionState::kStart:
        if (old_state == CrossGap::EdgeIntersectionState::kEnd) {
          gap.SetEdgeIntersectionState(CrossGap::EdgeIntersectionState::kBoth);
        } else if (old_state != CrossGap::EdgeIntersectionState::kBoth) {
          gap.SetEdgeIntersectionState(CrossGap::EdgeIntersectionState::kStart);
        }
        break;
      case CrossGap::EdgeIntersectionState::kEnd:
        if (old_state == CrossGap::EdgeIntersectionState::kStart) {
          gap.SetEdgeIntersectionState(CrossGap::EdgeIntersectionState::kBoth);
        } else if (old_state != CrossGap::EdgeIntersectionState::kBoth) {
          gap.SetEdgeIntersectionState(CrossGap::EdgeIntersectionState::kEnd);
        }
        break;
      case CrossGap::EdgeIntersectionState::kNone:
        gap.SetEdgeIntersectionState(CrossGap::EdgeIntersectionState::kNone);
        break;
    }
  }
}

}  // namespace blink
