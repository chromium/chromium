// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/saved_tab_groups/model/tab_group_service.h"

#import "base/check.h"
#import "ios/chrome/browser/shared/model/web_state_list/tab_group.h"

TabGroupService::TabGroupService(
    tab_groups::TabGroupSyncService* tab_group_sync_service)
    : tab_group_sync_service_(tab_group_sync_service) {
  DCHECK(tab_group_sync_service_);
}

TabGroupService::~TabGroupService() {}

void TabGroupService::Shutdown() {}

bool TabGroupService::ShouldDeleteGroup(const TabGroup* group) {
  return group->range().count() == 0;
}

std::unique_ptr<web::WebState> TabGroupService::WebStateToAddToEmptyGroup() {
  return nullptr;
}
