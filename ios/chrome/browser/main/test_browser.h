// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_MAIN_TEST_BROWSER_H_
#define IOS_CHROME_BROWSER_MAIN_TEST_BROWSER_H_

#include <CoreFoundation/CoreFoundation.h>

#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "ios/chrome/browser/main/browser.h"

class WebStateListDelegate;

class TestBrowser final : public Browser {
 public:
  // Constructor that takes a WebStateListDelegate.
  TestBrowser(ChromeBrowserState* browser_state,
              std::unique_ptr<WebStateListDelegate> web_state_list_delegate);

  // Constructor that takes only a BrowserState; a fake WebStateListDelegate
  // will be used.
  TestBrowser(ChromeBrowserState* browser_state);

  TestBrowser(const TestBrowser&) = delete;
  TestBrowser& operator=(const TestBrowser&) = delete;

  ~TestBrowser() final;

  // Browser.
  ChromeBrowserState* GetBrowserState() final;
  WebStateList* GetWebStateList() final;
  CommandDispatcher* GetCommandDispatcher() final;
  void AddObserver(BrowserObserver* observer) final;
  void RemoveObserver(BrowserObserver* observer) final;
  base::WeakPtr<Browser> AsWeakPtr() final;
  bool IsInactive() const final;

 private:
  ChromeBrowserState* browser_state_ = nullptr;
  std::unique_ptr<WebStateListDelegate> web_state_list_delegate_;
  std::unique_ptr<WebStateList> web_state_list_;
  __strong CommandDispatcher* command_dispatcher_ = nil;
  base::ObserverList<BrowserObserver, /* check_empty= */ true> observers_;

  // Needs to be the last member field to ensure all weak pointers are
  // invalidated before the other internal objects are destroyed.
  base::WeakPtrFactory<Browser> weak_ptr_factory_{this};
};

#endif  // IOS_CHROME_BROWSER_MAIN_TEST_BROWSER_H_
