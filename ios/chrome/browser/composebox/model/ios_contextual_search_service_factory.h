// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_COMPOSEBOX_MODEL_IOS_CONTEXTUAL_SEARCH_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_COMPOSEBOX_MODEL_IOS_CONTEXTUAL_SEARCH_SERVICE_FACTORY_H_

#import "base/no_destructor.h"
#import "ios/chrome/browser/shared/model/profile/profile_keyed_service_factory_ios.h"

class ProfileIOS;
namespace contextual_search {
class ContextualSearchService;
}

// Singleton that owns all ContextualSearchService and associates them
// with ProfileIOS.
class ContextualSearchServiceFactory : public ProfileKeyedServiceFactoryIOS {
 public:
  static contextual_search::ContextualSearchService* GetForProfile(
      ProfileIOS* profile);

  static ContextualSearchServiceFactory* GetInstance();

 private:
  friend class base::NoDestructor<ContextualSearchServiceFactory>;

  ContextualSearchServiceFactory();
  ~ContextualSearchServiceFactory() override;

  // ProfileKeyedServiceFactoryIOS implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const override;
};

#endif  // IOS_CHROME_BROWSER_COMPOSEBOX_MODEL_IOS_CONTEXTUAL_SEARCH_SERVICE_FACTORY_H_
