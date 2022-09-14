// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_BROWSING_DATA_BROWSING_DATA_REMOVER_FACTORY_H_
#define IOS_CHROME_BROWSER_BROWSING_DATA_BROWSING_DATA_REMOVER_FACTORY_H_

#include <memory>

#include "base/no_destructor.h"
#include "components/keyed_service/ios/browser_state_keyed_service_factory.h"

class BrowsingDataRemover;
class ChromeBrowserState;

// Singleton that owns all BrowsingDataRemovers and associates them with
// ChromeBrowserState.
class BrowsingDataRemoverFactory : public BrowserStateKeyedServiceFactory {
 public:
  static BrowsingDataRemover* GetForBrowserState(
      ChromeBrowserState* browser_state);
  static BrowsingDataRemover* GetForBrowserStateIfExists(
      ChromeBrowserState* browser_state);
  static BrowsingDataRemoverFactory* GetInstance();

  BrowsingDataRemoverFactory(const BrowsingDataRemoverFactory&) = delete;
  BrowsingDataRemoverFactory& operator=(const BrowsingDataRemoverFactory&) =
      delete;

 private:
  friend class base::NoDestructor<BrowsingDataRemoverFactory>;

  BrowsingDataRemoverFactory();
  ~BrowsingDataRemoverFactory() override;

  // BrowserStateKeyedServiceFactory implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
  web::BrowserState* GetBrowserStateToUse(
      web::BrowserState* context) const override;
};

#endif  // IOS_CHROME_BROWSER_BROWSING_DATA_BROWSING_DATA_REMOVER_FACTORY_H_
