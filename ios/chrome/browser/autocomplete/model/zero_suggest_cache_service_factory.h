// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOCOMPLETE_MODEL_ZERO_SUGGEST_CACHE_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_AUTOCOMPLETE_MODEL_ZERO_SUGGEST_CACHE_SERVICE_FACTORY_H_

#import "base/no_destructor.h"
#import "components/omnibox/browser/zero_suggest_cache_service.h"
#import "ios/chrome/browser/shared/model/profile/profile_keyed_service_factory_ios.h"

class ProfileIOS;

namespace ios {
// Singleton that owns all ZeroSuggestCacheServices and associates them with
// profiles.
class ZeroSuggestCacheServiceFactory : public ProfileKeyedServiceFactoryIOS {
 public:
  static ZeroSuggestCacheService* GetForProfile(ProfileIOS* profile);
  static ZeroSuggestCacheServiceFactory* GetInstance();

 private:
  friend class base::NoDestructor<ZeroSuggestCacheServiceFactory>;

  ZeroSuggestCacheServiceFactory();
  ~ZeroSuggestCacheServiceFactory() override;

  // BrowerStateKeyedServiceFactory implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const override;
};
}  // namespace ios

#endif  // IOS_CHROME_BROWSER_AUTOCOMPLETE_MODEL_ZERO_SUGGEST_CACHE_SERVICE_FACTORY_H_
