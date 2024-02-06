// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_TAB_RESUMPTION_TAB_RESUMPTION_HELPER_H_
#define IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_TAB_RESUMPTION_TAB_RESUMPTION_HELPER_H_

#import "base/callback_list.h"
#import "base/ios/block_types.h"
#import "base/scoped_observation.h"
#import "components/sessions/core/session_id.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/sync/service/sync_service_observer.h"
#import "ios/chrome/browser/favicon/model/favicon_loader.h"
#import "ios/chrome/browser/ui/start_surface/start_surface_recent_tab_browser_agent.h"
#import "url/gurl.h"

class Browser;
class FaviconLoader;
class PrefService;
class StartSurfaceRecentTabBrowserAgent;
@class TabResumptionItem;
@protocol TabResumptionHelperDelegate;

namespace base {
class Time;
}  // namespace base

namespace syncer {
class SyncService;
}  // namespace syncer

namespace sync_sessions {
class SessionSyncService;
}  // namespace sync_sessions

namespace synced_sessions {
struct DistantSession;
struct DistantTab;
}  // namespace synced_sessions

namespace web {
class WebState;
}  // namespace web

// Helper class to control the tab resumption feature.
class TabResumptionHelper : public signin::IdentityManager::Observer,
                            public syncer::SyncServiceObserver,
                            public StartSurfaceRecentTabObserver {
 public:
  TabResumptionHelper(const TabResumptionHelper&) = delete;
  TabResumptionHelper& operator=(const TabResumptionHelper&) = delete;

  explicit TabResumptionHelper(Browser* browser,
                               signin::IdentityManager* identity_manager,
                               PrefService* local_state);

  ~TabResumptionHelper() override;

  // Tries to fetch the last available TabResumptionItem.
  void LastTabResumptionItem();

  // Opens the last synced tab from another device.
  void OpenDistantTab();

  // Sets the delegate for this helper.
  void SetDelegate(id<TabResumptionHelperDelegate> delegate);

  TabResumptionItem* GetTabResumptionItem() { return tab_resumption_item_; }

 private:
  // Handles signal that the synced session has changed.
  void ForeignSessionsChanged();

  // Fetches the favicon for the given `item`.
  void FetchFaviconForItem(TabResumptionItem* item,
                           FaviconLoader* favicon_loader);

  // Handles the result of a favicon fetch.
  void OnFaviconForPageUrl(TabResumptionItem* item,
                           FaviconAttributes* attributes);

  // Creates a TabResumptionItem corresponding to the last active distant tab.
  void LastSyncedTabItemFromLastActiveDistantTab(
      const synced_sessions::DistantSession* session,
      const synced_sessions::DistantTab* tab,
      FaviconLoader* favicon_loader);

  // Creates a TabResumptionItem corresponding to the last synced tab.
  void MostRecentTabItemFromWebState(web::WebState* web_state,
                                     base::Time opened_time,
                                     FaviconLoader* favicon_loader);

  // syncer::SyncServiceObserver.
  void OnStateChanged(syncer::SyncService* sync) override;
  // signin::IdentityManager::Observer.
  void OnPrimaryAccountChanged(
      const signin::PrimaryAccountChangeEvent& event_details) override;
  // StartSurfaceBrowserAgentObserver.
  void MostRecentTabRemoved(web::WebState* web_state) override;
  void MostRecentTabFaviconUpdated(web::WebState* web_state,
                                   UIImage* image) override {}
  void MostRecentTabTitleUpdated(web::WebState* web_state,
                                 const std::u16string& title) override {}

  // Bool that tracks if a most recent tab item can be displayed.
  bool can_show_most_recent_item_ = true;
  // Last distant tab resumption item URL.
  GURL last_distant_item_url_;

  // Tab identifier of the last distant tab resumption item.
  SessionID tab_id_ = SessionID::InvalidValue();
  // Session tag of the last distant tab resumption item.
  std::string session_tag_;

  // The owning Browser.
  raw_ptr<Browser> browser_ = nullptr;
  raw_ptr<PrefService> local_state_ = nullptr;
  // Loads favicons.
  raw_ptr<FaviconLoader> favicon_loader_ = nullptr;
  // Browser Agent that manages the most recent WebState.
  raw_ptr<StartSurfaceRecentTabBrowserAgent> recent_tab_browser_agent_ =
      nullptr;
  // KeyedService responsible session sync.
  raw_ptr<sync_sessions::SessionSyncService> session_sync_service_ = nullptr;
  // KeyedService responsible for sync state.
  raw_ptr<syncer::SyncService> sync_service_ = nullptr;
  // CallbackListSubscription for the SessionSyncService method.
  base::CallbackListSubscription foreign_session_updated_subscription_;
  // The latest state of the item config for the Tab Resumption module.
  TabResumptionItem* tab_resumption_item_ = nullptr;
  base::ScopedObservation<syncer::SyncService, syncer::SyncServiceObserver>
      scoped_observation_{this};
  base::ScopedObservation<signin::IdentityManager,
                          signin::IdentityManager::Observer>
      identity_manager_observer_{this};
  base::ScopedObservation<StartSurfaceRecentTabBrowserAgent,
                          StartSurfaceRecentTabObserver>
      start_surface_recent_tab_observer_{this};
  // The delegate for this helper class.
  __weak id<TabResumptionHelperDelegate> delegate_ = nullptr;
  base::WeakPtrFactory<TabResumptionHelper> weak_ptr_factory_{this};
};

#endif  // IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_TAB_RESUMPTION_TAB_RESUMPTION_HELPER_H_
