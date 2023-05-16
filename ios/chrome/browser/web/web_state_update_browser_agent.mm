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
  web_state_list_observation_.Observe(web_state_list_);

  // All the BrowserAgent are attached to the Browser during the creation,
  // the WebStateList must be empty at this point.
  DCHECK(web_state_list_->empty())
      << "WebStateUpdateBrowserAgent created for a Browser with a non-empty "
         "WebStateList.";
}

WebStateUpdateBrowserAgent::~WebStateUpdateBrowserAgent() {}

#pragma mark - Public

void WebStateUpdateBrowserAgent::UpdateWebStateScrollViewOffset(
    CGFloat toolbar_height) {
  if (!web_state_list_) {
    return;
  }
  for (int index = 0; index < web_state_list_->count(); ++index) {
    web::WebState* web_state = web_state_list_->GetWebStateAt(index);
    CRWWebViewScrollViewProxy* scroll_proxy =
        web_state->GetWebViewProxy().scrollViewProxy;
    CGPoint scroll_offset = scroll_proxy.contentOffset;
    scroll_offset.y += toolbar_height;
    scroll_proxy.contentOffset = scroll_offset;
  }
}

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

void WebStateUpdateBrowserAgent::WebStateListDestroyed(
    WebStateList* web_state_list) {
  // Stop observing web state list.
  web_state_list_observation_.Reset();
}
