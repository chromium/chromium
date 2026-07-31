// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#include "third_party/blink/renderer/core/layout/grid_lanes/grid_lanes_item_iterator.h"

#include "third_party/blink/renderer/core/layout/block_break_token.h"

namespace blink {

GridLanesItemIterator::GridLanesItemIterator(
    const GridLanesDataVector& grid_lanes,
    const BlockBreakToken* break_token,
    bool is_column)
    : grid_lanes_(grid_lanes),
      break_token_(break_token),
      is_column_(is_column) {
  // Find the first lane with items to process.
  while (grid_lane_idx_ < grid_lanes_.size() &&
         (!grid_lanes_[grid_lane_idx_] ||
          grid_lanes_[grid_lane_idx_]->item_data.empty())) {
    ++grid_lane_idx_;
  }

  if (grid_lane_idx_ < grid_lanes_.size()) {
    DCHECK(grid_lanes_[grid_lane_idx_]->item_data.size());
    next_unstarted_item_ =
        grid_lanes_[grid_lane_idx_]->item_data[grid_lanes_item_idx_++];
  }

  if (break_token_) {
    const auto& child_break_tokens = break_token_->ChildBreakTokens();

    // If there are child break tokens, we don't yet know which one is the
    // next unstarted item (need to get past the child break tokens first). If
    // we've already seen all children, there will be no unstarted items.
    if (!child_break_tokens.empty() || break_token_->HasSeenAllChildren()) {
      next_unstarted_item_ = nullptr;
      grid_lane_idx_ = 0;
      grid_lanes_item_idx_ = 0;
    }

    // We're already done with this parent break token if there are no child
    // break tokens, so just forget it right away.
    if (child_break_tokens.empty()) {
      break_token_ = nullptr;
    }
  }
}

GridLanesItemIterator::Entry GridLanesItemIterator::NextItem() {
  DCHECK(is_column_);

  const BlockBreakToken* current_child_break_token = nullptr;
  GridLanesItemData* current_item = next_unstarted_item_;
  wtf_size_t current_item_idx = 0;
  wtf_size_t current_lane_idx = kNotFound;

  if (break_token_) {
    // If we're resuming layout after a fragmentainer break, we'll first resume
    // the items that fragmented earlier (represented by one break token
    // each).
    DCHECK(!next_unstarted_item_);
    const auto& child_break_tokens = break_token_->ChildBreakTokens();

    if (child_token_idx_ < child_break_tokens.size()) {
      current_child_break_token =
          To<BlockBreakToken>(child_break_tokens[child_token_idx_++].Get());
      DCHECK(current_child_break_token);
      current_item = FindNextItem(current_child_break_token);

      if (is_column_) {
        while (next_item_idx_for_lane_.size() <= grid_lane_idx_) {
          next_item_idx_for_lane_.push_back(0);
        }
        // Store the next item index to process for this column so that the
        // remaining items can be processed after the break tokens have been
        // handled.
        next_item_idx_for_lane_[grid_lane_idx_] = grid_lanes_item_idx_;
      }

      current_item_idx = grid_lanes_item_idx_ - 1;
      current_lane_idx = grid_lane_idx_;

      if (child_token_idx_ == child_break_tokens.size()) {
        // We reached the last child break token. Prepare for the next unstarted
        // sibling, and forget the parent break token.
        //
        // TODO(almaher): Similar to flex, we'll need to handle special row
        // logic here.
        if (!break_token_->HasSeenAllChildren()) {
          if (is_column_) {
            // Re-iterate over the columns to find any unprocessed items.
            grid_lane_idx_ = 0;
            grid_lanes_item_idx_ = next_item_idx_for_lane_[grid_lane_idx_];
          }
          next_unstarted_item_ = FindNextItem();
          break_token_ = nullptr;
        }
      }
    }
  } else {
    current_item_idx = grid_lanes_item_idx_ - 1;
    current_lane_idx = grid_lane_idx_;
    if (next_unstarted_item_) {
      next_unstarted_item_ = FindNextItem();
    }
  }

  return Entry(current_item, current_item_idx, current_lane_idx,
               current_child_break_token);
}

GridLanesItemData* GridLanesItemIterator::FindNextItem(
    const BlockBreakToken* item_break_token) {
  while (grid_lane_idx_ < grid_lanes_.size()) {
    GridLaneData* lane_data = grid_lanes_[grid_lane_idx_];
    if (lane_data && (!lane_data->has_seen_all_children || item_break_token)) {
      // TODO(almaher): Support fragmented items that were densely packed above
      // a spanner.
      while (grid_lanes_item_idx_ < lane_data->item_data.size()) {
        GridLanesItemData* item_data =
            lane_data->item_data[grid_lanes_item_idx_++];

        if (item_data->is_item_start &&
            (!item_break_token ||
             item_data->item->node == item_break_token->InputNode())) {
          return item_data;
        }
      }
    }

    // If the current column had a break token, but later columns do not, that
    // means that those later columns have completed layout and can be skipped.
    if (is_column_ && !item_break_token &&
        grid_lane_idx_ == next_item_idx_for_lane_.size() - 1) {
      break;
    }

    ++grid_lane_idx_;
    AdjustItemIndexForNewLane();
  }

  // We handle break tokens for all columns before moving to the unprocessed
  // items for each column. This means that we may process a break token in an
  // earlier column after a break token in a later column. Thus, if we haven't
  // found the item matching the current break token, re-iterate from the first
  // column.
  if (item_break_token) {
    DCHECK(is_column_);
    grid_lane_idx_ = 0;
    AdjustItemIndexForNewLane();
    return FindNextItem(item_break_token);
  }
  return nullptr;
}

void GridLanesItemIterator::AdjustItemIndexForNewLane() {
  if (grid_lane_idx_ < next_item_idx_for_lane_.size()) {
    grid_lanes_item_idx_ = next_item_idx_for_lane_[grid_lane_idx_];
  } else {
    grid_lanes_item_idx_ = 0;
  }
}

}  // namespace blink
