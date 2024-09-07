// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/tabs/model/ios_chrome_local_session_event_router.h"

#import <stddef.h>

#import "base/check.h"
#import "base/functional/bind.h"
#import "components/history/core/browser/history_service.h"
#import "components/keyed_service/core/service_access_type.h"
#import "components/sync/base/features.h"
#import "components/sync_sessions/sync_sessions_client.h"
#import "components/sync_sessions/synced_tab_delegate.h"
#import "ios/chrome/browser/history/model/history_service_factory.h"
#import "ios/chrome/browser/shared/model/browser/all_web_state_list_observation_registrar.h"
#import "ios/chrome/browser/shared/model/browser/browser_list.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/sync/model/glue/sync_start_util.h"
#import "ios/chrome/browser/tabs/model/ios_chrome_synced_tab_delegate.h"
#import "ios/chrome/browser/tabs/model/tab_parenting_global_observer.h"

namespace {

sync_sessions::SyncedTabDelegate* GetSyncedTabDelegateFromWebState(
    web::WebState* web_state) {
  sync_sessions::SyncedTabDelegate* delegate =
      IOSChromeSyncedTabDelegate::FromWebState(web_state);
  return delegate;
}

}  // namespace

IOSChromeLocalSessionEventRouter::IOSChromeLocalSessionEventRouter(
    BrowserList* browser_list,
    sync_sessions::SyncSessionsClient* sessions_client,
    const syncer::SyncableService::StartSyncFlare& flare)
    : registrar_(std::make_unique<AllWebStateListObservationRegistrar>(
          browser_list,
          std::make_unique<Observer>(this),
          AllWebStateListObservationRegistrar::Mode::REGULAR)),
      sessions_client_(sessions_client),
      flare_(flare),
      tab_parented_subscription_(
          TabParentingGlobalObserver::GetInstance()->RegisterCallback(
              base::BindRepeating(
                  &IOSChromeLocalSessionEventRouter::OnTabParented,
                  base::Unretained(this)))) {
  DCHECK(sessions_client_);
}

IOSChromeLocalSessionEventRouter::~IOSChromeLocalSessionEventRouter() {}

IOSChromeLocalSessionEventRouter::Observer::Observer(
    IOSChromeLocalSessionEventRouter* session_router)
    : router_(session_router) {}

IOSChromeLocalSessionEventRouter::Observer::~Observer() {}

#pragma mark - WebStateListObserver

void IOSChromeLocalSessionEventRouter::Observer::WebStateListDidChange(
    WebStateList* web_state_list,
    const WebStateListChange& change,
    const WebStateListStatus& status) {
  switch (change.type()) {
    case WebStateListChange::Type::kStatusOnly:
      // Do nothing when a WebState is selected and its status is updated.
      break;
    case WebStateListChange::Type::kDetach: {
      const WebStateListChangeDetach& detach_change =
          change.As<WebStateListChangeDetach>();
      web::WebState* detached_web_state = detach_change.detached_web_state();
      router_->OnWebStateChange(detached_web_state);
      detached_web_state->RemoveObserver(this);
      break;
    }
    case WebStateListChange::Type::kMove:
      // Do nothing when a WebState is moved.
      break;
    case WebStateListChange::Type::kReplace: {
      const WebStateListChangeReplace& replace_change =
          change.As<WebStateListChangeReplace>();
      web::WebState* replaced_web_state = replace_change.replaced_web_state();
      router_->OnWebStateChange(replaced_web_state);
      replaced_web_state->RemoveObserver(this);
      replace_change.inserted_web_state()->AddObserver(this);
      break;
    }
    case WebStateListChange::Type::kInsert: {
      const WebStateListChangeInsert& insert_change =
          change.As<WebStateListChangeInsert>();
      insert_change.inserted_web_state()->AddObserver(this);
      break;
    }
    case WebStateListChange::Type::kGroupCreate:
      // TODO(crbug.com/329640035): Notify the router about the group creation.
      break;
    case WebStateListChange::Type::kGroupVisualDataUpdate:
      // TODO(crbug.com/329640035): Notify the router about the group's visual
      // data update.
      break;
    case WebStateListChange::Type::kGroupMove:
      // Do nothing when a tab group is moved.
      break;
    case WebStateListChange::Type::kGroupDelete:
      // TODO(crbug.com/329640035): Notify the router about the group deletion.
      break;
  }
}

#pragma mark - WebStateObserver

void IOSChromeLocalSessionEventRouter::Observer::TitleWasSet(
    web::WebState* web_state) {
  router_->OnWebStateChange(web_state);
}

void IOSChromeLocalSessionEventRouter::Observer::DidFinishNavigation(
    web::WebState* web_state,
    web::NavigationContext* navigation_context) {
  router_->OnWebStateChange(web_state);
}

void IOSChromeLocalSessionEventRouter::Observer::PageLoaded(
    web::WebState* web_state,
    web::PageLoadCompletionStatus load_completion_status) {
  router_->OnWebStateChange(web_state);
}

void IOSChromeLocalSessionEventRouter::Observer::WasShown(
    web::WebState* web_state) {
  router_->OnWebStateChange(web_state);
}

void IOSChromeLocalSessionEventRouter::Observer::DidChangeBackForwardState(
    web::WebState* web_state) {
  router_->OnWebStateChange(web_state);
}

void IOSChromeLocalSessionEventRouter::Observer::WebStateDestroyed(
    web::WebState* web_state) {
  router_->OnWebStateChange(web_state);
  web_state->RemoveObserver(this);
}

void IOSChromeLocalSessionEventRouter::OnTabParented(web::WebState* web_state) {
  OnWebStateChange(web_state);
}

void IOSChromeLocalSessionEventRouter::Observer::WillBeginBatchOperation(
    WebStateList* web_state_list) {
  router_->OnSessionEventStarting();
}

void IOSChromeLocalSessionEventRouter::Observer::BatchOperationEnded(
    WebStateList* web_state_list) {
  router_->OnSessionEventEnded();
}

void IOSChromeLocalSessionEventRouter::OnSessionEventStarting() {
  batch_in_progress_++;
}

void IOSChromeLocalSessionEventRouter::OnSessionEventEnded() {
  DCHECK(batch_in_progress_ > 0);
  batch_in_progress_--;
  if (batch_in_progress_) {
    return;
  }
  // Batch operations are only used for restoration, close all tabs or undo
  // close all tabs. In any case, a full sync is necessary after this.
  if (handler_) {
    handler_->OnSessionRestoreComplete();
  }
  if (!flare_.is_null()) {
    std::move(flare_).Run(syncer::SESSIONS);
  }
}

void IOSChromeLocalSessionEventRouter::OnWebStateChange(
    web::WebState* web_state) {
  if (batch_in_progress_) {
    return;
  }
  sync_sessions::SyncedTabDelegate* tab =
      GetSyncedTabDelegateFromWebState(web_state);
  if (!tab) {
    return;
  }
  // Some WebState event happen during the navigation restoration. Ignore
  // them as the tab is still considered as placeholder by this point as
  // the session cannot be forwarded to sync yet.
  if (tab->IsPlaceholderTab()) {
    return;
  }
  if (handler_) {
    handler_->OnLocalTabModified(tab);
  }
  if (!tab->ShouldSync(sessions_client_)) {
    return;
  }

  if (!flare_.is_null()) {
    std::move(flare_).Run(syncer::SESSIONS);
  }
}

void IOSChromeLocalSessionEventRouter::StartRoutingTo(
    sync_sessions::LocalSessionEventHandler* handler) {
  DCHECK(!handler_);
  handler_ = handler;
}

void IOSChromeLocalSessionEventRouter::Stop() {
  handler_ = nullptr;
}
