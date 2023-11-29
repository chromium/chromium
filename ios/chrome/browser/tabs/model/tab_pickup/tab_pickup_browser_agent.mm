// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/tabs/model/tab_pickup/tab_pickup_browser_agent.h"

#import "base/metrics/histogram_functions.h"
#import "components/infobars/core/infobar.h"
#import "components/prefs/pref_service.h"
#import "components/sync_sessions/session_sync_service.h"
#import "ios/chrome/browser/infobars/infobar_ios.h"
#import "ios/chrome/browser/infobars/infobar_manager_impl.h"
#import "ios/chrome/browser/ntp/model/new_tab_page_util.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/sync/model/session_sync_service_factory.h"
#import "ios/chrome/browser/synced_sessions/model/distant_session.h"
#import "ios/chrome/browser/synced_sessions/model/distant_tab.h"
#import "ios/chrome/browser/synced_sessions/model/synced_sessions.h"
#import "ios/chrome/browser/tabs/model/tab_pickup/features.h"
#import "ios/chrome/browser/tabs/model/tab_pickup/tab_pickup_infobar_delegate.h"
#import "ios/chrome/browser/tabs/model/tab_sync_util.h"
#import "ios/web/public/web_state.h"

namespace {

// The minimum delay between the presentation of two tab pickup banners.
const base::TimeDelta kDelayBetweenTwoBanners = base::Hours(2);

// The maximum length of the last distant tab URL.
size_t kMaxLengthLastDistantTabURL = 2 * 1024;

}  // namespace

BROWSER_USER_DATA_KEY_IMPL(TabPickupBrowserAgent)

bool TabPickupBrowserAgent::transition_time_metric_recorded = false;
bool TabPickupBrowserAgent::infobar_displayed = false;

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

  foreground_notification_observer_ = [[NSNotificationCenter defaultCenter]
      addObserverForName:UIApplicationWillEnterForegroundNotification
                  object:nil
                   queue:nil
              usingBlock:^(NSNotification* notification) {
                this->AppWillEnterForeground();
              }];
}

TabPickupBrowserAgent::~TabPickupBrowserAgent() {
  DCHECK(!browser_);

  [[NSNotificationCenter defaultCenter]
      removeObserver:foreground_notification_observer_];
}

#pragma mark - BrowserObserver

void TabPickupBrowserAgent::BrowserDestroyed(Browser* browser) {
  DCHECK_EQ(browser, browser_);
  browser->GetWebStateList()->RemoveObserver(this);
  browser->RemoveObserver(this);
  browser_ = nullptr;
}

#pragma mark - WebStateListObserver

void TabPickupBrowserAgent::WebStateListDidChange(
    WebStateList* web_state_list,
    const WebStateListChange& change,
    const WebStateListStatus& status) {
  if (!status.active_web_state_change()) {
    return;
  }

  DCHECK_EQ(active_web_state_, status.old_active_web_state);
  active_web_state_ = status.new_active_web_state;
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

#pragma mark - infobars::InfoBarManager::Observer

void TabPickupBrowserAgent::OnInfoBarRemoved(infobars::InfoBar* infobar,
                                             bool animate) {
  if (infobar == infobar_) {
    infobar_manager_scoped_observation_.Reset();
    infobar_ = nullptr;
  }
}

#pragma mark - Private methods

void TabPickupBrowserAgent::ForeignSessionsChanged() {
  RecordTransitionTime();

  if (!CanShowTabPickupBanner()) {
    return;
  }

  if (!active_web_state_ || infobar_in_progress_ || infobar_displayed) {
    return;
  }

  // Don't present a tab pickup banner on the NTP if the tab resumption feature
  // is enabled.
  if (IsTabResumptionEnabled() &&
      IsURLNewTabPage(active_web_state_->GetVisibleURL())) {
    return;
  }

  if (!active_web_state_->IsRealized()) {
    web_state_observations_.AddObservation(active_web_state_);
    return;
  }

  auto const synced_sessions =
      std::make_unique<synced_sessions::SyncedSessions>(session_sync_service_);

  if (!IsTabPickupFaviconAvaible()) {
    CheckDistantTabsOrder(synced_sessions.get());
  }
  LastActiveDistantTab last_active_tab =
      GetLastActiveDistantTab(synced_sessions.get(), TabPickupTimeThreshold());
  if (last_active_tab.tab) {
    session_ = last_active_tab.session;
    tab_ = last_active_tab.tab;
    SetupInfoBarDelegate();
  }
}

void TabPickupBrowserAgent::SetupInfoBarDelegate() {
  CHECK(IsTabPickupEnabled());
  CHECK(!IsTabPickupDisabledByUser());

  delegate_ =
      std::make_unique<TabPickupInfobarDelegate>(browser_, session_, tab_);
  if (!UpdateNewDistantTab(delegate_->GetTabURL())) {
    return;
  }

  infobar_in_progress_ = true;
  delegate_->FetchFavIconImage(^{
    // Once the favicon image is fetched, display the infobar.
    ShowInfoBar();
  });
}

void TabPickupBrowserAgent::ShowInfoBar() {
  infobar_in_progress_ = false;

  if (!active_web_state_) {
    return;
  }

  if (infobar_) {
    infobars::InfoBarManager* previous_infobar_manager =
        InfoBarManagerImpl::FromWebState(infobar_web_state_);
    previous_infobar_manager->RemoveInfoBar(infobar_);
    DCHECK(!infobar_);
  }

  infobars::InfoBarManager* infobar_manager =
      InfoBarManagerImpl::FromWebState(active_web_state_);
  infobar_manager_scoped_observation_.Observe(infobar_manager);
  std::unique_ptr<infobars::InfoBar> infobar = std::make_unique<InfoBarIOS>(
      InfobarType::kInfobarTypeTabPickup, std::move(delegate_));
  infobar_ = infobar_manager->AddInfoBar(std::move(infobar),
                                         /*replace_existing=*/true);
  infobar_web_state_ = active_web_state_;
  infobar_displayed = true;
  GetApplicationContext()->GetLocalState()->SetTime(
      prefs::kTabPickupLastDisplayedTime, base::Time::Now());
}

bool TabPickupBrowserAgent::CanShowTabPickupBanner() {
  if (!IsTabPickupEnabled() || IsTabPickupDisabledByUser()) {
    return false;
  }

  if (!IsTabPickupMinimumDelayEnabled()) {
    return true;
  }

  const base::TimeDelta time_since_last_display =
      base::Time::Now() - GetApplicationContext()->GetLocalState()->GetTime(
                              prefs::kTabPickupLastDisplayedTime);
  return kDelayBetweenTwoBanners < time_since_last_display;
}

bool TabPickupBrowserAgent::UpdateNewDistantTab(GURL distant_tab_url) {
  PrefService* prefs = GetApplicationContext()->GetLocalState();

  std::string distant_tab_without_ref =
      GURL(distant_tab_url).GetWithoutRef().spec();
  if (distant_tab_without_ref.length() > kMaxLengthLastDistantTabURL) {
    distant_tab_without_ref =
        distant_tab_without_ref.substr(0, kMaxLengthLastDistantTabURL);
  }
  std::string previous_tab_with_no_ref =
      prefs->GetString(prefs::kTabPickupLastDisplayedURL);

  if (distant_tab_without_ref.compare(previous_tab_with_no_ref) == 0) {
    return false;
  }

  prefs->SetString(prefs::kTabPickupLastDisplayedURL, distant_tab_without_ref);
  return true;
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

void TabPickupBrowserAgent::AppWillEnterForeground() {
  infobar_displayed = false;
}
