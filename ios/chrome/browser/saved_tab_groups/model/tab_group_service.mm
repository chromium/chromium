// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/saved_tab_groups/model/tab_group_service.h"

#import "base/check.h"
#import "ios/chrome/browser/ntp/model/new_tab_page_tab_helper.h"
#import "ios/chrome/browser/saved_tab_groups/model/ios_tab_group_sync_util.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "ios/chrome/browser/shared/model/web_state_list/tab_group.h"
#import "ios/web/public/navigation/navigation_item.h"
#import "ios/web/public/navigation/navigation_manager.h"

TabGroupService::TabGroupService(
    ProfileIOS* profile,
    tab_groups::TabGroupSyncService* tab_group_sync_service)
    : profile_(profile), tab_group_sync_service_(tab_group_sync_service) {
  DCHECK(profile_);
  DCHECK(tab_group_sync_service_);
}

TabGroupService::~TabGroupService() {}

void TabGroupService::Shutdown() {}

bool TabGroupService::ShouldDeleteGroup(const TabGroup* group) {
  if (group->range().count() > 1) {
    return false;
  }
  if (IsSharedGroup(group)) {
    return false;
  }
  return true;
}

std::unique_ptr<web::WebState> TabGroupService::WebStateToAddToEmptyGroup() {
  // Returns a NTP.
  web::WebState::CreateParams create_params(profile_.get());
  std::unique_ptr<web::WebState> web_state =
      web::WebState::Create(create_params);
  GURL new_tab_page_url(kChromeUINewTabURL);
  web::NavigationManager::WebLoadParams load_params(new_tab_page_url);
  // Retrieve the view before loading the URL to ensure the tab's content is
  // fully loaded. This allows the grid display to accurately reflect the tab's
  // snapshot and title, rather than displaying 'Untitled' and a blank image.
  web_state->GetView();
  web_state->GetNavigationManager()->LoadURLWithParams(load_params);
  return web_state;
}

void TabGroupService::RegisterCollaborationControllerDelegate(
    tab_groups::LocalTabGroupID tab_group_id,
    base::WeakPtr<collaboration::IOSCollaborationControllerDelegate>
        controller_delegate) {
  CHECK(!group_to_controller_delegate_.contains(tab_group_id),
        base::NotFatalUntil::M142);
  group_to_controller_delegate_[tab_group_id] = controller_delegate;
}

void TabGroupService::UnregisterCollaborationControllerDelegate(
    tab_groups::LocalTabGroupID tab_group_id) {
  group_to_controller_delegate_.erase(tab_group_id);
}

collaboration::IOSCollaborationControllerDelegate*
TabGroupService::GetDelegateForGroup(tab_groups::LocalTabGroupID tab_group_id) {
  return group_to_controller_delegate_[tab_group_id].get();
}

bool TabGroupService::IsSharedGroup(const TabGroup* group) {
  CHECK(group);
  return tab_groups::utils::IsTabGroupShared(group, tab_group_sync_service_);
}

bool TabGroupService::ShouldDisplayLastTabCloseAlert(const TabGroup* group) {
  return group && IsSharedGroup(group) && group->range().count() == 1;
}
