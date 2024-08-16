// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/web/model/web_state_update_browser_agent.h"

#import "ios/web/public/ui/crw_web_view_proxy.h"
#import "ios/web/public/ui/crw_web_view_scroll_view_proxy.h"

BROWSER_USER_DATA_KEY_IMPL(WebStateUpdateBrowserAgent)

WebStateUpdateBrowserAgent::WebStateUpdateBrowserAgent(Browser* browser)
    : web_state_list_(browser->GetWebStateList()) {
  web_state_list_observation_.Observe(web_state_list_.get());

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

void WebStateUpdateBrowserAgent::WebStateListDidChange(
    WebStateList* web_state_list,
    const WebStateListChange& change,
    const WebStateListStatus& status) {
  if (change.type() == WebStateListChange::Type::kDetach) {
    // Inform the detached web state that it is no longer visible.
    const WebStateListChangeDetach& detach_change =
        change.As<WebStateListChangeDetach>();
    web::WebState* detached_web_state = detach_change.detached_web_state();
    if (detached_web_state->IsRealized()) {
      detached_web_state->WasHidden();
      detached_web_state->SetKeepRenderProcessAlive(false);
    }
  }

  if (status.active_web_state_change()) {
    // Inform the old web state that it is no longer visible.
    if (web::WebState* old_active = status.old_active_web_state;
        old_active && old_active->IsRealized()) {
      old_active->WasHidden();
      old_active->SetKeepRenderProcessAlive(false);
    }
    if (web::WebState* new_active = status.new_active_web_state;
        new_active && new_active->IsRealized()) {
      new_active->GetWebViewProxy().scrollViewProxy.clipsToBounds = NO;
    }
  }
}

void WebStateUpdateBrowserAgent::WebStateListDestroyed(
    WebStateList* web_state_list) {
  // Stop observing web state list.
  web_state_list_observation_.Reset();
}
