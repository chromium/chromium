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

    String segment_state_ranges_str;
    if (gap_segment_state_ranges_.has_value()) {
      segment_state_ranges_str = "[";
      for (const auto& range : gap_segment_state_ranges_.value()) {
        segment_state_ranges_str = StrCat(
            {segment_state_ranges_str, "[", String::Number(range.start), ", ",
             String::Number(range.end), ") ", range.state.ToString(), ", "});
      }
      segment_state_ranges_str = StrCat({segment_state_ranges_str, "]"});
    }

    return StrCat({
        "CrossStartOffset(",
        gap_logical_offset_.inline_offset.ToString(),
        ", ",
        gap_logical_offset_.block_offset.ToString(),
        "); ",
        "EdgeState: ",
        edge_state,
        "; ",
        "SegmentStateRanges: ",
        segment_state_ranges_str,
    });
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

void CrossGap::AdjustGapSegmentStateRangesForFragmentation(
    wtf_size_t last_track_in_previous_fragment,
    wtf_size_t first_track_in_next_fragment,
    wtf_size_t& range_start_idx) {
  CHECK(HasGapSegmentStateRanges());
  GapSegmentStateRanges adjusted_ranges;

  const auto& ranges = gap_segment_state_ranges_.value();
  for (; range_start_idx < ranges.size(); ++range_start_idx) {
    // If the start of the range is greater than the first unprocessed track, it
    // means all subsequent ranges will also be beyond the current fragment, so
    // we can break out of the loop.
    if (ranges[range_start_idx].start > first_track_in_next_fragment) {
      break;
    }

    // Adjust ranges relative to `last_track_in_previous_fragment` to keep
    // indices fragment-relative. A range may begin before the current fragment
    // and still overlap into the current fragment. In that case we clamp the
    // fragment-relative start to 0 and carry the overlap forward.
    wtf_size_t adjusted_start =
        ranges[range_start_idx].start > last_track_in_previous_fragment
            ? ranges[range_start_idx].start - last_track_in_previous_fragment
            : 0;
    CHECK_GT(ranges[range_start_idx].end, last_track_in_previous_fragment);
    wtf_size_t adjusted_end =
        ranges[range_start_idx].end - last_track_in_previous_fragment;
    adjusted_ranges.emplace_back(GapSegmentStateRange{
        adjusted_start, adjusted_end, ranges[range_start_idx].state});
  }

  // If the last included range extends beyond the
  // `first_track_in_next_fragment`, we need to re-visit that same range in
  // subsequent fragmentainers, so set `range_start_idx` to the prior index.
  if (!adjusted_ranges.empty() && range_start_idx > 0 &&
      ranges[range_start_idx - 1].end > first_track_in_next_fragment) {
    --range_start_idx;
  }
  gap_segment_state_ranges_ = adjusted_ranges;
}

}  // namespace blink
