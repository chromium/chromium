// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_MODEL_WEB_STATE_LIST_ORDER_CONTROLLER_SOURCE_H_
#define IOS_CHROME_BROWSER_SHARED_MODEL_WEB_STATE_LIST_ORDER_CONTROLLER_SOURCE_H_

#import "ios/chrome/browser/shared/model/web_state_list/tab_group_range.h"

// Class abstracting access to the WebStateList storage for use by
// OrderController, allowing the use of the algorithms on either a
// WebStateList or its serialized representation.
class OrderControllerSource {
 public:
  OrderControllerSource() = default;
  virtual ~OrderControllerSource() = default;

  // Returns the total number of items stored in the list.
  virtual int GetCount() const = 0;

  // Returns the number of pinned items stored in the list.
  virtual int GetPinnedCount() const = 0;

  // Returns the index of the opener of the item at `index`, or
  // WebStateList::kInvalidIndex if the item has no opener.
  virtual int GetOpenerOfItemAt(int index) const = 0;

  // Returns whether the item at `index` is opened by the item at
  // `opener_index`. If `check_navigation_index`, also validates
  // that the opener has not navigated from the page it was when
  // the child was opened.
  virtual bool IsOpenerOfItemAt(int index,
                                int opener_index,
                                bool check_navigation_index) const = 0;

  // Returns the `TabGroupRange` for the item at `index`. If the item does not
  // belong to a group, returns `TabGroupRange::InvalidRange()`.
  virtual TabGroupRange GetGroupRangeOfItemAt(int index) const = 0;

  // Returns a set of indexes that correspond to all tab in collapsed groups.
  virtual std::set<int> GetCollapsedGroupIndexes() const = 0;
};

#endif  // IOS_CHROME_BROWSER_SHARED_MODEL_WEB_STATE_LIST_ORDER_CONTROLLER_SOURCE_H_
