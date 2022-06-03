// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/web_state_list/web_state_list_order_controller.h"

#include <cstdint>

#include "base/check_op.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/chrome/browser/web_state_list/web_state_opener.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

WebStateListOrderController::WebStateListOrderController(
    WebStateList* web_state_list)
    : web_state_list_(web_state_list) {
  DCHECK(web_state_list_);
}

WebStateListOrderController::~WebStateListOrderController() = default;

int WebStateListOrderController::DetermineInsertionIndex(
    web::WebState* opener) const {
  if (!opener)
    return web_state_list_->count();

  int opener_index = web_state_list_->GetIndexOfWebState(opener);
  DCHECK_NE(WebStateList::kInvalidIndex, opener_index);

  int list_child_index = web_state_list_->GetIndexOfLastWebStateOpenedBy(
      opener, opener_index, true);

  int reference_index = list_child_index != WebStateList::kInvalidIndex
                            ? list_child_index
                            : opener_index;

  // Check for overflows (just a DCHECK as INT_MAX open WebState is unlikely).
  DCHECK_LT(reference_index, INT_MAX);
  return reference_index + 1;
}

int WebStateListOrderController::DetermineNewActiveIndex(
    int active_index,
    int removing_index) const {
  DCHECK(web_state_list_->ContainsIndex(removing_index));

  // If there is no active element, then there will be no new active element
  // after closing the element.
  if (active_index == WebStateList::kInvalidIndex)
    return WebStateList::kInvalidIndex;

  // Otherwise the index needs to be valid.
  DCHECK(web_state_list_->ContainsIndex(active_index));

  // If the element removed is the the sole remaining element in the
  // WebStateList, clear the selection (as the list will be empty).
  if (web_state_list_->count() == 1)
    return WebStateList::kInvalidIndex;

  // If the element removed is not the active element, then the active element
  // won't change but its index may be decremented if after the removed element.
  if (active_index != removing_index)
    return GetValidIndex(active_index, removing_index);

  // If the element removed has any "child" then select the first child in the
  // group (start looking from removing_index, but may return a child that is
  // before the removed element).
  const int child_index = web_state_list_->GetIndexOfNextWebStateOpenedBy(
      web_state_list_->GetWebStateAt(removing_index), removing_index, false);

  if (child_index != WebStateList::kInvalidIndex)
    return GetValidIndex(child_index, removing_index);

  // If the element removed has no opener, then it is not part of a group. In
  // that case, select the next element, unless the last element is removed
  // in which case the previous one is selected.
  const WebStateOpener opener =
      web_state_list_->GetOpenerOfWebStateAt(removing_index);
  if (!opener.opener) {
    if (removing_index == web_state_list_->count() - 1)
      return removing_index - 1;

    return removing_index;
  }

  // If the element removed is part of a group, select the next sibling in the
  // group (start looking from removing_index, but may return a sibling that
  // is before the removed element).
  const int sibling_index = web_state_list_->GetIndexOfNextWebStateOpenedBy(
      opener.opener, removing_index, false);

  if (sibling_index != WebStateList::kInvalidIndex)
    return GetValidIndex(sibling_index, removing_index);

  // Otherwise, select the opener.
  const int opener_index = web_state_list_->GetIndexOfWebState(opener.opener);
  DCHECK_NE(opener_index, WebStateList::kInvalidIndex);
  return GetValidIndex(opener_index, removing_index);
}

int WebStateListOrderController::GetValidIndex(int active_index,
                                               int removing_index) const {
  if (active_index < removing_index)
    return active_index;

  DCHECK_NE(active_index, removing_index);
  return active_index - 1;
}
