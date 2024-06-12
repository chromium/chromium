// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SAVED_TAB_GROUPS_MODEL_IOS_TAB_GROUP_SYNC_DELEGATE_H_
#define IOS_CHROME_BROWSER_SAVED_TAB_GROUPS_MODEL_IOS_TAB_GROUP_SYNC_DELEGATE_H_

#import "components/saved_tab_groups/saved_tab_group.h"
#import "components/saved_tab_groups/types.h"

class ChromeBrowserState;

namespace tab_groups {

// TODO(crbug.com/329640035): Subclass TabGroupSyncDelegate.
// IOS Subclass of the TabGroupSyncDelegate.
class IOSTabGroupSyncDelegate {
 public:
  explicit IOSTabGroupSyncDelegate(ChromeBrowserState* browser_state);

  IOSTabGroupSyncDelegate(const IOSTabGroupSyncDelegate&) = delete;
  IOSTabGroupSyncDelegate& operator=(const IOSTabGroupSyncDelegate&) = delete;

  // Creates a local tab group for `synced_tab_group`.
  void CreateNewTabGroup(const SavedTabGroup& synced_tab_group);

  // Closes the specified local tab group.
  void CloseTabGroup(const LocalTabGroupID& local_tab_group_id);

  // Updates the local tab group to match `synced_tab_group`.
  void UpdateTabGroup(const SavedTabGroup& synced_tab_group);

 private:
  ChromeBrowserState* browser_state_;
};

}  // namespace tab_groups

#endif  // IOS_CHROME_BROWSER_SAVED_TAB_GROUPS_MODEL_IOS_TAB_GROUP_SYNC_DELEGATE_H_
