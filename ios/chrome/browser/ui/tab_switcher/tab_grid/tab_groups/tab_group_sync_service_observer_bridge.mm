// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_groups/tab_group_sync_service_observer_bridge.h"

TabGroupSyncServiceObserverBridge::TabGroupSyncServiceObserverBridge(
    id<TabGroupSyncServiceObserverDelegate> delegate)
    : delegate_(delegate) {}

TabGroupSyncServiceObserverBridge::~TabGroupSyncServiceObserverBridge() {}

void TabGroupSyncServiceObserverBridge::OnInitialized() {
  [delegate_ tabGroupSyncServiceInitialized];
}

void TabGroupSyncServiceObserverBridge::OnTabGroupAdded(
    const tab_groups::SavedTabGroup& group,
    tab_groups::TriggerSource source) {
  [delegate_ tabGroupSyncServiceTabGroupAdded:group fromSource:source];
}

void TabGroupSyncServiceObserverBridge::OnTabGroupUpdated(
    const tab_groups::SavedTabGroup& group,
    tab_groups::TriggerSource source) {
  [delegate_ tabGroupSyncServiceTabGroupUpdated:group fromSource:source];
}

void TabGroupSyncServiceObserverBridge::OnTabGroupRemoved(
    const tab_groups::LocalTabGroupID& local_id,
    tab_groups::TriggerSource source) {
  [delegate_ tabGroupSyncServiceLocalTabGroupRemoved:local_id
                                          fromSource:source];
}

void TabGroupSyncServiceObserverBridge::OnTabGroupRemoved(
    const base::Uuid& sync_id,
    tab_groups::TriggerSource source) {
  [delegate_ tabGroupSyncServiceSavedTabGroupRemoved:sync_id fromSource:source];
}
