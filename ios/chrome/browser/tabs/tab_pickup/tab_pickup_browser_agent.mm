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
#import "ios/chrome/browser/tabs/tab_pickup/features.h"
#import "ios/web/public/web_state.h"

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
  browser_->GetWebStateList()->AddObserver(this);

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
  browser->GetWebStateList()->RemoveObserver(this);
  browser->RemoveObserver(this);
  browser_ = nullptr;
}

#pragma mark - WebStateListObserver

void TabPickupBrowserAgent::WebStateActivatedAt(
    WebStateList* web_state_list,
    web::WebState* old_web_state,
    web::WebState* new_web_state,
    int active_index,
    ActiveWebStateChangeReason reason) {
  DCHECK_EQ(active_web_state_, old_web_state);
  active_web_state_ = new_web_state;
}

#pragma mark - web::WebStateObserver

void TabPickupBrowserAgent::WebStateDestroyed(web::WebState* web_state) {
  DCHECK_EQ(web_state, active_web_state_);
  web_state_observations_.RemoveObservation(web_state);
  active_web_state_ = nullptr;
}

void TabPickupBrowserAgent::WebStateRealized(web::WebState* web_state) {
  DCHECK_EQ(web_state, active_web_state_);
  web_state_observations_.RemoveObservation(web_state);
  ForeignSessionsChanged();
}

#pragma mark - Private methods

void TabPickupBrowserAgent::ForeignSessionsChanged() {
  RecordTransitionTime();

  if (!IsTabPickupEnabled()) {
    return;
  }

  if (!active_web_state_ || infobar_in_progress_) {
    return;
  }

  if (!active_web_state_->IsRealized()) {
    web_state_observations_.AddObservation(active_web_state_);
    return;
  }

  auto const synced_sessions =
      std::make_unique<synced_sessions::SyncedSessions>(session_sync_service_);
  if (synced_sessions->GetSessionCount()) {
    // Get the last synced tab.
    const synced_sessions::DistantSession* session =
        synced_sessions->GetSession(0);

    // Check that the last synced tab is yougner than the tab pickup time
    // threshold.
    const base::TimeDelta modified_time =
        base::Time::Now() - session->modified_time;
    if (modified_time < TabPickupTimeThreshold()) {
      SetupInfoBarDelegate();
    }
  }
}

void TabPickupBrowserAgent::SetupInfoBarDelegate() {
  DCHECK(IsTabPickupEnabled());
  infobar_in_progress_ = true;
  // TODO(crbug.com/1129482): Implement this.
}

void TabPickupBrowserAgent::RecordTransitionTime() {
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
