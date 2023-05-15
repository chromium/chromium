// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_MAIN_BROWSER_LIST_IMPL_H_
#define IOS_CHROME_BROWSER_MAIN_BROWSER_LIST_IMPL_H_

#include "base/observer_list.h"
#import "ios/chrome/browser/shared/model/browser/browser_list.h"
#include "ios/chrome/browser/shared/model/browser/browser_list_observer.h"
#import "ios/chrome/browser/shared/model/browser/browser_observer.h"

// The concrete implementation of BrowserList returned by the
// BrowserListFactory.
class BrowserListImpl : public BrowserList, public BrowserObserver {
 public:
  BrowserListImpl();

  // Not copyable or moveable.
  BrowserListImpl(const BrowserListImpl&) = delete;
  BrowserListImpl& operator=(const BrowserListImpl&) = delete;

  ~BrowserListImpl() override;

  // KeyedService
  void Shutdown() override;

  // BrowserList
  void AddBrowser(Browser* browser) override;
  void AddIncognitoBrowser(Browser* browser) override;
  void RemoveBrowser(Browser* browser) override;
  void RemoveIncognitoBrowser(Browser* browser) override;
  std::set<Browser*> AllRegularBrowsers() const override;
  std::set<Browser*> AllIncognitoBrowsers() const override;
  void AddObserver(BrowserListObserver* observer) override;
  void RemoveObserver(BrowserListObserver* observer) override;

  // BrowserObserver:
  void BrowserDestroyed(Browser* browser) override;

 private:
  std::set<Browser*> browsers_;
  std::set<Browser*> incognito_browsers_;

  base::ObserverList<BrowserListObserver, true> observers_;
};

#endif  // IOS_CHROME_BROWSER_MAIN_BROWSER_LIST_IMPL_H_
