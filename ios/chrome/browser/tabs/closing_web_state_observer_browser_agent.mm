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

// To get access to UseSessionSerializationOptimizations().
// TODO(crbug.com/1383087): remove once the feature is fully launched.
#import "ios/web/common/features.h"

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

  // No need to record history if the tab has no navigation or has only
  // presented the NTP or the bookmark UI.
  if (web_state->GetNavigationItemCount() <= 1) {
    const GURL& last_committed_url = web_state->GetLastCommittedURL();
    if (!last_committed_url.is_valid() ||
        (last_committed_url.host_piece() == kChromeUINewTabHost)) {
      return;
    }
  }

  // It is possible to call this method with "unrealized" WebState. Check if
  // the WebState is in that state before accessing the NavigationManager as
  // that would force the realization of the WebState. The serialized state
  // can be retrieved in the same way as for a WebState whose restoration is
  // in progress.
  const web::NavigationManager* navigation_manager = nullptr;
  if (web_state->IsRealized()) {
    navigation_manager = web_state->GetNavigationManager();
    DCHECK(navigation_manager);
  }

  if (!navigation_manager || navigation_manager->IsRestoreSessionInProgress()) {
    if (!web::features::UseSessionSerializationOptimizations()) {
      CRWSessionStorage* storage = web_state->BuildSessionStorage();
      auto live_tab = std::make_unique<sessions::RestoreIOSLiveTab>(storage);
      restore_service_->CreateHistoricalTab(live_tab.get(), index);
    }
    return;
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

void ClosingWebStateObserverBrowserAgent::WebStateListWillChange(
    WebStateList* web_state_list,
    const WebStateListChangeDetach& detach_change,
    const WebStateListStatus& status) {
  if (!detach_change.is_closing()) {
    return;
  }

  web::WebState* detached_web_state = detach_change.detached_web_state();
  RecordHistoryForWebStateAtIndex(detached_web_state, status.index);
  if (detach_change.is_user_action()) {
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
    case WebStateListChange::Type::kDetach:
      // Do nothing when a WebState is detached.
      break;
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
  }
}
