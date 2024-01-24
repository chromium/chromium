// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DEVICE_SHARING_MODEL_DEVICE_SHARING_BROWSER_AGENT_H_
#define IOS_CHROME_BROWSER_DEVICE_SHARING_MODEL_DEVICE_SHARING_BROWSER_AGENT_H_

#import "base/memory/raw_ptr.h"
#import "ios/chrome/browser/shared/model/browser/browser_observer.h"
#import "ios/chrome/browser/shared/model/browser/browser_user_data.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list_observer.h"
#import "ios/web/public/web_state_observer.h"

class ActiveWebStateObservationForwarder;
class Browser;

// A browser agent (Brower-scoped model extension) that monitors the browser's
// web states for activations and navigations and updates the assoicated
// DeviceSharingService as the active URL changes. For browsers with incognito
// browser states, all updates clear the shared URL.
class DeviceSharingBrowserAgent
    : public BrowserUserData<DeviceSharingBrowserAgent>,
      public WebStateListObserver,
      public BrowserObserver,
      public web::WebStateObserver {
 public:
  // Not copyable or moveable
  DeviceSharingBrowserAgent(const DeviceSharingBrowserAgent&) = delete;
  DeviceSharingBrowserAgent& operator=(const DeviceSharingBrowserAgent&) =
      delete;
  ~DeviceSharingBrowserAgent() override;

  // Tell the device sharing manager that this is the active browser.
  void UpdateForActiveBrowser();

 private:
  friend class BrowserUserData<DeviceSharingBrowserAgent>;
  BROWSER_USER_DATA_KEY_DECL();

  explicit DeviceSharingBrowserAgent(Browser* browser);

  // Update the active URL for the current web state of the browser.
  void UpdateForActiveWebState(web::WebState* active_web_state);

  // WebStateListObserver
  void WebStateListDidChange(WebStateList* web_state_list,
                             const WebStateListChange& change,
                             const WebStateListStatus& status) override;

  // BrowserObserver
  void BrowserDestroyed(Browser* browser) override;

  // web::WebStateObserver
  void DidFinishNavigation(web::WebState* web_state,
                           web::NavigationContext* navigation_context) override;

  // web::WebStateObserver
  void TitleWasSet(web::WebState* web_state) override;

  // The Browser this agent is associated with.
  raw_ptr<Browser> browser_;
  // Whether the browser state associated with `browser_` is incognito or not.
  const bool is_incognito_ = true;
  // Observer for the active web state in `browser_`'s browser list.
  std::unique_ptr<ActiveWebStateObservationForwarder>
      active_web_state_observer_;
};

#endif  // IOS_CHROME_BROWSER_DEVICE_SHARING_MODEL_DEVICE_SHARING_BROWSER_AGENT_H_
