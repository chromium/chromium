// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_MAIN_BROWSER_IMPL_H_
#define IOS_CHROME_BROWSER_MAIN_BROWSER_IMPL_H_

#include <CoreFoundation/CoreFoundation.h>

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/observer_list.h"
#import "ios/chrome/browser/main/browser.h"

class ChromeBrowserState;
@class SceneState;
class WebStateList;
class WebStateListDelegate;

// Browser is the model for a window containing multiple tabs. Instances
// are owned by a BrowserList to allow multiple windows for a single user
// session.
//
// See src/docs/ios/objects.md for more information.
class BrowserImpl : public Browser {
 public:
  // Constructs a BrowserImpl attached to |browser_state|.
  BrowserImpl(ChromeBrowserState* browser_state);
  // Creates a The tab Model, this method has to be called for the tabmodel to
  // exist. Tab Model can't be created on the constructor as it depends on
  // browser agents.
  void CreateTabModel();
  ~BrowserImpl() override;

  // Browser.
  ChromeBrowserState* GetBrowserState() const override;
  WebStateList* GetWebStateList() const override;
  CommandDispatcher* GetCommandDispatcher() const override;
  void AddObserver(BrowserObserver* observer) override;
  void RemoveObserver(BrowserObserver* observer) override;

 private:
  // Exposed to allow unittests to inject a WebStateList
  FRIEND_TEST_ALL_PREFIXES(BrowserImplTest, TestAccessors);
  BrowserImpl(ChromeBrowserState* browser_state,
              std::unique_ptr<WebStateList> web_state_list);

  ChromeBrowserState* browser_state_;
  std::unique_ptr<WebStateListDelegate> web_state_list_delegate_;
  std::unique_ptr<WebStateList> web_state_list_;
  __strong CommandDispatcher* command_dispatcher_;
  base::ObserverList<BrowserObserver, /* check_empty= */ true> observers_;

  DISALLOW_COPY_AND_ASSIGN(BrowserImpl);
};

#endif  // IOS_CHROME_BROWSER_MAIN_BROWSER_IMPL_H_
