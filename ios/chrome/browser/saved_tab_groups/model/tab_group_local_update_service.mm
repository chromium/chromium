// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/saved_tab_groups/model/tab_group_local_update_service.h"

#import <memory>
#import <optional>

#import "base/check.h"
#import "components/saved_tab_groups/tab_group_sync_service.h"
#import "components/saved_tab_groups/types.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser/browser_list.h"
#import "ios/chrome/browser/shared/model/browser/browser_list_factory.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/shared/model/web_state_list/browser_util.h"
#import "ios/chrome/browser/shared/model/web_state_list/tab_group.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list_observer.h"
#import "ios/web/public/web_state.h"
#import "ios/web/public/web_state_observer.h"

namespace tab_groups {

TabGroupLocalUpdateService::TabGroupLocalUpdateService(
    BrowserList* browser_list,
    TabGroupSyncService* tab_group_sync_service)
    : tab_group_sync_service_(tab_group_sync_service),
      browser_list_(browser_list) {
  browser_list_observation_.Observe(browser_list);

  std::set<Browser*> all_browsers = browser_list_->AllRegularBrowsers();
  for (Browser* browser : all_browsers) {
    StartObservingBrowser(browser);
  }
}

TabGroupLocalUpdateService::~TabGroupLocalUpdateService() {}

void TabGroupLocalUpdateService::Shutdown() {
  browser_list_observation_.Reset();
}

void TabGroupLocalUpdateService::OnBrowserAdded(const BrowserList* browser_list,
                                                Browser* browser) {
  StartObservingBrowser(browser);
}

void TabGroupLocalUpdateService::OnBrowserRemoved(
    const BrowserList* browser_list,
    Browser* browser) {
  if (!browser->IsInactive()) {
    StopObservingWebStateList(browser->GetWebStateList());
  }
}

void TabGroupLocalUpdateService::OnBrowserListShutdown(
    BrowserList* browser_list) {
  browser_list_observation_.Reset();
}

void TabGroupLocalUpdateService::WebStateListDidChange(
    WebStateList* web_state_list,
    const WebStateListChange& change,
    const WebStateListStatus& status) {
  if (web_state_list->IsBatchInProgress()) {
    return;
  }
  switch (change.type()) {
    case WebStateListChange::Type::kStatusOnly: {
      const WebStateListChangeStatusOnly& status_only =
          change.As<WebStateListChangeStatusOnly>();
      if (status_only.old_group()) {
        // TODO(crbug.com/329640035): Remove from the old group.
        StopObservingWebState(status_only.web_state());
      }
      if (status_only.new_group()) {
        // TODO(crbug.com/329640035): Insert into the new group.
        StartObservingWebState(status_only.web_state());
      }
    } break;
    case WebStateListChange::Type::kDetach: {
      const WebStateListChangeDetach& detach =
          change.As<WebStateListChangeDetach>();
      if (detach.group()) {
        // TODO(crbug.com/329640035): Remove from the group.
        StopObservingWebState(detach.detached_web_state());
      }
    } break;
    case WebStateListChange::Type::kReplace: {
      const WebStateListChangeReplace& replace =
          change.As<WebStateListChangeReplace>();
      if (web_state_list->GetGroupOfWebStateAt(replace.index())) {
        StopObservingWebState(replace.replaced_web_state());
        StartObservingWebState(replace.inserted_web_state());
        // TODO(crbug.com/329640035): Is it necessary to update the associated
        // tab?
      }
    } break;
    case WebStateListChange::Type::kInsert: {
      const WebStateListChangeInsert& insert =
          change.As<WebStateListChangeInsert>();
      if (insert.group()) {
        // TODO(crbug.com/329640035): Add to the group.
        StartObservingWebState(insert.inserted_web_state());
      }
    } break;
    case WebStateListChange::Type::kGroupCreate:
      // TODO(crbug.com/329640035): Once groups have ID, check if the new group
      // is already saved, if not, save it.
      break;
    case WebStateListChange::Type::kGroupVisualDataUpdate:
      // TODO(crbug.com/329640035): Once groups have ID, update the saved one.
      break;
    case WebStateListChange::Type::kMove:
    case WebStateListChange::Type::kGroupMove:
    case WebStateListChange::Type::kGroupDelete:
      // No need to update the sync model in that case. In case of delete, the
      // caller needs to update it directly.
      break;
  }
}

void TabGroupLocalUpdateService::BatchOperationEnded(
    WebStateList* web_state_list) {
  // TODO(crbug.com/329640035): Re-create the local model.
}

void TabGroupLocalUpdateService::WebStateListDestroyed(
    WebStateList* web_state_list) {
  StopObservingWebStateList(web_state_list);
}

void TabGroupLocalUpdateService::TitleWasSet(web::WebState* web_state) {
  BrowserAndIndex browser_and_index = FindBrowserAndIndex(
      web_state->GetUniqueIdentifier(), browser_list_->AllRegularBrowsers());
  const TabGroup* tab_group =
      browser_and_index.browser->GetWebStateList()->GetGroupOfWebStateAt(
          browser_and_index.tab_index);
  int tab_position =
      browser_and_index.tab_index - tab_group->range().range_begin();
  CHECK(tab_position >= 0);
  CHECK(tab_position < tab_group->range().count());

  tab_group_sync_service_->UpdateTab(
      tab_group->tab_group_id(), web_state->GetUniqueIdentifier().identifier(),
      web_state->GetTitle(), web_state->GetVisibleURL(),
      std::make_optional(tab_position));
}

void TabGroupLocalUpdateService::DidFinishNavigation(
    web::WebState* web_state,
    web::NavigationContext* navigation_context) {
  // TODO(crbug.com/329640035): Update the model with the new URL. The first
  // navigation should be ignored and the other navigations should be checked.
}

void TabGroupLocalUpdateService::WebStateDestroyed(web::WebState* web_state) {
  StopObservingWebState(web_state);
}

void TabGroupLocalUpdateService::StartObservingBrowser(Browser* browser) {
  if (browser->IsInactive()) {
    // The updates of the inactive browser should not be propagated.
    return;
  }

  WebStateList* web_state_list = browser->GetWebStateList();
  web_state_list_observation_.AddObservation(web_state_list);
  for (const TabGroup* group : web_state_list->GetGroups()) {
    for (int index : group->range()) {
      web_state_observation_.AddObservation(
          web_state_list->GetWebStateAt(index));
    }
  }
}

void TabGroupLocalUpdateService::StopObservingWebStateList(
    WebStateList* web_state_list) {
  web_state_list_observation_.RemoveObservation(web_state_list);
}

void TabGroupLocalUpdateService::StartObservingWebState(
    web::WebState* web_state) {
  web_state_observation_.AddObservation(web_state);
}

void TabGroupLocalUpdateService::StopObservingWebState(
    web::WebState* web_state) {
  web_state_observation_.RemoveObservation(web_state);
}

}  // namespace tab_groups
