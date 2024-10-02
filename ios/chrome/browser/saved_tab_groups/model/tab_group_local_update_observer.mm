// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/saved_tab_groups/model/tab_group_local_update_observer.h"

#import <memory>
#import <optional>

#import "base/check.h"
#import "base/strings/sys_string_conversions.h"
#import "components/saved_tab_groups/public/tab_group_sync_service.h"
#import "components/saved_tab_groups/public/types.h"
#import "components/saved_tab_groups/public/utils.h"
#import "ios/chrome/browser/saved_tab_groups/model/ios_tab_group_sync_util.h"
#import "ios/chrome/browser/sessions/model/session_restoration_service.h"
#import "ios/chrome/browser/sessions/model/session_restoration_service_factory.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser/browser_list.h"
#import "ios/chrome/browser/shared/model/browser/browser_list_factory.h"
#import "ios/chrome/browser/shared/model/web_state_list/browser_util.h"
#import "ios/chrome/browser/shared/model/web_state_list/tab_group.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list_observer.h"
#import "ios/web/public/web_state.h"
#import "ios/web/public/web_state_observer.h"

using tab_groups::utils::GetLocalTabInfo;
using tab_groups::utils::LocalTabInfo;

namespace tab_groups {

TabGroupLocalUpdateObserver::TabGroupLocalUpdateObserver(
    BrowserList* browser_list,
    TabGroupSyncService* sync_service)
    : sync_service_(sync_service),
      browser_list_(browser_list) {
  browser_list_observation_.Observe(browser_list);
  CHECK(browser_list_->BrowsersOfType(BrowserList::BrowserType::kRegular)
            .empty());
}

TabGroupLocalUpdateObserver::~TabGroupLocalUpdateObserver() = default;

#pragma mark - Public

void TabGroupLocalUpdateObserver::IgnoreNavigationForWebState(
    web::WebState* web_state) {
  ignored_web_state_identifiers_.insert(web_state->GetUniqueIdentifier());
}

#pragma mark - BrowserListObserver

void TabGroupLocalUpdateObserver::OnBrowserAdded(
    const BrowserList* browser_list,
    Browser* browser) {
  if (browser->type() != Browser::Type::kRegular) {
    return;
  }
  StartObservingBrowser(browser);
}

void TabGroupLocalUpdateObserver::OnBrowserRemoved(
    const BrowserList* browser_list,
    Browser* browser) {
  if (browser->type() != Browser::Type::kRegular) {
    return;
  }
  StopObservingWebStateList(browser->GetWebStateList());
}

void TabGroupLocalUpdateObserver::OnBrowserListShutdown(
    BrowserList* browser_list) {
  browser_list_observation_.Reset();
  session_restoration_service_observation_.Reset();
}

#pragma mark - WebStateListObserver

void TabGroupLocalUpdateObserver::WebStateListDidChange(
    WebStateList* web_state_list,
    const WebStateListChange& change,
    const WebStateListStatus& status) {
  switch (change.type()) {
    case WebStateListChange::Type::kStatusOnly: {
      const WebStateListChangeStatusOnly& status_only =
          change.As<WebStateListChangeStatusOnly>();
      const TabGroup* old_group = status_only.old_group();
      const TabGroup* new_group = status_only.new_group();

      if (old_group != new_group) {
        // There is a change of group.
        if (old_group) {
          // Remove the tab from the synced `old_group`.
          RemoveLocalWebStateFromSyncedGroup(status_only.web_state(),
                                             old_group);
        }
        if (new_group) {
          // Insert the tab into the synced `new_group`.
          AddLocalWebStateToSyncedGroup(status_only.web_state(),
                                        web_state_list);
        }
      }
    } break;
    case WebStateListChange::Type::kDetach: {
      const WebStateListChangeDetach& detach =
          change.As<WebStateListChangeDetach>();
      const TabGroup* detach_group = detach.group();
      if (detach_group) {
        // Remove the tab from the `detach_group`.
        RemoveLocalWebStateFromSyncedGroup(detach.detached_web_state(),
                                           detach_group);
      }
    } break;
    case WebStateListChange::Type::kReplace: {
      const WebStateListChangeReplace& replace =
          change.As<WebStateListChangeReplace>();
      const TabGroup* group =
          web_state_list->GetGroupOfWebStateAt(replace.index());
      if (group) {
        AddLocalWebStateToSyncedGroup(replace.inserted_web_state(),
                                      web_state_list);
        RemoveLocalWebStateFromSyncedGroup(replace.replaced_web_state(), group);
      }
    } break;
    case WebStateListChange::Type::kInsert: {
      const WebStateListChangeInsert& insert =
          change.As<WebStateListChangeInsert>();
      if (insert.group()) {
        // Insert the tab into the synced `new_group`.
        AddLocalWebStateToSyncedGroup(insert.inserted_web_state(),
                                      web_state_list);
      }
    } break;
    case WebStateListChange::Type::kGroupCreate: {
      const WebStateListChangeGroupCreate& group_create =
          change.As<WebStateListChangeGroupCreate>();
      CreateSyncedGroup(web_state_list, group_create.created_group());
      break;
    }
    case WebStateListChange::Type::kGroupVisualDataUpdate: {
      const WebStateListChangeGroupVisualDataUpdate& visual_data =
          change.As<WebStateListChangeGroupVisualDataUpdate>();
      UpdateVisualDataSyncedGroup(visual_data.updated_group());
      break;
    }
    case WebStateListChange::Type::kMove: {
      const WebStateListChangeMove& move = change.As<WebStateListChangeMove>();
      const TabGroup* old_group = move.old_group();
      const TabGroup* new_group = move.new_group();
      if (old_group != new_group) {
        if (old_group) {
          // Remove the tab from the synced `old_group`.
          RemoveLocalWebStateFromSyncedGroup(move.moved_web_state(), old_group);
        }
        if (new_group) {
          // Insert the tab into the synced `new_group`.
          AddLocalWebStateToSyncedGroup(move.moved_web_state(), web_state_list);
        }
      } else if (old_group) {
        // Move the tab in the group.
        MoveLocalWebStateToSyncedGroup(move.moved_web_state(), web_state_list);
      }
      break;
    }
    case WebStateListChange::Type::kGroupDelete: {
      const WebStateListChangeGroupDelete& delete_group =
          change.As<WebStateListChangeGroupDelete>();
      DeleteSyncedGroup(delete_group.deleted_group());
      break;
    }
    case WebStateListChange::Type::kGroupMove:
      break;
  }

  web::WebState* web_state = status.new_active_web_state;
  if (status.active_web_state_change() && web_state) {
    const TabGroup* tab_group =
        web_state_list->GetGroupOfWebStateAt(web_state_list->active_index());
    if (tab_group) {
      sync_service_->OnTabSelected(
          tab_group->tab_group_id(),
          web_state->GetUniqueIdentifier().identifier());
    }
  }
}

void TabGroupLocalUpdateObserver::WebStateListDestroyed(
    WebStateList* web_state_list) {
  StopObservingWebStateList(web_state_list);
}

#pragma mark - WebStateObserver

void TabGroupLocalUpdateObserver::TitleWasSet(web::WebState* web_state) {
  if (sync_update_paused_) {
    return;
  }

  // Updates before the first navigation should be ignored.
  web::WebStateID identifier = web_state->GetUniqueIdentifier();
  if (ignored_web_state_identifiers_.contains(identifier)) {
    return;
  }

  UpdateLocalWebStateInSyncedGroup(web_state);
}

void TabGroupLocalUpdateObserver::DidFinishNavigation(
    web::WebState* web_state,
    web::NavigationContext* navigation_context) {
  if (sync_update_paused_) {
    return;
  }

  // The first navigation after a sync update should be ignored.
  web::WebStateID identifier = web_state->GetUniqueIdentifier();
  if (ignored_web_state_identifiers_.contains(identifier)) {
    ignored_web_state_identifiers_.erase(identifier);
    return;
  }

  if (!utils::IsSaveableNavigation(navigation_context)) {
    return;
  }

  UpdateLocalWebStateInSyncedGroup(web_state);
}

void TabGroupLocalUpdateObserver::WebStateDestroyed(web::WebState* web_state) {
  StopObservingWebState(web_state);
}

#pragma mark - SessionRestorationObserver

void TabGroupLocalUpdateObserver::WillStartSessionRestoration(
    Browser* browser) {
  SetSyncUpdatePaused(true);
}

void TabGroupLocalUpdateObserver::SessionRestorationFinished(
    Browser* browser,
    const std::vector<web::WebState*>& restored_web_states) {
  SetSyncUpdatePaused(false);
}

#pragma mark - Private

void TabGroupLocalUpdateObserver::SetSyncUpdatePaused(bool paused) {
  sync_update_paused_ += paused ? 1 : -1;
  CHECK_GE(sync_update_paused_, 0);
}

void TabGroupLocalUpdateObserver::StartObservingBrowser(Browser* browser) {
  // Observer should be set once the session restoration service has started.
  // TODO(crbug.com/350885825): Directly inject the SessionRestorationService to
  // this class when it's no longer necessary for MigrateSessionStorageFormat to
  // instantiate it.
  if (!session_restoration_service_observation_.IsObserving()) {
    session_restoration_service_observation_.Observe(
        SessionRestorationServiceFactory::GetForProfile(browser->GetProfile()));
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

void TabGroupLocalUpdateObserver::StopObservingWebStateList(
    WebStateList* web_state_list) {
  web_state_list_observation_.RemoveObservation(web_state_list);
}

void TabGroupLocalUpdateObserver::StartObservingWebState(
    web::WebState* web_state) {
  web_state_observation_.AddObservation(web_state);
}

void TabGroupLocalUpdateObserver::StopObservingWebState(
    web::WebState* web_state) {
  // Try to remove the `web_state` identifier from
  // `ignored_web_state_identifiers_`.
  ignored_web_state_identifiers_.erase(web_state->GetUniqueIdentifier());
  web_state_observation_.RemoveObservation(web_state);
}

void TabGroupLocalUpdateObserver::UpdateLocalWebStateInSyncedGroup(
    web::WebState* web_state) {
  CHECK(!sync_update_paused_);

  LocalTabInfo tab_info =
      utils::GetLocalTabInfo(browser_list_, web_state->GetUniqueIdentifier());

  GURL url = web_state->GetVisibleURL();
  std::u16string title = web_state->GetTitle();
  if (!IsURLValidForSavedTabGroups(url)) {
    url = GetDefaultUrlAndTitle().first;
    title = GetDefaultUrlAndTitle().second;
  }

  SavedTabGroupTabBuilder tab_builder;
  tab_builder.SetURL(url);
  tab_builder.SetTitle(title);
  sync_service_->UpdateTab(tab_info.tab_group->tab_group_id(),
                           web_state->GetUniqueIdentifier().identifier(),
                           std::move(tab_builder));
}

void TabGroupLocalUpdateObserver::AddLocalWebStateToSyncedGroup(
    web::WebState* web_state,
    WebStateList* web_state_list) {
  StartObservingWebState(web_state);
  if (sync_update_paused_) {
    // Early return after starting observing new tabs.
    return;
  }

  LocalTabInfo tab_info =
      web_state_list ? utils::GetLocalTabInfo(web_state_list,
                                              web_state->GetUniqueIdentifier())
                     : utils::GetLocalTabInfo(browser_list_,
                                              web_state->GetUniqueIdentifier());
  utils::GetLocalTabInfo(browser_list_, web_state->GetUniqueIdentifier());
  sync_service_->AddTab(tab_info.tab_group->tab_group_id(),
                        web_state->GetUniqueIdentifier().identifier(),
                        web_state->GetTitle(), web_state->GetVisibleURL(),
                        std::make_optional(tab_info.index_in_group));
}

void TabGroupLocalUpdateObserver::MoveLocalWebStateToSyncedGroup(
    web::WebState* web_state,
    WebStateList* web_state_list) {
  if (sync_update_paused_) {
    return;
  }

  LocalTabInfo tab_info =
      utils::GetLocalTabInfo(web_state_list, web_state->GetUniqueIdentifier());
  sync_service_->MoveTab(tab_info.tab_group->tab_group_id(),
                         web_state->GetUniqueIdentifier().identifier(),
                         tab_info.index_in_group);
}

void TabGroupLocalUpdateObserver::RemoveLocalWebStateFromSyncedGroup(
    web::WebState* web_state,
    const TabGroup* tab_group) {
  StopObservingWebState(web_state);
  if (sync_update_paused_) {
    // Early return after stoping observing new tabs.
    return;
  }

  if (!sync_service_->GetGroup(tab_group->tab_group_id())) {
    // The group has been closed locally.
    return;
  }

  sync_service_->RemoveTab(tab_group->tab_group_id(),
                           web_state->GetUniqueIdentifier().identifier());
}

void TabGroupLocalUpdateObserver::CreateSyncedGroup(
    WebStateList* web_state_list,
    const TabGroup* tab_group) {
  LocalTabGroupID local_id = tab_group->tab_group_id();
  if (sync_service_->GetGroup(local_id)) {
    // The group already exists.
    return;
  }

  // Generate and id for the synced tab group.
  base::Uuid saved_tab_group_id = base::Uuid::GenerateRandomV4();

  // Create a vector of `saved_tabs` based on local tabs.
  std::vector<SavedTabGroupTab> saved_tabs;
  for (int index = 0; index < tab_group->range().count(); ++index) {
    int web_state_index = tab_group->range().range_begin() + index;
    web::WebState* web_state = web_state_list->GetWebStateAt(web_state_index);

    // Start observing the `web_state`.
    StartObservingWebState(web_state);

    SavedTabGroupTab saved_tab(web_state->GetVisibleURL(),
                               web_state->GetTitle(), saved_tab_group_id,
                               std::make_optional(index), std::nullopt,
                               web_state->GetUniqueIdentifier().identifier());
    saved_tabs.push_back(saved_tab);
  }

  if (sync_update_paused_) {
    // Early return after starting observing new tabs.
    return;
  }

  SavedTabGroup saved_group(base::SysNSStringToUTF16(tab_group->GetRawTitle()),
                            tab_group->visual_data().color(), saved_tabs,
                            std::nullopt, saved_tab_group_id,
                            tab_group->tab_group_id());
  sync_service_->AddGroup(saved_group);
}

void TabGroupLocalUpdateObserver::UpdateVisualDataSyncedGroup(
    const TabGroup* tab_group) {
  if (sync_update_paused_) {
    return;
  }

  sync_service_->UpdateVisualData(tab_group->tab_group_id(),
                                  &tab_group->visual_data());
}

void TabGroupLocalUpdateObserver::DeleteSyncedGroup(const TabGroup* tab_group) {
  if (sync_update_paused_ ||
      !sync_service_->GetGroup(tab_group->tab_group_id())) {
    // The group has been closed locally.
    return;
  }
  sync_service_->RemoveGroup(tab_group->tab_group_id());
}

}  // namespace tab_groups
