// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/gap/gap_utils.h"

#include "third_party/blink/renderer/core/layout/gap/cross_gap.h"
#include "third_party/blink/renderer/core/layout/gap/main_gap.h"

namespace blink {

// GapSegmentStateAggregator implementations
void GapSegmentStateAggregator::ProcessItem(const GridSpan& primary_span,
                                            const GridSpan& secondary_span) {
  const wtf_size_t end_line = primary_span.EndLine();
  for (wtf_size_t track_index = primary_span.StartLine();
       track_index < end_line; ++track_index) {
    UpdateCellEndLinesFor(track_index, secondary_span, end_line);
  }
}

template <typename T>
std::enable_if_t<std::is_same_v<T, MainGap> || std::is_same_v<T, CrossGap>,
                 void>
GapSegmentStateAggregator::FinalizeGapSegmentStateRangesFor(
    T& gap,
    wtf_size_t track_index) const {
  // If no item covers a track, all of its cells have an end line of zero.
  auto current_it = track_to_cell_end_lines_.find(track_index);
  auto next_it = track_to_cell_end_lines_.find(track_index + 1);
  CellEndLines current_cells = current_it != track_to_cell_end_lines_.end()
                                   ? current_it->value
                                   : CellEndLines(cell_count_, 0);
  CellEndLines next_cells = next_it != track_to_cell_end_lines_.end()
                                ? next_it->value
                                : CellEndLines(cell_count_, 0);

  const wtf_size_t gap_line = track_index + 1;
  const auto ComputeGapMask = [gap_line](wtf_size_t current_end_line,
                                         wtf_size_t next_end_line) {
    if (current_end_line > gap_line) {
      return GapSegmentState(GapSegmentState::kBlocked);
    }

    GapSegmentState mask(GapSegmentState::kNone);
    if (current_end_line == 0) {
      mask |= GapSegmentState::kEmptyBefore;
    }
    if (next_end_line == 0) {
      mask |= GapSegmentState::kEmptyAfter;
    }
    return mask;
  };

  GapSegmentState current_state =
      ComputeGapMask(current_cells[0], next_cells[0]);
  wtf_size_t current_index = 0;

  GapSegmentStateRanges gap_segment_state_ranges;
  for (wtf_size_t i = 1; i < current_cells.size(); ++i) {
    GapSegmentState candidate_state =
        ComputeGapMask(current_cells[i], next_cells[i]);

    // The state changed between the current and candidate state. End the
    // current range and start a new one.
    if (candidate_state.status_ != current_state.status_) {
      if (current_state.status_ != GapSegmentState::kNone) {
        gap.AddGapSegmentStateRange(
            GapSegmentStateRange{current_index, i, current_state});
      }

      current_state = candidate_state;
      current_index = i;
    }
  }

  // Add the final range that extends to the end of the cell array.
  if (current_state.status_ != GapSegmentState::kNone) {
    gap.AddGapSegmentStateRange(GapSegmentStateRange{
        current_index, current_cells.size(), current_state});
  }
}

void GapSegmentStateAggregator::UpdateCellEndLinesFor(
    wtf_size_t track_index,
    const GridSpan& secondary_span,
    wtf_size_t end_line) {
  // Look up the track's cell end lines with a single hash lookup, initializing
  // them to empty only when this is a newly inserted entry. Reusing the
  // returned reference also avoids re-looking-up the key on every iteration.
  auto add_result =
      track_to_cell_end_lines_.insert(track_index, CellEndLines());
  CellEndLines& cell_end_lines = add_result.stored_value->value;
  if (add_result.is_new_entry) {
    cell_end_lines = CellEndLines(cell_count_, 0);
  }

  for (wtf_size_t i = secondary_span.StartLine(); i < secondary_span.EndLine();
       ++i) {
    if (cell_end_lines[i] < end_line) {
      cell_end_lines[i] = end_line;
    }
  }
}

String GapSegmentState::ToString() const {
  if (status_ == kNone) {
    return "NONE";
  } else {
    if (HasGapStatus(kEmptyBefore)) {
      return "EMPTY_BEFORE ";
    }
    if (HasGapStatus(kEmptyAfter)) {
      return "EMPTY_AFTER ";
    }
    if (HasGapStatus(kBlocked)) {
      return "BLOCKED ";
    }
  }

  return "UNKNOWN";
}

template void GapSegmentStateAggregator::FinalizeGapSegmentStateRangesFor<
    MainGap>(MainGap& gap, wtf_size_t gap_index) const;
template void GapSegmentStateAggregator::FinalizeGapSegmentStateRangesFor<
    CrossGap>(CrossGap& gap, wtf_size_t gap_index) const;

}  // namespace blink
