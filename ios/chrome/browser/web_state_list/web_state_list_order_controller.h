// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_WEB_STATE_LIST_WEB_STATE_LIST_ORDER_CONTROLLER_H_
#define IOS_CHROME_BROWSER_WEB_STATE_LIST_WEB_STATE_LIST_ORDER_CONTROLLER_H_

class WebStateList;
class WebStateListRemovingIndexes;

namespace web {
class WebState;
}

// WebStateListOrderController allows different types of ordering and
// selection heuristics to be plugged into WebStateList.
class WebStateListOrderController {
 public:
  explicit WebStateListOrderController(const WebStateList& web_state_list);
  ~WebStateListOrderController();

  // Determines where to place a newly opened WebState given its opener.
  int DetermineInsertionIndex(const web::WebState* opener) const;

  // Determines where to shift the active index after a WebState is closed.
  // The returned index will either be WebStateList::kInvalidIndex or in be
  // in range for the WebStateList once the element has been removed (i.e.
  // this function accounts for the fact that the element at |removing_index|
  // will be removed from the WebStateList).
  int DetermineNewActiveIndex(
      int active_index,
      WebStateListRemovingIndexes removing_indexes) const;

 private:
  const WebStateList& web_state_list_;
};

#endif  // IOS_CHROME_BROWSER_WEB_STATE_LIST_WEB_STATE_LIST_ORDER_CONTROLLER_H_
