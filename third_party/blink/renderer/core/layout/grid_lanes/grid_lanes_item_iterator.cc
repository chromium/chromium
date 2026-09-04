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
      is_column_(is_column),
      next_item_idx_for_lane_(grid_lanes.size(), 0u) {
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

  if (is_column_ && next_unstarted_item_ &&
      next_unstarted_item_->item->Span(kForColumns).SpanSize() > 1) {
    // A spanner can only be returned after the items before it in every lane
    // it spans, so let the lane walk pick the first item instead.
    --grid_lanes_item_idx_;
    next_unstarted_item_ = FindNextItem();
  }
}

GridLanesItemIterator::Entry GridLanesItemIterator::NextItem() {
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
        // Store the next item index to process for this column so that the
        // remaining items can be processed after the break tokens have been
        // handled.
        next_item_idx_for_lane_[grid_lane_idx_] = grid_lanes_item_idx_;

        if (current_item) {
          // A spanner has an entry in every lane it occupies but only one child
          // break token. Since that token means the item started in an earlier
          // fragmentainer, advance every spanned lane past its entry so it
          // isn't returned again as an unstarted item.
          MaybeAdvanceLanesPastColumnSpanner(*current_item);
        }
      }

      current_item_idx = grid_lanes_item_idx_ - 1;
      current_lane_idx = grid_lane_idx_;

      if (child_token_idx_ == child_break_tokens.size()) {
        // We reached the last child break token. Prepare for the next unstarted
        // sibling, and forget the parent break token.
        //
        // TODO(almaher): Similar to flex, the iterator will need to stay in the
        // current row here if the row itself broke before.
        if (!is_column_) {
          // All items in a row are started in the same (grid lanes) container
          // fragment, so a sibling of the current item without a break token
          // has already finished layout. Move on to the next row.
          //
          // Note: Rows don't produce a layout result, so if the row broke
          // before, the first item in the row will have broken before.
          break_token_ = nullptr;
          NextLane();
        } else if (!break_token_->HasSeenAllChildren()) {
          // Re-iterate over the columns to find any unprocessed items.
          grid_lane_idx_ = 0;
          grid_lanes_item_idx_ = next_item_idx_for_lane_[grid_lane_idx_];

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

        if (is_column_ && IsPendingColumnSpanner(*item_data)) {
          // Every item before the pending spanner in this lane has been
          // processed, so move on to the next lane it spans.
          break;
        }

        if (item_data->is_item_start &&
            (!item_break_token ||
             item_data->item->node == item_break_token->InputNode())) {
          if (!item_break_token && is_column_ &&
              StartPendingColumnSpanner(*item_data)) {
            // If this item is the start of a spanner, we must process the items
            // before it in every lane it spans first, so move on to the next
            // lane.
            break;
          }
          return item_data;
        }
      }
    }

    if (!pending_column_spanners_.empty()) {
      // The iterator returns to this lane once the pending spanner is
      // processed.
      next_item_idx_for_lane_[grid_lane_idx_] = grid_lanes_item_idx_;
    }

    ++grid_lane_idx_;

    // Advancing past the spanner's last lane means that every lane it spans has
    // reached its entry, so the spanner can now be processed.
    if (GridLanesItemData* pending_column_spanner =
            is_column_ ? MaybeProcessNextPendingColumnSpanner() : nullptr) {
      return pending_column_spanner;
    }

    AdjustItemIndexForNewLane();
  }

  // We handle break tokens for all columns before moving to the unprocessed
  // items for each column. This means that we may process a break token in an
  // earlier column after a break token in a later column. Thus, if we haven't
  // found the item matching the current break token, re-iterate from the first
  // column.
  if (item_break_token) {
    DCHECK(is_column_);
    DCHECK(pending_column_spanners_.empty());
    grid_lane_idx_ = 0;
    AdjustItemIndexForNewLane();
    return FindNextItem(item_break_token);
  }

  DCHECK(pending_column_spanners_.empty());
  return nullptr;
}

bool GridLanesItemIterator::StartPendingColumnSpanner(
    const GridLanesItemData& item_data) {
  CHECK(is_column_);
  CHECK(item_data.is_item_start);

  const GridSpan& lane_span = item_data.item->Span(kForColumns);
  if (lane_span.SpanSize() == 1) {
    return false;
  }

  DCHECK_EQ(lane_span.StartLine(), grid_lane_idx_);

  // The items before this spanner in the rest of the columns it spans have to
  // be processed before we can process the spanner, so remember it and continue
  // to the next column.
  pending_column_spanners_.push_back(PendingColumnSpanner{
      grid_lane_idx_, grid_lanes_item_idx_ - 1, lane_span.EndLine()});
  return true;
}

GridLanesItemData*
GridLanesItemIterator::MaybeProcessNextPendingColumnSpanner() {
  CHECK(is_column_);
  if (pending_column_spanners_.empty() ||
      pending_column_spanners_.back().end_lane_idx != grid_lane_idx_) {
    return nullptr;
  }

  // This spanner was deferred while the iterator visited the other lanes it
  // occupies. Those lanes have now all advanced to the spanner's position.
  // Restore the saved position of its entry in the start lane; only that entry
  // is marked as the start of the item and should be returned for layout.
  const PendingColumnSpanner& pending_column_spanner =
      pending_column_spanners_.back();

  grid_lane_idx_ = pending_column_spanner.lane_idx;
  grid_lanes_item_idx_ = pending_column_spanner.item_idx + 1;

  const GridLaneData* lane_data = grid_lanes_[grid_lane_idx_];
  CHECK(lane_data);

  GridLanesItemData* item_data =
      lane_data->item_data[pending_column_spanner.item_idx];
  pending_column_spanners_.pop_back();
  return item_data;
}

bool GridLanesItemIterator::IsPendingColumnSpanner(
    const GridLanesItemData& item_data) const {
  CHECK(is_column_);
  if (pending_column_spanners_.empty() || item_data.is_item_start) {
    return false;
  }

  const PendingColumnSpanner& pending_column_spanner =
      pending_column_spanners_.back();
  const GridLaneData* lane_data = grid_lanes_[pending_column_spanner.lane_idx];
  CHECK(lane_data);

  return lane_data->item_data[pending_column_spanner.item_idx]->item ==
         item_data.item;
}

void GridLanesItemIterator::MaybeAdvanceLanesPastColumnSpanner(
    const GridLanesItemData& item_data) {
  CHECK(is_column_);
  const GridSpan& lane_span = item_data.item->Span(kForColumns);
  if (lane_span.SpanSize() == 1) {
    return;
  }

  for (wtf_size_t lane_idx = lane_span.StartLine() + 1;
       lane_idx < lane_span.EndLine(); ++lane_idx) {
    const GridLaneData* lane_data = grid_lanes_[lane_idx];
    CHECK(lane_data);

    // A spanner has one entry in every lane it occupies; this lane may have
    // already resumed past it.
    const auto& lane_items = lane_data->item_data;
    for (wtf_size_t item_idx = next_item_idx_for_lane_[lane_idx];
         item_idx < lane_items.size(); ++item_idx) {
      if (lane_items[item_idx]->item == item_data.item) {
        next_item_idx_for_lane_[lane_idx] = item_idx + 1;
        break;
      }
    }
  }
}

void GridLanesItemIterator::NextLane() {
  if (grid_lanes_item_idx_ == 0) {
    return;
  }

  ++grid_lane_idx_;
  AdjustItemIndexForNewLane();
  if (!break_token_) {
    next_unstarted_item_ = FindNextItem();
  }
}

void GridLanesItemIterator::AdjustItemIndexForNewLane() {
  if (grid_lane_idx_ < next_item_idx_for_lane_.size()) {
    grid_lanes_item_idx_ = next_item_idx_for_lane_[grid_lane_idx_];
  } else {
    grid_lanes_item_idx_ = 0;
  }
}

}  // namespace blink
