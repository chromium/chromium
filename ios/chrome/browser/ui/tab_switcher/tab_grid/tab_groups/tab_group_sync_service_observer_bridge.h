// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_TAB_GROUPS_TAB_GROUP_SYNC_SERVICE_OBSERVER_BRIDGE_H_
#define IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_TAB_GROUPS_TAB_GROUP_SYNC_SERVICE_OBSERVER_BRIDGE_H_

#import "components/saved_tab_groups/public/tab_group_sync_service.h"

namespace tab_groups {
class TabGroupSyncService;
}  // namespace tab_groups

// Objective-C protocol equivalent of the
// tab_groups::TabGroupSyncService::Observer C++ class. Those methods are called
// through the bridge. The method names are similar to the C++ ones.
@protocol TabGroupSyncServiceObserverDelegate
- (void)tabGroupSyncServiceInitialized;
- (void)tabGroupSyncServiceTabGroupAdded:(const tab_groups::SavedTabGroup&)group
                              fromSource:(tab_groups::TriggerSource)source;
- (void)tabGroupSyncServiceTabGroupUpdated:
            (const tab_groups::SavedTabGroup&)group
                                fromSource:(tab_groups::TriggerSource)source;
- (void)tabGroupSyncServiceLocalTabGroupRemoved:
            (const tab_groups::LocalTabGroupID&)localID
                                     fromSource:
                                         (tab_groups::TriggerSource)source;
- (void)tabGroupSyncServiceSavedTabGroupRemoved:(const base::Uuid&)syncID
                                     fromSource:
                                         (tab_groups::TriggerSource)source;
@end

// Bridge class to forward events from the tab_groups::TabGroupSyncService to
// Objective-C protocol TabGroupSyncServiceObserverDelegate.
class TabGroupSyncServiceObserverBridge final
    : public tab_groups::TabGroupSyncService::Observer {
 public:
  explicit TabGroupSyncServiceObserverBridge(
      id<TabGroupSyncServiceObserverDelegate> delegate);

  TabGroupSyncServiceObserverBridge(const TabGroupSyncServiceObserverBridge&) =
      delete;
  TabGroupSyncServiceObserverBridge& operator=(
      const TabGroupSyncServiceObserverBridge&) = delete;

  ~TabGroupSyncServiceObserverBridge() override;

  // TabGroupSyncService::Observer implementation.
  void OnInitialized() override;
  void OnTabGroupAdded(const tab_groups::SavedTabGroup& group,
                       tab_groups::TriggerSource source) override;
  void OnTabGroupUpdated(const tab_groups::SavedTabGroup& group,
                         tab_groups::TriggerSource source) override;
  void OnTabGroupRemoved(const tab_groups::LocalTabGroupID& local_id,
                         tab_groups::TriggerSource source) override;
  void OnTabGroupRemoved(const base::Uuid& sync_id,
                         tab_groups::TriggerSource source) override;

 private:
  __weak id<TabGroupSyncServiceObserverDelegate> delegate_ = nil;
};

#endif  // IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_TAB_GROUPS_TAB_GROUP_SYNC_SERVICE_OBSERVER_BRIDGE_H_
