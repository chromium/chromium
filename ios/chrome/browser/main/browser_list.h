// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_MAIN_BROWSER_LIST_H_
#define IOS_CHROME_BROWSER_MAIN_BROWSER_LIST_H_

#include <set>

#include "components/keyed_service/core/keyed_service.h"

class BrowserListObserver;
class Browser;

// An observable KeyedService which tracks the addition and removal of Browsers
// which use the owning BrowserState. The object creating browsers is
// responsible for informing the service of their addition; Browsers themselves
// don't know anything about this service.
//
// This service doesn't modify the lifetimes of Browser objects; it keeps (and
// vends) only weak pointers to them.
//
// There's a single service instance for both regular and OTR browser states;
// fetching the service for the OTR browser state will return the regular
// browser state's service instance.
class BrowserList : public KeyedService {
 public:
  explicit BrowserList() = default;

  BrowserList(const BrowserList&) = delete;
  BrowserList& operator=(const BrowserList&) = delete;

  // Adds a regular browser to the list. It's an error to add an incognito
  // browser with this method.
  virtual void AddBrowser(Browser* browser) = 0;

  // Adds an incognoito browser to the list.It's an error to add a regular
  // (non-incognito) browser with this method.
  virtual void AddIncognitoBrowser(Browser* browser) = 0;

  // Removes a regular browser from the list. Removing any browser not
  // previously added is a no-op; observers are not informed. This includes
  // calling RemoveBrowser() with an incognito browser.
  virtual void RemoveBrowser(Browser* browser) = 0;

  // Removes an incognito browser from the list. Removing any incognito browser
  // not previously added is a no-op; observers are not informed. This includes
  // calling RemoveIncognitoBrowser() with a regular browser.
  virtual void RemoveIncognitoBrowser(Browser* browser) = 0;

  // Returns the current set of regular browsers in the list.
  virtual std::set<Browser*> AllRegularBrowsers() const = 0;

  // Returns the current set of incognito browsers in the list.
  virtual std::set<Browser*> AllIncognitoBrowsers() const = 0;

  // Adds an observer to the service.
  virtual void AddObserver(BrowserListObserver* observer) = 0;

  // Removes an observer from the service. The service must have no observers
  // when it is destroyed.
  virtual void RemoveObserver(BrowserListObserver* observer) = 0;

  // true if this service has been shut down.
  virtual bool IsShutdown() = 0;
};

#endif  // IOS_CHROME_BROWSER_MAIN_BROWSER_LIST_H_
