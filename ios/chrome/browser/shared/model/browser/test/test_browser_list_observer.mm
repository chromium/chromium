// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/model/browser/test/test_browser_list_observer.h"

#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser/browser_list.h"

namespace {

std::set<raw_ptr<Browser>> BrowsersOfType(const BrowserList* browser_list,
                                          BrowserList::BrowserType type) {
  std::set<raw_ptr<Browser>> browsers;
  for (Browser* browser : browser_list->BrowsersOfType(type)) {
    browsers.insert(browser);
  }
  return browsers;
}

}  // namespace

TestBrowserListObserver::TestBrowserListObserver() = default;

TestBrowserListObserver::~TestBrowserListObserver() = default;

void TestBrowserListObserver::OnBrowserAdded(const BrowserList* browser_list,
                                             Browser* browser) {
  CHECK_NE(browser->type(), Browser::Type::kTemporary);
  if (browser->type() == Browser::Type::kRegular ||
      browser->type() == Browser::Type::kInactive) {
    last_added_browser_ = browser;
    last_browsers_ = BrowsersOfType(
        browser_list, BrowserList::BrowserType::kRegularAndInactive);
  } else {
    last_added_incognito_browser_ = browser;
    last_incognito_browsers_ =
        BrowsersOfType(browser_list, BrowserList::BrowserType::kIncognito);
  }
}

void TestBrowserListObserver::OnBrowserRemoved(const BrowserList* browser_list,
                                               Browser* browser) {
  CHECK_NE(browser->type(), Browser::Type::kTemporary);
  if (browser->type() == Browser::Type::kRegular ||
      browser->type() == Browser::Type::kInactive) {
    last_removed_browser_ = browser;
    last_browsers_ = BrowsersOfType(
        browser_list, BrowserList::BrowserType::kRegularAndInactive);
  } else {
    last_removed_incognito_browser_ = browser;
    last_incognito_browsers_ =
        BrowsersOfType(browser_list, BrowserList::BrowserType::kIncognito);
  }
}

void TestBrowserListObserver::OnBrowserListShutdown(BrowserList* browser_list) {
  browser_list->RemoveObserver(this);
}
