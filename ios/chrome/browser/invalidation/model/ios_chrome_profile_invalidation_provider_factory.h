// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INVALIDATION_MODEL_IOS_CHROME_PROFILE_INVALIDATION_PROVIDER_FACTORY_H_
#define IOS_CHROME_BROWSER_INVALIDATION_MODEL_IOS_CHROME_PROFILE_INVALIDATION_PROVIDER_FACTORY_H_

#import <memory>

#import "base/no_destructor.h"
#import "ios/chrome/browser/shared/model/profile/profile_keyed_service_factory_ios.h"

class ProfileIOS;

namespace invalidation {
class ProfileInvalidationProvider;
}

// A BrowserContextKeyedServiceFactory to construct InvalidationServices wrapped
// in ProfileInvalidationProviders.
class IOSChromeProfileInvalidationProviderFactory
    : public ProfileKeyedServiceFactoryIOS {
 public:
  static invalidation::ProfileInvalidationProvider* GetForProfile(
      ProfileIOS* profile);
  static IOSChromeProfileInvalidationProviderFactory* GetInstance();

 private:
  friend class base::NoDestructor<IOSChromeProfileInvalidationProviderFactory>;

  IOSChromeProfileInvalidationProviderFactory();
  ~IOSChromeProfileInvalidationProviderFactory() override;

  // ProfileKeyedServiceFactoryIOS:
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const override;
};

#endif  // IOS_CHROME_BROWSER_INVALIDATION_MODEL_IOS_CHROME_PROFILE_INVALIDATION_PROVIDER_FACTORY_H_
