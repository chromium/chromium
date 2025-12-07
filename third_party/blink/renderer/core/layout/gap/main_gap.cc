// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/gap/main_gap.h"

#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

String MainGap::ToString(bool verbose) const {
  if (verbose) {
    return StrCat({"MainOffset(", gap_offset_.ToString(), "); ",
                   "Before: ", range_of_cross_gaps_before_.ToString(), ";",
                   "After: ", range_of_cross_gaps_after_.ToString(), ";"});
  }
  return StrCat({"MainOffset(", gap_offset_.ToString(), "); "});
}

void MainGap::AddGapSegmentStateRange(
    const GapSegmentStateRange& gap_segment_state_range) {
  if (!HasGapSegmentStateRanges()) {
    gap_segment_state_ranges_ = GapSegmentStateRanges();
  }
  gap_segment_state_ranges_->emplace_back(gap_segment_state_range);
}

wtf_size_t MainGap::GetCrossGapBeforeStart() const {
  CHECK(HasCrossGapsBefore());
  return range_of_cross_gaps_before_.Start();
}

wtf_size_t MainGap::GetCrossGapBeforeEnd() const {
  CHECK(HasCrossGapsBefore());
  return range_of_cross_gaps_before_.End();
}

wtf_size_t MainGap::GetCrossGapAfterStart() const {
  CHECK(HasCrossGapsAfter());
  return range_of_cross_gaps_after_.Start();
}

wtf_size_t MainGap::GetCrossGapAfterEnd() const {
  CHECK(HasCrossGapsAfter());
  return range_of_cross_gaps_after_.End();
}

const GapSegmentStateRanges& MainGap::GetGapSegmentStateRanges() const {
  CHECK(gap_segment_state_ranges_.has_value());
  return gap_segment_state_ranges_.value();
}

}  // namespace blink
