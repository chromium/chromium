// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/main/test_browser_list_observer.h"

#import "ios/chrome/browser/main/browser_list.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

TestBrowserListObserver::TestBrowserListObserver() = default;

TestBrowserListObserver::~TestBrowserListObserver() = default;

void TestBrowserListObserver::OnBrowserAdded(const BrowserList* browser_list,
                                             Browser* browser) {
  last_added_browser_ = browser;
  last_browsers_ = browser_list->AllRegularBrowsers();
}

void TestBrowserListObserver::OnIncognitoBrowserAdded(
    const BrowserList* browser_list,
    Browser* browser) {
  last_added_incognito_browser_ = browser;
  last_incognito_browsers_ = browser_list->AllIncognitoBrowsers();
}

void TestBrowserListObserver::OnBrowserRemoved(const BrowserList* browser_list,
                                               Browser* browser) {
  last_removed_browser_ = browser;
  last_browsers_ = browser_list->AllRegularBrowsers();
}

void TestBrowserListObserver::OnIncognitoBrowserRemoved(
    const BrowserList* browser_list,
    Browser* browser) {
  last_removed_incognito_browser_ = browser;
  last_incognito_browsers_ = browser_list->AllIncognitoBrowsers();
}
