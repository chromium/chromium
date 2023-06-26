// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TABS_TAB_PICKUP_TAB_PICKUP_BROWSER_AGENT_H_
#define IOS_CHROME_BROWSER_TABS_TAB_PICKUP_TAB_PICKUP_BROWSER_AGENT_H_

#import "base/callback_list.h"
#import "ios/chrome/browser/shared/model/browser/browser_observer.h"
#import "ios/chrome/browser/shared/model/browser/browser_user_data.h"

class Browser;

namespace sync_sessions {
class SessionSyncService;
}

// Service that creates/replaces tab pickup infobar.
class TabPickupBrowserAgent : public BrowserObserver,
                              public BrowserUserData<TabPickupBrowserAgent> {
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

  // BrowserObserver methods.
  void BrowserDestroyed(Browser* browser) override;

  // The owning Browser.
  raw_ptr<Browser> browser_ = nullptr;
  // KeyedService responsible session sync.
  raw_ptr<sync_sessions::SessionSyncService> session_sync_service_ = nullptr;
  // CallbackListSubscription for the SessionSyncService method.
  base::CallbackListSubscription foreign_session_updated_subscription_;

  // Tracks if the `IOS.TabPickup.TimeSinceLastCrossDeviceSync` metric has been
  // recorded.
  static bool transition_time_metric_recorded;
};

#endif  // IOS_CHROME_BROWSER_TABS_TAB_PICKUP_TAB_PICKUP_BROWSER_AGENT_H_
