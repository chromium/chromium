// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/sync/sessions/ios_chrome_local_session_event_router.h"

#include <stddef.h>

#include "base/bind.h"
#include "base/logging.h"
#include "components/history/core/browser/history_service.h"
#include "components/keyed_service/core/service_access_type.h"
#include "components/sync_sessions/sync_sessions_client.h"
#include "components/sync_sessions/synced_tab_delegate.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/history/history_service_factory.h"
#include "ios/chrome/browser/sync/glue/sync_start_util.h"
#include "ios/chrome/browser/sync/ios_chrome_synced_tab_delegate.h"
#import "ios/chrome/browser/tabs/tab_model.h"
#import "ios/chrome/browser/tabs/tab_model_list.h"
#include "ios/chrome/browser/tabs/tab_parenting_global_observer.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

sync_sessions::SyncedTabDelegate* GetSyncedTabDelegateFromWebState(
    web::WebState* web_state) {
  sync_sessions::SyncedTabDelegate* delegate =
      IOSChromeSyncedTabDelegate::FromWebState(web_state);
  return delegate;
}

}  // namespace

IOSChromeLocalSessionEventRouter::IOSChromeLocalSessionEventRouter(
    ios::ChromeBrowserState* browser_state,
    sync_sessions::SyncSessionsClient* sessions_client,
    const syncer::SyncableService::StartSyncFlare& flare)
    : handler_(NULL),
      browser_state_(browser_state),
      sessions_client_(sessions_client),
      flare_(flare) {
  tab_parented_subscription_ =
      TabParentingGlobalObserver::GetInstance()->RegisterCallback(
          base::Bind(&IOSChromeLocalSessionEventRouter::OnTabParented,
                     base::Unretained(this)));

  history::HistoryService* history_service =
      ios::HistoryServiceFactory::GetForBrowserState(
          browser_state, ServiceAccessType::EXPLICIT_ACCESS);
  if (history_service) {
    favicon_changed_subscription_ = history_service->AddFaviconsChangedCallback(
        base::Bind(&IOSChromeLocalSessionEventRouter::OnFaviconsChanged,
                   base::Unretained(this)));
  }

  for (TabModel* tab_model in TabModelList::GetTabModelsForChromeBrowserState(
           browser_state_)) {
    StartObservingWebStateList(tab_model.webStateList);
  }

  TabModelList::AddObserver(this);
}

IOSChromeLocalSessionEventRouter::~IOSChromeLocalSessionEventRouter() {
  for (TabModel* tab_model in TabModelList::GetTabModelsForChromeBrowserState(
           browser_state_)) {
    StopObservingWebStateList(tab_model.webStateList);
  }
  TabModelList::RemoveObserver(this);
}

void IOSChromeLocalSessionEventRouter::TabModelRegisteredWithBrowserState(
    TabModel* tab_model,
    ios::ChromeBrowserState* browser_state) {
  StartObservingWebStateList(tab_model.webStateList);
}

void IOSChromeLocalSessionEventRouter::TabModelUnregisteredFromBrowserState(
    TabModel* tab_model,
    ios::ChromeBrowserState* browser_state) {
  StopObservingWebStateList(tab_model.webStateList);
}

void IOSChromeLocalSessionEventRouter::WebStateInsertedAt(
    WebStateList* web_state_list,
    web::WebState* web_state,
    int index,
    bool activating) {
  web_state->AddObserver(this);
}

void IOSChromeLocalSessionEventRouter::WebStateReplacedAt(
    WebStateList* web_state_list,
    web::WebState* old_web_state,
    web::WebState* new_web_state,
    int index) {
  old_web_state->RemoveObserver(this);

  if (new_web_state)
    new_web_state->AddObserver(this);
}

void IOSChromeLocalSessionEventRouter::WebStateDetachedAt(
    WebStateList* web_state_list,
    web::WebState* web_state,
    int index) {
  web_state->RemoveObserver(this);
}

void IOSChromeLocalSessionEventRouter::NavigationItemsPruned(
    web::WebState* web_state,
    size_t pruned_item_count) {
  OnWebStateChange(web_state);
}

void IOSChromeLocalSessionEventRouter::TitleWasSet(web::WebState* web_state) {
  OnWebStateChange(web_state);
}

void IOSChromeLocalSessionEventRouter::DidFinishNavigation(
    web::WebState* web_state,
    web::NavigationContext* navigation_context) {
  OnWebStateChange(web_state);
}

void IOSChromeLocalSessionEventRouter::PageLoaded(
    web::WebState* web_state,
    web::PageLoadCompletionStatus load_completion_status) {
  OnWebStateChange(web_state);
}

void IOSChromeLocalSessionEventRouter::DidChangeBackForwardState(
    web::WebState* web_state) {
  OnWebStateChange(web_state);
}

void IOSChromeLocalSessionEventRouter::WebStateDestroyed(
    web::WebState* web_state) {
  OnWebStateChange(web_state);
}

void IOSChromeLocalSessionEventRouter::StartObservingWebStateList(
    WebStateList* web_state_list) {
  web_state_list->AddObserver(this);
  for (int index = 0; index < web_state_list->count(); ++index) {
    web::WebState* web_state = web_state_list->GetWebStateAt(index);
    web_state->AddObserver(this);
  }
}

void IOSChromeLocalSessionEventRouter::StopObservingWebStateList(
    WebStateList* web_state_list) {
  for (int index = 0; index < web_state_list->count(); ++index) {
    web::WebState* web_state = web_state_list->GetWebStateAt(index);
    web_state->RemoveObserver(this);
  }
  web_state_list->RemoveObserver(this);
}

void IOSChromeLocalSessionEventRouter::OnTabParented(web::WebState* web_state) {
  OnWebStateChange(web_state);
}

void IOSChromeLocalSessionEventRouter::OnWebStateChange(
    web::WebState* web_state) {
  sync_sessions::SyncedTabDelegate* tab =
      GetSyncedTabDelegateFromWebState(web_state);
  if (!tab)
    return;
  if (handler_)
    handler_->OnLocalTabModified(tab);
  if (!tab->ShouldSync(sessions_client_))
    return;

  if (!flare_.is_null()) {
    flare_.Run(syncer::SESSIONS);
    flare_.Reset();
  }
}

void IOSChromeLocalSessionEventRouter::OnFaviconsChanged(
    const std::set<GURL>& page_urls,
    const GURL& icon_url) {
  if (handler_ && !page_urls.empty())
    handler_->OnFaviconsChanged(page_urls, icon_url);
}

void IOSChromeLocalSessionEventRouter::StartRoutingTo(
    sync_sessions::LocalSessionEventHandler* handler) {
  DCHECK(!handler_);
  handler_ = handler;
}

void IOSChromeLocalSessionEventRouter::Stop() {
  handler_ = nullptr;
}
