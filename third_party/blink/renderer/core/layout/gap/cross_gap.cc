// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/gap/cross_gap.h"

namespace blink {

void CrossGapRange::Increment(wtf_size_t cross_gap_index) {
  if (!start_index_.has_value()) {
    start_index_ = cross_gap_index;

    // Both start at the same index, but subsequent calls will increment the
    // end index.
    end_index_ = cross_gap_index;
  } else {
    CHECK(end_index_.has_value());
    CHECK_GT(cross_gap_index, *end_index_);
    CHECK_GT(cross_gap_index, *start_index_);
    end_index_ = cross_gap_index;
  }
}

String CrossGapRange::ToString() const {
  return StrCat(
      {"(", (start_index_.has_value() ? String::Number(*start_index_) : "null"),
       " --> ", (end_index_.has_value() ? String::Number(*end_index_) : "null"),
       ")"});
}

String CrossGap::ToString(bool verbose) const {
  if (verbose) {
    String edge_state;
    if (edge_state_ == EdgeIntersectionState::kStart) {
      edge_state = "kStart";
    } else if (edge_state_ == EdgeIntersectionState::kEnd) {
      edge_state = "kEnd";
    } else if (edge_state_ == EdgeIntersectionState::kBoth) {
      edge_state = "kBoth";
    } else {
      edge_state = "kNone";
    }
    return StrCat({"CrossStartOffset(",
                   gap_logical_offset_.inline_offset.ToString(), ", ",
                   gap_logical_offset_.block_offset.ToString(), "); ",
                   "EdgeState: ", edge_state, ";"});
  }

  return StrCat({"CrossStartOffset(",
                 gap_logical_offset_.inline_offset.ToString(), ", ",
                 gap_logical_offset_.block_offset.ToString(), ")"});
}

void CrossGap::AddGapSegmentStateRange(
    const GapSegmentStateRange& gap_segment_state_range) {
  if (!HasGapSegmentStateRanges()) {
    gap_segment_state_ranges_ = GapSegmentStateRanges();
  }
  gap_segment_state_ranges_->emplace_back(gap_segment_state_range);
}

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
