// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/main/browser_list_impl.h"

#import "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/main/browser.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

BrowserListImpl::BrowserListImpl() {}
BrowserListImpl::~BrowserListImpl() {
  observers_.Clear();
}

// KeyedService:
void BrowserListImpl::Shutdown() {
  for (auto& observer : observers_)
    observer.OnBrowserListShutdown(this);
  observers_.Clear();
  for (Browser* browser : browsers_) {
    browser->RemoveObserver(this);
  }
  for (Browser* browser : incognito_browsers_) {
    browser->RemoveObserver(this);
  }
  is_shutdown_ = true;
}

// BrowserList:
void BrowserListImpl::AddBrowser(Browser* browser) {
  DCHECK(!browser->GetBrowserState()->IsOffTheRecord());
  browsers_.insert(browser);
  browser->AddObserver(this);
  for (auto& observer : observers_)
    observer.OnBrowserAdded(this, browser);
}

void BrowserListImpl::AddIncognitoBrowser(Browser* browser) {
  DCHECK(browser->GetBrowserState()->IsOffTheRecord());
  incognito_browsers_.insert(browser);
  browser->AddObserver(this);
  for (auto& observer : observers_)
    observer.OnIncognitoBrowserAdded(this, browser);
}

void BrowserListImpl::RemoveBrowser(Browser* browser) {
  if (browsers_.erase(browser) > 0) {
    browser->RemoveObserver(this);
    for (auto& observer : observers_)
      observer.OnBrowserRemoved(this, browser);
  }
}

void BrowserListImpl::RemoveIncognitoBrowser(Browser* browser) {
  if (incognito_browsers_.erase(browser) > 0) {
    browser->RemoveObserver(this);
    for (auto& observer : observers_)
      observer.OnIncognitoBrowserRemoved(this, browser);
  }
}

std::set<Browser*> BrowserListImpl::AllRegularBrowsers() const {
  return browsers_;
}

std::set<Browser*> BrowserListImpl::AllIncognitoBrowsers() const {
  return incognito_browsers_;
}

// Adds an observer to the model.
void BrowserListImpl::AddObserver(BrowserListObserver* observer) {
  observers_.AddObserver(observer);
}

// Removes an observer from the model.
void BrowserListImpl::RemoveObserver(BrowserListObserver* observer) {
  observers_.RemoveObserver(observer);
}

bool BrowserListImpl::IsShutdown() {
  return is_shutdown_;
}

// BrowserObserver
void BrowserListImpl::BrowserDestroyed(Browser* browser) {
  if (browser->GetBrowserState()->IsOffTheRecord()) {
    RemoveIncognitoBrowser(browser);
  } else {
    RemoveBrowser(browser);
  }
  browser->RemoveObserver(this);
}
