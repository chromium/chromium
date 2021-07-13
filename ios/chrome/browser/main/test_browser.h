// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_MAIN_TEST_BROWSER_H_
#define IOS_CHROME_BROWSER_MAIN_TEST_BROWSER_H_

#include "ios/chrome/browser/main/browser.h"

#include <CoreFoundation/CoreFoundation.h>

#include "base/macros.h"
#include "base/observer_list.h"
#import "ios/chrome/browser/web_state_list/fake_web_state_list_delegate.h"

class TestBrowser : public Browser {
 public:
  // Constructor that takes a WebStateList.
  TestBrowser(ChromeBrowserState* browser_state, WebStateList* web_state_list);

  // Constructor that takes only a BrowserState; an empty web state list will be
  // created internally.
  TestBrowser(ChromeBrowserState* browser_state);

  // Constructor that creates a test browser state and an empty web state list.
  // Test fixtures will need to include a base::test::TaskEnvironment member if
  // this constructor is used (since it creates a TestChromeBrowserState that
  // requires a task environment).
  TestBrowser();

  ~TestBrowser() override;

  // Browser.
  ChromeBrowserState* GetBrowserState() const override;
  TabModel* GetTabModel() const override;
  WebStateList* GetWebStateList() const override;
  CommandDispatcher* GetCommandDispatcher() const override;
  void AddObserver(BrowserObserver* observer) override;
  void RemoveObserver(BrowserObserver* observer) override;

 private:
  // Used when the test browser creates and owns its own browser state.
  std::unique_ptr<ChromeBrowserState> owned_browser_state_;
  // Used when the test browser creates and owns its own web state list.
  std::unique_ptr<WebStateList> owned_web_state_list_;
  FakeWebStateListDelegate web_state_list_delegate_;
  // Used in all cases.
  __strong CommandDispatcher* command_dispatcher_ = nil;
  ChromeBrowserState* browser_state_ = nullptr;
  TabModel* tab_model_ = nil;
  WebStateList* web_state_list_ = nullptr;
  base::ObserverList<BrowserObserver, /* check_empty= */ true> observers_;

  DISALLOW_COPY_AND_ASSIGN(TestBrowser);
};

#endif  // IOS_CHROME_BROWSER_MAIN_TEST_BROWSER_H_
