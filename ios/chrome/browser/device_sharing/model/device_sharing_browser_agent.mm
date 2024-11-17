// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/device_sharing/model/device_sharing_browser_agent.h"

#import "ios/chrome/browser/device_sharing/model/device_sharing_manager.h"
#import "ios/chrome/browser/device_sharing/model/device_sharing_manager_factory.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/active_web_state_observation_forwarder.h"

BROWSER_USER_DATA_KEY_IMPL(DeviceSharingBrowserAgent)

DeviceSharingBrowserAgent::DeviceSharingBrowserAgent(Browser* browser)
    : browser_(browser),
      is_incognito_(browser->GetProfile()->IsOffTheRecord()),
      active_web_state_observer_(
          std::make_unique<ActiveWebStateObservationForwarder>(
              browser_->GetWebStateList(),
              this)) {
  browser_->AddObserver(this);
  browser_->GetWebStateList()->AddObserver(this);
}

DeviceSharingBrowserAgent::~DeviceSharingBrowserAgent() {}

void DeviceSharingBrowserAgent::UpdateForActiveBrowser() {
  // Tell the manager that this is now the active browser, and update.
  DeviceSharingManagerFactory::GetForProfile(browser_->GetProfile())
      ->SetActiveBrowser(browser_);
  UpdateForActiveWebState(browser_->GetWebStateList()->GetActiveWebState());
}

void DeviceSharingBrowserAgent::UpdateForActiveWebState(
    web::WebState* active_web_state) {
  DeviceSharingManager* manager =
      DeviceSharingManagerFactory::GetForProfile(browser_->GetProfile());
  if (is_incognito_) {
    // For all events on an incognito browser, clear the active URL, ensuring
    // that no URL is shared.
    manager->ClearActiveUrl(browser_);
    return;
  }

  if (active_web_state) {
    manager->UpdateActiveUrl(browser_, active_web_state->GetVisibleURL());
    manager->UpdateActiveTitle(browser_, active_web_state->GetTitle());
    return;
  }

  // Clear the the ative URL if no web state is active -- for example if the
  // web state list is empty.
  manager->ClearActiveUrl(browser_);
}

#pragma mark - WebStateListObserver

void DeviceSharingBrowserAgent::WebStateListDidChange(
    WebStateList* web_state_list,
    const WebStateListChange& change,
    const WebStateListStatus& status) {
  if (status.active_web_state_change()) {
    UpdateForActiveWebState(status.new_active_web_state);
  }
}

#pragma mark - BrowserObserver

void DeviceSharingBrowserAgent::BrowserDestroyed(Browser* browser) {
  DCHECK_EQ(browser, browser_);
  // Signal no active URL. If this is the active browser in the manager, then
  // no further updates will be sent, so until another browser becomes active,
  // there will be no active URL.
  DeviceSharingManagerFactory::GetForProfile(browser_->GetProfile())
      ->ClearActiveUrl(browser);
  // Unhook all observers.
  active_web_state_observer_.reset();
  browser->RemoveObserver(this);
  browser_->GetWebStateList()->RemoveObserver(this);
}

#pragma mark - WebStateObserver

void DeviceSharingBrowserAgent::DidFinishNavigation(
    web::WebState* web_state,
    web::NavigationContext* navigation_context) {
  UpdateForActiveWebState(browser_->GetWebStateList()->GetActiveWebState());
}

void DeviceSharingBrowserAgent::TitleWasSet(web::WebState* web_state) {
  UpdateForActiveWebState(browser_->GetWebStateList()->GetActiveWebState());
}
