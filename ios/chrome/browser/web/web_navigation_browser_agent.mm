// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/web/web_navigation_browser_agent.h"

#import "ios/chrome/browser/web/web_navigation_ntp_delegate.h"
#import "ios/chrome/browser/web/web_navigation_util.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/web/public/web_state.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

BROWSER_USER_DATA_KEY_IMPL(WebNavigationBrowserAgent)

WebNavigationBrowserAgent::WebNavigationBrowserAgent(Browser* browser)
    : web_state_list_(browser->GetWebStateList()) {}

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
    // |check_for_repost| is true because the reload is explicitly initiated
    // by the user.
    web_state_list_->GetActiveWebState()->GetNavigationManager()->Reload(
        web::ReloadType::NORMAL, true /* check_for_repost */);
  }
}

void WebNavigationBrowserAgent::SetDelegate(
    id<WebNavigationNTPDelegate> delegate) {
  delegate_ = delegate;
}
