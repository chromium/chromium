// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_MAIN_BROWSER_IMPL_H_
#define IOS_CHROME_BROWSER_MAIN_BROWSER_IMPL_H_

#import "ios/chrome/browser/main/browser.h"

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/observer_list.h"

@class TabModel;
class WebStateList;
class WebStateListDelegate;

namespace ios {
class ChromeBrowserState;
}

// Browser is the model for a window containing multiple tabs. Instances
// are owned by a BrowserList to allow multiple windows for a single user
// session.
//
// See src/docs/ios/objects.md for more information.
class BrowserImpl : public Browser {
 public:
  // Constructs a BrowserImpl attached to |browser_state|.
  explicit BrowserImpl(ios::ChromeBrowserState* browser_state);
  ~BrowserImpl() override;

  // Browser.
  ios::ChromeBrowserState* GetBrowserState() const override;
  TabModel* GetTabModel() const override;
  WebStateList* GetWebStateList() const override;
  void AddObserver(BrowserObserver* observer) override;
  void RemoveObserver(BrowserObserver* observer) override;

 private:
  // Exposed to allow unittests to inject a TabModel and WebStateList
  FRIEND_TEST_ALL_PREFIXES(BrowserImplTest, TestAccessors);
  BrowserImpl(ios::ChromeBrowserState* browser_state,
              TabModel* tab_model,
              std::unique_ptr<WebStateList> web_state_list);

  ios::ChromeBrowserState* browser_state_;
  __strong TabModel* tab_model_;
  std::unique_ptr<WebStateListDelegate> web_state_list_delegate_;
  std::unique_ptr<WebStateList> web_state_list_;
  base::ObserverList<BrowserObserver, /* check_empty= */ true> observers_;

  DISALLOW_COPY_AND_ASSIGN(BrowserImpl);
};

#endif  // IOS_CHROME_BROWSER_MAIN_BROWSER_IMPL_H_
