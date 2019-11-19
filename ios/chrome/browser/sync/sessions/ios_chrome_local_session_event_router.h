// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SYNC_SESSIONS_IOS_CHROME_LOCAL_SESSION_EVENT_ROUTER_H_
#define IOS_CHROME_BROWSER_SYNC_SESSIONS_IOS_CHROME_LOCAL_SESSION_EVENT_ROUTER_H_

#include <stddef.h>

#include <memory>
#include <set>

#include "base/callback_list.h"
#include "base/macros.h"
#include "components/sync/model/syncable_service.h"
#include "components/sync_sessions/local_session_event_router.h"
#import "ios/chrome/browser/tabs/tab_model_list_observer.h"
#include "ios/chrome/browser/web_state_list/web_state_list_observer.h"
#include "ios/web/public/web_state_observer.h"

class GURL;

namespace ios {
class ChromeBrowserState;
}

namespace sync_sessions {
class SyncSessionsClient;
}

// A LocalEventRouter that drives session sync via observation of
// web::WebState-related events.
class IOSChromeLocalSessionEventRouter
    : public sync_sessions::LocalSessionEventRouter,
      public web::WebStateObserver,
      public WebStateListObserver,
      public TabModelListObserver {
 public:
  IOSChromeLocalSessionEventRouter(
      ios::ChromeBrowserState* browser_state,
      sync_sessions::SyncSessionsClient* sessions_client_,
      const syncer::SyncableService::StartSyncFlare& flare);
  ~IOSChromeLocalSessionEventRouter() override;

  // LocalEventRouter:
  void StartRoutingTo(
      sync_sessions::LocalSessionEventHandler* handler) override;
  void Stop() override;

  // TabModelListObserver:
  void TabModelRegisteredWithBrowserState(
      TabModel* tab_model,
      ios::ChromeBrowserState* browser_state) override;
  void TabModelUnregisteredFromBrowserState(
      TabModel* tab_model,
      ios::ChromeBrowserState* browser_state) override;

  // web::WebStateObserver:
  void NavigationItemsPruned(web::WebState* web_state,
                             size_t pruned_item_count) override;
  void TitleWasSet(web::WebState* web_state) override;
  void DidFinishNavigation(web::WebState* web_state,
                           web::NavigationContext* navigation_context) override;
  void PageLoaded(
      web::WebState* web_state,
      web::PageLoadCompletionStatus load_completion_status) override;
  void DidChangeBackForwardState(web::WebState* web_state) override;
  void WebStateDestroyed(web::WebState* web_state) override;

  // web::WebStateListObserver:
  void WebStateInsertedAt(WebStateList* web_state_list,
                          web::WebState* web_state,
                          int index,
                          bool activating) override;
  void WebStateReplacedAt(WebStateList* web_state_list,
                          web::WebState* old_web_state,
                          web::WebState* new_web_state,
                          int index) override;
  void WebStateDetachedAt(WebStateList* web_state_list,
                          web::WebState* web_state,
                          int index) override;

 private:
  // Methods to add and remove WebStateList observer.
  void StartObservingWebStateList(WebStateList* web_state_list);
  void StopObservingWebStateList(WebStateList* web_state_list);

  // Called when a tab is parented.
  void OnTabParented(web::WebState* web_state);

  // Called on observation of a change in |web_state|.
  void OnWebStateChange(web::WebState* web_state);

  // Called when the favicons for the given page URLs
  // (e.g. http://www.google.com) and the given icon URL (e.g.
  // http://www.google.com/favicon.ico) have changed. It is valid to call
  // OnFaviconsChanged() with non-empty |page_urls| and an empty |icon_url|
  // and vice versa.
  void OnFaviconsChanged(const std::set<GURL>& page_urls, const GURL& icon_url);

  sync_sessions::LocalSessionEventHandler* handler_;
  ios::ChromeBrowserState* const browser_state_;
  sync_sessions::SyncSessionsClient* const sessions_client_;
  syncer::SyncableService::StartSyncFlare flare_;

  std::unique_ptr<base::CallbackList<void(const std::set<GURL>&,
                                          const GURL&)>::Subscription>
      favicon_changed_subscription_;

  std::unique_ptr<base::CallbackList<void(web::WebState*)>::Subscription>
      tab_parented_subscription_;

  DISALLOW_COPY_AND_ASSIGN(IOSChromeLocalSessionEventRouter);
};

#endif  // IOS_CHROME_BROWSER_SYNC_SESSIONS_IOS_CHROME_LOCAL_SESSION_EVENT_ROUTER_H_
