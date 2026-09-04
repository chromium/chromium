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
// `NextItem()` processes items in one column at a time, with spanners being an
// exception to the rule. When a spanner is hit, all items before it in each
// lane it spans must be processed before the spanner itself.
//
// In the case of rows, every item in a lane is started in the same (grid lanes)
// container fragment, so once the break tokens of a row have been handled the
// iterator moves on to the next row.
//
// In all cases, though child break tokens are processed first, in break token
// order. The iterator then resumes unstarted items from those positions onward.
//
// This class does not handle modifications to its arguments after it has been
// constructed.
//
// TODO(almaher): Handle spanners for row containers.
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

  bool HasMoreBreakTokens() const { return break_token_; }

  // Move the iterator to the next lane, unless we are already at the start of a
  // lane.
  void NextLane();

  // Returns true if the next item to be processed is in the same lane as
  // `grid_lane_idx`.
  bool HasNextItemInLane(wtf_size_t grid_lane_idx) const {
    DCHECK_LT(grid_lane_idx, grid_lanes_.size());

    // If there is no next item, then we are past the last lane with items.
    return grid_lane_idx == grid_lane_idx_ && next_unstarted_item_;
  }

 private:
  GridLanesItemData* FindNextItem(
      const BlockBreakToken* item_break_token = nullptr);
  void AdjustItemIndexForNewLane();

  // Identifies a column spanner that cannot be returned until every preceding
  // item in each lane it spans has been processed.
  struct PendingColumnSpanner {
    // The index of the lane the spanner starts in, as well as its index in that
    // lanes.
    wtf_size_t lane_idx;
    wtf_size_t item_idx;
    // The index immediately after the last lane occupied by the spanner. The
    // iterator visits lanes in increasing order, so when it reaches this index,
    // it has processed every lane the spanner occupies and can return to the
    // spanner.
    wtf_size_t end_lane_idx;
  };

  // If `item_data` starts a spanner in a column container, this adds the
  // spanner to `pending_column_spanners_` so that preceding items in the rest
  // of its span are processed first and returns true. Otherwise returns false.
  bool StartPendingColumnSpanner(const GridLanesItemData& item_data);

  // Returns the next pending spanner once every lane it spans has processed
  // items before the spanner, moving the iterator to that entry. Returns
  // nullptr if there is no pending spanner or if the next pending spanner isn't
  // ready to be processed yet.
  GridLanesItemData* MaybeProcessNextPendingColumnSpanner();

  // Returns true if `item_data` is an entry in `pending_column_spanners_`.
  bool IsPendingColumnSpanner(const GridLanesItemData& item_data) const;

  void MaybeAdvanceLanesPastColumnSpanner(const GridLanesItemData& item_data);

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
  // When a spanner is reached in a column grid lanes container, every item
  // before it in each lane it spans must be laid out first. A break in a later
  // lane may otherwise push the spanner into another fragmentainer after it has
  // already been laid out. This stack holds spanners while those earlier items
  // are processed; a spanner reached while processing another is resolved
  // first.
  Vector<PendingColumnSpanner, 1> pending_column_spanners_;
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
