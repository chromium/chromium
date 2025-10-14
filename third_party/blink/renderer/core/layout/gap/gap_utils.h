// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_GAP_GAP_UTILS_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_GAP_GAP_UTILS_H_

#include <optional>

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/style/grid_area.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

class MainGap;
class CrossGap;

// Describes the occupancy of each cell adjacent to a gap
// along the primary axis. Each cell is:
//   kEmpty    – no item is in this cell.
//   kOccupied – covered by a non‑spanning item in the primary axis.
//   kSpanner  – covered by an item spanning 2 or more tracks in the primary
//   axis.
//
// We aggregate these cell states to derive gap segment ranges (blocked or
// empty).
//
// Example grid (rows x columns):
//   +---+---+---+
//   |       |   |
//   +---+---+   +
//   |   |   |   |
//   +---+---+---+
//   |   |       |
//   +---+---+---+
//
// Row-wise cell state matrix:
// [
//   [Occupied, Occupied, Spanner],
//   [Occupied, Occupied, Spanner],
//   [Occupied, Occupied, Occupied]
// ]
//
// TODO(samomekarajr): This enum could be extended to include direction-based
// spanner information to distinguish between column spanners and row spanners.
// This will avoid us having to maintain two separate aggregators for rows and
// columns. The plan is to update this after the implementation of empty areas.

enum CellState {
  kEmpty = 0,
  kOccupied = 1,
  kSpanner = 2,
};

using CellStates = Vector<CellState>;

// Represents the state of a gap segment, which can be:
//   kNone       – the gap segment is adjacent to occupied cells on both sides.
//   kEmptyBefore – the gap segment is adjacent to an empty cell before it.
//   kEmptyAfter  – the gap segment is adjacent to an empty cell after it.
//   kBlocked    – the gap segment is blocked by spanning items on both sides.
class CORE_EXPORT GapSegmentState {
 public:
  enum GapSegmentStateId : unsigned {
    kNone = 0,
    kEmptyBefore = 1 << 0,
    kEmptyAfter = 1 << 1,
    kBlocked = 1 << 2,
  };

  static const wtf_size_t kEmptyBoth = kEmptyAfter | kEmptyBefore;

  GapSegmentState() : status_(kEmptyBoth) {}
  explicit GapSegmentState(wtf_size_t status) : status_(status) {}

  inline bool HasGapStatus(GapSegmentStateId status) const {
    return (status_ & status) != 0;
  }
  inline void SetGapStatus(GapSegmentStateId status) { status_ |= status; }
  inline bool IsEmpty() const { return status_ == kEmptyBoth; }
  inline GapSegmentState& operator|=(const GapSegmentState& other) {
    status_ |= other.status_;
    return *this;
  }
  inline GapSegmentState& operator|=(const GapSegmentStateId status) {
    status_ |= status;
    return *this;
  }
  inline bool operator==(const GapSegmentState& other) const {
    return status_ == other.status_;
  }
  inline bool operator!=(const GapSegmentState& other) const {
    return !(*this == other);
  }

  wtf_size_t status_;
};

// Represents a contiguous range of gap segments sharing the same
// `GapSegmentState`.
struct GapSegmentStateRange {
  wtf_size_t start;
  wtf_size_t end;
  GapSegmentState state;
};

using GapSegmentStateRanges = Vector<GapSegmentStateRange>;

// Aggregates cell states along the primary axis to compute
// `GapSegmentStateRanges` for each gap along that axis. This is only applicable
// for layout types with 2D constraints and create cells, like grid.
class CORE_EXPORT GapSegmentStateAggregator {
  STACK_ALLOCATED();

 public:
  GapSegmentStateAggregator() = default;
  explicit GapSegmentStateAggregator(wtf_size_t cell_count)
      : cell_count_(cell_count) {}

  // Processes an item occupying a grid area defined by `primary_span` and
  // `secondary_span`. Updates the cell states for all tracks along the primary
  // axis covered by `primary_span`.
  void ProcessItem(const GridSpan& primary_span,
                   const GridSpan& secondary_span);

  // Finalizes and adds each `GapSegmentStateRange` for `gap` at `gap_index`
  // along the primary axis. Creates continuous ranges of gap segments that
  // share the same state by comparing the cell states of tracks adjacent to
  // `gap`, determined using `gap_index`.
  template <typename T>
  std::enable_if_t<std::is_same_v<T, MainGap> || std::is_same_v<T, CrossGap>,
                   void>
  FinalizeGapSegmentStateRangesFor(T& gap, wtf_size_t gap_index) const;

 private:
  // Updates the cell states for the track at `gap_index` along the primary
  // axis, for all cells covered by `secondary_span`.
  void UpdateGapStateFor(wtf_size_t gap_index,
                         const GridSpan& secondary_span,
                         CellState cell_state);

  wtf_size_t cell_count_;
  // Maps each track index along the primary axis to its corresponding cell
  // states.
  HashMap<wtf_size_t, CellStates, blink::IntWithZeroKeyHashTraits<int>>
      track_to_cell_states_;
};
}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_GAP_GAP_UTILS_H_
