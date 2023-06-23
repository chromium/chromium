// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TABS_TAB_PICKUP_TAB_PICKUP_BROWSER_AGENT_H_
#define IOS_CHROME_BROWSER_TABS_TAB_PICKUP_TAB_PICKUP_BROWSER_AGENT_H_

#import "base/callback_list.h"
#import "base/scoped_multi_source_observation.h"
#import "base/scoped_observation.h"
#import "ios/chrome/browser/shared/model/browser/browser_observer.h"
#import "ios/chrome/browser/shared/model/browser/browser_user_data.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list_observer.h"
#import "ios/web/public/web_state_observer.h"

class Browser;

namespace sync_sessions {
class SessionSyncService;
}

namespace web {
class WebState;
}

// Service that creates/replaces tab pickup infobar.
class TabPickupBrowserAgent : public BrowserObserver,
                              public BrowserUserData<TabPickupBrowserAgent>,
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

  // BrowserObserver methods.
  void BrowserDestroyed(Browser* browser) override;

  // WebStateListObserver methods.
  void WebStateActivatedAt(WebStateList* web_state_list,
                           web::WebState* old_web_state,
                           web::WebState* new_web_state,
                           int active_index,
                           ActiveWebStateChangeReason reason) override;

  // web::WebStateObserver methods.
  void WebStateDestroyed(web::WebState* web_state) override;
  void WebStateRealized(web::WebState* web_state) override;

  // Track if an infobar will be displayed.
  bool infobar_in_progress_ = false;
  // Tracks if the `IOS.TabPickup.TimeSinceLastCrossDeviceSync` metric has been
  // recorded.
  static bool transition_time_metric_recorded;

  // The owning Browser.
  raw_ptr<Browser> browser_ = nullptr;
  // The active webState.
  raw_ptr<web::WebState> active_web_state_ = nullptr;
  // KeyedService responsible session sync.
  raw_ptr<sync_sessions::SessionSyncService> session_sync_service_ = nullptr;
  // CallbackListSubscription for the SessionSyncService method.
  base::CallbackListSubscription foreign_session_updated_subscription_;
  // Scoped observer observing unrealized WebStates.
  base::ScopedMultiSourceObservation<web::WebState, web::WebStateObserver>
      web_state_observations_{this};
};

#endif  // IOS_CHROME_BROWSER_TABS_TAB_PICKUP_TAB_PICKUP_BROWSER_AGENT_H_
