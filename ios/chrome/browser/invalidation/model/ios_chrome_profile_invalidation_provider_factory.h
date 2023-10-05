// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INVALIDATION_MODEL_IOS_CHROME_PROFILE_INVALIDATION_PROVIDER_FACTORY_H_
#define IOS_CHROME_BROWSER_INVALIDATION_MODEL_IOS_CHROME_PROFILE_INVALIDATION_PROVIDER_FACTORY_H_

#include <memory>

#include "base/no_destructor.h"
#include "components/keyed_service/ios/browser_state_keyed_service_factory.h"

class ChromeBrowserState;

namespace invalidation {
class ProfileInvalidationProvider;
}

// A BrowserContextKeyedServiceFactory to construct InvalidationServices wrapped
// in ProfileInvalidationProviders.
class IOSChromeProfileInvalidationProviderFactory
    : public BrowserStateKeyedServiceFactory {
 public:
  // Returns the ProfileInvalidationProvider for the given `browser_state`,
  // lazily creating one first if required.
  static invalidation::ProfileInvalidationProvider* GetForBrowserState(
      ChromeBrowserState* browser_state);

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
