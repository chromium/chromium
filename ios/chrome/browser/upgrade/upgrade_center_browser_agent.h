// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UPGRADE_UPGRADE_CENTER_BROWSER_AGENT_H_
#define IOS_CHROME_BROWSER_UPGRADE_UPGRADE_CENTER_BROWSER_AGENT_H_

#import "ios/chrome/browser/shared/model/browser/browser_observer.h"
#import "ios/chrome/browser/shared/model/browser/browser_user_data.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list_observer.h"

class Browser;
@class UpgradeCenter;

// Browser agent that handles informing the update center of new tabs.
class UpgradeCenterBrowserAgent
    : public BrowserObserver,
      public WebStateListObserver,
      public BrowserUserData<UpgradeCenterBrowserAgent> {
 public:
  // Not copyable or moveable
  UpgradeCenterBrowserAgent(const UpgradeCenterBrowserAgent&) = delete;
  UpgradeCenterBrowserAgent& operator=(const UpgradeCenterBrowserAgent&) =
      delete;

  ~UpgradeCenterBrowserAgent() override;

 private:
  friend class BrowserUserData<UpgradeCenterBrowserAgent>;
  BROWSER_USER_DATA_KEY_DECL();

  UpgradeCenterBrowserAgent(Browser* browser, UpgradeCenter* upgradeCenter);

  // BrowserObserver methods
  void BrowserDestroyed(Browser* browser) override;

  // WebStateListObserver methods
  void WebStateInsertedAt(WebStateList* web_state_list,
                          web::WebState* web_state,
                          int index,
                          bool activating) override;

  void WillDetachWebStateAt(WebStateList* web_state_list,
                            web::WebState* web_state,
                            int index) override;

  __strong UpgradeCenter* upgrade_center_ = nullptr;
};

#endif  // IOS_CHROME_BROWSER_UPGRADE_UPGRADE_CENTER_BROWSER_AGENT_H_
