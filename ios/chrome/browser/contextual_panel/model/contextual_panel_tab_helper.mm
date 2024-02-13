// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/contextual_panel/model/contextual_panel_tab_helper.h"

#import "ios/web/public/web_state.h"

ContextualPanelTabHelper::ContextualPanelTabHelper(web::WebState* web_state)
    : web_state_(web_state) {
  web_state_observation_.Observe(web_state_);
}

ContextualPanelTabHelper::~ContextualPanelTabHelper() = default;

WEB_STATE_USER_DATA_KEY_IMPL(ContextualPanelTabHelper)

#pragma mark - WebStateObserver

void ContextualPanelTabHelper::DidFinishNavigation(
    web::WebState* web_state,
    web::NavigationContext* navigation_context) {
  // Ask individual models for their data.
}

void ContextualPanelTabHelper::WebStateDestroyed(web::WebState* web_state) {
  DCHECK_EQ(web_state_, web_state);
  web_state_observation_.Reset();
  web_state_ = nullptr;
}
