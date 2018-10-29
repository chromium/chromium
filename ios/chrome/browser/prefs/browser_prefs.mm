// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/prefs/browser_prefs.h"

#include "components/autofill/core/common/autofill_prefs.h"
#include "components/browsing_data/core/pref_names.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/dom_distiller/core/distilled_page_prefs.h"
#include "components/flags_ui/pref_service_flags_storage.h"
#include "components/gcm_driver/gcm_channel_status_syncer.h"
#import "components/handoff/handoff_manager.h"
#include "components/language/core/browser/pref_names.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/network_time/network_time_tracker.h"
#include "components/ntp_snippets/category_rankers/click_based_category_ranker.h"
#include "components/ntp_snippets/content_suggestions_service.h"
#include "components/ntp_snippets/remote/remote_suggestions_provider_impl.h"
#include "components/ntp_snippets/remote/remote_suggestions_scheduler_impl.h"
#include "components/ntp_snippets/remote/request_throttler.h"
#include "components/ntp_snippets/user_classifier.h"
#include "components/ntp_tiles/most_visited_sites.h"
#include "components/ntp_tiles/popular_sites_impl.h"
#include "components/omnibox/browser/zero_suggest_provider.h"
#include "components/password_manager/core/browser/password_manager.h"
#include "components/payments/core/payment_prefs.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/proxy_config/pref_proxy_config_tracker_impl.h"
#include "components/rappor/rappor_service_impl.h"
#include "components/search_engines/template_url_prepopulate_data.h"
#include "components/sessions/core/session_id_generator.h"
#include "components/signin/core/browser/signin_pref_names.h"
#include "components/strings/grit/components_locale_settings.h"
#include "components/sync/base/sync_prefs.h"
#include "components/sync_sessions/session_sync_prefs.h"
#include "components/translate/core/browser/translate_pref_names.h"
#include "components/translate/core/browser/translate_prefs.h"
#include "components/unified_consent/unified_consent_service.h"
#include "components/update_client/update_client.h"
#include "components/variations/service/variations_service.h"
#include "components/web_resource/web_resource_pref_names.h"
#include "ios/chrome/browser/browser_state/browser_state_info_cache.h"
#include "ios/chrome/browser/desktop_promotion/desktop_promotion_sync_service.h"
#include "ios/chrome/browser/first_run/first_run.h"
#import "ios/chrome/browser/geolocation/omnibox_geolocation_local_state.h"
#import "ios/chrome/browser/memory/memory_debugger_manager.h"
#import "ios/chrome/browser/metrics/ios_chrome_metrics_service_client.h"
#include "ios/chrome/browser/notification_promo.h"
#include "ios/chrome/browser/pref_names.h"
#include "ios/chrome/browser/signin/signin_manager_factory.h"
#import "ios/chrome/browser/ui/authentication/signin_promo_view_mediator.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_mediator.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_path_cache.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_utils_ios.h"
#include "ios/chrome/browser/voice/voice_search_prefs_registration.h"
#include "ios/public/provider/chrome/browser/chrome_browser_provider.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
const char kReverseAutologinEnabled[] = "reverse_autologin.enabled";
}

void RegisterLocalStatePrefs(PrefRegistrySimple* registry) {
  BrowserStateInfoCache::RegisterPrefs(registry);
  flags_ui::PrefServiceFlagsStorage::RegisterPrefs(registry);
  gcm::GCMChannelStatusSyncer::RegisterPrefs(registry);
  ios::SigninManagerFactory::RegisterPrefs(registry);
  IOSChromeMetricsServiceClient::RegisterPrefs(registry);
  network_time::NetworkTimeTracker::RegisterPrefs(registry);
  ios::NotificationPromo::RegisterPrefs(registry);
  PrefProxyConfigTrackerImpl::RegisterPrefs(registry);
  rappor::RapporServiceImpl::RegisterPrefs(registry);
  sessions::SessionIdGenerator::RegisterPrefs(registry);
  update_client::RegisterPrefs(registry);
  variations::VariationsService::RegisterPrefs(registry);

  // Preferences related to the browser state manager.
  registry->RegisterStringPref(prefs::kBrowserStateLastUsed, std::string());
  registry->RegisterIntegerPref(prefs::kBrowserStatesNumCreated, 1);
  registry->RegisterListPref(prefs::kBrowserStatesLastActive);

  [OmniboxGeolocationLocalState registerLocalState:registry];
  [MemoryDebuggerManager registerLocalState:registry];

  registry->RegisterBooleanPref(prefs::kBrowsingDataMigrationHasBeenPossible,
                                false);

  // Preferences related to the application context.
  registry->RegisterStringPref(language::prefs::kApplicationLocale,
                               std::string());
  registry->RegisterBooleanPref(prefs::kEulaAccepted, false);
  registry->RegisterBooleanPref(metrics::prefs::kMetricsReportingEnabled,
                                false);
  registry->RegisterBooleanPref(prefs::kLastSessionExitedCleanly, true);
  registry->RegisterBooleanPref(prefs::kMetricsReportingWifiOnly, true);
}

void RegisterBrowserStatePrefs(user_prefs::PrefRegistrySyncable* registry) {
  autofill::prefs::RegisterProfilePrefs(registry);
  DesktopPromotionSyncService::RegisterDesktopPromotionUserPrefs(registry);
  dom_distiller::DistilledPagePrefs::RegisterProfilePrefs(registry);
  FirstRun::RegisterProfilePrefs(registry);
  gcm::GCMChannelStatusSyncer::RegisterProfilePrefs(registry);
  HostContentSettingsMap::RegisterProfilePrefs(registry);
  ntp_snippets::ClickBasedCategoryRanker::RegisterProfilePrefs(registry);
  ntp_snippets::ContentSuggestionsService::RegisterProfilePrefs(registry);
  ntp_snippets::RemoteSuggestionsProviderImpl::RegisterProfilePrefs(registry);
  ntp_snippets::RemoteSuggestionsSchedulerImpl::RegisterProfilePrefs(registry);
  ntp_snippets::RequestThrottler::RegisterProfilePrefs(registry);
  ntp_snippets::UserClassifier::RegisterProfilePrefs(registry);
  ntp_tiles::MostVisitedSites::RegisterProfilePrefs(registry);
  ntp_tiles::PopularSitesImpl::RegisterProfilePrefs(registry);
  ios::NotificationPromo::RegisterProfilePrefs(registry);
  password_manager::PasswordManager::RegisterProfilePrefs(registry);
  payments::RegisterProfilePrefs(registry);
  PrefProxyConfigTrackerImpl::RegisterProfilePrefs(registry);
  RegisterVoiceSearchBrowserStatePrefs(registry);
  sync_sessions::SessionSyncPrefs::RegisterProfilePrefs(registry);
  syncer::SyncPrefs::RegisterProfilePrefs(registry);
  TemplateURLPrepopulateData::RegisterProfilePrefs(registry);
  translate::TranslatePrefs::RegisterProfilePrefs(registry);
  unified_consent::UnifiedConsentService::RegisterPrefs(registry);
  variations::VariationsService::RegisterProfilePrefs(registry);
  ZeroSuggestProvider::RegisterProfilePrefs(registry);

  [BookmarkMediator registerBrowserStatePrefs:registry];
  [BookmarkPathCache registerBrowserStatePrefs:registry];
  [SigninPromoViewMediator registerBrowserStatePrefs:registry];
  [HandoffManager registerBrowserStatePrefs:registry];

  registry->RegisterBooleanPref(prefs::kDataSaverEnabled, false);
  registry->RegisterBooleanPref(
      prefs::kEnableDoNotTrack, false,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterBooleanPref(
      prefs::kOfferTranslateEnabled, true,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterStringPref(prefs::kAcceptLanguages,
                               l10n_util::GetStringUTF8(IDS_ACCEPT_LANGUAGES));
  registry->RegisterStringPref(prefs::kDefaultCharset,
                               l10n_util::GetStringUTF8(IDS_DEFAULT_ENCODING),
                               user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterBooleanPref(
      prefs::kNetworkPredictionEnabled, true,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterBooleanPref(
      prefs::kNetworkPredictionWifiOnly, true,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterStringPref(prefs::kContextualSearchEnabled, std::string(),
                               user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterBooleanPref(
      prefs::kSearchSuggestEnabled, true,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterBooleanPref(prefs::kSavingBrowserHistoryDisabled, false);
  registry->RegisterIntegerPref(prefs::kNtpShownPage, 1 << 10);

  // This comes from components/bookmarks/core/browser/bookmark_model.h
  // Defaults to 3, which is the id of bookmarkModel_->mobile_node()
  registry->RegisterInt64Pref(prefs::kNtpShownBookmarksFolder, 3);

  // Register prefs used by Clear Browsing Data UI.
  browsing_data::prefs::RegisterBrowserUserPrefs(registry);

  registry->RegisterBooleanPref(kReverseAutologinEnabled, true);
}

// This method should be periodically pruned of year+ old migrations.
void MigrateObsoleteLocalStatePrefs(PrefService* prefs) {
}

// This method should be periodically pruned of year+ old migrations.
void MigrateObsoleteBrowserStatePrefs(PrefService* prefs) {
  // Added 01/2018.
  prefs->ClearPref(::prefs::kNtpShownPage);

  // Added 8/2018.
  autofill::prefs::MigrateDeprecatedAutofillPrefs(prefs);

  // Added 10/2018.
  prefs->ClearPref(kReverseAutologinEnabled);
}
