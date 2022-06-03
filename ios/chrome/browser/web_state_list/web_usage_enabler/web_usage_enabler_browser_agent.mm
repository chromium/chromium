// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/web_state_list/web_usage_enabler/web_usage_enabler_browser_agent.h"

#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "ios/web/public/web_state.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

BROWSER_USER_DATA_KEY_IMPL(WebUsageEnablerBrowserAgent)

WebUsageEnablerBrowserAgent::WebUsageEnablerBrowserAgent(Browser* browser)
    : browser_(browser) {
  browser_->AddObserver(this);
  browser_->GetWebStateList()->AddObserver(this);
  UpdateWebUsageForAllWebStates();
}

WebUsageEnablerBrowserAgent::~WebUsageEnablerBrowserAgent() {}

bool WebUsageEnablerBrowserAgent::IsWebUsageEnabled() const {
  return web_usage_enabled_;
}

void WebUsageEnablerBrowserAgent::SetWebUsageEnabled(bool web_usage_enabled) {
  if (web_usage_enabled_ == web_usage_enabled)
    return;
  web_usage_enabled_ = web_usage_enabled;
  UpdateWebUsageForAllWebStates();
}

bool WebUsageEnablerBrowserAgent::TriggersInitialLoad() const {
  return triggers_initial_load_;
}

void WebUsageEnablerBrowserAgent::SetTriggersInitialLoad(
    bool triggers_initial_load) {
  triggers_initial_load_ = triggers_initial_load;
}

void WebUsageEnablerBrowserAgent::UpdateWebUsageForAllWebStates() {
  if (!browser_)
    return;
  WebStateList* web_state_list = browser_->GetWebStateList();
  for (int index = 0; index < web_state_list->count(); ++index) {
    web::WebState* web_state = web_state_list->GetWebStateAt(index);
    web_state->SetWebUsageEnabled(web_usage_enabled_);
  }
}

void WebUsageEnablerBrowserAgent::UpdateWebUsageForAddedWebState(
    web::WebState* web_state) {
  web_state->SetWebUsageEnabled(web_usage_enabled_);
  if (web_usage_enabled_ && triggers_initial_load_)
    web_state->GetNavigationManager()->LoadIfNecessary();
}

void WebUsageEnablerBrowserAgent::BrowserDestroyed(Browser* browser) {
  browser_->GetWebStateList()->RemoveObserver(this);
  browser_->RemoveObserver(this);
}

void WebUsageEnablerBrowserAgent::WebStateInsertedAt(
    WebStateList* web_state_list,
    web::WebState* web_state,
    int index,
    bool activating) {
  UpdateWebUsageForAddedWebState(web_state);
}

void WebUsageEnablerBrowserAgent::WebStateReplacedAt(
    WebStateList* web_state_list,
    web::WebState* old_web_state,
    web::WebState* new_web_state,
    int index) {
  UpdateWebUsageForAddedWebState(new_web_state);
}
