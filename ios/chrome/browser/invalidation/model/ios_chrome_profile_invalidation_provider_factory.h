// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INVALIDATION_MODEL_IOS_CHROME_PROFILE_INVALIDATION_PROVIDER_FACTORY_H_
#define IOS_CHROME_BROWSER_INVALIDATION_MODEL_IOS_CHROME_PROFILE_INVALIDATION_PROVIDER_FACTORY_H_

#import <memory>

#import "components/invalidation/profile_invalidation_provider.h"
#import "ios/chrome/browser/shared/model/profile/typed_profile_keyed_service_factory_ios.h"

// A BrowserContextKeyedServiceFactory to construct InvalidationServices wrapped
// in ProfileInvalidationProviders.
class IOSChromeProfileInvalidationProviderFactory final
    : public TypedProfileKeyedServiceFactoryIOS<
          IOSChromeProfileInvalidationProviderFactory,
          invalidation::ProfileInvalidationProvider> {
 public:
  explicit IOSChromeProfileInvalidationProviderFactory(PassKey key);

 private:
  // ProfileKeyedServiceFactoryIOS:
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const final;
};

#endif  // IOS_CHROME_BROWSER_INVALIDATION_MODEL_IOS_CHROME_PROFILE_INVALIDATION_PROVIDER_FACTORY_H_
