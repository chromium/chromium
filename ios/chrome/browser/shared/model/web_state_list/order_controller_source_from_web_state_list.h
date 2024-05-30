// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_MODEL_WEB_STATE_LIST_ORDER_CONTROLLER_SOURCE_FROM_WEB_STATE_LIST_H_
#define IOS_CHROME_BROWSER_SHARED_MODEL_WEB_STATE_LIST_ORDER_CONTROLLER_SOURCE_FROM_WEB_STATE_LIST_H_

#include "base/memory/raw_ref.h"
#include "ios/chrome/browser/shared/model/web_state_list/order_controller_source.h"

class WebStateList;

// A concrete implementation of OrderControllerSource that query data
// from a genuine WebStateList.
class OrderControllerSourceFromWebStateList final
    : public OrderControllerSource {
 public:
  // Constructor taking the `web_state_list` used to return the data.
  explicit OrderControllerSourceFromWebStateList(
      const WebStateList& web_state_list);

  // OrderControllerSource implementation.
  int GetCount() const final;
  int GetPinnedCount() const final;
  int GetOpenerOfItemAt(int index) const final;
  bool IsOpenerOfItemAt(int index,
                        int opener_index,
                        bool check_navigation_index) const final;
  TabGroupRange GetGroupRangeOfItemAt(int index) const final;
  std::set<int> GetCollapsedGroupIndexes() const final;

 private:
  raw_ref<const WebStateList> web_state_list_;
};

#endif  // IOS_CHROME_BROWSER_SHARED_MODEL_WEB_STATE_LIST_ORDER_CONTROLLER_SOURCE_FROM_WEB_STATE_LIST_H_
