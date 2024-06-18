// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/saved_tab_groups/model/ios_tab_group_sync_delegate.h"

#import <vector>

#import "base/check.h"
#import "base/uuid.h"
#import "components/saved_tab_groups/saved_tab_group_tab.h"
#import "components/saved_tab_groups/tab_group_sync_service.h"
#import "components/saved_tab_groups/types.h"
#import "components/tab_groups/tab_group_id.h"
#import "components/tab_groups/tab_group_visual_data.h"
#import "ios/chrome/app/application_delegate/app_state.h"
#import "ios/chrome/browser/saved_tab_groups/model/tab_group_sync_service_factory.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser/browser_list.h"
#import "ios/chrome/browser/shared/model/browser/browser_list_factory.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider_interface.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/shared/model/web_state_list/tab_group.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/tab_insertion/model/tab_insertion_browser_agent.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "ios/web/public/web_state.h"
#import "ios/web/public/web_state_id.h"

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
  if (synced_tab_group.saved_tabs().size() == 0) {
    return;
  }

  Browser* browser = GetMostActiveSceneBrowser();
  if (!browser) {
    return;
  }

  WebStateList* web_state_list = browser->GetWebStateList();
  TabInsertionBrowserAgent* tab_insertion_browser_agent =
      TabInsertionBrowserAgent::FromBrowser(browser);
  int insertion_index = web_state_list->count();
  std::set<int> inserted_indexes;
  // To do the mapping on the service, the local group ID is necessary. Keep a
  // temporary mapping until the group is created.
  std::map<const base::Uuid, const LocalTabID> distant_to_local_tab_mapping;
  // Create a copy of the vector to be able to sort it.
  std::vector<SavedTabGroupTab> tabs = synced_tab_group.saved_tabs();
  std::sort(tabs.begin(), tabs.end(),
            [](const SavedTabGroupTab& a, const SavedTabGroupTab& b) {
              return a.position() < b.position();
            });

  for (const SavedTabGroupTab& tab : tabs) {
    web::WebState* web_state =
        InsertDistantTab(tab, tab_insertion_browser_agent, insertion_index);
    distant_to_local_tab_mapping.insert(
        {tab.saved_tab_guid(), web_state->GetUniqueIdentifier().identifier()});
    inserted_indexes.insert(insertion_index);
    insertion_index++;
  }

  TabGroupSyncService* sync_service =
      TabGroupSyncServiceFactory::GetForBrowserState(browser_state_);

  TabGroupVisualData visual_data = {synced_tab_group.title(),
                                    synced_tab_group.color()};
  TabGroupId local_group_id = TabGroupId::GenerateNew();

  // Do the association on the server before creating it in the WebStateList to
  // avoid creating another group in the service.
  sync_service->UpdateLocalTabGroupMapping(synced_tab_group.saved_guid(),
                                           local_group_id);
  for (auto const& [key, val] : distant_to_local_tab_mapping) {
    sync_service->UpdateLocalTabId(local_group_id, key, val);
  }

  web_state_list->CreateGroup(inserted_indexes, visual_data, local_group_id);
}

void IOSTabGroupSyncDelegate::CloseLocalTabGroup(
    const LocalTabGroupID& local_tab_group_id) {
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

Browser* IOSTabGroupSyncDelegate::GetMostActiveSceneBrowser() {
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
  return browser;
}

web::WebState* IOSTabGroupSyncDelegate::InsertDistantTab(
    const SavedTabGroupTab& tab,
    TabInsertionBrowserAgent* tab_insertion_browser_agent,
    int web_state_index) {
  web::NavigationManager::WebLoadParams web_params(tab.url());
  TabInsertion::Params tab_insertion_params;
  tab_insertion_params.index = web_state_index;
  tab_insertion_params.in_background = true;
  tab_insertion_params.instant_load = false;
  tab_insertion_params.placeholder_title = tab.title();
  return tab_insertion_browser_agent->InsertWebState(web_params,
                                                     tab_insertion_params);
}

}  // namespace tab_groups
