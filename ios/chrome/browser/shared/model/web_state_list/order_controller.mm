// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/model/web_state_list/order_controller.h"

#import <algorithm>
#import <cstdint>
#import <set>

#import "base/check_op.h"
#import "base/containers/contains.h"
#import "ios/chrome/browser/shared/model/web_state_list/order_controller_source.h"
#import "ios/chrome/browser/shared/model/web_state_list/removing_indexes.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"

namespace {

using Range = OrderController::Range;

// Finds the index of the `n`-th child of the WebState at `opener_index` that
// is not removed. If there are less than `n` such item, returns the index of
// the last one. If there are no items returns WebStateList::kInvalidIndex.
//
// If `check_navigation_index` is true, the opener-opened relationship is
// considered as broken if the opener has navigated away since the child has
// been opened.
int FindIndexOfNthWebStateOpenedBy(const RemovingIndexes& removing_indexes,
                                   const OrderControllerSource& source,
                                   bool check_navigation_index,
                                   int opener_index,
                                   int start_index,
                                   int n) {
  DCHECK_GT(n, 0);
  const int count = source.GetCount();

  int found_index = WebStateList::kInvalidIndex;
  for (int i = 1; i < count; ++i) {
    const int index = (start_index + i) % count;
    DCHECK_NE(index, start_index);

    // An item can't be its own opener.
    if (index == opener_index) {
      continue;
    }

    if (removing_indexes.Contains(index)) {
      continue;
    }

    if (!source.IsOpenerOfItemAt(index, opener_index, check_navigation_index)) {
      continue;
    }

    found_index = index;
    if (--n == 0) {
      break;
    }
  }

  return found_index;
}

// Returns the index of the closest item from `index` in `range` that is not
// scheduled for deletion, collapsed or WebStateList::kInvalidIndex. Prefer a
// WebState after `index` over one before.
int FindClosestWebStateInRange(
    const RemovingIndexes& removing_indexes,
    int index,
    Range range,
    const std::set<int>& collapsed_group_indexes = std::set<int>()) {
  for (int i = index + 1; i < range.end; ++i) {
    if (!removing_indexes.Contains(i) && !collapsed_group_indexes.contains(i)) {
      return i;
    }
  }

  for (int i = range.begin; i < index; ++i) {
    const int j = index - 1 - (i - range.begin);
    if (!removing_indexes.Contains(j) && !collapsed_group_indexes.contains(j)) {
      return j;
    }
  }

  return WebStateList::kInvalidIndex;
}

}  // anonymous namespace

// static
OrderController::InsertionParams OrderController::InsertionParams::Automatic(
    Range range) {
  return InsertionParams{
      .desired_index = WebStateList::kInvalidIndex,
      .opener_index = WebStateList::kInvalidIndex,
      .range = range,
  };
}

// static
OrderController::InsertionParams OrderController::InsertionParams::ForceIndex(
    int desired_index,
    Range range) {
  DCHECK_NE(desired_index, WebStateList::kInvalidIndex);
  return InsertionParams{
      .desired_index = desired_index,
      .opener_index = WebStateList::kInvalidIndex,
      .range = range,
  };
}

// static
OrderController::InsertionParams OrderController::InsertionParams::WithOpener(
    int opener_index,
    Range range) {
  DCHECK_NE(opener_index, WebStateList::kInvalidIndex);
  return InsertionParams{
      .desired_index = WebStateList::kInvalidIndex,
      .opener_index = opener_index,
      .range = range,
  };
}

OrderController::OrderController(const OrderControllerSource& source)
    : source_(source) {}

OrderController::~OrderController() = default;

int OrderController::DetermineInsertionIndex(InsertionParams params) const {
  DCHECK_GE(params.range.begin, 0);
  DCHECK_LE(params.range.begin, params.range.end);
  DCHECK_LE(params.range.end, source_->GetCount());

  int desired_index = WebStateList::kInvalidIndex;
  if (params.desired_index != WebStateList::kInvalidIndex) {
    // "Forced position" has the highest priority.
    desired_index = params.desired_index;
  } else if (params.opener_index != WebStateList::kInvalidIndex) {
    // "Relative to opener" means either after the last children of opener
    // or after the opener if there is no sibling.
    const int last_child_index = FindIndexOfNthWebStateOpenedBy(
        RemovingIndexes({}), source_.get(), /* check_navigation_index */ true,
        params.opener_index, params.opener_index, INT_MAX);

    if (last_child_index != WebStateList::kInvalidIndex) {
      desired_index = last_child_index + 1;
    } else {
      desired_index = params.opener_index + 1;
    }
  }

  // In all cases, ensure that the index is in the correct range.
  if (desired_index < params.range.begin || desired_index > params.range.end) {
    return params.range.end;
  }

  return desired_index;
}

int OrderController::DetermineNewActiveIndex(
    int active_index,
    const RemovingIndexes& removing_indexes) const {
  // If there is no active element, then there will be no new active element
  // after closing the element.
  if (active_index == WebStateList::kInvalidIndex) {
    return WebStateList::kInvalidIndex;
  }

  // Otherwise the index needs to be valid.
  const int count = source_->GetCount();
  DCHECK_GE(active_index, 0);
  DCHECK_LT(active_index, count);

  // If the elements removed are the the sole remaining elements in the
  // WebStateList, clear the selection (as the list will be empty).
  if (count == removing_indexes.count()) {
    return WebStateList::kInvalidIndex;
  }

  // If the active element is not removed, then the active element won't change
  // but its index may need to be tweaked if after some of the removed elements.
  if (!removing_indexes.Contains(active_index)) {
    return active_index;
  }

  // Check if any of the "child" of the active WebState can be selected to be
  // the new active element. Prefer childs located after the active element,
  // but this may end up selecting an element before it.
  const int child_index = FindIndexOfNthWebStateOpenedBy(
      removing_indexes, source_.get(), /* check_navigation_index */ false,
      active_index, active_index, 1);
  if (child_index != WebStateList::kInvalidIndex) {
    return child_index;
  }

  const int opener_index = source_->GetOpenerOfItemAt(active_index);
  if (opener_index != WebStateList::kInvalidIndex) {
    // Check if any of the "sibling" of the active WebState can be selected
    // to be the new active element. Prefer siblings located after the active
    // element, but this may end up selecting an element before it.
    const int sibling_index = FindIndexOfNthWebStateOpenedBy(
        removing_indexes, source_.get(), /* check_navigation_index */ false,
        opener_index, active_index, 1);
    if (sibling_index != WebStateList::kInvalidIndex) {
      return sibling_index;
    }

    // If the opener is not removed, select it as the next WebState.
    if (!removing_indexes.Contains(opener_index)) {
      return opener_index;
    }
  }

  const int pinned_count = source_->GetPinnedCount();
  const bool is_pinned = active_index < pinned_count;

  // The minimum range that contains all closed WebStates.
  RemovingIndexes::Range removing_indexes_span = removing_indexes.span();
  // Get the group range of the first and the last removed indexes.
  const TabGroupRange first_group_range =
      source_->GetGroupRangeOfItemAt(removing_indexes_span.start);
  const TabGroupRange last_group_range = source_->GetGroupRangeOfItemAt(
      removing_indexes_span.start + removing_indexes_span.count - 1);

  // If all the `removing_indexes` are in the same tab group, the closest
  // WebState in that group should become the next active cell.
  if (first_group_range != TabGroupRange::InvalidRange() &&
      first_group_range == last_group_range) {
    Range range = Range{.begin = first_group_range.range_begin(),
                        .end = first_group_range.range_end()};
    int closest_index =
        FindClosestWebStateInRange(removing_indexes, active_index, range);
    // `False` when all WebStates in the tab group are begging to be closed.
    if (closest_index != WebStateList::kInvalidIndex) {
      return closest_index;
    }
  }

  // Look for the closest non-removed and non-collapsed WebState (pinned or
  // regular WebStates).
  Range range = Range{.begin = is_pinned ? 0 : pinned_count,
                      .end = is_pinned ? pinned_count : count};
  int closest_index =
      FindClosestWebStateInRange(removing_indexes, active_index, range,
                                 source_->GetCollapsedGroupIndexes());
  if (closest_index != WebStateList::kInvalidIndex) {
    return closest_index;
  }

  // Look for the closest non-removed WebState (pinned or regular WebStates).
  closest_index =
      FindClosestWebStateInRange(removing_indexes, active_index, range);

  if (closest_index == WebStateList::kInvalidIndex) {
    // If all items in the same group are removed, look for the closest
    // WebState in the other group (pinned or regular WebStates).
    closest_index = FindClosestWebStateInRange(
        removing_indexes, active_index,
        Range{.begin = is_pinned ? pinned_count : 0,
              .end = is_pinned ? count : pinned_count});
  }

  DCHECK_NE(closest_index, WebStateList::kInvalidIndex);
  return closest_index;
}
