// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TABS_MODEL_TAB_PICKUP_TAB_PICKUP_BROWSER_AGENT_H_
#define IOS_CHROME_BROWSER_TABS_MODEL_TAB_PICKUP_TAB_PICKUP_BROWSER_AGENT_H_

#import "base/callback_list.h"
#import "base/scoped_multi_source_observation.h"
#import "base/scoped_observation.h"
#import "components/infobars/core/confirm_infobar_delegate.h"
#import "components/infobars/core/infobar.h"
#import "ios/chrome/browser/shared/model/browser/browser_observer.h"
#import "ios/chrome/browser/shared/model/browser/browser_user_data.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list_observer.h"
#import "ios/web/public/web_state_observer.h"

class Browser;
class GURL;
class TabPickupInfobarDelegate;

namespace sync_sessions {
class SessionSyncService;
}

namespace synced_sessions {
struct DistantSession;
struct DistantTab;
}

namespace web {
class WebState;
}

// Service that creates/replaces tab pickup infobar.
class TabPickupBrowserAgent : public BrowserObserver,
                              public BrowserUserData<TabPickupBrowserAgent>,
                              public infobars::InfoBarManager::Observer,
                              public WebStateListObserver,
                              public web::WebStateObserver {
 public:
  TabPickupBrowserAgent(const TabPickupBrowserAgent&) = delete;
  TabPickupBrowserAgent& operator=(const TabPickupBrowserAgent&) = delete;
  ~TabPickupBrowserAgent() override;

 private:
  friend class BrowserUserData<TabPickupBrowserAgent>;
  BROWSER_USER_DATA_KEY_DECL();

  explicit TabPickupBrowserAgent(Browser* browser);

  // Called when foreign sessions change.
  void ForeignSessionsChanged();

  // Records the `IOS.TabPickup.TimeSinceLastCrossDeviceSync` metric.
  void RecordTransitionTime();

  // Setups the infobar delegate before showing the infobar.
  void SetupInfoBarDelegate();

  // Adds/replaces the infobar and show the banner.
  void ShowInfoBar();

  // Returns whether a tab pickup can be presented or not.
  bool CanShowTabPickupBanner();

  // Returns true if the given `distant_tab_url` is different from the previous
  // URL that was used to display the tab pickup banner. It also sets the
  // distant_tab_url to be the previous URL.
  bool UpdateNewDistantTab(GURL distant_tab_url);

  // Called when the app has been foregrounded.
  void AppWillEnterForeground();

  // BrowserObserver methods.
  void BrowserDestroyed(Browser* browser) override;

  // WebStateListObserver methods.
  void WebStateListDidChange(WebStateList* web_state_list,
                             const WebStateListChange& change,
                             const WebStateListStatus& status) override;

  // web::WebStateObserver methods.
  void WebStateDestroyed(web::WebState* web_state) override;
  void WebStateRealized(web::WebState* web_state) override;

  // infobars::InfoBarManager::Observer methods.
  void OnInfoBarRemoved(infobars::InfoBar* infobar, bool animate) override;

  // Tracks if an infobar will be displayed.
  bool infobar_in_progress_ = false;
  // Tracks if an infobar has been displayed since the last app foreground.
  static bool infobar_displayed;
  // Tracks if the `IOS.TabPickup.TimeSinceLastCrossDeviceSync` metric has been
  // recorded.
  static bool transition_time_metric_recorded;
  // The currently displayed infobar.
  raw_ptr<infobars::InfoBar> infobar_ = nullptr;
  // The owning Browser.
  raw_ptr<Browser> browser_ = nullptr;
  // The active webState.
  raw_ptr<web::WebState> active_web_state_ = nullptr;
  // The infobar's webState.
  raw_ptr<web::WebState> infobar_web_state_ = nullptr;
  // The infobar's delegate.
  std::unique_ptr<TabPickupInfobarDelegate> delegate_;
  // The distant session used to display the infobar.
  raw_ptr<const synced_sessions::DistantSession> session_;
  // The distant tab used to display the infobar.
  raw_ptr<const synced_sessions::DistantTab> tab_;
  // KeyedService responsible session sync.
  raw_ptr<sync_sessions::SessionSyncService> session_sync_service_ = nullptr;
  // CallbackListSubscription for the SessionSyncService method.
  base::CallbackListSubscription foreign_session_updated_subscription_;
  // Scoped observer observing unrealized WebStates.
  base::ScopedMultiSourceObservation<web::WebState, web::WebStateObserver>
      web_state_observations_{this};
  // Scoped observer that facilitates observing the infobar manager.
  base::ScopedObservation<infobars::InfoBarManager,
                          infobars::InfoBarManager::Observer>
      infobar_manager_scoped_observation_{this};
  // Holds references to foreground NSNotification callback observer.
  id foreground_notification_observer_;
};

#endif  // IOS_CHROME_BROWSER_TABS_MODEL_TAB_PICKUP_TAB_PICKUP_BROWSER_AGENT_H_
