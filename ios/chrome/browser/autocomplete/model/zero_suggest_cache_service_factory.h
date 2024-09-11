// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOCOMPLETE_MODEL_ZERO_SUGGEST_CACHE_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_AUTOCOMPLETE_MODEL_ZERO_SUGGEST_CACHE_SERVICE_FACTORY_H_

#import <memory>

#import "base/no_destructor.h"
#import "components/keyed_service/ios/browser_state_keyed_service_factory.h"
#import "components/omnibox/browser/zero_suggest_cache_service.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios_forward.h"

namespace ios {
// Singleton that owns all ZeroSuggestCacheServices and associates them with
// profiles.
class ZeroSuggestCacheServiceFactory : public BrowserStateKeyedServiceFactory {
 public:
  // TODO(crbug.com/358301380): remove this method.
  static ZeroSuggestCacheService* GetForBrowserState(ProfileIOS* profile);

  static ZeroSuggestCacheService* GetForProfile(ProfileIOS* profile);
  static ZeroSuggestCacheServiceFactory* GetInstance();
  // Returns the default factory used to build ZeroSuggestCacheService. Can be
  // registered with SetTestingFactory to use real instances during testing.
  static TestingFactory GetDefaultFactory();

  ZeroSuggestCacheServiceFactory(const ZeroSuggestCacheServiceFactory&) =
      delete;
  ZeroSuggestCacheServiceFactory& operator=(
      const ZeroSuggestCacheServiceFactory&) = delete;

 private:
  friend class base::NoDestructor<ZeroSuggestCacheServiceFactory>;

  ZeroSuggestCacheServiceFactory();
  ~ZeroSuggestCacheServiceFactory() override;

  // BrowerStateKeyedServiceFactory implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
};
}  // namespace ios

#endif  // IOS_CHROME_BROWSER_AUTOCOMPLETE_MODEL_ZERO_SUGGEST_CACHE_SERVICE_FACTORY_H_
