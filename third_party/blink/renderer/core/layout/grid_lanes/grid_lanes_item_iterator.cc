// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#include "third_party/blink/renderer/core/layout/grid_lanes/grid_lanes_item_iterator.h"

#include "third_party/blink/renderer/core/layout/block_break_token.h"

namespace blink {

namespace {

bool MoveToNextSibling(const GridLaneData* lane_data,
                       ItemIndexPath& item_index_path) {
  CHECK(!item_index_path.empty());
  wtf_size_t sibling_count = lane_data->item_data.size();
  if (item_index_path.size() > 1) {
    ItemIndexPath parent_path = item_index_path;
    parent_path.pop_back();
    sibling_count = GridLanesItemDataFromPath(*lane_data, parent_path)
                        ->items_densely_packed_above.size();
  }
  if (item_index_path.back() + 1 >= sibling_count) {
    return false;
  }
  ++item_index_path.back();
  return true;
}

void AppendFirstPostorderedDenselyPackedItem(const GridLaneData* lane_data,
                                             ItemIndexPath& item_index_path) {
  while (!GridLanesItemDataFromPath(*lane_data, item_index_path)
              ->items_densely_packed_above.empty()) {
    item_index_path.push_back(0u);
  }
}

ItemIndexPath FirstPostorderItemPath(const GridLaneData* lane_data) {
  CHECK(lane_data);
  if (lane_data->item_data.empty()) {
    return {};
  }

  ItemIndexPath item_index_path;
  item_index_path.push_back(0u);
  AppendFirstPostorderedDenselyPackedItem(lane_data, item_index_path);
  return item_index_path;
}

ItemIndexPath NextPostorderItemPath(const GridLaneData* lane_data,
                                    const ItemIndexPath& item_index_path) {
  CHECK(lane_data);
  CHECK(!item_index_path.empty());
  ItemIndexPath path = item_index_path;

  if (MoveToNextSibling(lane_data, path)) {
    AppendFirstPostorderedDenselyPackedItem(lane_data, path);
    return path;
  }

  path.pop_back();
  return path;
}

ItemIndexPath FirstPreorderItemPath(const GridLaneData* lane_data) {
  ItemIndexPath path;
  if (!lane_data->item_data.empty()) {
    path.push_back(0u);
  }
  return path;
}

ItemIndexPath NextPreorderItemPath(const GridLaneData* lane_data,
                                   const ItemIndexPath& item_index_path) {
  CHECK(!item_index_path.empty());
  ItemIndexPath path = item_index_path;
  if (!GridLanesItemDataFromPath(*lane_data, path)
           ->items_densely_packed_above.empty()) {
    path.push_back(0u);
    return path;
  }

  while (!path.empty()) {
    if (MoveToNextSibling(lane_data, path)) {
      return path;
    }
    path.pop_back();
  }
  return path;
}

}  // namespace

GridLanesItemIterator::GridLanesItemIterator(
    const GridLanesDataVector& grid_lanes,
    const BlockBreakToken* break_token,
    TraversalOrder traversal_order,
    bool is_column)
    : grid_lanes_(grid_lanes),
      break_token_(break_token),
      is_column_(is_column),
      traversal_order_(traversal_order),
      next_item_index_path_for_lane_(grid_lanes.size()) {
  CHECK(traversal_order_ != kPreorder || is_column_);

  // Find the first lane with items to process.
  while (grid_lane_idx_ < grid_lanes_.size()) {
    AdjustItemIndexForNewLane();
    if (!grid_lanes_item_index_path_.empty()) {
      break;
    }
    ++grid_lane_idx_;
  }

  bool should_defer_unstarted_item_lookup = false;
  if (break_token_) {
    const auto& child_break_tokens = break_token_->ChildBreakTokens();

    // If there are child break tokens, we don't yet know which one is the
    // next unstarted item (need to get past the child break tokens first). If
    // we've already seen all children, there will be no unstarted items.
    if (!child_break_tokens.empty() || break_token_->HasSeenAllChildren()) {
      should_defer_unstarted_item_lookup = true;
      grid_lane_idx_ = 0;
      AdjustItemIndexForNewLane();
    }

    // We're already done with this parent break token if there are no child
    // break tokens, so just forget it right away.
    if (child_break_tokens.empty()) {
      break_token_ = nullptr;
    }
  }

  if (!should_defer_unstarted_item_lookup) {
    next_unstarted_item_ = FindNextItem();
  }
}

GridLanesItemIterator::Entry GridLanesItemIterator::NextItem() {
  const BlockBreakToken* current_child_break_token = nullptr;
  GridLanesItemData* current_item = next_unstarted_item_;
  wtf_size_t current_lane_idx = kNotFound;
  bool is_last_item_in_lane = false;

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
      CHECK(current_item);

      current_lane_idx = grid_lane_idx_;
      is_last_item_in_lane =
          IsLastItemInLane(current_lane_idx, grid_lanes_item_index_path_);

      if (is_column_) {
        // Store the next item path to process for this column so that the
        // remaining items can be processed after the break tokens.
        next_item_index_path_for_lane_[grid_lane_idx_] =
            grid_lanes_item_index_path_;

        if (current_item->item->Span(kForColumns).SpanSize() > 1) {
          // A spanner has an entry in every lane it occupies but only one child
          // break token. Since that token means the item started in an earlier
          // fragmentainer, advance every spanned lane past its entry so it
          // isn't returned again as an unstarted item.
          MaybeAdvanceLanesPastColumnSpanner(*current_item);
        }
      }

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
          AdjustItemIndexForNewLane();

          next_unstarted_item_ = FindNextItem();
          break_token_ = nullptr;
        }
      }
    }
  } else {
    if (next_unstarted_item_) {
      current_lane_idx = grid_lane_idx_;
      is_last_item_in_lane =
          IsLastItemInLane(current_lane_idx, grid_lanes_item_index_path_);
      next_unstarted_item_ = FindNextItem();
    }
  }

  return Entry(current_item, current_lane_idx, is_last_item_in_lane,
               current_child_break_token);
}

GridLanesItemData* GridLanesItemIterator::FindNextItem(
    const BlockBreakToken* item_break_token) {
  while (grid_lane_idx_ < grid_lanes_.size()) {
    GridLaneData* lane_data = grid_lanes_[grid_lane_idx_];
    if (lane_data && (lane_data->has_unfinished_items || item_break_token)) {
      while (!grid_lanes_item_index_path_.empty()) {
        const ItemIndexPath item_index_path = grid_lanes_item_index_path_;
        GridLanesItemData* item_data =
            GridLanesItemDataFromPath(*lane_data, item_index_path);
        grid_lanes_item_index_path_ = NextItemPath(lane_data, item_index_path);

        if (is_column_ && IsPendingColumnSpanner(*item_data)) {
          // Every item before the pending spanner in this lane has been
          // processed, so move on to the next lane it spans.
          break;
        }

        if (item_data->is_item_start &&
            (!item_break_token ||
             item_data->item->node == item_break_token->InputNode())) {
          if (!item_break_token && is_column_ &&
              StartPendingColumnSpanner(*item_data, item_index_path)) {
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
      next_item_index_path_for_lane_[grid_lane_idx_] =
          grid_lanes_item_index_path_;
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
    const GridLanesItemData& item_data,
    const ItemIndexPath& item_index_path) {
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
      grid_lane_idx_, item_index_path, lane_span.EndLine()});
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
  grid_lanes_item_index_path_ = pending_column_spanner.item_index_path;

  const GridLaneData* lane_data = grid_lanes_[grid_lane_idx_];
  CHECK(lane_data);

  GridLanesItemData* item_data = GridLanesItemDataFromPath(
      *lane_data, pending_column_spanner.item_index_path);
  CHECK(item_data->is_item_start);
  grid_lanes_item_index_path_ =
      NextItemPath(lane_data, grid_lanes_item_index_path_);
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

  return GridLanesItemDataFromPath(*lane_data,
                                   pending_column_spanner.item_index_path)
             ->item == item_data.item;
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
    for (ItemIndexPath path = FirstItemPath(lane_data); !path.empty();
         path = NextItemPath(lane_data, path)) {
      if (GridLanesItemDataFromPath(*lane_data, path)->item == item_data.item) {
        next_item_index_path_for_lane_[lane_idx] =
            NextItemPath(lane_data, path);
        break;
      }
    }
  }
}

ItemIndexPath GridLanesItemIterator::FirstItemPath(
    const GridLaneData* lane_data) const {
  CHECK(lane_data);
  return traversal_order_ == kPreorder ? FirstPreorderItemPath(lane_data)
                                       : FirstPostorderItemPath(lane_data);
}

ItemIndexPath GridLanesItemIterator::NextItemPath(
    const GridLaneData* lane_data,
    const ItemIndexPath& item_index_path) const {
  CHECK(lane_data);
  return traversal_order_ == kPreorder
             ? NextPreorderItemPath(lane_data, item_index_path)
             : NextPostorderItemPath(lane_data, item_index_path);
}

bool GridLanesItemIterator::IsLastItemInLane(
    wtf_size_t grid_lane_idx,
    const ItemIndexPath& next_item_index_path) const {
  CHECK_LT(grid_lane_idx, grid_lanes_.size());
  const GridLaneData* lane_data = grid_lanes_[grid_lane_idx];
  CHECK(lane_data);

  for (ItemIndexPath path = next_item_index_path; !path.empty();
       path = NextItemPath(lane_data, path)) {
    if (GridLanesItemDataFromPath(*lane_data, path)->is_item_start) {
      return false;
    }
  }
  return true;
}

void GridLanesItemIterator::NextLane() {
  CHECK_LT(grid_lane_idx_, grid_lanes_.size());
  const GridLaneData* lane_data = grid_lanes_[grid_lane_idx_];
  if (lane_data && grid_lanes_item_index_path_ == FirstItemPath(lane_data)) {
    return;
  }

  ++grid_lane_idx_;
  AdjustItemIndexForNewLane();
  if (!break_token_) {
    next_unstarted_item_ = FindNextItem();
  }
}

void GridLanesItemIterator::AdjustItemIndexForNewLane() {
  if (grid_lane_idx_ < next_item_index_path_for_lane_.size() &&
      grid_lanes_[grid_lane_idx_]) {
    std::optional<ItemIndexPath>& next_item_index_path =
        next_item_index_path_for_lane_[grid_lane_idx_];
    if (!next_item_index_path) {
      next_item_index_path = FirstItemPath(grid_lanes_[grid_lane_idx_].Get());
    }
    grid_lanes_item_index_path_ = next_item_index_path.value();
  } else {
    grid_lanes_item_index_path_.clear();
  }
}

}  // namespace blink
