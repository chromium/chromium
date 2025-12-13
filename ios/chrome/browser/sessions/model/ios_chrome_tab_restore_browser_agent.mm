// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/sessions/model/ios_chrome_tab_restore_browser_agent.h"

#import "base/check.h"
#import "base/functional/bind.h"
#import "components/sessions/core/tab_restore_service.h"
#import "components/sessions/ios/ios_restore_live_tab.h"
#import "components/sessions/ios/ios_webstate_live_tab.h"
#import "ios/chrome/browser/sessions/model/ios_chrome_tab_restore_service_factory.h"
#import "ios/chrome/browser/sessions/model/session_restoration_service.h"
#import "ios/chrome/browser/sessions/model/session_restoration_service_factory.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "ios/web/public/session/proto/navigation.pb.h"
#import "ios/web/public/session/proto/storage.pb.h"
#import "ios/web/public/web_state.h"
#import "url/gurl.h"

IOSChromeTabRestoreBrowserAgent::~IOSChromeTabRestoreBrowserAgent() = default;

IOSChromeTabRestoreBrowserAgent::IOSChromeTabRestoreBrowserAgent(
    Browser* browser)
    : BrowserUserData(browser) {
  CHECK(!browser_->GetProfile()->IsOffTheRecord());
  web_state_list_observation_.Observe(browser_->GetWebStateList());
}

void IOSChromeTabRestoreBrowserAgent::RecordHistoryForWebState(
    int index,
    web::WebState* web_state) {
  // No need to record history if the tab has no navigation or has only
  // presented the NTP.
  if (web_state->GetNavigationItemCount() <= 1) {
    const GURL& url = web_state->GetLastCommittedURL();
    if (!url.is_valid() || url.host() == kChromeUINewTabHost) {
      return;
    }
  }

  ProfileIOS* profile = browser_->GetProfile();
  CHECK(profile);

  // If the WebState is still unrealized, ask the SessionRestorationService
  // to load the data from storage (it should exists otherwise the WebState
  // would have no navigation history).
  if (!web_state->IsRealized()) {
    SessionRestorationServiceFactory::GetForProfile(profile)
        ->LoadWebStateStorage(
            browser_, web_state,
            base::BindOnce(
                &IOSChromeTabRestoreBrowserAgent::RecordHistoryFromStorage,
                weak_ptr_factory_.GetWeakPtr(), index));
    return;
  }

  IOSChromeTabRestoreServiceFactory::GetForProfile(profile)
      ->CreateHistoricalTab(
          sessions::IOSWebStateLiveTab::GetForWebState(web_state), index);
}

void IOSChromeTabRestoreBrowserAgent::RecordHistoryFromStorage(
    int index,
    web::proto::WebStateStorage storage) {
  ProfileIOS* profile = browser_->GetProfile();
  CHECK(profile);

  sessions::RestoreIOSLiveTab live_tab(storage.navigation());
  IOSChromeTabRestoreServiceFactory::GetForProfile(profile)
      ->CreateHistoricalTab(&live_tab, index);
}

void IOSChromeTabRestoreBrowserAgent::WebStateListWillChange(
    WebStateList* web_state_list,
    const WebStateListChangeDetach& detach_change,
    const WebStateListStatus& status) {
  // Do not record history if the tabs is simply detached (i.e. moved
  // from between Browser) or if it is closed due to tabs cleanup.
  if (!detach_change.is_closing() || detach_change.is_tabs_cleanup()) {
    return;
  }

  RecordHistoryForWebState(detach_change.detached_from_index(),
                           detach_change.detached_web_state());
}
