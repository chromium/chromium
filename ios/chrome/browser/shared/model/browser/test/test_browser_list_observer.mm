// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/model/browser/test/test_browser_list_observer.h"

#import "ios/chrome/browser/shared/model/browser/browser_list.h"

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

void TestBrowserListObserver::OnBrowserListShutdown(BrowserList* browser_list) {
  browser_list->RemoveObserver(this);
}
