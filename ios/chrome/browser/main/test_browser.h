// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_MAIN_TEST_BROWSER_H_
#define IOS_CHROME_BROWSER_MAIN_TEST_BROWSER_H_

#include "ios/chrome/browser/main/browser.h"

#include <CoreFoundation/CoreFoundation.h>

#include "base/observer_list.h"

class WebStateListDelegate;

class TestBrowser : public Browser {
 public:
  // Constructor that takes a WebStateListDelegate.
  TestBrowser(ChromeBrowserState* browser_state,
              std::unique_ptr<WebStateListDelegate> web_state_list_delegate);

  // Constructor that takes only a BrowserState; an empty web state list will be
  // created internally.
  TestBrowser(ChromeBrowserState* browser_state);

  // Constructor that creates a test browser state and an empty web state list.
  // Test fixtures will need to include a base::test::TaskEnvironment member if
  // this constructor is used (since it creates a TestChromeBrowserState that
  // requires a task environment).
  TestBrowser();

  TestBrowser(const TestBrowser&) = delete;
  TestBrowser& operator=(const TestBrowser&) = delete;

  ~TestBrowser() override;

  // Browser.
  ChromeBrowserState* GetBrowserState() override;
  WebStateList* GetWebStateList() override;
  CommandDispatcher* GetCommandDispatcher() override;
  void AddObserver(BrowserObserver* observer) override;
  void RemoveObserver(BrowserObserver* observer) override;

 private:
  ChromeBrowserState* browser_state_ = nullptr;
  // Used when the test browser creates and owns its own browser state.
  std::unique_ptr<ChromeBrowserState> owned_browser_state_;
  std::unique_ptr<WebStateListDelegate> web_state_list_delegate_;
  std::unique_ptr<WebStateList> web_state_list_;
  __strong CommandDispatcher* command_dispatcher_ = nil;
  base::ObserverList<BrowserObserver, /* check_empty= */ true> observers_;
};

#endif  // IOS_CHROME_BROWSER_MAIN_TEST_BROWSER_H_
