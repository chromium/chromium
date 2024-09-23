// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/web/model/web_navigation_browser_agent.h"

#import "components/feature_engagement/public/event_constants.h"
#import "components/feature_engagement/public/tracker.h"
#import "ios/chrome/browser/feature_engagement/model/tracker_factory.h"
#import "ios/chrome/browser/lens/model/lens_browser_agent.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/lens_commands.h"
#import "ios/chrome/browser/web/model/web_navigation_ntp_delegate.h"
#import "ios/chrome/browser/web/model/web_navigation_util.h"
#import "ios/web/common/user_agent.h"
#import "ios/web/public/navigation/navigation_item.h"
#import "ios/web/public/web_state.h"

BROWSER_USER_DATA_KEY_IMPL(WebNavigationBrowserAgent)

WebNavigationBrowserAgent::WebNavigationBrowserAgent(Browser* browser)
    : web_state_list_(browser->GetWebStateList()), browser_(browser) {}

WebNavigationBrowserAgent::~WebNavigationBrowserAgent() {}

bool WebNavigationBrowserAgent::CanGoBack(const web::WebState* web_state) {
  if (!web_state || !web_state->IsRealized()) {
    return false;
  }

  const web::NavigationManager* navigation_manager =
      web_state->GetNavigationManager();
  DCHECK(navigation_manager);
  if (navigation_manager->CanGoBack()) {
    return true;
  }

  const LensBrowserAgent* lens_browser_agent =
      LensBrowserAgent::FromBrowser(browser_);
  if (!lens_browser_agent) {
    return false;
  }

  return lens_browser_agent->CanGoBackToLensViewFinder();
}

bool WebNavigationBrowserAgent::CanGoBack() {
  const web::WebState* active_web_state = web_state_list_->GetActiveWebState();
  return WebNavigationBrowserAgent::CanGoBack(active_web_state);
}

bool WebNavigationBrowserAgent::CanGoForward(const web::WebState* web_state) {
  if (!web_state || !web_state->IsRealized()) {
    return false;
  }

  const web::NavigationManager* navigation_manager =
      web_state->GetNavigationManager();
  DCHECK(navigation_manager);
  return navigation_manager->CanGoForward();
}

bool WebNavigationBrowserAgent::CanGoForward() {
  const web::WebState* active_web_state = web_state_list_->GetActiveWebState();
  return WebNavigationBrowserAgent::CanGoForward(active_web_state);
}

void WebNavigationBrowserAgent::GoBack() {
  web::WebState* active_web_state = web_state_list_->GetActiveWebState();
  if (!active_web_state ||
      !active_web_state->GetNavigationManager()->CanGoBack()) {
    return;
  }

  web_navigation_util::GoBack(active_web_state);

  // If the previous page was an eligible Lens Web Page, then display the LVF.
  const LensBrowserAgent* lens_browser_agent =
      LensBrowserAgent::FromBrowser(browser_);
  if (lens_browser_agent && lens_browser_agent->CanGoBackToLensViewFinder()) {
    lens_browser_agent->GoBackToLensViewFinder();
  }
}

void WebNavigationBrowserAgent::GoForward() {
  web::WebState* active_web_state = web_state_list_->GetActiveWebState();
  if (active_web_state)
    web_navigation_util::GoForward(active_web_state);
}

void WebNavigationBrowserAgent::StopLoading() {
  web::WebState* active_web_state = web_state_list_->GetActiveWebState();
  if (active_web_state)
    active_web_state->Stop();
}

void WebNavigationBrowserAgent::Reload() {
  web::WebState* active_web_state = web_state_list_->GetActiveWebState();
  if (!active_web_state)
    return;

  if (delegate_.NTPActiveForCurrentWebState) {
    [delegate_ reloadNTPForWebState:active_web_state];
  } else {
    // `check_for_repost` is true because the reload is explicitly initiated
    // by the user.
    active_web_state->GetNavigationManager()->Reload(
        web::ReloadType::NORMAL, true /* check_for_repost */);
  }
}

void WebNavigationBrowserAgent::SetDelegate(
    id<WebNavigationNTPDelegate> delegate) {
  delegate_ = delegate;
}

void WebNavigationBrowserAgent::RequestDesktopSite() {
  ReloadWithUserAgentType(web::UserAgentType::DESKTOP);

  feature_engagement::TrackerFactory::GetForProfile(browser_->GetProfile())
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
  web::NavigationItem* visible_item =
      web_state->GetNavigationManager()->GetVisibleItem();
  if (!visible_item) {
    return web::UserAgentType::NONE;
  }

  return visible_item->GetUserAgentType();
}
