// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/unified_consent/unified_consent_service_factory.h"

#include <string>
#include <vector>

#include "components/keyed_service/ios/browser_state_dependency_manager.h"
#include "components/sync/driver/sync_service.h"
#include "components/sync_preferences/pref_service_syncable.h"
#include "components/unified_consent/unified_consent_metrics.h"
#include "components/unified_consent/unified_consent_service.h"
#include "ios/chrome/browser/application_context.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/pref_names.h"
#include "ios/chrome/browser/signin/identity_manager_factory.h"
#include "ios/chrome/browser/sync/profile_sync_service_factory.h"

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
  static base::NoDestructor<UnifiedConsentServiceFactory> instance;
  return instance.get();
}

std::unique_ptr<KeyedService>
UnifiedConsentServiceFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  ios::ChromeBrowserState* browser_state =
      ios::ChromeBrowserState::FromBrowserState(context);
  sync_preferences::PrefServiceSyncable* user_pref_service =
      browser_state->GetSyncablePrefs();

  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForBrowserState(browser_state);
  syncer::SyncService* sync_service =
      ProfileSyncServiceFactory::GetForBrowserState(browser_state);

  // Record settings for pre- and post-UnifiedConsent users.
  unified_consent::metrics::RecordSettingsHistogram(user_pref_service);

  // List of synced prefs that can be configured during the settings opt-in
  // flow.
  std::vector<std::string> synced_service_pref_names;
  synced_service_pref_names.push_back(prefs::kSearchSuggestEnabled);

  return std::make_unique<unified_consent::UnifiedConsentService>(
      user_pref_service, identity_manager, sync_service,
      synced_service_pref_names);
}
