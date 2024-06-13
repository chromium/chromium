// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SAVED_TAB_GROUPS_MODEL_IOS_TAB_GROUP_SYNC_DELEGATE_H_
#define IOS_CHROME_BROWSER_SAVED_TAB_GROUPS_MODEL_IOS_TAB_GROUP_SYNC_DELEGATE_H_

#import "components/saved_tab_groups/saved_tab_group.h"
#import "components/saved_tab_groups/tab_group_sync_delegate.h"
#import "components/saved_tab_groups/types.h"

class ChromeBrowserState;

namespace tab_groups {

// IOS Subclass of the TabGroupSyncDelegate.
class IOSTabGroupSyncDelegate : public TabGroupSyncDelegate {
 public:
  explicit IOSTabGroupSyncDelegate(ChromeBrowserState* browser_state);

  IOSTabGroupSyncDelegate(const IOSTabGroupSyncDelegate&) = delete;
  IOSTabGroupSyncDelegate& operator=(const IOSTabGroupSyncDelegate&) = delete;

  // TabGroupSyncDelegate.
  void HandleOpenTabGroupRequest(
      const base::Uuid& sync_tab_group_id,
      std::unique_ptr<TabGroupActionContext> context) override;
  void CreateLocalTabGroup(const SavedTabGroup& synced_tab_group) override;
  void CloseLocalTabGroup(const LocalTabGroupID& local_tab_group_id) override;
  void UpdateLocalTabGroup(const SavedTabGroup& synced_tab_group) override;

 private:
  ChromeBrowserState* browser_state_;
};

}  // namespace tab_groups

#endif  // IOS_CHROME_BROWSER_SAVED_TAB_GROUPS_MODEL_IOS_TAB_GROUP_SYNC_DELEGATE_H_
