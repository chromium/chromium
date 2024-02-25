// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TABS_MODEL_TAB_PARENTING_BROWSER_AGENT_H_
#define IOS_CHROME_BROWSER_TABS_MODEL_TAB_PARENTING_BROWSER_AGENT_H_

#import "ios/chrome/browser/shared/model/browser/browser_observer.h"
#import "ios/chrome/browser/shared/model/browser/browser_user_data.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list_observer.h"

class TabParentingBrowserAgent
    : public BrowserObserver,
      public BrowserUserData<TabParentingBrowserAgent>,
      public WebStateListObserver {
 public:
  ~TabParentingBrowserAgent() override;
  TabParentingBrowserAgent(const TabParentingBrowserAgent&) = delete;
  TabParentingBrowserAgent& operator=(const TabParentingBrowserAgent&) = delete;

  // BrowserObserver implementation.
  void BrowserDestroyed(Browser* browser) override;

  // WebStateListObserver implementation.
  void WebStateListDidChange(WebStateList* web_state_list,
                             const WebStateListChange& change,
                             const WebStateListStatus& status) override;

 private:
  friend class BrowserUserData<TabParentingBrowserAgent>;
  BROWSER_USER_DATA_KEY_DECL();

  explicit TabParentingBrowserAgent(Browser* browser);
};

#endif  // IOS_CHROME_BROWSER_TABS_MODEL_TAB_PARENTING_BROWSER_AGENT_H_
