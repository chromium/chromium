// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TABS_MODEL_IOS_CHROME_LOCAL_SESSION_EVENT_ROUTER_H_
#define IOS_CHROME_BROWSER_TABS_MODEL_IOS_CHROME_LOCAL_SESSION_EVENT_ROUTER_H_

#include <stddef.h>

#include <memory>

#include "base/memory/raw_ptr.h"
#include "components/sync/model/syncable_service.h"
#include "components/sync_sessions/local_session_event_router.h"

class BrowserList;
namespace web {
class WebState;
}

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
  // Observer implementation for all Browsers.
  class Observer;

  // Called before the Batch operation starts for a web state list.
  void OnSessionEventStarting();

  // Called when Batch operation is completed for a web state list.
  void OnSessionEventEnded();

  // Called on observation of a change in `web_state`.
  void OnWebStateChange(web::WebState* web_state);

  // Called when a `web_state` is closed.
  void OnWebStateClosed();

  // Observer.
  std::unique_ptr<Observer> const observer_;

  raw_ptr<sync_sessions::LocalSessionEventHandler> handler_ = nullptr;
  const raw_ptr<sync_sessions::SyncSessionsClient> sessions_client_;
  syncer::SyncableService::StartSyncFlare flare_;

  // Track the number of WebStateList we are observing that are in a batch
  // operation.
  int batch_in_progress_ = 0;
};

#endif  // IOS_CHROME_BROWSER_TABS_MODEL_IOS_CHROME_LOCAL_SESSION_EVENT_ROUTER_H_
