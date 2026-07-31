// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_GRID_LANES_GRID_LANES_ITEM_ITERATOR_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_GRID_LANES_GRID_LANES_ITEM_ITERATOR_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/grid_lanes/grid_lane_data.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

class BlockBreakToken;

// A utility class for grid lanes layout which given a list of grid lanes and a
// break token will iterate through unfinished grid lanes items.
//
// `NextItem()` processes items in one lane at a time, with spanners being an
// exception to the rule. When a spanner is hit, all items before it in each
// lane it spans must be processed before the spanner itself. In all cases,
// though child break tokens are processed first, in break token order. The
// iterator then resumes unstarted items from those positions onward.
//
// This class does not handle modifications to its arguments after it has been
// constructed.
//
// TODO(almaher): Handle spanners correctly (as mentioned above).
class CORE_EXPORT GridLanesItemIterator {
  STACK_ALLOCATED();

 public:
  GridLanesItemIterator(const GridLanesDataVector& grid_lanes,
                        const BlockBreakToken* break_token,
                        bool is_column);

  // Returns the next grid lanes item which should be laid out, along with its
  // respective break token.
  //
  // TODO(almaher): Similar to flex, we will likely need the break before row
  // state here for row containers.
  struct Entry;
  Entry NextItem();

 private:
  GridLanesItemData* FindNextItem(
      const BlockBreakToken* item_break_token = nullptr);
  void AdjustItemIndexForNewLane();

  GridLanesItemData* next_unstarted_item_ = nullptr;
  const GridLanesDataVector& grid_lanes_;
  const BlockBreakToken* break_token_;
  bool is_column_ = false;

  // An index into `break_token_`'s `ChildBreakTokens()` vector. Used for
  // keeping track of the next child break token to inspect.
  wtf_size_t child_token_idx_ = 0;
  // An index into the `grid_lanes_` vector. Used for keeping track of the next
  // grid lane to inspect.
  wtf_size_t grid_lane_idx_ = 0;
  // An index into the current `GridLaneData::item_data` vector.
  wtf_size_t grid_lanes_item_idx_ = 0;
  // Stores the next item index to process for each lane, if applicable.
  Vector<wtf_size_t> next_item_idx_for_lane_;
};

struct GridLanesItemIterator::Entry {
  STACK_ALLOCATED();

 public:
  Entry(GridLanesItemData* grid_lanes_item,
        wtf_size_t grid_lanes_item_idx,
        wtf_size_t grid_lane_idx,
        const BlockBreakToken* token)
      : grid_lanes_item(grid_lanes_item),
        grid_lanes_item_idx(grid_lanes_item_idx),
        grid_lane_idx(grid_lane_idx),
        token(token) {}

  GridLanesItemData* grid_lanes_item = nullptr;
  wtf_size_t grid_lanes_item_idx = kNotFound;
  wtf_size_t grid_lane_idx = kNotFound;
  const BlockBreakToken* token = nullptr;

  bool operator==(const GridLanesItemIterator::Entry& other) const {
    return grid_lanes_item == other.grid_lanes_item &&
           grid_lanes_item_idx == other.grid_lanes_item_idx &&
           grid_lane_idx == other.grid_lane_idx && token == other.token;
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_GRID_LANES_GRID_LANES_ITEM_ITERATOR_H_
