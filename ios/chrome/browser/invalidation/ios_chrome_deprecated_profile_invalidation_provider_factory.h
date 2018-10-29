// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INVALIDATION_IOS_CHROME_DEPRECATED_PROFILE_INVALIDATION_PROVIDER_FACTORY_H_
#define IOS_CHROME_BROWSER_INVALIDATION_IOS_CHROME_DEPRECATED_PROFILE_INVALIDATION_PROVIDER_FACTORY_H_

#include <memory>

#include "base/macros.h"
#include "components/keyed_service/ios/browser_state_keyed_service_factory.h"

namespace base {
template <typename T>
struct DefaultSingletonTraits;
}

namespace invalidation {
class ProfileInvalidationProvider;
}

namespace ios {
class ChromeBrowserState;
}

namespace user_prefs {
class PrefRegistrySyncable;
}

// A BrowserContextKeyedServiceFactory to construct InvalidationServices wrapped
// in ProfileInvalidationProviders.
class IOSChromeDeprecatedProfileInvalidationProviderFactory
    : public BrowserStateKeyedServiceFactory {
 public:
  // Returns the ProfileInvalidationProvider for the given |browser_state|,
  // lazily creating one first if required.
  static invalidation::ProfileInvalidationProvider* GetForBrowserState(
      ios::ChromeBrowserState* browser_state);

  static IOSChromeDeprecatedProfileInvalidationProviderFactory* GetInstance();

 private:
  friend struct base::DefaultSingletonTraits<
      IOSChromeDeprecatedProfileInvalidationProviderFactory>;

  IOSChromeDeprecatedProfileInvalidationProviderFactory();
  ~IOSChromeDeprecatedProfileInvalidationProviderFactory() override;

  // BrowserStateKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
  void RegisterBrowserStatePrefs(
      user_prefs::PrefRegistrySyncable* registry) override;

  DISALLOW_COPY_AND_ASSIGN(
      IOSChromeDeprecatedProfileInvalidationProviderFactory);
};

#endif  // IOS_CHROME_BROWSER_INVALIDATION_IOS_CHROME_DEPRECATED_PROFILE_INVALIDATION_PROVIDER_FACTORY_H_
