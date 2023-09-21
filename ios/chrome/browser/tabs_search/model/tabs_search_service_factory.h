// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TABS_SEARCH_MODEL_TABS_SEARCH_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_TABS_SEARCH_MODEL_TABS_SEARCH_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "components/keyed_service/ios/browser_state_keyed_service_factory.h"

class ChromeBrowserState;
class TabsSearchService;

// Singleton that owns all TabsSearchServices and associates them with
// ChromeBrowserState.
class TabsSearchServiceFactory : public BrowserStateKeyedServiceFactory {
 public:
  TabsSearchServiceFactory(const TabsSearchServiceFactory&) = delete;
  TabsSearchServiceFactory& operator=(const TabsSearchServiceFactory&) = delete;

  static TabsSearchService* GetForBrowserState(
      ChromeBrowserState* browser_state);
  static TabsSearchServiceFactory* GetInstance();

 private:
  friend class base::NoDestructor<TabsSearchServiceFactory>;

  TabsSearchServiceFactory();
  ~TabsSearchServiceFactory() override;

  // BrowserStateKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
  web::BrowserState* GetBrowserStateToUse(
      web::BrowserState* context) const override;
};

#endif  // IOS_CHROME_BROWSER_TABS_SEARCH_MODEL_TABS_SEARCH_SERVICE_FACTORY_H_
