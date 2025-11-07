// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/device_sharing/model/device_sharing_browser_agent.h"

#import "base/check_deref.h"
#import "ios/chrome/browser/device_sharing/model/device_sharing_manager.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"

DeviceSharingBrowserAgent::DeviceSharingBrowserAgent(
    Browser* browser,
    DeviceSharingManager* manager)
    : BrowserUserData(browser), manager_(CHECK_DEREF(manager)) {
  web_state_list_observation_.Observe(browser_->GetWebStateList());
}

DeviceSharingBrowserAgent::~DeviceSharingBrowserAgent() {
  // Signal no active URL. If this is the active browser in the manager, then
  // no further updates will be sent, so until another browser becomes active,
  // there will be no active URL.
  manager_->ClearActiveUrl(browser_);
}

void DeviceSharingBrowserAgent::UpdateForActiveBrowser() {
  // Tell the manager that this is now the active browser, and update.
  manager_->SetActiveBrowser(browser_);
  UpdateForActiveWebState(browser_->GetWebStateList()->GetActiveWebState());
}

void DeviceSharingBrowserAgent::UpdateForActiveWebState(
    web::WebState* active_web_state) {
  if (browser_->type() == Browser::Type::kIncognito) {
    // For all events on an incognito browser, clear the active URL, ensuring
    // that no URL is shared.
    manager_->ClearActiveUrl(browser_);
    return;
  }

  if (active_web_state) {
    manager_->UpdateActiveUrl(browser_, active_web_state->GetVisibleURL());
    manager_->UpdateActiveTitle(browser_, active_web_state->GetTitle());
    return;
  }

  // Clear the the ative URL if no web state is active -- for example if the
  // web state list is empty.
  manager_->ClearActiveUrl(browser_);
}

#pragma mark - WebStateListObserver

void DeviceSharingBrowserAgent::WebStateListDidChange(
    WebStateList* web_state_list,
    const WebStateListChange& change,
    const WebStateListStatus& status) {
  if (!status.active_web_state_change()) {
    return;
  }

  // Update which WebState is observed.
  active_web_state_observation_.Reset();
  web::WebState* active_web_state = status.new_active_web_state;
  if (active_web_state) {
    active_web_state_observation_.Observe(active_web_state);
  }

  UpdateForActiveWebState(active_web_state);
}

#pragma mark - WebStateObserver

void DeviceSharingBrowserAgent::DidFinishNavigation(
    web::WebState* web_state,
    web::NavigationContext* navigation_context) {
  UpdateForActiveWebState(web_state);
}

void DeviceSharingBrowserAgent::TitleWasSet(web::WebState* web_state) {
  UpdateForActiveWebState(web_state);
}
