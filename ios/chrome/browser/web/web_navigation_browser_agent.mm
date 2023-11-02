// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/web/web_navigation_browser_agent.h"

#import "components/feature_engagement/public/event_constants.h"
#import "components/feature_engagement/public/tracker.h"
#import "ios/chrome/browser/feature_engagement/tracker_factory.h"
#import "ios/chrome/browser/web/web_navigation_ntp_delegate.h"
#import "ios/chrome/browser/web/web_navigation_util.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/web/common/user_agent.h"
#import "ios/web/public/navigation/navigation_item.h"
#import "ios/web/public/web_state.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

BROWSER_USER_DATA_KEY_IMPL(WebNavigationBrowserAgent)

WebNavigationBrowserAgent::WebNavigationBrowserAgent(Browser* browser)
    : web_state_list_(browser->GetWebStateList()), browser_(browser) {}

WebNavigationBrowserAgent::~WebNavigationBrowserAgent() {}

bool WebNavigationBrowserAgent::CanGoBack() {
  return web_state_list_->GetActiveWebState() &&
         web_state_list_->GetActiveWebState()
             ->GetNavigationManager()
             ->CanGoBack();
}

bool WebNavigationBrowserAgent::CanGoForward() {
  return web_state_list_->GetActiveWebState() &&
         web_state_list_->GetActiveWebState()
             ->GetNavigationManager()
             ->CanGoForward();
}

void WebNavigationBrowserAgent::GoBack() {
  if (web_state_list_->GetActiveWebState())
    web_navigation_util::GoBack(web_state_list_->GetActiveWebState());
}
void WebNavigationBrowserAgent::GoForward() {
  if (web_state_list_->GetActiveWebState())
    web_navigation_util::GoForward(web_state_list_->GetActiveWebState());
}

void WebNavigationBrowserAgent::StopLoading() {
  if (web_state_list_->GetActiveWebState())
    web_state_list_->GetActiveWebState()->Stop();
}

void WebNavigationBrowserAgent::Reload() {
  if (!web_state_list_->GetActiveWebState())
    return;

  if (delegate_.NTPActiveForCurrentWebState) {
    [delegate_ reloadNTPForWebState:web_state_list_->GetActiveWebState()];
  } else {
    // `check_for_repost` is true because the reload is explicitly initiated
    // by the user.
    web_state_list_->GetActiveWebState()->GetNavigationManager()->Reload(
        web::ReloadType::NORMAL, true /* check_for_repost */);
  }
}

void WebNavigationBrowserAgent::SetDelegate(
    id<WebNavigationNTPDelegate> delegate) {
  delegate_ = delegate;
}

void WebNavigationBrowserAgent::RequestDesktopSite() {
  ReloadWithUserAgentType(web::UserAgentType::DESKTOP);

  feature_engagement::TrackerFactory::GetForBrowserState(
      browser_->GetBrowserState())
      ->NotifyEvent(feature_engagement::events::kDesktopVersionRequested);
}

void WebNavigationBrowserAgent::RequestMobileSite() {
  ReloadWithUserAgentType(web::UserAgentType::MOBILE);
}

void WebNavigationBrowserAgent::ReloadWithUserAgentType(
    web::UserAgentType userAgentType) {
  web::WebState* web_state = web_state_list_->GetActiveWebState();
  if (UserAgentType(web_state) == userAgentType)
    return;

  web_state->GetNavigationManager()->ReloadWithUserAgentType(userAgentType);
}

web::UserAgentType WebNavigationBrowserAgent::UserAgentType(
    web::WebState* web_state) {
  if (!web_state) {
    return web::UserAgentType::NONE;
  }
  web::NavigationItem* visibleItem =
      web_state->GetNavigationManager()->GetVisibleItem();
  if (!visibleItem) {
    return web::UserAgentType::NONE;
  }

  return visibleItem->GetUserAgentType();
}
