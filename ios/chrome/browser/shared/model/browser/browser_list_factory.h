// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_MODEL_BROWSER_BROWSER_LIST_FACTORY_H_
#define IOS_CHROME_BROWSER_SHARED_MODEL_BROWSER_BROWSER_LIST_FACTORY_H_

#include "base/gtest_prod_util.h"
#include "base/no_destructor.h"
#include "components/keyed_service/ios/browser_state_keyed_service_factory.h"

class BrowserList;
class ChromeBrowserState;

// Keyed service factory for BrowserList.
// This factory returns the same instance for regular and OTR browser states.
class BrowserListFactory : public BrowserStateKeyedServiceFactory {
 public:
  // Convenience getter that typecasts the value returned to a
  // BrowserList.
  static BrowserList* GetForBrowserState(ChromeBrowserState* browser_state);
  // Getter for singleton instance.
  static BrowserListFactory* GetInstance();

  // Not copyable or moveable.
  BrowserListFactory(const BrowserListFactory&) = delete;
  BrowserListFactory& operator=(const BrowserListFactory&) = delete;

 private:
  friend class base::NoDestructor<BrowserListFactory>;

  BrowserListFactory();

  // BrowserStateKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
  web::BrowserState* GetBrowserStateToUse(
      web::BrowserState* context) const override;
};

#endif  // IOS_CHROME_BROWSER_SHARED_MODEL_BROWSER_BROWSER_LIST_FACTORY_H_
