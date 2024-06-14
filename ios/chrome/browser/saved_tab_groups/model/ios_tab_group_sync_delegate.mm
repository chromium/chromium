// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/saved_tab_groups/model/ios_tab_group_sync_delegate.h"

#import <vector>

#import "base/check.h"
#import "ios/chrome/app/application_delegate/app_state.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser/browser_list.h"
#import "ios/chrome/browser/shared/model/browser/browser_list_factory.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider_interface.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/shared/model/web_state_list/tab_group.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"

namespace tab_groups {

IOSTabGroupSyncDelegate::IOSTabGroupSyncDelegate(
    ChromeBrowserState* browser_state)
    : browser_state_(browser_state) {
  CHECK(!browser_state_->IsOffTheRecord());
}

void IOSTabGroupSyncDelegate::HandleOpenTabGroupRequest(
    const base::Uuid& sync_tab_group_id,
    std::unique_ptr<TabGroupActionContext> context) {
  // TODO(crbug.com/329640329): Send the command to update the UI here.
}

void IOSTabGroupSyncDelegate::CreateLocalTabGroup(
    const SavedTabGroup& synced_tab_group) {
  BrowserList* browser_list =
      BrowserListFactory::GetForBrowserState(browser_state_);
  std::set<Browser*> all_browsers = browser_list->AllRegularBrowsers();
  CHECK(all_browsers.size() > 0);

  Browser* browser = nullptr;
  for (Browser* browser_to_check : all_browsers) {
    // The pointer to the scene state is weak, so it could be nil. In that case,
    // the activation level will be 0 (lowest).
    if (browser && browser->GetSceneState().activationLevel >=
                       browser_to_check->GetSceneState().activationLevel) {
      continue;
    }
    browser = browser_to_check;
    if (browser_to_check->GetSceneState().activationLevel ==
        SceneActivationLevelForegroundActive) {
      break;
    }
  }

  if (!browser) {
    return;
  }
  // TODO(crbug.com/329640329): open the new group.
}

void IOSTabGroupSyncDelegate::CloseLocalTabGroup(
    const LocalTabGroupID& local_id) {
  BrowserList* browser_list =
      BrowserListFactory::GetForBrowserState(browser_state_);
  for (Browser* browser : browser_list->AllRegularBrowsers()) {
    WebStateList* web_state_list = browser->GetWebStateList();
    for (const TabGroup* _ : web_state_list->GetGroups()) {
      // TODO(crbug.com/329631494): once we have local ID for groups, delete the
      // group.
    }
  }
}

void IOSTabGroupSyncDelegate::UpdateLocalTabGroup(
    const SavedTabGroup& synced_tab_group) {
  BrowserList* browser_list =
      BrowserListFactory::GetForBrowserState(browser_state_);
  for (Browser* browser : browser_list->AllRegularBrowsers()) {
    WebStateList* web_state_list = browser->GetWebStateList();
    for (const TabGroup* _ : web_state_list->GetGroups()) {
      // TODO(crbug.com/329631494): once we have local ID for groups, update the
      // group if it is opened (exist in WSL).
    }
  }
}

}  // namespace tab_groups
