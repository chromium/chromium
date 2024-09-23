// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/internal/sync/web_view_profile_invalidation_provider_factory.h"

#import <memory>
#import <utility>

#import "base/functional/bind.h"
#import "base/functional/callback.h"
#import "base/no_destructor.h"
#import "components/gcm_driver/gcm_profile_service.h"
#import "components/gcm_driver/instance_id/instance_id_profile_service.h"
#import "components/invalidation/impl/invalidator_registrar_with_memory.h"
#import "components/invalidation/impl/per_user_topic_subscription_manager.h"
#import "components/invalidation/impl/profile_identity_provider.h"
#import "components/invalidation/profile_invalidation_provider.h"
#import "components/keyed_service/ios/browser_state_dependency_manager.h"
#import "components/pref_registry/pref_registry_syncable.h"
#import "components/prefs/pref_registry.h"
#import "ios/web_view/internal/signin/web_view_identity_manager_factory.h"
#import "ios/web_view/internal/sync/web_view_gcm_profile_service_factory.h"
#import "ios/web_view/internal/sync/web_view_instance_id_profile_service_factory.h"
#import "ios/web_view/internal/web_view_browser_state.h"
#import "services/network/public/cpp/shared_url_loader_factory.h"

using invalidation::ProfileInvalidationProvider;

namespace ios_web_view {

// static
invalidation::ProfileInvalidationProvider*
WebViewProfileInvalidationProviderFactory::GetForBrowserState(
    WebViewBrowserState* browser_state) {
  return static_cast<ProfileInvalidationProvider*>(
      GetInstance()->GetServiceForBrowserState(browser_state, true));
}

// static
WebViewProfileInvalidationProviderFactory*
WebViewProfileInvalidationProviderFactory::GetInstance() {
  static base::NoDestructor<WebViewProfileInvalidationProviderFactory> instance;
  return instance.get();
}

WebViewProfileInvalidationProviderFactory::
    WebViewProfileInvalidationProviderFactory()
    : BrowserStateKeyedServiceFactory(
          "InvalidationService",
          BrowserStateDependencyManager::GetInstance()) {
  DependsOn(WebViewInstanceIDProfileServiceFactory::GetInstance());
  DependsOn(WebViewIdentityManagerFactory::GetInstance());
  DependsOn(WebViewGCMProfileServiceFactory::GetInstance());
}

WebViewProfileInvalidationProviderFactory::
    ~WebViewProfileInvalidationProviderFactory() = default;

std::unique_ptr<KeyedService>
WebViewProfileInvalidationProviderFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  WebViewBrowserState* browser_state =
      WebViewBrowserState::FromBrowserState(context);

  auto identity_provider =
      std::make_unique<invalidation::ProfileIdentityProvider>(
          WebViewIdentityManagerFactory::GetForBrowserState(browser_state));

  return std::make_unique<ProfileInvalidationProvider>(
      std::move(identity_provider));
}

void WebViewProfileInvalidationProviderFactory::RegisterBrowserStatePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  ProfileInvalidationProvider::RegisterProfilePrefs(registry);
  invalidation::InvalidatorRegistrarWithMemory::RegisterProfilePrefs(registry);
  invalidation::PerUserTopicSubscriptionManager::RegisterProfilePrefs(registry);
}

}  // namespace ios_web_view
