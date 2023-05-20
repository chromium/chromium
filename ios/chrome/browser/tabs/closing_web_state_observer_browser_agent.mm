// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/tabs/closing_web_state_observer_browser_agent.h"

#import "base/strings/string_piece.h"
#import "components/sessions/core/tab_restore_service.h"
#import "components/sessions/ios/ios_restore_live_tab.h"
#import "components/sessions/ios/ios_webstate_live_tab.h"
#import "ios/chrome/browser/sessions/ios_chrome_tab_restore_service_factory.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/snapshots/snapshot_tab_helper.h"
#import "ios/web/public/navigation/navigation_item.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "ios/web/public/web_state.h"
#import "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

BROWSER_USER_DATA_KEY_IMPL(ClosingWebStateObserverBrowserAgent)

ClosingWebStateObserverBrowserAgent::ClosingWebStateObserverBrowserAgent(
    Browser* browser)
    : restore_service_(IOSChromeTabRestoreServiceFactory::GetForBrowserState(
          browser->GetBrowserState())) {
  browser->AddObserver(this);
  browser->GetWebStateList()->AddObserver(this);
}

ClosingWebStateObserverBrowserAgent::~ClosingWebStateObserverBrowserAgent() {}

#pragma mark - Private methods

void ClosingWebStateObserverBrowserAgent::RecordHistoryForWebStateAtIndex(
    web::WebState* web_state,
    int index) {
  // The RestoreService will be null if navigation is off the record.
  if (!restore_service_)
    return;

  // It is possible to call this method with "unrealized" WebState. Check if
  // the WebState is in that state before accessing the NavigationManager as
  // that would force the realization of the WebState. The serialized state
  // can be retrieved in the same way as for a WebState whoe restoration is
  // in progress.
  const web::NavigationManager* navigation_manager = nullptr;
  if (web_state->IsRealized()) {
    navigation_manager = web_state->GetNavigationManager();
    DCHECK(navigation_manager);
  }

  if (!navigation_manager || navigation_manager->IsRestoreSessionInProgress()) {
    CRWSessionStorage* storage = web_state->BuildSessionStorage();
    auto live_tab = std::make_unique<sessions::RestoreIOSLiveTab>(storage);
    restore_service_->CreateHistoricalTab(live_tab.get(), index);
    return;
  }

  // No need to record history if the tab has no navigation or has only
  // presented the NTP or the bookmark UI.
  if (web_state->GetNavigationItemCount() <= 1) {
    const GURL& last_committed_url = web_state->GetLastCommittedURL();
    if (!last_committed_url.is_valid() ||
        (last_committed_url.host_piece() == kChromeUINewTabHost)) {
      return;
    }
  }

  restore_service_->CreateHistoricalTab(
      sessions::IOSWebStateLiveTab::GetForWebState(web_state), index);
}

#pragma mark - BrowserObserver

void ClosingWebStateObserverBrowserAgent::BrowserDestroyed(Browser* browser) {
  browser->RemoveObserver(this);
  browser->GetWebStateList()->RemoveObserver(this);
}

#pragma mark - WebStateListObserving

void ClosingWebStateObserverBrowserAgent::WebStateReplacedAt(
    WebStateList* web_state_list,
    web::WebState* old_web_state,
    web::WebState* new_web_state,
    int index) {
  SnapshotTabHelper::FromWebState(old_web_state)->RemoveSnapshot();
}

void ClosingWebStateObserverBrowserAgent::WillCloseWebStateAt(
    WebStateList* web_state_list,
    web::WebState* web_state,
    int index,
    bool user_action) {
  RecordHistoryForWebStateAtIndex(web_state, index);
  if (user_action) {
    SnapshotTabHelper::FromWebState(web_state)->RemoveSnapshot();
  }
}
