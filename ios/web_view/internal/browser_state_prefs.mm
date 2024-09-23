// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/internal/browser_state_prefs.h"

#import "components/autofill/core/common/autofill_prefs.h"
#import "components/history/core/common/pref_names.h"
#import "components/keyed_service/ios/browser_state_dependency_manager.h"
#import "components/language/core/browser/language_prefs.h"
#import "components/metrics/demographics/user_demographics.h"
#import "components/password_manager/core/browser/password_manager.h"
#import "components/pref_registry/pref_registry_syncable.h"
#import "components/safe_browsing/core/common/safe_browsing_prefs.h"
#import "components/sync/service/glue/sync_transport_data_prefs.h"
#import "components/sync/service/sync_prefs.h"
#import "components/sync_device_info/device_info_prefs.h"
#import "components/translate/core/browser/translate_pref_names.h"
#import "components/translate/core/browser/translate_prefs.h"
#import "components/unified_consent/unified_consent_service.h"

namespace ios_web_view {

void RegisterBrowserStatePrefs(
    user_prefs::PrefRegistrySyncable* pref_registry) {
  pref_registry->RegisterBooleanPref(translate::prefs::kOfferTranslateEnabled,
                                     true);
  pref_registry->RegisterBooleanPref(prefs::kSavingBrowserHistoryDisabled,
                                     true);
  language::LanguagePrefs::RegisterProfilePrefs(pref_registry);
  metrics::RegisterDemographicsProfilePrefs(pref_registry);
  translate::TranslatePrefs::RegisterProfilePrefs(pref_registry);
  autofill::prefs::RegisterProfilePrefs(pref_registry);
  password_manager::PasswordManager::RegisterProfilePrefs(pref_registry);
  syncer::SyncPrefs::RegisterProfilePrefs(pref_registry);
  syncer::SyncTransportDataPrefs::RegisterProfilePrefs(pref_registry);
  syncer::DeviceInfoPrefs::RegisterProfilePrefs(pref_registry);
  safe_browsing::RegisterProfilePrefs(pref_registry);
  unified_consent::UnifiedConsentService::RegisterPrefs(pref_registry);

  BrowserStateDependencyManager::GetInstance()
      ->RegisterBrowserStatePrefsForServices(pref_registry);
}

}  // namespace ios_web_view
