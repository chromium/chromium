// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TABS_SEARCH_MODEL_TABS_SEARCH_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_TABS_SEARCH_MODEL_TABS_SEARCH_SERVICE_FACTORY_H_

#import "ios/chrome/browser/shared/model/profile/typed_profile_keyed_service_factory_ios.h"
#import "ios/chrome/browser/tabs_search/model/tabs_search_service.h"

// Singleton that owns all TabsSearchServices and associates them with
// ProfileIOS.
class TabsSearchServiceFactory final
    : public TypedProfileKeyedServiceFactoryIOS<TabsSearchServiceFactory,
                                                TabsSearchService> {
 public:
  explicit TabsSearchServiceFactory(PassKey key);

 private:
  // ProfileKeyedServiceFactoryIOS:
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const final;
};

#endif  // IOS_CHROME_BROWSER_TABS_SEARCH_MODEL_TABS_SEARCH_SERVICE_FACTORY_H_
