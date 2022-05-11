// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UPGRADE_UPGRADE_CENTER_BROWSER_AGENT_H_
#define IOS_CHROME_BROWSER_UPGRADE_UPGRADE_CENTER_BROWSER_AGENT_H_

#import "ios/chrome/browser/main/browser_observer.h"
#import "ios/chrome/browser/main/browser_user_data.h"
#import "ios/chrome/browser/web_state_list/web_state_list_observer.h"

class Browser;

// Browser agent that handles informing the update center of new tabs.
class UpgradeCenterBrowserAgent
    : public BrowserObserver,
      public WebStateListObserver,
      public BrowserUserData<UpgradeCenterBrowserAgent> {
 public:
  UpgradeCenterBrowserAgent(const UpgradeCenterBrowserAgent&) = delete;
  UpgradeCenterBrowserAgent& operator=(const UpgradeCenterBrowserAgent&) =
      delete;

  ~UpgradeCenterBrowserAgent() override;

 private:
  explicit UpgradeCenterBrowserAgent(Browser* browser);
  friend class BrowserUserData<UpgradeCenterBrowserAgent>;
  BROWSER_USER_DATA_KEY_DECL();

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

  Browser* browser_ = nullptr;
};

#endif  // IOS_CHROME_BROWSER_UPGRADE_UPGRADE_CENTER_BROWSER_AGENT_H_
