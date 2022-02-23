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

  // Constructor that takes only a BrowserState; a fake WebStateListDelegate
  // will be used.
  TestBrowser(ChromeBrowserState* browser_state);

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
  std::unique_ptr<WebStateListDelegate> web_state_list_delegate_;
  std::unique_ptr<WebStateList> web_state_list_;
  __strong CommandDispatcher* command_dispatcher_ = nil;
  base::ObserverList<BrowserObserver, /* check_empty= */ true> observers_;
};

#endif  // IOS_CHROME_BROWSER_MAIN_TEST_BROWSER_H_
