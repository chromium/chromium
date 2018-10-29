// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/unified_consent/unified_consent_service_factory.h"

#include "components/browser_sync/profile_sync_service.h"
#include "components/keyed_service/ios/browser_state_dependency_manager.h"
#include "components/sync/driver/sync_service.h"
#include "components/unified_consent/feature.h"
#include "components/unified_consent/unified_consent_service.h"
#include "ios/chrome/browser/application_context.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/signin/identity_manager_factory.h"
#include "ios/chrome/browser/sync/profile_sync_service_factory.h"
#include "ios/chrome/browser/unified_consent/unified_consent_service_client_impl.h"

UnifiedConsentServiceFactory::UnifiedConsentServiceFactory()
    : BrowserStateKeyedServiceFactory(
          "UnifiedConsentService",
          BrowserStateDependencyManager::GetInstance()) {
  DependsOn(IdentityManagerFactory::GetInstance());
  DependsOn(ProfileSyncServiceFactory::GetInstance());
}

UnifiedConsentServiceFactory::~UnifiedConsentServiceFactory() = default;

// static
unified_consent::UnifiedConsentService*
UnifiedConsentServiceFactory::GetForBrowserState(
    ios::ChromeBrowserState* browser_state) {
  return static_cast<unified_consent::UnifiedConsentService*>(
      GetInstance()->GetServiceForBrowserState(browser_state, true));
}

// static
unified_consent::UnifiedConsentService*
UnifiedConsentServiceFactory::GetForBrowserStateIfExists(
    ios::ChromeBrowserState* browser_state) {
  return static_cast<unified_consent::UnifiedConsentService*>(
      GetInstance()->GetServiceForBrowserState(browser_state, false));
}

// static
UnifiedConsentServiceFactory* UnifiedConsentServiceFactory::GetInstance() {
  return base::Singleton<UnifiedConsentServiceFactory>::get();
}

std::unique_ptr<KeyedService>
UnifiedConsentServiceFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  ios::ChromeBrowserState* browser_state =
      ios::ChromeBrowserState::FromBrowserState(context);
  PrefService* user_pref_service = browser_state->GetPrefs();
  PrefService* local_pref_service = GetApplicationContext()->GetLocalState();
  std::unique_ptr<unified_consent::UnifiedConsentServiceClient> service_client =
      std::make_unique<UnifiedConsentServiceClientImpl>(user_pref_service,
                                                        local_pref_service);

  identity::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForBrowserState(browser_state);
  syncer::SyncService* sync_service =
      ProfileSyncServiceFactory::GetForBrowserState(browser_state);

  if (!unified_consent::IsUnifiedConsentFeatureEnabled()) {
    unified_consent::UnifiedConsentService::RollbackIfNeeded(
        user_pref_service, sync_service, service_client.get());
    return nullptr;
  }

  return std::make_unique<unified_consent::UnifiedConsentService>(
      std::move(service_client), user_pref_service, identity_manager,
      sync_service);
}
