// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/web_state_list/web_state_list_order_controller.h"

#import <algorithm>
#import <cstdint>
#import <set>

#import "base/check_op.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_opener.h"
#import "ios/chrome/browser/web_state_list/web_state_list_removing_indexes.h"

namespace {

// Find the index of next non-removed WebState opened by `web_state`. It
// may return WebStateList::kInvalidIndex if there is no such indexes.
int FindIndexOfNextNonRemovedWebStateOpenedBy(
    const WebStateListRemovingIndexes& removing_indexes,
    const WebStateList& web_state_list,
    const web::WebState* web_state,
    int starting_index) {
  std::set<int> children;
  for (;;) {
    const int child_index = web_state_list.GetIndexOfNextWebStateOpenedBy(
        web_state, starting_index, false);

    // The active WebState has no child, fall back to the next heuristic.
    if (child_index == WebStateList::kInvalidIndex) {
      break;
    }

    // All children are going to be removed, fallback to the next heuristic.
    if (children.find(child_index) != children.end()) {
      break;
    }

    // Found a child that is not removed, select it as the next active
    // WebState.
    const int child_index_after_removal =
        removing_indexes.IndexAfterRemoval(child_index);

    if (child_index_after_removal != WebStateList::kInvalidIndex) {
      return child_index_after_removal;
    }

    children.insert(child_index);
    starting_index = child_index;
  }

  return WebStateList::kInvalidIndex;
}

}  // anonymous namespace

WebStateListOrderController::WebStateListOrderController(
    const WebStateList& web_state_list)
    : web_state_list_(web_state_list) {}

WebStateListOrderController::~WebStateListOrderController() = default;

int WebStateListOrderController::DetermineInsertionIndex(
    int desired_index,
    const web::WebState* opener,
    bool forced,
    bool pinned) const {
  // Forced index has superiority over anything else. The only thing that should
  // be checked here if `desired_index` is within the proper range of WebStates
  // (e.g. pinned or regular).
  if (forced) {
    return ConstrainInsertionIndex(desired_index, pinned);
  }

  // If there is no opener, WebState should be added at the end of the list of
  // the same kind of WebStates (e.g. pinned or regular).
  if (!opener) {
    return ConstrainInsertionIndex(WebStateList::kInvalidIndex, pinned);
  }

  int opener_index = web_state_list_.GetIndexOfWebState(opener);
  DCHECK_NE(WebStateList::kInvalidIndex, opener_index);

  const int list_child_index = web_state_list_.GetIndexOfLastWebStateOpenedBy(
      opener, opener_index, true);

  const int reference_index = list_child_index != WebStateList::kInvalidIndex
                                  ? list_child_index
                                  : opener_index;

  // Check for overflows (just a DCHECK as INT_MAX open WebState is unlikely).
  DCHECK_LT(reference_index, INT_MAX);
  return ConstrainInsertionIndex(reference_index + 1, pinned);
}

int WebStateListOrderController::DetermineNewActiveIndex(
    int active_index,
    WebStateListRemovingIndexes removing_indexes) const {
  // If there is no active element, then there will be no new active element
  // after closing the element.
  if (active_index == WebStateList::kInvalidIndex) {
    return WebStateList::kInvalidIndex;
  }

  // Otherwise the index needs to be valid.
  DCHECK(web_state_list_.ContainsIndex(active_index));

  // If the elements removed are the the sole remaining elements in the
  // WebStateList, clear the selection (as the list will be empty).
  const int count = web_state_list_.count();
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
  const int child_index_after_removal =
      FindIndexOfNextNonRemovedWebStateOpenedBy(
          removing_indexes, web_state_list_,
          web_state_list_.GetWebStateAt(active_index), active_index);
  if (child_index_after_removal != WebStateList::kInvalidIndex) {
    return child_index_after_removal;
  }

  const WebStateOpener opener =
      web_state_list_.GetOpenerOfWebStateAt(active_index);
  if (opener.opener) {
    // Check if any of the "sibling" of the active WebState can be selected
    // to be the new active element. Prefer siblings located after the active
    // element, but this may end up selecting an element before it.
    const int sibling_index_after_removal =
        FindIndexOfNextNonRemovedWebStateOpenedBy(
            removing_indexes, web_state_list_, opener.opener, active_index);
    if (sibling_index_after_removal != WebStateList::kInvalidIndex) {
      return sibling_index_after_removal;
    }

    // If the opener is not removed, select it as the next WebState.
    const int opener_index_after_removal = removing_indexes.IndexAfterRemoval(
        web_state_list_.GetIndexOfWebState(opener.opener));
    if (opener_index_after_removal != WebStateList::kInvalidIndex) {
      return opener_index_after_removal;
    }
  }

  const bool is_pinned = web_state_list_.IsWebStatePinnedAt(active_index);

  const int first_non_pinned_tab =
      web_state_list_.GetIndexOfFirstNonPinnedWebState();

  const int start = is_pinned ? 0 : first_non_pinned_tab;
  const int end = is_pinned ? first_non_pinned_tab : count;

  // Look for the closest non-removed WebState after the active WebState in the
  // same pinned/regular group.
  for (int index = active_index + 1; index < end; ++index) {
    const int index_after_removal = removing_indexes.IndexAfterRemoval(index);
    if (index_after_removal != WebStateList::kInvalidIndex) {
      return index_after_removal;
    }
  }

  // Look for the closest non-removed WebState before the active WebState in the
  // same pinned/regular group.
  for (int index = active_index - 1; index >= start; --index) {
    const int index_after_removal = removing_indexes.IndexAfterRemoval(index);
    if (index_after_removal != WebStateList::kInvalidIndex) {
      return index_after_removal;
    }
  }

  // Look for the closest non-removed WebState after the active WebState.
  for (int index = active_index + 1; index < count; ++index) {
    const int index_after_removal = removing_indexes.IndexAfterRemoval(index);
    if (index_after_removal != WebStateList::kInvalidIndex) {
      return index_after_removal;
    }
  }

  // Look for the closest non-removed WebState before the active WebState.
  for (int index = active_index - 1; index >= 0; --index) {
    const int index_after_removal = removing_indexes.IndexAfterRemoval(index);
    if (index_after_removal != WebStateList::kInvalidIndex) {
      return index_after_removal;
    }
  }

  NOTREACHED() << "No active WebState selected by WebStateList not empty";
  return WebStateList::kInvalidIndex;
}

int WebStateListOrderController::ConstrainInsertionIndex(int index,
                                                         bool pinned) const {
  int min = pinned ? 0 : web_state_list_.GetIndexOfFirstNonPinnedWebState();
  int max = pinned ? web_state_list_.GetIndexOfFirstNonPinnedWebState()
                   : web_state_list_.count();

  if (index < min || index > max) {
    return max;
  }

  return index;
}
