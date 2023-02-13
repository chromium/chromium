// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/web/web_state_update_browser_agent.h"

#import "ios/web/public/ui/crw_web_view_proxy.h"
#import "ios/web/public/ui/crw_web_view_scroll_view_proxy.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

BROWSER_USER_DATA_KEY_IMPL(WebStateUpdateBrowserAgent)

WebStateUpdateBrowserAgent::WebStateUpdateBrowserAgent(Browser* browser)
    : web_state_list_(browser->GetWebStateList()) {
  browser_ = browser;
  browser_observation_.Observe(browser);
  web_state_list_observation_.Observe(web_state_list_);

  // All the BrowserAgent are attached to the Browser during the creation,
  // the WebStateList must be empty at this point.
  DCHECK(web_state_list_->empty())
      << "WebStateUpdateBrowserAgent created for a Browser with a non-empty "
         "WebStateList.";
}

WebStateUpdateBrowserAgent::~WebStateUpdateBrowserAgent() {}

#pragma mark - WebStateListObserver

void WebStateUpdateBrowserAgent::WebStateActivatedAt(
    WebStateList* web_state_list,
    web::WebState* old_web_state,
    web::WebState* new_web_state,
    int active_index,
    ActiveWebStateChangeReason reason) {
  // Inform the old web state that it is no longer visible.
  if (old_web_state) {
    old_web_state->WasHidden();
    old_web_state->SetKeepRenderProcessAlive(false);
  }
  if (new_web_state) {
    new_web_state->GetWebViewProxy().scrollViewProxy.clipsToBounds = NO;
  }
}

void WebStateUpdateBrowserAgent::WebStateDetachedAt(
    WebStateList* web_state_list,
    web::WebState* web_state,
    int index) {
  // Inform the detached web state that it is no longer visible.
  if (web_state->IsRealized()) {
    web_state->WasHidden();
    web_state->SetKeepRenderProcessAlive(false);
  }
}

#pragma mark - BrowserObserver

void WebStateUpdateBrowserAgent::BrowserDestroyed(Browser* browser) {
  DCHECK_EQ(browser, browser_);
  // Stop observing web state list.
  browser->GetWebStateList()->RemoveObserver(this);
  browser->RemoveObserver(this);
  web_state_observations_.RemoveAllObservations();
  web_state_list_observation_.Reset();
  browser_observation_.Reset();
}
