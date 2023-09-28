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

// Finds the index of the `n`-th child of the WebState at `opener_index` that
// is not removed. If there are less than `n` such item, returns the index of
// the last one. If there are no items returns WebStateList::kInvalidIndex.
//
// If `check_navigation_index` is true, the opener-opened relationship is
// considered as broken if the opener has navigated away since the child has
// been opened.
//
// The returned index, if not WebStateList::kInvalidIndex, is the updated
// index after all the WebState scheduled for removal have been removed.
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

  return removing_indexes.IndexAfterRemoval(found_index);
}

// Returns the index of the closest item from `index` in range `start` - `end`
// that is not scheduled for deletion or WebStateList::kInvalidIndex. Prefer a
// WebState after `index` over one before.
//
// The returned index, if not WebStateList::kInvalidIndex, is the updated
// index after all the WebState scheduled for removal have been removed.
int FindClosestWebStateInRange(const RemovingIndexes& removing_indexes,
                               int index,
                               int start,
                               int end) {
  for (int i = index + 1; i < end; ++i) {
    if (!removing_indexes.Contains(i)) {
      return removing_indexes.IndexAfterRemoval(i);
    }
  }

  for (int i = start; i < index; ++i) {
    const int j = index - 1 - (i - start);
    if (!removing_indexes.Contains(j)) {
      return removing_indexes.IndexAfterRemoval(j);
    }
  }

  return WebStateList::kInvalidIndex;
}

}  // anonymous namespace

// static
OrderController::InsertionParams OrderController::InsertionParams::Automatic(
    ItemGroup group) {
  return InsertionParams{
      .desired_index = WebStateList::kInvalidIndex,
      .opener_index = WebStateList::kInvalidIndex,
      .group = group,
  };
}

// static
OrderController::InsertionParams OrderController::InsertionParams::ForceIndex(
    int desired_index,
    ItemGroup group) {
  DCHECK_NE(desired_index, WebStateList::kInvalidIndex);
  return InsertionParams{
      .desired_index = desired_index,
      .opener_index = WebStateList::kInvalidIndex,
      .group = group,
  };
}

// static
OrderController::InsertionParams OrderController::InsertionParams::WithOpener(
    int opener_index,
    ItemGroup group) {
  DCHECK_NE(opener_index, WebStateList::kInvalidIndex);
  return InsertionParams{
      .desired_index = WebStateList::kInvalidIndex,
      .opener_index = opener_index,
      .group = group,
  };
}

OrderController::OrderController(const OrderControllerSource& source)
    : source_(source) {}

OrderController::~OrderController() = default;

int OrderController::DetermineInsertionIndex(InsertionParams params) const {
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

  // In all cases, ensure that the index is in the correct range according
  // to the `pinned` status of the item.
  const int pinned_items_count = source_->GetPinnedCount();
  const int min = params.pinned() ? 0 : pinned_items_count;
  const int max = params.pinned() ? pinned_items_count : source_->GetCount();
  if (desired_index < min || desired_index > max) {
    return max;
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
  const int active_index_after_removal =
      removing_indexes.IndexAfterRemoval(active_index);
  if (active_index_after_removal != WebStateList::kInvalidIndex) {
    return active_index_after_removal;
  }

  // Check if any of the "child" of the active WebState can be selected to be
  // the new active element. Prefer childs located after the active element,
  // but this may end up selecting an element before it.
  const int child_index_after_removal = FindIndexOfNthWebStateOpenedBy(
      removing_indexes, source_.get(), /* check_navigation_index */ false,
      active_index, active_index, 1);
  if (child_index_after_removal != WebStateList::kInvalidIndex) {
    return child_index_after_removal;
  }

  const int opener_index = source_->GetOpenerOfItemAt(active_index);
  if (opener_index != WebStateList::kInvalidIndex) {
    // Check if any of the "sibling" of the active WebState can be selected
    // to be the new active element. Prefer siblings located after the active
    // element, but this may end up selecting an element before it.
    const int sibling_index_after_removal = FindIndexOfNthWebStateOpenedBy(
        removing_indexes, source_.get(), /* check_navigation_index */ false,
        opener_index, active_index, 1);
    if (sibling_index_after_removal != WebStateList::kInvalidIndex) {
      return sibling_index_after_removal;
    }

    // If the opener is not removed, select it as the next WebState.
    const int opener_index_after_removal =
        removing_indexes.IndexAfterRemoval(opener_index);
    if (opener_index_after_removal != WebStateList::kInvalidIndex) {
      return opener_index_after_removal;
    }
  }

  const int pinned_count = source_->GetPinnedCount();
  const bool is_pinned = active_index < pinned_count;

  // Look for the closest non-removed WebState in the same group (pinned or
  // regular WebStates).
  int closest_index = FindClosestWebStateInRange(
      removing_indexes, active_index, is_pinned ? 0 : pinned_count,
      is_pinned ? pinned_count : count);

  if (closest_index == WebStateList::kInvalidIndex) {
    // If all items in the same group are removed, look for the closest
    // WebState in the other group (pinned or regular WebStates).
    closest_index = FindClosestWebStateInRange(
        removing_indexes, active_index, is_pinned ? pinned_count : 0,
        is_pinned ? count : pinned_count);
  }

  DCHECK_NE(closest_index, WebStateList::kInvalidIndex);
  return closest_index;
}
