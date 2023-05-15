// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/web_state_list/web_usage_enabler/web_usage_enabler_browser_agent.h"

#import "ios/web/public/navigation/navigation_manager.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

BROWSER_USER_DATA_KEY_IMPL(WebUsageEnablerBrowserAgent)

WebUsageEnablerBrowserAgent::WebUsageEnablerBrowserAgent(Browser* browser)
    : browser_(browser) {
  browser_observation_.Observe(browser_);

  WebStateList* web_state_list = browser_->GetWebStateList();
  web_state_list_observation_.Observe(browser->GetWebStateList());

  // All the BrowserAgent are attached to the Browser during the creation,
  // the WebStateList must be empty at this point.
  DCHECK(web_state_list->empty()) << "WebUsageEnablerBrowserAgent created for "
                                     "a Browser with a non-empty WebStateList.";
}

WebUsageEnablerBrowserAgent::~WebUsageEnablerBrowserAgent() = default;

bool WebUsageEnablerBrowserAgent::IsWebUsageEnabled() const {
  return web_usage_enabled_;
}

void WebUsageEnablerBrowserAgent::SetWebUsageEnabled(bool web_usage_enabled) {
  if (web_usage_enabled_ == web_usage_enabled) {
    return;
  }

  web_usage_enabled_ = web_usage_enabled;
  UpdateWebUsageForAllWebStates();
}

void WebUsageEnablerBrowserAgent::UpdateWebUsageForAllWebStates() {
  WebStateList* web_state_list = browser_->GetWebStateList();
  for (int index = 0; index < web_state_list->count(); ++index) {
    web::WebState* web_state = web_state_list->GetWebStateAt(index);
    UpdateWebUsageForAddedWebState(web_state, /*triggers_initial_load=*/false);
  }
}

void WebUsageEnablerBrowserAgent::UpdateWebUsageForAddedWebState(
    web::WebState* web_state,
    bool triggers_initial_load) {
  if (web_state->IsRealized()) {
    web_state->SetWebUsageEnabled(web_usage_enabled_);
    if (web_usage_enabled_ && triggers_initial_load) {
      web_state->GetNavigationManager()->LoadIfNecessary();
    }
  } else if (!web_state_observations_.IsObservingSource(web_state)) {
    web_state_observations_.AddObservation(web_state);
  }
}

void WebUsageEnablerBrowserAgent::BrowserDestroyed(Browser* browser) {
  web_state_observations_.RemoveAllObservations();
  web_state_list_observation_.Reset();
  browser_observation_.Reset();
}

void WebUsageEnablerBrowserAgent::WebStateInsertedAt(
    WebStateList* web_state_list,
    web::WebState* web_state,
    int index,
    bool activating) {
  UpdateWebUsageForAddedWebState(web_state,
                                 /*triggers_initial_load=*/activating);
}

void WebUsageEnablerBrowserAgent::WebStateReplacedAt(
    WebStateList* web_state_list,
    web::WebState* old_web_state,
    web::WebState* new_web_state,
    int index) {
  if (web_state_observations_.IsObservingSource(old_web_state)) {
    web_state_observations_.RemoveObservation(old_web_state);
  }

  UpdateWebUsageForAddedWebState(new_web_state, /*triggers_initial_load=*/true);
}

void WebUsageEnablerBrowserAgent::WebStateDetachedAt(
    WebStateList* web_state_list,
    web::WebState* web_state,
    int index) {
  if (web_state_observations_.IsObservingSource(web_state)) {
    web_state_observations_.RemoveObservation(web_state);
  }
}

void WebUsageEnablerBrowserAgent::WebStateRealized(web::WebState* web_state) {
  UpdateWebUsageForAddedWebState(web_state, /*triggers_initial_load=*/false);
  web_state_observations_.RemoveObservation(web_state);
}

void WebUsageEnablerBrowserAgent::WebStateDestroyed(web::WebState* web_state) {
  web_state_observations_.RemoveObservation(web_state);
}
