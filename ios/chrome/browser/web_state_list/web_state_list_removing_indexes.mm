// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/web_state_list/web_state_list_removing_indexes.h"

#include <algorithm>
#include <set>

#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/chrome/browser/web_state_list/web_state_opener.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

WebStateListRemovingIndexes::WebStateListRemovingIndexes(
    std::vector<int> indexes)
    : indexes_(std::move(indexes)) {
  std::sort(indexes_.begin(), indexes_.end());
  indexes_.erase(std::unique(indexes_.begin(), indexes_.end()), indexes_.end());
}

WebStateListRemovingIndexes::WebStateListRemovingIndexes(
    std::initializer_list<int> indexes)
    : WebStateListRemovingIndexes(std::vector<int>(indexes)) {}

WebStateListRemovingIndexes::~WebStateListRemovingIndexes() = default;

int WebStateListRemovingIndexes::IndexAfterRemoval(int index) const {
  const auto lower_bound =
      std::lower_bound(indexes_.begin(), indexes_.end(), index);

  if (lower_bound == indexes_.end() || *lower_bound != index)
    return index - std::distance(indexes_.begin(), lower_bound);

  // The index is scheduled for removal.
  return WebStateList::kInvalidIndex;
}

int WebStateListRemovingIndexes::FindIndexOfNextNonRemovedWebStateOpenedBy(
    const WebStateList& web_state_list,
    const web::WebState* web_state,
    int starting_index) {
  std::set<int> children;
  for (;;) {
    const int child_index = web_state_list.GetIndexOfNextWebStateOpenedBy(
        web_state, starting_index, false);

    // The active WebState has no child, fall back to the next heuristic.
    if (child_index == WebStateList::kInvalidIndex)
      break;

    // All children are going to be removed, fallback to the next heuristic.
    if (children.find(child_index) != children.end())
      break;

    // Found a child that is not removed, select it as the next active
    // WebState.
    const int child_index_after_removal = IndexAfterRemoval(child_index);
    if (child_index_after_removal != WebStateList::kInvalidIndex)
      return child_index_after_removal;

    children.insert(child_index);
    starting_index = child_index;
  }

  return WebStateList::kInvalidIndex;
}
