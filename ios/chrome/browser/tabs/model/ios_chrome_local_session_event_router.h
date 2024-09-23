// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TABS_MODEL_IOS_CHROME_LOCAL_SESSION_EVENT_ROUTER_H_
#define IOS_CHROME_BROWSER_TABS_MODEL_IOS_CHROME_LOCAL_SESSION_EVENT_ROUTER_H_

#include <stddef.h>

#include <memory>

#include "base/callback_list.h"
#import "base/memory/raw_ptr.h"
#include "components/sync/model/syncable_service.h"
#include "components/sync_sessions/local_session_event_router.h"
#include "ios/chrome/browser/shared/model/web_state_list/web_state_list_observer.h"
#include "ios/web/public/web_state_observer.h"

class AllWebStateListObservationRegistrar;
class BrowserList;

namespace sync_sessions {
class SyncSessionsClient;
}  // namespace sync_sessions

// A LocalEventRouter that drives session sync via observation of
// web::WebState-related events.
class IOSChromeLocalSessionEventRouter
    : public sync_sessions::LocalSessionEventRouter {
 public:
  IOSChromeLocalSessionEventRouter(
      BrowserList* browser_list,
      sync_sessions::SyncSessionsClient* sessions_client_,
      const syncer::SyncableService::StartSyncFlare& flare);

  IOSChromeLocalSessionEventRouter(const IOSChromeLocalSessionEventRouter&) =
      delete;
  IOSChromeLocalSessionEventRouter& operator=(
      const IOSChromeLocalSessionEventRouter&) = delete;

  ~IOSChromeLocalSessionEventRouter() override;

  // LocalEventRouter:
  void StartRoutingTo(
      sync_sessions::LocalSessionEventHandler* handler) override;
  void Stop() override;

 private:
  // Observer implementation for each profile.
  class Observer : public WebStateListObserver, public web::WebStateObserver {
   public:
    explicit Observer(IOSChromeLocalSessionEventRouter* session_router);
    Observer(const Observer&) = delete;
    Observer& operator=(const Observer&) = delete;
    ~Observer() override;

   private:
    // WebStateListObserver:
    void WebStateListDidChange(WebStateList* web_state_list,
                               const WebStateListChange& change,
                               const WebStateListStatus& status) override;
    void WillBeginBatchOperation(WebStateList* web_state_list) override;
    void BatchOperationEnded(WebStateList* web_state_list) override;

    // web::WebStateObserver:
    void TitleWasSet(web::WebState* web_state) override;
    void DidFinishNavigation(
        web::WebState* web_state,
        web::NavigationContext* navigation_context) override;
    void PageLoaded(
        web::WebState* web_state,
        web::PageLoadCompletionStatus load_completion_status) override;
    void WasShown(web::WebState* web_state) override;
    void DidChangeBackForwardState(web::WebState* web_state) override;
    void WebStateDestroyed(web::WebState* web_state) override;

    raw_ptr<IOSChromeLocalSessionEventRouter> router_;
  };

  // Called before the Batch operation starts for a web state list.
  void OnSessionEventStarting();

  // Called when Batch operation is completed for a web state list.
  void OnSessionEventEnded();

  // Called when a tab is parented.
  void OnTabParented(web::WebState* web_state);

  // Called on observation of a change in `web_state`.
  void OnWebStateChange(web::WebState* web_state);

  // Observation registrar for the associated browser list; owns an instance
  // of IOSChromeLocalSessionEventRouter::Observer.
  std::unique_ptr<AllWebStateListObservationRegistrar> const registrar_;

  raw_ptr<sync_sessions::LocalSessionEventHandler> handler_ = nullptr;
  const raw_ptr<sync_sessions::SyncSessionsClient> sessions_client_;
  syncer::SyncableService::StartSyncFlare flare_;

  base::CallbackListSubscription const tab_parented_subscription_;

  // Track the number of WebStateList we are observing that are in a batch
  // operation.
  int batch_in_progress_ = 0;
};

#endif  // IOS_CHROME_BROWSER_TABS_MODEL_IOS_CHROME_LOCAL_SESSION_EVENT_ROUTER_H_
