// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/main/test_browser.h"

#include "ios/chrome/browser/main/browser_observer.h"
#import "ios/chrome/browser/tabs/tab_model.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

TestBrowser::TestBrowser(ios::ChromeBrowserState* browser_state,
                         TabModel* tab_model)
    : browser_state_(browser_state),
      tab_model_(tab_model),
      web_state_list_(tab_model_.webStateList) {}

TestBrowser::TestBrowser(ios::ChromeBrowserState* browser_state,
                         WebStateList* web_state_list)
    : browser_state_(browser_state), web_state_list_(web_state_list) {}

TestBrowser::~TestBrowser() {
  for (auto& observer : observers_) {
    observer.BrowserDestroyed(this);
  }
}

ios::ChromeBrowserState* TestBrowser::GetBrowserState() const {
  return browser_state_;
}

TabModel* TestBrowser::GetTabModel() const {
  return tab_model_;
}

WebStateList* TestBrowser::GetWebStateList() const {
  return web_state_list_;
}

void TestBrowser::AddObserver(BrowserObserver* observer) {
  observers_.AddObserver(observer);
}

void TestBrowser::RemoveObserver(BrowserObserver* observer) {
  observers_.RemoveObserver(observer);
}
