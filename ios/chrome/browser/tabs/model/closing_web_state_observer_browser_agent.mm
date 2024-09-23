// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/tabs/model/closing_web_state_observer_browser_agent.h"

#import "base/metrics/histogram_macros.h"
#import "components/sessions/core/tab_restore_service.h"
#import "components/sessions/ios/ios_restore_live_tab.h"
#import "components/sessions/ios/ios_webstate_live_tab.h"
#import "ios/chrome/browser/sessions/model/ios_chrome_tab_restore_service_factory.h"
#import "ios/chrome/browser/sessions/model/session_restoration_service.h"
#import "ios/chrome/browser/sessions/model/session_restoration_service_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/snapshots/model/snapshot_tab_helper.h"
#import "ios/web/public/navigation/navigation_item.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "ios/web/public/session/proto/navigation.pb.h"
#import "ios/web/public/session/proto/storage.pb.h"
#import "ios/web/public/web_state.h"
#import "url/gurl.h"

BROWSER_USER_DATA_KEY_IMPL(ClosingWebStateObserverBrowserAgent)

ClosingWebStateObserverBrowserAgent::ClosingWebStateObserverBrowserAgent(
    Browser* browser)
    : browser_(browser) {
  DCHECK(!browser_->GetProfile()->IsOffTheRecord());
  browser_->AddObserver(this);
  browser_->GetWebStateList()->AddObserver(this);
}

ClosingWebStateObserverBrowserAgent::~ClosingWebStateObserverBrowserAgent() {}

#pragma mark - Private methods

void ClosingWebStateObserverBrowserAgent::RecordHistoryForWebStateAtIndex(
    web::WebState* web_state,
    int index) {
  // No need to record history if the tab has no navigation or has only
  // presented the NTP or the bookmark UI.
  if (web_state->GetNavigationItemCount() <= 1) {
    const GURL& last_committed_url = web_state->GetLastCommittedURL();
    if (!last_committed_url.is_valid() ||
        (last_committed_url.host_piece() == kChromeUINewTabHost)) {
      return;
    }
  }

  // If the WebState is unrealized, ask the SessionRestorationService to load
  // the data from storage (it should exists otherwise the WebState could not
  // transition to the realized state).
  if (!web_state->IsRealized()) {
    ProfileIOS* profile = browser_->GetProfile();
    SessionRestorationServiceFactory::GetForProfile(profile)
        ->LoadWebStateStorage(
            browser_, web_state,
            base::BindOnce(
                &ClosingWebStateObserverBrowserAgent::RecordHistoryFromStorage,
                weak_ptr_factory_.GetWeakPtr(), index));
    return;
  }

  IOSChromeTabRestoreServiceFactory::GetForProfile(browser_->GetProfile())
      ->CreateHistoricalTab(
          sessions::IOSWebStateLiveTab::GetForWebState(web_state), index);
}

void ClosingWebStateObserverBrowserAgent::RecordHistoryFromStorage(
    int index,
    web::proto::WebStateStorage storage) {
  DCHECK(browser_);
  sessions::RestoreIOSLiveTab live_tab(storage.navigation());
  IOSChromeTabRestoreServiceFactory::GetForProfile(browser_->GetProfile())
      ->CreateHistoricalTab(&live_tab, index);
}

#pragma mark - BrowserObserver

void ClosingWebStateObserverBrowserAgent::BrowserDestroyed(Browser* browser) {
  DCHECK_EQ(browser, browser_.get());
  // Prevent any posted callbacks to be invoked.
  weak_ptr_factory_.InvalidateWeakPtrs();

  browser_->RemoveObserver(this);
  browser_->GetWebStateList()->RemoveObserver(this);
  browser_ = nullptr;
}

#pragma mark - WebStateListObserving

void ClosingWebStateObserverBrowserAgent::WebStateListWillChange(
    WebStateList* web_state_list,
    const WebStateListChangeDetach& detach_change,
    const WebStateListStatus& status) {
  if (!detach_change.is_closing()) {
    return;
  }

  web::WebState* detached_web_state = detach_change.detached_web_state();
  if (!detach_change.is_tabs_cleanup()) {
    RecordHistoryForWebStateAtIndex(detached_web_state,
                                    detach_change.detached_from_index());
  }
  if (detach_change.is_user_action() || detach_change.is_tabs_cleanup()) {
    SnapshotTabHelper::FromWebState(detached_web_state)->RemoveSnapshot();
  }
}

void ClosingWebStateObserverBrowserAgent::WebStateListDidChange(
    WebStateList* web_state_list,
    const WebStateListChange& change,
    const WebStateListStatus& status) {
  switch (change.type()) {
    case WebStateListChange::Type::kStatusOnly:
      // Do nothing when a WebState is selected and its status is updated.
      break;
    case WebStateListChange::Type::kDetach: {
      const WebStateListChangeDetach& detach_change =
          change.As<WebStateListChangeDetach>();
      web::WebState* detached_web_state = detach_change.detached_web_state();
      GURL url = detached_web_state->GetLastCommittedURL();
      UMA_HISTOGRAM_BOOLEAN("IOS.ClosedTabIsAboutBlank", url.IsAboutBlank());
      break;
    }
    case WebStateListChange::Type::kMove:
      // Do nothing when a WebState is moved.
      break;
    case WebStateListChange::Type::kReplace: {
      const WebStateListChangeReplace& replace_change =
          change.As<WebStateListChangeReplace>();
      SnapshotTabHelper::FromWebState(replace_change.replaced_web_state())
          ->RemoveSnapshot();
      break;
    }
    case WebStateListChange::Type::kInsert:
      // Do nothing when a new WebState is inserted.
      break;
    case WebStateListChange::Type::kGroupCreate:
      // Do nothing when a group is created.
      break;
    case WebStateListChange::Type::kGroupVisualDataUpdate:
      // Do nothing when a tab group's visual data are updated.
      break;
    case WebStateListChange::Type::kGroupMove:
      // Do nothing when a tab group is moved.
      break;
    case WebStateListChange::Type::kGroupDelete:
      // Do nothing when a group is deleted.
      break;
  }
}
