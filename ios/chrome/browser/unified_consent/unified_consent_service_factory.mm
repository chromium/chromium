// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/unified_consent/unified_consent_service_factory.h"

#import <string>
#import <vector>

#import "base/no_destructor.h"
#import "components/keyed_service/ios/browser_state_dependency_manager.h"
#import "components/sync/driver/sync_service.h"
#import "components/sync_preferences/pref_service_syncable.h"
#import "components/unified_consent/unified_consent_metrics.h"
#import "components/unified_consent/unified_consent_service.h"
#import "ios/chrome/browser/application_context/application_context.h"
#import "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/prefs/pref_names.h"
#import "ios/chrome/browser/signin/identity_manager_factory.h"
#import "ios/chrome/browser/sync/sync_service_factory.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

UnifiedConsentServiceFactory::UnifiedConsentServiceFactory()
    : BrowserStateKeyedServiceFactory(
          "UnifiedConsentService",
          BrowserStateDependencyManager::GetInstance()) {
  DependsOn(IdentityManagerFactory::GetInstance());
  DependsOn(SyncServiceFactory::GetInstance());
}

UnifiedConsentServiceFactory::~UnifiedConsentServiceFactory() = default;

// static
unified_consent::UnifiedConsentService*
UnifiedConsentServiceFactory::GetForBrowserState(
    ChromeBrowserState* browser_state) {
  return static_cast<unified_consent::UnifiedConsentService*>(
      GetInstance()->GetServiceForBrowserState(browser_state, true));
}

// static
unified_consent::UnifiedConsentService*
UnifiedConsentServiceFactory::GetForBrowserStateIfExists(
    ChromeBrowserState* browser_state) {
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
  ChromeBrowserState* browser_state =
      ChromeBrowserState::FromBrowserState(context);
  sync_preferences::PrefServiceSyncable* user_pref_service =
      browser_state->GetSyncablePrefs();

  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForBrowserState(browser_state);
  syncer::SyncService* sync_service =
      SyncServiceFactory::GetForBrowserState(browser_state);

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
