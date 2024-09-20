// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TABS_SEARCH_MODEL_TABS_SEARCH_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_TABS_SEARCH_MODEL_TABS_SEARCH_SERVICE_FACTORY_H_

#import "base/no_destructor.h"
#import "components/keyed_service/ios/browser_state_keyed_service_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios_forward.h"

class TabsSearchService;

// Singleton that owns all TabsSearchServices and associates them with
// ProfileIOS.
class TabsSearchServiceFactory : public BrowserStateKeyedServiceFactory {
 public:
  TabsSearchServiceFactory(const TabsSearchServiceFactory&) = delete;
  TabsSearchServiceFactory& operator=(const TabsSearchServiceFactory&) = delete;

  // TODO(crbug.com/358301380): remove this method.
  static TabsSearchService* GetForBrowserState(ProfileIOS* profile);

  static TabsSearchService* GetForProfile(ProfileIOS* profile);
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
