// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_INTERNAL_SYNC_WEB_VIEW_PROFILE_INVALIDATION_PROVIDER_FACTORY_H_
#define IOS_WEB_VIEW_INTERNAL_SYNC_WEB_VIEW_PROFILE_INVALIDATION_PROVIDER_FACTORY_H_

#include <memory>

#include "base/no_destructor.h"
#include "components/keyed_service/ios/browser_state_keyed_service_factory.h"

namespace invalidation {
class ProfileInvalidationProvider;
}

namespace user_prefs {
class PrefRegistrySyncable;
}

namespace ios_web_view {
class WebViewBrowserState;

// A BrowserContextKeyedServiceFactory to construct InvalidationServices wrapped
// in ProfileInvalidationProviders.
class WebViewProfileInvalidationProviderFactory
    : public BrowserStateKeyedServiceFactory {
 public:
  // Returns the ProfileInvalidationProvider for the given |browser_state|,
  // lazily creating one first if required.
  static invalidation::ProfileInvalidationProvider* GetForBrowserState(
      WebViewBrowserState* browser_state);

  static WebViewProfileInvalidationProviderFactory* GetInstance();

  WebViewProfileInvalidationProviderFactory(
      const WebViewProfileInvalidationProviderFactory&) = delete;
  WebViewProfileInvalidationProviderFactory& operator=(
      const WebViewProfileInvalidationProviderFactory&) = delete;

 private:
  friend class base::NoDestructor<WebViewProfileInvalidationProviderFactory>;

  WebViewProfileInvalidationProviderFactory();
  ~WebViewProfileInvalidationProviderFactory() override;

  // BrowserStateKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
  void RegisterBrowserStatePrefs(
      user_prefs::PrefRegistrySyncable* registry) override;
};

}  // namespace ios_web_view

#endif  // IOS_WEB_VIEW_INTERNAL_SYNC_WEB_VIEW_PROFILE_INVALIDATION_PROVIDER_FACTORY_H_
