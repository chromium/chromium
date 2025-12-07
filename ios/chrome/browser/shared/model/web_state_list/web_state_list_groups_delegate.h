// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_MODEL_WEB_STATE_LIST_WEB_STATE_LIST_GROUPS_DELEGATE_H_
#define IOS_CHROME_BROWSER_SHARED_MODEL_WEB_STATE_LIST_WEB_STATE_LIST_GROUPS_DELEGATE_H_

class TabGroup;

namespace web {
class WebState;
}

// A delegate interface that the WebStateList uses to perform work for groups
// that it cannot do itself.
class WebStateListGroupsDelegate {
 public:
  WebStateListGroupsDelegate() = default;

  WebStateListGroupsDelegate(const WebStateListGroupsDelegate&) = delete;
  WebStateListGroupsDelegate& operator=(const WebStateListGroupsDelegate&) =
      delete;

  virtual ~WebStateListGroupsDelegate() = default;

  // Notifies the delegate that the specified TabGroup is going to be deleted if
  // no action is taken. If the group should not be deleted, then
  // `no_deletion_callback` is called.
  virtual bool ShouldDeleteGroup(const TabGroup* group) = 0;

  // Returns the web state to insert when a group is empty and should not be
  // deleted.
  virtual std::unique_ptr<web::WebState> WebStateToAddToEmptyGroup() = 0;
};

#endif  // IOS_CHROME_BROWSER_SHARED_MODEL_WEB_STATE_LIST_WEB_STATE_LIST_GROUPS_DELEGATE_H_
