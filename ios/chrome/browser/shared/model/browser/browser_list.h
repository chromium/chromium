// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_MODEL_BROWSER_BROWSER_LIST_H_
#define IOS_CHROME_BROWSER_SHARED_MODEL_BROWSER_BROWSER_LIST_H_

#include <set>

#include "base/observer_list.h"
#include "components/keyed_service/core/keyed_service.h"
#include "ios/chrome/browser/shared/model/browser/browser_list_observer.h"
#include "ios/chrome/browser/shared/model/browser/browser_observer.h"

class Browser;

// An observable KeyedService which tracks the addition and removal of Browsers
// which use the owning BrowserState. The object creating browsers is
// responsible for informing the service of their addition; Browsers themselves
// don't know anything about this service.
//
// This service doesn't modify the lifetimes of Browser objects; it keeps (and
// vends) only weak pointers to them.
//
// There's a single service instance for both regular and OTR profiles;
// fetching the service for the OTR profile will return the regular
// profile's service instance.
class BrowserList final : public KeyedService, public BrowserObserver {
 public:
  enum class BrowserType {
    kRegular,
    kInactive,
    kIncognito,
    kRegularAndInactive,
    kAll,
  };

  BrowserList();

  BrowserList(const BrowserList&) = delete;
  BrowserList& operator=(const BrowserList&) = delete;

  ~BrowserList() final;

  // BrowserObserver:
  void BrowserDestroyed(Browser* browser) final;

  // Adds `browser` to the registered Browser set. It is an error to add a
  // temporary Browser.
  void AddBrowser(Browser* browser);

  // Removes `browser` from the registered Browser set. It is a no-op to
  // remove a Browser that has not been previously registered (and in that
  // case the observers are not notified).
  void RemoveBrowser(Browser* browser);

  // Returns the current set of browsers in the list matching the `type`.
  std::set<Browser*> BrowsersOfType(BrowserType type) const;

  // Adds an observer to the service.
  void AddObserver(BrowserListObserver* observer);

  // Removes an observer from the service. The service must have no observers
  // when it is destroyed.
  void RemoveObserver(BrowserListObserver* observer);

 private:
  // The list of observers.
  base::ObserverList<BrowserListObserver, true> observers_;

  // The set of registered Browsers.
  std::set<Browser*> browsers_;
};

#endif  // IOS_CHROME_BROWSER_SHARED_MODEL_BROWSER_BROWSER_LIST_H_
