// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/tab_groups/tab_group_sync_service_observer_bridge.h"

TabGroupSyncServiceObserverBridge::TabGroupSyncServiceObserverBridge(
    id<TabGroupSyncServiceObserverDelegate> delegate)
    : delegate_(delegate) {}

TabGroupSyncServiceObserverBridge::~TabGroupSyncServiceObserverBridge() {}

void TabGroupSyncServiceObserverBridge::OnInitialized() {
  if ([delegate_
          respondsToSelector:@selector(tabGroupSyncServiceInitialized)]) {
    [delegate_ tabGroupSyncServiceInitialized];
  }
}

void TabGroupSyncServiceObserverBridge::OnTabGroupAdded(
    const tab_groups::SavedTabGroup& group,
    tab_groups::TriggerSource source) {
  if ([delegate_ respondsToSelector:@selector
                 (tabGroupSyncServiceTabGroupAdded:fromSource:)]) {
    [delegate_ tabGroupSyncServiceTabGroupAdded:group fromSource:source];
  }
}

void TabGroupSyncServiceObserverBridge::OnTabGroupUpdated(
    const tab_groups::SavedTabGroup& group,
    tab_groups::TriggerSource source) {
  if ([delegate_ respondsToSelector:@selector
                 (tabGroupSyncServiceTabGroupUpdated:fromSource:)]) {
    [delegate_ tabGroupSyncServiceTabGroupUpdated:group fromSource:source];
  }
}

void TabGroupSyncServiceObserverBridge::OnTabGroupRemoved(
    const tab_groups::LocalTabGroupID& local_id,
    tab_groups::TriggerSource source) {
  if ([delegate_ respondsToSelector:@selector
                 (tabGroupSyncServiceLocalTabGroupRemoved:fromSource:)]) {
    [delegate_ tabGroupSyncServiceLocalTabGroupRemoved:local_id
                                            fromSource:source];
  }
}

void TabGroupSyncServiceObserverBridge::OnTabGroupRemoved(
    const base::Uuid& sync_id,
    tab_groups::TriggerSource source) {
  if ([delegate_ respondsToSelector:@selector
                 (tabGroupSyncServiceSavedTabGroupRemoved:fromSource:)]) {
    [delegate_ tabGroupSyncServiceSavedTabGroupRemoved:sync_id
                                            fromSource:source];
  }
}

void TabGroupSyncServiceObserverBridge::OnTabGroupMigrated(
    const tab_groups::SavedTabGroup& new_group,
    const base::Uuid& old_sync_id,
    tab_groups::TriggerSource source) {
  if ([delegate_ respondsToSelector:@selector
                 (tabGroupSyncServiceTabGroupMigrated:oldSyncID:fromSource:)]) {
    [delegate_ tabGroupSyncServiceTabGroupMigrated:new_group
                                         oldSyncID:old_sync_id
                                        fromSource:source];
  }
}

void TabGroupSyncServiceObserverBridge::OnTabGroupLocalIdChanged(
    const base::Uuid& sync_id,
    const std::optional<tab_groups::LocalTabGroupID>& local_id) {
  if ([delegate_ respondsToSelector:@selector
                 (tabGroupSyncServiceSavedTabGroupLocalIdChanged:localID:)]) {
    [delegate_ tabGroupSyncServiceSavedTabGroupLocalIdChanged:sync_id
                                                      localID:local_id];
  }
}
