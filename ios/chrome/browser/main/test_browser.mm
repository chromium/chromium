// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/main/test_browser.h"

#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#include "ios/chrome/browser/main/browser_observer.h"
#import "ios/chrome/browser/ui/commands/command_dispatcher.h"
#include "ios/chrome/browser/web_state_list/web_state_list.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

TestBrowser::TestBrowser(ChromeBrowserState* browser_state,
                         WebStateList* web_state_list)
    : command_dispatcher_([[CommandDispatcher alloc] init]),
      browser_state_(browser_state),
      web_state_list_(web_state_list) {}

TestBrowser::TestBrowser(ChromeBrowserState* browser_state)
    : command_dispatcher_([[CommandDispatcher alloc] init]),
      browser_state_(browser_state) {
  owned_web_state_list_ =
      std::make_unique<WebStateList>(&web_state_list_delegate_);
  web_state_list_ = owned_web_state_list_.get();
}

TestBrowser::TestBrowser()
    : command_dispatcher_([[CommandDispatcher alloc] init]) {
  // Production code creates the browser state before the WebStateList, so
  // that ordering is preserved here.
  TestChromeBrowserState::Builder test_cbs_builder;
  owned_browser_state_ = test_cbs_builder.Build();
  browser_state_ = owned_browser_state_.get();

  owned_web_state_list_ =
      std::make_unique<WebStateList>(&web_state_list_delegate_);
  web_state_list_ = owned_web_state_list_.get();
}

TestBrowser::~TestBrowser() {
  for (auto& observer : observers_) {
    observer.BrowserDestroyed(this);
  }
}

#pragma mark - Browser

ChromeBrowserState* TestBrowser::GetBrowserState() const {
  return browser_state_;
}

WebStateList* TestBrowser::GetWebStateList() const {
  return web_state_list_;
}

CommandDispatcher* TestBrowser::GetCommandDispatcher() const {
  return command_dispatcher_;
}

void TestBrowser::AddObserver(BrowserObserver* observer) {
  observers_.AddObserver(observer);
}

void TestBrowser::RemoveObserver(BrowserObserver* observer) {
  observers_.RemoveObserver(observer);
}
