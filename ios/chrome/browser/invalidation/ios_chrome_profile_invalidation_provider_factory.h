// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INVALIDATION_IOS_CHROME_PROFILE_INVALIDATION_PROVIDER_FACTORY_H_
#define IOS_CHROME_BROWSER_INVALIDATION_IOS_CHROME_PROFILE_INVALIDATION_PROVIDER_FACTORY_H_

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

// A BrowserContextKeyedServiceFactory to construct InvalidationServices wrapped
// in ProfileInvalidationProviders.
class IOSChromeProfileInvalidationProviderFactory
    : public BrowserStateKeyedServiceFactory {
 public:
  // Returns the ProfileInvalidationProvider for the given |browser_state|,
  // lazily creating one first if required.
  static invalidation::ProfileInvalidationProvider* GetForBrowserState(
      ios::ChromeBrowserState* browser_state);

  static IOSChromeProfileInvalidationProviderFactory* GetInstance();

 private:
  friend struct base::DefaultSingletonTraits<
      IOSChromeProfileInvalidationProviderFactory>;

  IOSChromeProfileInvalidationProviderFactory();
  ~IOSChromeProfileInvalidationProviderFactory() override;

  // BrowserStateKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;

  DISALLOW_COPY_AND_ASSIGN(IOSChromeProfileInvalidationProviderFactory);
};

#endif  // IOS_CHROME_BROWSER_INVALIDATION_IOS_CHROME_PROFILE_INVALIDATION_PROVIDER_FACTORY_H_
