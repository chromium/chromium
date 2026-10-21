// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOCOMPLETE_MODEL_ZERO_SUGGEST_CACHE_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_AUTOCOMPLETE_MODEL_ZERO_SUGGEST_CACHE_SERVICE_FACTORY_H_

#import "components/omnibox/browser/zero_suggest_cache_service.h"
#import "ios/chrome/browser/shared/model/profile/typed_profile_keyed_service_factory_ios.h"

namespace ios {
// Singleton that owns all ZeroSuggestCacheServices and associates them with
// profiles.
class ZeroSuggestCacheServiceFactory final
    : public TypedProfileKeyedServiceFactoryIOS<ZeroSuggestCacheServiceFactory,
                                                ZeroSuggestCacheService> {
 public:
  explicit ZeroSuggestCacheServiceFactory(PassKey key);

 private:
  // BrowerStateKeyedServiceFactory implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const final;
};
}  // namespace ios

#endif  // IOS_CHROME_BROWSER_AUTOCOMPLETE_MODEL_ZERO_SUGGEST_CACHE_SERVICE_FACTORY_H_
