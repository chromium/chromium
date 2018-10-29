// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/invalidation/ios_chrome_deprecated_profile_invalidation_provider_factory.h"

#include <memory>
#include <utility>

#include "base/callback.h"
#include "base/memory/singleton.h"
#include "components/gcm_driver/gcm_profile_service.h"
#include "components/invalidation/impl/invalidator_storage.h"
#include "components/invalidation/impl/profile_identity_provider.h"
#include "components/invalidation/impl/profile_invalidation_provider.h"
#include "components/invalidation/impl/ticl_invalidation_service.h"
#include "components/invalidation/impl/ticl_profile_settings_provider.h"
#include "components/keyed_service/ios/browser_state_dependency_manager.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_registry.h"
#include "components/signin/core/browser/signin_manager.h"
#include "ios/chrome/browser/application_context.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/gcm/ios_chrome_gcm_profile_service_factory.h"
#include "ios/chrome/browser/signin/identity_manager_factory.h"
#include "ios/web/public/web_client.h"
#include "net/url_request/url_request_context_getter.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using invalidation::InvalidatorStorage;
using invalidation::ProfileInvalidationProvider;
using invalidation::TiclInvalidationService;

// static
invalidation::ProfileInvalidationProvider*
IOSChromeDeprecatedProfileInvalidationProviderFactory::GetForBrowserState(
    ios::ChromeBrowserState* browser_state) {
  return static_cast<ProfileInvalidationProvider*>(
      GetInstance()->GetServiceForBrowserState(browser_state, true));
}

// static
IOSChromeDeprecatedProfileInvalidationProviderFactory*
IOSChromeDeprecatedProfileInvalidationProviderFactory::GetInstance() {
  return base::Singleton<
      IOSChromeDeprecatedProfileInvalidationProviderFactory>::get();
}

IOSChromeDeprecatedProfileInvalidationProviderFactory::
    IOSChromeDeprecatedProfileInvalidationProviderFactory()
    : BrowserStateKeyedServiceFactory(
          "InvalidationService",
          BrowserStateDependencyManager::GetInstance()) {
  DependsOn(IdentityManagerFactory::GetInstance());
  DependsOn(IOSChromeGCMProfileServiceFactory::GetInstance());
}

IOSChromeDeprecatedProfileInvalidationProviderFactory::
    ~IOSChromeDeprecatedProfileInvalidationProviderFactory() {}

std::unique_ptr<KeyedService>
IOSChromeDeprecatedProfileInvalidationProviderFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  ios::ChromeBrowserState* browser_state =
      ios::ChromeBrowserState::FromBrowserState(context);

  auto identity_provider =
      std::make_unique<invalidation::ProfileIdentityProvider>(
          IdentityManagerFactory::GetForBrowserState(browser_state));

  std::unique_ptr<TiclInvalidationService> service(new TiclInvalidationService(
      web::GetWebClient()->GetUserAgent(web::UserAgentType::MOBILE),
      identity_provider.get(),
      std::make_unique<invalidation::TiclProfileSettingsProvider>(
          browser_state->GetPrefs()),
      IOSChromeGCMProfileServiceFactory::GetForBrowserState(browser_state)
          ->driver(),
      browser_state->GetRequestContext(),
      browser_state->GetSharedURLLoaderFactory(),
      GetApplicationContext()->GetNetworkConnectionTracker()));
  service->Init(
      std::make_unique<InvalidatorStorage>(browser_state->GetPrefs()));

  return std::make_unique<ProfileInvalidationProvider>(
      std::move(service), std::move(identity_provider));
}

void IOSChromeDeprecatedProfileInvalidationProviderFactory::
    RegisterBrowserStatePrefs(user_prefs::PrefRegistrySyncable* registry) {
  ProfileInvalidationProvider::RegisterProfilePrefs(registry);
  InvalidatorStorage::RegisterProfilePrefs(registry);
}
