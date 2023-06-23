// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/tabs/tab_pickup/tab_pickup_browser_agent.h"

#import "base/metrics/histogram_functions.h"
#import "components/sync_sessions/session_sync_service.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/sync/session_sync_service_factory.h"
#import "ios/chrome/browser/synced_sessions/distant_session.h"
#import "ios/chrome/browser/synced_sessions/synced_sessions.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

BROWSER_USER_DATA_KEY_IMPL(TabPickupBrowserAgent)

bool TabPickupBrowserAgent::transition_time_metric_recorded = false;

TabPickupBrowserAgent::TabPickupBrowserAgent(Browser* browser)
    : browser_(browser),
      session_sync_service_(SessionSyncServiceFactory::GetForBrowserState(
          browser_->GetBrowserState())) {
  browser_->AddObserver(this);

  // base::Unretained() is safe below because the subscription itself is a class
  // member field and handles destruction well.
  foreign_session_updated_subscription_ =
      session_sync_service_->SubscribeToForeignSessionsChanged(
          base::BindRepeating(&TabPickupBrowserAgent::ForeignSessionsChanged,
                              base::Unretained(this)));
}

TabPickupBrowserAgent::~TabPickupBrowserAgent() {
  DCHECK(!browser_);
}

#pragma mark - BrowserObserver

void TabPickupBrowserAgent::BrowserDestroyed(Browser* browser) {
  DCHECK_EQ(browser, browser_);
  browser->RemoveObserver(this);
  browser_ = nullptr;
}

#pragma mark - Private methods

void TabPickupBrowserAgent::ForeignSessionsChanged() {
  if (transition_time_metric_recorded) {
    return;
  }

  auto const synced_sessions =
      std::make_unique<synced_sessions::SyncedSessions>(session_sync_service_);
  if (synced_sessions->GetSessionCount()) {
    transition_time_metric_recorded = true;
    const synced_sessions::DistantSession* session =
        synced_sessions->GetSession(0);
    const base::TimeDelta time_since_last_sync =
        base::Time::Now() - session->modified_time;
    base::UmaHistogramCustomTimes("IOS.TabPickup.TimeSinceLastCrossDeviceSync",
                                  time_since_last_sync, base::Minutes(1),
                                  base::Days(24), 50);
  }
}
