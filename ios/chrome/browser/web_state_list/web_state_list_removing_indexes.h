// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_WEB_STATE_LIST_WEB_STATE_LIST_REMOVING_INDEXES_H_
#define IOS_CHROME_BROWSER_WEB_STATE_LIST_WEB_STATE_LIST_REMOVING_INDEXES_H_

#include <initializer_list>
#include <vector>

class WebStateList;

namespace web {
class WebState;
}

// WebStateListRemovingIndexes is a class storing a list of indexes that will
// soon be closed in a WebStateList, and providing support method to fix the
// indexes of other WebStates. It is used by WebStateListOrderController to
// implement DetermineNewActiveIndex().
class WebStateListRemovingIndexes {
 public:
  explicit WebStateListRemovingIndexes(std::vector<int> indexes);
  WebStateListRemovingIndexes(std::initializer_list<int> indexes);
  ~WebStateListRemovingIndexes();

  // Returns the number of WebState that will be closed.
  int count() const { return static_cast<int>(indexes_.size()); }

  // Returns whether index is present in the list of indexes to close.
  bool Contains(int index) const;

  // Returns the new value of index after the removal. For indexes that are
  // scheduled to be removed, will return WebStateList::kInvalidIndex.
  int IndexAfterRemoval(int index) const;

  // Find the index of next non-removed WebState opened by |web_state|. It
  // may return WebStateList::kInvalidIndex if there is no such indexes.
  int FindIndexOfNextNonRemovedWebStateOpenedBy(
      const WebStateList& web_state_list,
      const web::WebState* web_state,
      int starting_index);

 private:
  std::vector<int> indexes_;
};

#endif  // IOS_CHROME_BROWSER_WEB_STATE_LIST_WEB_STATE_LIST_REMOVING_INDEXES_H_
