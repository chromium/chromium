// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/model/browser/browser_list_impl.h"

#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"

BrowserListImpl::BrowserListImpl() {}

BrowserListImpl::~BrowserListImpl() {}

// KeyedService:
void BrowserListImpl::Shutdown() {
  for (auto& observer : observers_) {
    observer.OnBrowserListShutdown(this);
  }
  for (Browser* browser : browsers_) {
    browser->RemoveObserver(this);
  }
}

// BrowserList:
void BrowserListImpl::AddBrowser(Browser* browser) {
  CHECK(!browsers_.contains(browser)) << "cannot insert the same Browser twice";
  browsers_.insert(browser);
  browser->AddObserver(this);
  for (auto& observer : observers_) {
    observer.OnBrowserAdded(this, browser);
  }
}

void BrowserListImpl::RemoveBrowser(Browser* browser) {
  auto iter = browsers_.find(browser);
  if (iter != browsers_.end()) {
    browsers_.erase(iter);
    browser->RemoveObserver(this);
    for (auto& observer : observers_) {
      observer.OnBrowserRemoved(this, browser);
    }
  }
}

std::set<Browser*> BrowserListImpl::BrowsersOfType(
    int browser_type_mask) const {
  std::set<Browser*> browsers;
  base::ranges::copy_if(browsers_, std::inserter(browsers, browsers.end()),
                        [browser_type_mask](Browser* browser) {
                          switch (browser->type()) {
                            case Browser::Type::kRegular:
                              return browser_type_mask & BrowserType::kRegular;
                            case Browser::Type::kIncognito:
                              return browser_type_mask &
                                     BrowserType::kIncognito;
                            case Browser::Type::kInactive:
                              return browser_type_mask & BrowserType::kInactive;
                            case Browser::Type::kTemporary:
                              return 0;
                          }
                        });
  return browsers;
}

std::set<Browser*> BrowserListImpl::AllRegularBrowsers() const {
  return BrowsersOfType(BrowserType::kRegular | BrowserType::kInactive);
}

std::set<Browser*> BrowserListImpl::AllIncognitoBrowsers() const {
  return BrowsersOfType(BrowserType::kIncognito);
}

// Adds an observer to the model.
void BrowserListImpl::AddObserver(BrowserListObserver* observer) {
  observers_.AddObserver(observer);
}

// Removes an observer from the model.
void BrowserListImpl::RemoveObserver(BrowserListObserver* observer) {
  observers_.RemoveObserver(observer);
}

// BrowserObserver
void BrowserListImpl::BrowserDestroyed(Browser* browser) {
  RemoveBrowser(browser);
  browser->RemoveObserver(this);
}
