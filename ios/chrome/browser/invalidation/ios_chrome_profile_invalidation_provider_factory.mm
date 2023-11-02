// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/invalidation/ios_chrome_profile_invalidation_provider_factory.h"

#import <memory>
#import <utility>

#import "base/bind.h"
#import "base/callback.h"
#import "base/no_destructor.h"
#import "components/gcm_driver/gcm_profile_service.h"
#import "components/gcm_driver/instance_id/instance_id_profile_service.h"
#import "components/invalidation/impl/fcm_invalidation_service.h"
#import "components/invalidation/impl/fcm_network_handler.h"
#import "components/invalidation/impl/profile_identity_provider.h"
#import "components/invalidation/impl/profile_invalidation_provider.h"
#import "components/keyed_service/ios/browser_state_dependency_manager.h"
#import "components/pref_registry/pref_registry_syncable.h"
#import "components/prefs/pref_registry.h"
#import "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/gcm/instance_id/ios_chrome_instance_id_profile_service_factory.h"
#import "ios/chrome/browser/gcm/ios_chrome_gcm_profile_service_factory.h"
#import "ios/chrome/browser/signin/identity_manager_factory.h"
#import "services/network/public/cpp/shared_url_loader_factory.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using invalidation::ProfileInvalidationProvider;

// static
invalidation::ProfileInvalidationProvider*
IOSChromeProfileInvalidationProviderFactory::GetForBrowserState(
    ChromeBrowserState* browser_state) {
  return static_cast<ProfileInvalidationProvider*>(
      GetInstance()->GetServiceForBrowserState(browser_state, true));
}

// static
IOSChromeProfileInvalidationProviderFactory*
IOSChromeProfileInvalidationProviderFactory::GetInstance() {
  static base::NoDestructor<IOSChromeProfileInvalidationProviderFactory>
      instance;
  return instance.get();
}

IOSChromeProfileInvalidationProviderFactory::
    IOSChromeProfileInvalidationProviderFactory()
    : BrowserStateKeyedServiceFactory(
          "InvalidationService",
          BrowserStateDependencyManager::GetInstance()) {
  DependsOn(IdentityManagerFactory::GetInstance());
  DependsOn(IOSChromeGCMProfileServiceFactory::GetInstance());
  DependsOn(IOSChromeInstanceIDProfileServiceFactory::GetInstance());
}

IOSChromeProfileInvalidationProviderFactory::
    ~IOSChromeProfileInvalidationProviderFactory() {}

std::unique_ptr<KeyedService>
IOSChromeProfileInvalidationProviderFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  ChromeBrowserState* browser_state =
      ChromeBrowserState::FromBrowserState(context);

  auto identity_provider =
      std::make_unique<invalidation::ProfileIdentityProvider>(
          IdentityManagerFactory::GetForBrowserState(browser_state));

  std::unique_ptr<invalidation::FCMInvalidationService> service =
      std::make_unique<invalidation::FCMInvalidationService>(
          identity_provider.get(),
          base::BindRepeating(
              &invalidation::FCMNetworkHandler::Create,
              IOSChromeGCMProfileServiceFactory::GetForBrowserState(
                  browser_state)
                  ->driver(),
              IOSChromeInstanceIDProfileServiceFactory::GetForBrowserState(
                  browser_state)
                  ->driver()),
          base::BindRepeating(
              &invalidation::PerUserTopicSubscriptionManager::Create,
              identity_provider.get(), browser_state->GetPrefs(),
              browser_state->GetURLLoaderFactory()),
          IOSChromeInstanceIDProfileServiceFactory::GetForBrowserState(
              browser_state)
              ->driver(),
          browser_state->GetPrefs());
  service->Init();

  return std::make_unique<ProfileInvalidationProvider>(
      std::move(service), std::move(identity_provider));
}

void IOSChromeProfileInvalidationProviderFactory::RegisterBrowserStatePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  ProfileInvalidationProvider::RegisterProfilePrefs(registry);
}
