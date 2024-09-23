// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/model/browser/browser_list.h"

#import "base/check.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"

namespace {

// Returns whether a Browser's type matches `type`.
bool IsBrowserOfType(Browser* browser, BrowserList::BrowserType type) {
  switch (browser->type()) {
    case Browser::Type::kRegular:
      switch (type) {
        case BrowserList::BrowserType::kRegular:
        case BrowserList::BrowserType::kRegularAndInactive:
        case BrowserList::BrowserType::kAll:
          return true;

        case BrowserList::BrowserType::kInactive:
        case BrowserList::BrowserType::kIncognito:
          return false;
      }

    case Browser::Type::kIncognito:
      switch (type) {
        case BrowserList::BrowserType::kIncognito:
        case BrowserList::BrowserType::kAll:
          return true;

        case BrowserList::BrowserType::kRegular:
        case BrowserList::BrowserType::kRegularAndInactive:
        case BrowserList::BrowserType::kInactive:
          return false;
      }

    case Browser::Type::kInactive:
      switch (type) {
        case BrowserList::BrowserType::kInactive:
        case BrowserList::BrowserType::kRegularAndInactive:
        case BrowserList::BrowserType::kAll:
          return true;

        case BrowserList::BrowserType::kRegular:
        case BrowserList::BrowserType::kIncognito:
          return false;
      }

    case Browser::Type::kTemporary:
      return false;
  }
}

}  // namespace

BrowserList::BrowserList() = default;

BrowserList::~BrowserList() {
  for (auto& observer : observers_) {
    observer.OnBrowserListShutdown(this);
  }
  for (Browser* browser : browsers_) {
    browser->RemoveObserver(this);
  }
}

void BrowserList::BrowserDestroyed(Browser* browser) {
  RemoveBrowser(browser);
  browser->RemoveObserver(this);
}

void BrowserList::AddBrowser(Browser* browser) {
  CHECK(!browsers_.contains(browser)) << "cannot insert the same Browser twice";
  browsers_.insert(browser);
  browser->AddObserver(this);
  for (auto& observer : observers_) {
    observer.OnBrowserAdded(this, browser);
  }
}

void BrowserList::RemoveBrowser(Browser* browser) {
  auto iter = browsers_.find(browser);
  if (iter != browsers_.end()) {
    browsers_.erase(iter);
    browser->RemoveObserver(this);
    for (auto& observer : observers_) {
      observer.OnBrowserRemoved(this, browser);
    }
  }
}

std::set<Browser*> BrowserList::BrowsersOfType(
    BrowserList::BrowserType type) const {
  std::set<Browser*> browsers;
  base::ranges::copy_if(
      browsers_, std::inserter(browsers, browsers.end()),
      [type](Browser* browser) { return IsBrowserOfType(browser, type); });
  return browsers;
}

void BrowserList::AddObserver(BrowserListObserver* observer) {
  observers_.AddObserver(observer);
}

void BrowserList::RemoveObserver(BrowserListObserver* observer) {
  observers_.RemoveObserver(observer);
}
