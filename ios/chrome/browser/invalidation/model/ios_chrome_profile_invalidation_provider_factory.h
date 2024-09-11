// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INVALIDATION_MODEL_IOS_CHROME_PROFILE_INVALIDATION_PROVIDER_FACTORY_H_
#define IOS_CHROME_BROWSER_INVALIDATION_MODEL_IOS_CHROME_PROFILE_INVALIDATION_PROVIDER_FACTORY_H_

#import <memory>

#import "base/no_destructor.h"
#import "components/keyed_service/ios/browser_state_keyed_service_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios_forward.h"

namespace invalidation {
class ProfileInvalidationProvider;
}

// A BrowserContextKeyedServiceFactory to construct InvalidationServices wrapped
// in ProfileInvalidationProviders.
class IOSChromeProfileInvalidationProviderFactory
    : public BrowserStateKeyedServiceFactory {
 public:
  // TODO(crbug.com/358301380): remove this method.
  static invalidation::ProfileInvalidationProvider* GetForBrowserState(
      ProfileIOS* profile);

  static invalidation::ProfileInvalidationProvider* GetForProfile(
      ProfileIOS* profile);
  static IOSChromeProfileInvalidationProviderFactory* GetInstance();

  IOSChromeProfileInvalidationProviderFactory(
      const IOSChromeProfileInvalidationProviderFactory&) = delete;
  IOSChromeProfileInvalidationProviderFactory& operator=(
      const IOSChromeProfileInvalidationProviderFactory&) = delete;

 private:
  friend class base::NoDestructor<IOSChromeProfileInvalidationProviderFactory>;

  IOSChromeProfileInvalidationProviderFactory();
  ~IOSChromeProfileInvalidationProviderFactory() override;

  // BrowserStateKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
  void RegisterBrowserStatePrefs(
      user_prefs::PrefRegistrySyncable* registry) override;
};

#endif  // IOS_CHROME_BROWSER_INVALIDATION_MODEL_IOS_CHROME_PROFILE_INVALIDATION_PROVIDER_FACTORY_H_
