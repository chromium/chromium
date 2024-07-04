// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/saved_tab_groups/model/tab_group_local_update_observer.h"

#import <memory>
#import <optional>

#import "base/check.h"
#import "base/strings/sys_string_conversions.h"
#import "components/saved_tab_groups/tab_group_sync_service.h"
#import "components/saved_tab_groups/types.h"
#import "ios/chrome/browser/sessions/session_restoration_service.h"
#import "ios/chrome/browser/sessions/session_restoration_service_factory.h"
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

#pragma mark - ScopedPauseSyncOperation

TabGroupLocalUpdateObserver::ScopedPauseSyncOperation::ScopedPauseSyncOperation(
    TabGroupLocalUpdateObserver* observer)
    : observer_(observer) {
  observer_->SetSyncUpdatePaused(true);
}

TabGroupLocalUpdateObserver::ScopedPauseSyncOperation::
    ~ScopedPauseSyncOperation() {
  if (observer_) {
    observer_->SetSyncUpdatePaused(false);
  }
}

#pragma mark - TabGroupLocalUpdateObserver

TabGroupLocalUpdateObserver::TabGroupLocalUpdateObserver(
    BrowserList* browser_list,
    TabGroupSyncService* sync_service)
    : sync_service_(sync_service),
      browser_list_(browser_list) {
  browser_list_observation_.Observe(browser_list);
  CHECK(browser_list_->AllRegularBrowsers().empty());
}

TabGroupLocalUpdateObserver::~TabGroupLocalUpdateObserver() = default;

#pragma mark - Public

void TabGroupLocalUpdateObserver::IgnoreNavigationForWebState(
    web::WebState* web_state) {
  ignored_web_state_identifiers_.insert(web_state->GetUniqueIdentifier());
}

TabGroupLocalUpdateObserver::ScopedPauseSyncOperation
TabGroupLocalUpdateObserver::PauseSyncUpdate() {
  return ScopedPauseSyncOperation(this);
}

#pragma mark - BrowserListObserver

void TabGroupLocalUpdateObserver::OnBrowserAdded(
    const BrowserList* browser_list,
    Browser* browser) {
  StartObservingBrowser(browser);
}

void TabGroupLocalUpdateObserver::OnBrowserRemoved(
    const BrowserList* browser_list,
    Browser* browser) {
  if (!browser->IsInactive()) {
    StopObservingWebStateList(browser->GetWebStateList());
  }
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
          // TODO(crbug.com/329640035): Remove from the old group.
          StopObservingWebState(status_only.web_state());
        }
        if (new_group) {
          // TODO(crbug.com/329640035): Insert into the new group.
          StartObservingWebState(status_only.web_state());
        }
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
    case WebStateListChange::Type::kGroupCreate: {
      const WebStateListChangeGroupCreate& groupCreateChange =
          change.As<WebStateListChangeGroupCreate>();
      CreateSyncedGroup(web_state_list, groupCreateChange.created_group());
      break;
    }
    case WebStateListChange::Type::kGroupVisualDataUpdate:
      // TODO(crbug.com/329640035): Once groups have ID, update the saved one.
      break;
    case WebStateListChange::Type::kMove: {
      const WebStateListChangeMove& move = change.As<WebStateListChangeMove>();
      const TabGroup* old_group = move.old_group();
      const TabGroup* new_group = move.new_group();
      if (old_group || new_group) {
        if (old_group && old_group != new_group) {
          // TODO(crbug.com/329640035): Implement.
          StopObservingWebState(move.moved_web_state());
        }
        if (new_group && old_group != new_group) {
          // TODO(crbug.com/329640035): Implement.
          StartObservingWebState(move.moved_web_state());
        }
        if (old_group == new_group) {
          // TODO(crbug.com/329640035): Implement.
        }
      }
      break;
    }
    case WebStateListChange::Type::kGroupMove:
    case WebStateListChange::Type::kGroupDelete:
      // No need to update the sync model in that case. In case of delete, the
      // caller needs to update it directly.
      break;
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

  BrowserAndIndex browser_and_index = FindBrowserAndIndex(
      web_state->GetUniqueIdentifier(), browser_list_->AllRegularBrowsers());
  const TabGroup* tab_group =
      browser_and_index.browser->GetWebStateList()->GetGroupOfWebStateAt(
          browser_and_index.tab_index);
  int tab_position =
      browser_and_index.tab_index - tab_group->range().range_begin();
  CHECK(tab_position >= 0);
  CHECK(tab_position < tab_group->range().count());

  sync_service_->UpdateTab(
      tab_group->tab_group_id(), web_state->GetUniqueIdentifier().identifier(),
      web_state->GetTitle(), web_state->GetVisibleURL(),
      std::make_optional(tab_position));
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

  // TODO(crbug.com/329640035): Update the model with the new URL.
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
  if (browser->IsInactive()) {
    // The updates of the inactive browser should not be propagated.
    return;
  }

  // Observer should be set once the session restoration service has started.
  // TODO(crbug.com/350885825): Directly inject the SessionRestorationService to
  // this class when it's no longer necessary for MigrateSessionStorageFormat to
  // instantiate it.
  if (!session_restoration_service_observation_.IsObserving()) {
    session_restoration_service_observation_.Observe(
        SessionRestorationServiceFactory::GetForBrowserState(
            browser->GetBrowserState()));
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

}  // namespace tab_groups
