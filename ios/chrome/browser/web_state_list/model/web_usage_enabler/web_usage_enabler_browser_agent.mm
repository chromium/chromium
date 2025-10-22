// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/web_state_list/model/web_usage_enabler/web_usage_enabler_browser_agent.h"

#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/web_state_list/model/web_usage_enabler/web_usage_enabler_browser_agent_observer.h"
#import "ios/web/public/navigation/navigation_manager.h"

WebUsageEnablerBrowserAgent::WebUsageEnablerBrowserAgent(Browser* browser)
    : BrowserUserData(browser) {
  StartObserving(browser, Policy::kOnlyRealized);
}

WebUsageEnablerBrowserAgent::~WebUsageEnablerBrowserAgent() {
  StopObserving();
}

bool WebUsageEnablerBrowserAgent::IsWebUsageEnabled() const {
  return web_usage_enabled_;
}

void WebUsageEnablerBrowserAgent::SetWebUsageEnabled(bool web_usage_enabled) {
  if (web_usage_enabled_ == web_usage_enabled) {
    return;
  }

  web_usage_enabled_ = web_usage_enabled;
  UpdateWebUsageForAllWebStates();
  for (auto& observer : observers_) {
    observer.WebUsageEnablerValueChanged(this);
  }
}

void WebUsageEnablerBrowserAgent::AddObserver(
    WebUsageEnablerBrowserAgentObserver* observer) {
  observers_.AddObserver(observer);
}

void WebUsageEnablerBrowserAgent::RemoveObserver(
    WebUsageEnablerBrowserAgentObserver* observer) {
  observers_.RemoveObserver(observer);
}

void WebUsageEnablerBrowserAgent::UpdateWebUsageForAllWebStates() {
  WebStateList* web_state_list = browser_->GetWebStateList();
  for (int index = 0; index < web_state_list->count(); ++index) {
    web::WebState* web_state = web_state_list->GetWebStateAt(index);
    if (web_state->IsRealized()) {
      UpdateWebUsageForAddedWebState(web_state, LoadPolicy::kDoNothing);
    }
  }
}

void WebUsageEnablerBrowserAgent::UpdateWebUsageForAddedWebState(
    web::WebState* web_state,
    LoadPolicy policy) {
  CHECK(web_state->IsRealized());
  web_state->SetWebUsageEnabled(web_usage_enabled_);
  if (web_usage_enabled_ && policy == LoadPolicy::kLoadIfNecessary) {
    web_state->GetNavigationManager()->LoadIfNecessary();
  }
}

#pragma mark - TabsDependencyInstaller

void WebUsageEnablerBrowserAgent::OnWebStateInserted(web::WebState* web_state) {
  UpdateWebUsageForAddedWebState(
      web_state, web_state == browser_->GetWebStateList()->GetActiveWebState()
                     ? LoadPolicy::kLoadIfNecessary
                     : LoadPolicy::kDoNothing);
}

void WebUsageEnablerBrowserAgent::OnWebStateRemoved(web::WebState* web_state) {
  // Nothing to do!
}

void WebUsageEnablerBrowserAgent::OnWebStateDeleted(web::WebState* web_state) {
  // Nothing to do!
}

void WebUsageEnablerBrowserAgent::OnActiveWebStateChanged(
    web::WebState* old_active,
    web::WebState* new_active) {
  // Nothing to do!
}
