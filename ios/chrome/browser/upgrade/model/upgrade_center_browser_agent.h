// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UPGRADE_MODEL_UPGRADE_CENTER_BROWSER_AGENT_H_
#define IOS_CHROME_BROWSER_UPGRADE_MODEL_UPGRADE_CENTER_BROWSER_AGENT_H_

#import "base/scoped_multi_source_observation.h"
#import "base/scoped_observation.h"
#import "ios/chrome/browser/shared/model/browser/browser_user_data.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list_observer.h"
#import "ios/web/public/web_state.h"
#import "ios/web/public/web_state_observer.h"

class Browser;
@class UpgradeCenter;

// Browser agent that handles informing the update center of new tabs.
class UpgradeCenterBrowserAgent
    : public WebStateListObserver,
      public web::WebStateObserver,
      public BrowserUserData<UpgradeCenterBrowserAgent> {
 public:
  // Not copyable or moveable
  UpgradeCenterBrowserAgent(const UpgradeCenterBrowserAgent&) = delete;
  UpgradeCenterBrowserAgent& operator=(const UpgradeCenterBrowserAgent&) =
      delete;

  ~UpgradeCenterBrowserAgent() override;

  // WebStateListObserver methods
  void WebStateListDidChange(WebStateList* web_state_list,
                             const WebStateListChange& change,
                             const WebStateListStatus& status) override;
  void WebStateListDestroyed(WebStateList* web_state_list) override;

  // web::WebStateObserver methods
  void WebStateRealized(web::WebState* web_state) override;
  void WebStateDestroyed(web::WebState* web_state) override;

 private:
  friend class BrowserUserData<UpgradeCenterBrowserAgent>;
  BROWSER_USER_DATA_KEY_DECL();

  UpgradeCenterBrowserAgent(Browser* browser, UpgradeCenter* upgradeCenter);

  // Helper to register a newly inserted WebState.
  void WebStateAttached(web::WebState* web_state);

  // Helper to unregister a detached WebState.
  void WebStateDetached(web::WebState* web_state);

  // The UpgradeCenter used by this BrowserAgent.
  __strong UpgradeCenter* upgrade_center_ = nullptr;

  // Scoped observation of WebStateList.
  base::ScopedObservation<WebStateList, WebStateListObserver>
      web_state_list_observation_{this};

  // Scoped observation of WebStates.
  base::ScopedMultiSourceObservation<web::WebState, web::WebStateObserver>
      web_state_observations_{this};
};

#endif  // IOS_CHROME_BROWSER_UPGRADE_MODEL_UPGRADE_CENTER_BROWSER_AGENT_H_
