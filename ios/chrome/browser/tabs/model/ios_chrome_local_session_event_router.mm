// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/tabs/model/ios_chrome_local_session_event_router.h"

#import <stddef.h>

#import "base/check.h"
#import "base/functional/bind.h"
#import "base/scoped_multi_source_observation.h"
#import "base/scoped_observation.h"
#import "components/history/core/browser/history_service.h"
#import "components/keyed_service/core/service_access_type.h"
#import "components/sync/base/features.h"
#import "components/sync_sessions/sync_sessions_client.h"
#import "components/sync_sessions/synced_tab_delegate.h"
#import "ios/chrome/browser/history/model/history_service_factory.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser/browser_list.h"
#import "ios/chrome/browser/shared/model/browser/browser_list_observer.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list_observer.h"
#import "ios/chrome/browser/sync/model/glue/sync_start_util.h"
#import "ios/chrome/browser/tabs/model/ios_chrome_synced_tab_delegate.h"
#import "ios/web/public/web_state_observer.h"

namespace {

sync_sessions::SyncedTabDelegate* GetSyncedTabDelegateFromWebState(
    web::WebState* web_state) {
  sync_sessions::SyncedTabDelegate* delegate =
      IOSChromeSyncedTabDelegate::FromWebState(web_state);
  return delegate;
}

}  // namespace

#pragma mark - IOSChromeLocalSessionEventRouter::Observer

class IOSChromeLocalSessionEventRouter::Observer
    : public BrowserListObserver,
      public WebStateListObserver,
      public web::WebStateObserver {
 public:
  explicit Observer(IOSChromeLocalSessionEventRouter* session_router);
  ~Observer() override = default;

  void Start(BrowserList* browser_list);

 private:
  // BrowserListObserver:
  void OnBrowserAdded(const BrowserList* browser_list,
                      Browser* browser) override;
  void OnBrowserRemoved(const BrowserList* browser_list,
                        Browser* browser) override;
  void OnBrowserListShutdown(BrowserList* browser_list) override;

  // WebStateListObserver:
  void WebStateListDidChange(WebStateList* web_state_list,
                             const WebStateListChange& change,
                             const WebStateListStatus& status) override;
  void WillBeginBatchOperation(WebStateList* web_state_list) override;
  void BatchOperationEnded(WebStateList* web_state_list) override;

  // web::WebStateObserver:
  void TitleWasSet(web::WebState* web_state) override;
  void DidFinishNavigation(web::WebState* web_state,
                           web::NavigationContext* navigation_context) override;
  void PageLoaded(
      web::WebState* web_state,
      web::PageLoadCompletionStatus load_completion_status) override;
  void WasShown(web::WebState* web_state) override;
  void DidChangeBackForwardState(web::WebState* web_state) override;
  void WebStateDestroyed(web::WebState* web_state) override;

  raw_ptr<IOSChromeLocalSessionEventRouter> router_;

  // Scoped observations.
  base::ScopedObservation<BrowserList, BrowserListObserver>
      browser_list_observation_{this};
  base::ScopedMultiSourceObservation<WebStateList, WebStateListObserver>
      web_state_list_observations_{this};
  base::ScopedMultiSourceObservation<web::WebState, web::WebStateObserver>
      web_state_observations_{this};
};

IOSChromeLocalSessionEventRouter::Observer::Observer(
    IOSChromeLocalSessionEventRouter* session_router)
    : router_(session_router) {}

void IOSChromeLocalSessionEventRouter::Observer::Start(
    BrowserList* browser_list) {
  browser_list_observation_.Observe(browser_list);

  // The WebStateList may not be empty when the Observer is created, so
  // start observing any pre-existing Browsers.
  for (Browser* browser :
       browser_list->BrowsersOfType(BrowserList::BrowserType::kRegular)) {
    OnBrowserAdded(browser_list, browser);
  }
}

#pragma mark - IOSChromeLocalSessionEventRouter::Observer (BrowserListObserver)

void IOSChromeLocalSessionEventRouter::Observer::OnBrowserAdded(
    const BrowserList* browser_list,
    Browser* browser) {
  if (browser->type() != Browser::Type::kRegular) {
    return;
  }

  WebStateList* web_state_list = browser->GetWebStateList();
  web_state_list_observations_.AddObservation(web_state_list);

  // In the unlikely event that the WebStateList is not empty, observe
  // all existing WebStates (pretend this is a batch operation).
  if (const int count = web_state_list->count(); count != 0) {
    router_->OnSessionEventStarting();
    for (int index = 0; index < count; ++index) {
      web::WebState* web_state = web_state_list->GetWebStateAt(index);
      web_state_observations_.AddObservation(web_state);
    }
    router_->OnSessionEventEnded();
  }
}

void IOSChromeLocalSessionEventRouter::Observer::OnBrowserRemoved(
    const BrowserList* browser_list,
    Browser* browser) {
  if (browser->type() != Browser::Type::kRegular) {
    return;
  }

  WebStateList* web_state_list = browser->GetWebStateList();
  web_state_list_observations_.RemoveObservation(web_state_list);

  // The Browser are closed before the WebStateList itself, so it is
  // possible for the list to be non-empty. Ensure that we stop the
  // observation of the WebState if this is the case.
  if (const int count = web_state_list->count(); count != 0) {
    router_->OnSessionEventStarting();
    for (int index = 0; index < count; ++index) {
      web::WebState* web_state = web_state_list->GetWebStateAt(index);
      web_state_observations_.RemoveObservation(web_state);
    }
    router_->OnSessionEventEnded();
  }
}

void IOSChromeLocalSessionEventRouter::Observer::OnBrowserListShutdown(
    BrowserList* browser_list) {
  browser_list_observation_.Reset();
  web_state_list_observations_.RemoveAllObservations();
  web_state_observations_.RemoveAllObservations();
}

#pragma mark - IOSChromeLocalSessionEventRouter::Observer (WebStateListObserver)

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
      web_state_observations_.RemoveObservation(detached_web_state);
      if (detach_change.is_closing()) {
        router_->OnWebStateClosed();
      } else {
        router_->OnWebStateChange(detached_web_state);
      }
      break;
    }
    case WebStateListChange::Type::kMove:
      // Do nothing when a WebState is moved.
      break;
    case WebStateListChange::Type::kReplace: {
      const WebStateListChangeReplace& replace_change =
          change.As<WebStateListChangeReplace>();
      web::WebState* replaced_web_state = replace_change.replaced_web_state();
      web_state_observations_.RemoveObservation(replaced_web_state);
      router_->OnWebStateChange(replaced_web_state);

      web::WebState* inserted_web_state = replace_change.inserted_web_state();
      web_state_observations_.AddObservation(inserted_web_state);
      router_->OnWebStateChange(inserted_web_state);
      break;
    }
    case WebStateListChange::Type::kInsert: {
      const WebStateListChangeInsert& insert_change =
          change.As<WebStateListChangeInsert>();
      web::WebState* inserted_web_state = insert_change.inserted_web_state();
      web_state_observations_.AddObservation(inserted_web_state);
      router_->OnWebStateChange(inserted_web_state);
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

#pragma mark - IOSChromeLocalSessionEventRouter::Observer (WebStateObserver)

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
  NOTREACHED();
}

void IOSChromeLocalSessionEventRouter::Observer::WillBeginBatchOperation(
    WebStateList* web_state_list) {
  router_->OnSessionEventStarting();
}

void IOSChromeLocalSessionEventRouter::Observer::BatchOperationEnded(
    WebStateList* web_state_list) {
  router_->OnSessionEventEnded();
}

#pragma mark - IOSChromeLocalSessionEventRouter

IOSChromeLocalSessionEventRouter::IOSChromeLocalSessionEventRouter(
    BrowserList* browser_list,
    sync_sessions::SyncSessionsClient* sessions_client,
    const syncer::SyncableService::StartSyncFlare& flare)
    : observer_(std::make_unique<Observer>(this)),
      sessions_client_(sessions_client),
      flare_(flare) {
  DCHECK(sessions_client_);
  // Start the observer now that the object is fully initialized. This may
  // call methods on the current instance if the BrowserList is not empty.
  observer_->Start(browser_list);
}

IOSChromeLocalSessionEventRouter::~IOSChromeLocalSessionEventRouter() = default;

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

void IOSChromeLocalSessionEventRouter::OnWebStateClosed() {
  if (batch_in_progress_) {
    return;
  }
  if (handler_) {
    handler_->OnLocalTabClosed();
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
