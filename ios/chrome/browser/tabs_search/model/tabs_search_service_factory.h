// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TABS_SEARCH_MODEL_TABS_SEARCH_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_TABS_SEARCH_MODEL_TABS_SEARCH_SERVICE_FACTORY_H_

#import "base/no_destructor.h"
#import "ios/chrome/browser/shared/model/profile/profile_keyed_service_factory_ios.h"

class ProfileIOS;
class TabsSearchService;

// Singleton that owns all TabsSearchServices and associates them with
// ProfileIOS.
class TabsSearchServiceFactory : public ProfileKeyedServiceFactoryIOS {
 public:
  static TabsSearchService* GetForProfile(ProfileIOS* profile);
  static TabsSearchServiceFactory* GetInstance();

 private:
  friend class base::NoDestructor<TabsSearchServiceFactory>;

  TabsSearchServiceFactory();
  ~TabsSearchServiceFactory() override;

  // BrowserStateKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
};

#endif  // IOS_CHROME_BROWSER_TABS_SEARCH_MODEL_TABS_SEARCH_SERVICE_FACTORY_H_
