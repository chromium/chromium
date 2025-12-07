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
  if (primary_span.SpanSize() >= 2) {
    for (wtf_size_t track_index = primary_span.StartLine();
         track_index < primary_span.EndLine(); ++track_index) {
      UpdateGapStateFor(track_index, secondary_span, kSpanner);
    }
  } else {
    UpdateGapStateFor(primary_span.StartLine(), secondary_span, kOccupied);
  }
}

template <typename T>
std::enable_if_t<std::is_same_v<T, MainGap> || std::is_same_v<T, CrossGap>,
                 void>
GapSegmentStateAggregator::FinalizeGapSegmentStateRangesFor(
    T& gap,
    wtf_size_t gap_index) const {
  // If no cell states exist for a given track, all cells are empty.
  auto current_it = track_to_cell_states_.find(gap_index);
  auto next_it = track_to_cell_states_.find(gap_index + 1);
  CellStates current_cells = current_it != track_to_cell_states_.end()
                                 ? current_it->value
                                 : CellStates(cell_count_, kEmpty);
  CellStates next_cells = next_it != track_to_cell_states_.end()
                              ? next_it->value
                              : CellStates(cell_count_, kEmpty);

  constexpr auto ComputeGapMask = [](CellState current, CellState next) {
    if (current == kSpanner && next == kSpanner) {
      return GapSegmentState(GapSegmentState::kBlocked);
    }

    GapSegmentState mask(GapSegmentState::kNone);
    if (current == kEmpty) {
      mask |= GapSegmentState::kEmptyBefore;
    }
    if (next == kEmpty) {
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

void GapSegmentStateAggregator::UpdateGapStateFor(
    wtf_size_t gap_index,
    const GridSpan& secondary_span,
    CellState cell_state) {
  // Initialize the cell states for this track if not already present.
  if (track_to_cell_states_.find(gap_index) == track_to_cell_states_.end()) {
    track_to_cell_states_.insert(gap_index, CellStates(cell_count_, kEmpty));
  }

  for (wtf_size_t i = secondary_span.StartLine(); i < secondary_span.EndLine();
       ++i) {
    auto cell_it = track_to_cell_states_.find(gap_index);
    cell_it->value[i] = cell_state;
  }
}

template void GapSegmentStateAggregator::FinalizeGapSegmentStateRangesFor<
    MainGap>(MainGap& gap, wtf_size_t gap_index) const;
template void GapSegmentStateAggregator::FinalizeGapSegmentStateRangesFor<
    CrossGap>(CrossGap& gap, wtf_size_t gap_index) const;

}  // namespace blink
