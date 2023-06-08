// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/model/prefs/browser_prefs.h"

#import "base/time/time.h"
#import "base/types/cxx23_to_underlying.h"
#import "components/autofill/core/common/autofill_prefs.h"
#import "components/browsing_data/core/pref_names.h"
#import "components/commerce/core/pref_names.h"
#import "components/component_updater/component_updater_service.h"
#import "components/component_updater/installer_policies/autofill_states_component_installer.h"
#import "components/content_settings/core/browser/host_content_settings_map.h"
#import "components/dom_distiller/core/distilled_page_prefs.h"
#import "components/enterprise/browser/reporting/common_pref_names.h"
#import "components/feed/core/v2/public/ios/pref_names.h"
#import "components/flags_ui/pref_service_flags_storage.h"
#import "components/handoff/handoff_manager.h"
#import "components/history/core/common/pref_names.h"
#import "components/invalidation/impl/invalidator_registrar_with_memory.h"
#import "components/invalidation/impl/per_user_topic_subscription_manager.h"
#import "components/language/core/browser/language_prefs.h"
#import "components/language/core/browser/pref_names.h"
#import "components/metrics/demographics/user_demographics.h"
#import "components/metrics/metrics_pref_names.h"
#import "components/network_time/network_time_tracker.h"
#import "components/ntp_snippets/register_prefs.h"
#import "components/ntp_tiles/most_visited_sites.h"
#import "components/ntp_tiles/popular_sites_impl.h"
#import "components/omnibox/browser/zero_suggest_provider.h"
#import "components/optimization_guide/core/optimization_guide_prefs.h"
#import "components/password_manager/core/browser/password_manager.h"
#import "components/payments/core/payment_prefs.h"
#import "components/policy/core/browser/browser_policy_connector.h"
#import "components/policy/core/browser/url_blocklist_manager.h"
#import "components/policy/core/common/policy_pref_names.h"
#import "components/policy/core/common/policy_statistics_collector.h"
#import "components/pref_registry/pref_registry_syncable.h"
#import "components/prefs/pref_service.h"
#import "components/proxy_config/pref_proxy_config_tracker_impl.h"
#import "components/safe_browsing/core/common/safe_browsing_prefs.h"
#import "components/search_engines/template_url_prepopulate_data.h"
#import "components/segmentation_platform/embedder/default_model/device_switcher_result_dispatcher.h"
#import "components/segmentation_platform/public/segmentation_platform_service.h"
#import "components/sessions/core/session_id_generator.h"
#import "components/signin/public/base/signin_pref_names.h"
#import "components/signin/public/identity_manager/identity_manager.h"
#import "components/strings/grit/components_locale_settings.h"
#import "components/sync/base/sync_prefs.h"
#import "components/sync/service/glue/sync_transport_data_prefs.h"
#import "components/sync_device_info/device_info_prefs.h"
#import "components/sync_sessions/session_sync_prefs.h"
#import "components/translate/core/browser/translate_pref_names.h"
#import "components/translate/core/browser/translate_prefs.h"
#import "components/unified_consent/unified_consent_service.h"
#import "components/update_client/update_client.h"
#import "components/variations/service/variations_service.h"
#import "components/web_resource/web_resource_pref_names.h"
#import "ios/chrome/app/variations_app_state_agent.h"
#import "ios/chrome/browser/first_run/first_run.h"
#import "ios/chrome/browser/memory/memory_debugger_manager.h"
#import "ios/chrome/browser/metrics/ios_chrome_metrics_service_client.h"
#import "ios/chrome/browser/ntp/set_up_list_prefs.h"
#import "ios/chrome/browser/policy/policy_util.h"
#import "ios/chrome/browser/prerender/prerender_pref.h"
#import "ios/chrome/browser/push_notification/push_notification_service.h"
#import "ios/chrome/browser/shared/model/browser_state/browser_state_info_cache.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/ui/authentication/signin/signin_coordinator.h"
#import "ios/chrome/browser/ui/authentication/signin_promo_view_mediator.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_mediator.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_path_cache.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_mediator.h"
#import "ios/chrome/browser/ui/incognito_reauth/incognito_reauth_scene_agent.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_field_trial.h"
#import "ios/chrome/browser/ui/ntp/synced_segments_field_trial.h"
#import "ios/chrome/browser/voice/voice_search_prefs_registration.h"
#import "ios/chrome/browser/web/font_size/font_size_tab_helper.h"
#import "ios/web/common/features.h"
#import "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Deprecated 09/2021
const char kTrialGroupPrefName[] = "location_permissions.trial_group";

// Deprecated 10/2021
const char kSigninBottomSheetShownCount[] =
    "ios.signin.bottom_sheet_shown_count";

// Deprecated 03/2022
const char kShowReadingListInBookmarkBar[] = "bookmark_bar.show_reading_list";

// Deprecated 03/2022
const char kPrefReadingListMessagesNeverShow[] =
    "reading_list_message_never_show";

// Deprecated 04/2022
const char kFRETrialGroupPrefName[] = "fre_refactoring.trial_group";
const char kOptimizationGuideRemoteFetchingEnabled[] =
    "optimization_guide.fetching_enabled";

// Deprecated 05/2022.
const char kTrialGroupV3PrefName[] = "fre_refactoringV3.trial_group";

// Deprecated 05/2022.
const char kAccountIdMigrationState[] = "account_id_migration_state";

// Deprecated 09/2022.
const char kDataSaverEnabled[] = "spdy_proxy.enabled";

// Deprecated 09/2022.
const char kPrefPromoObject[] = "ios.ntppromo";

// Deprecated 11/2022.
const char kLocalConsentsDictionary[] = "local_consents";

// Deprecated 12/2022.
const char kDeprecatedReadingListHasUnseenEntries[] =
    "reading_list.has_unseen_entries";

// Deprecated 01/2023.
const char* kTrialGroupMICeAndDefaultBrowserVersionPrefName =
    "fre_refactoring_mice_and_default_browser.trial_version";

// Deprecated 04/2023.
const char kTrialPrefName[] = "trending_queries.trial_version";
}  // namespace

void RegisterLocalStatePrefs(PrefRegistrySimple* registry) {
  BrowserStateInfoCache::RegisterPrefs(registry);
  flags_ui::PrefServiceFlagsStorage::RegisterPrefs(registry);
  signin::IdentityManager::RegisterLocalStatePrefs(registry);
  IOSChromeMetricsServiceClient::RegisterPrefs(registry);
  metrics::RegisterDemographicsLocalStatePrefs(registry);
  network_time::NetworkTimeTracker::RegisterPrefs(registry);
  policy::BrowserPolicyConnector::RegisterPrefs(registry);
  policy::PolicyStatisticsCollector::RegisterPrefs(registry);
  PrefProxyConfigTrackerImpl::RegisterPrefs(registry);
  sessions::SessionIdGenerator::RegisterPrefs(registry);
  set_up_list_prefs::RegisterPrefs(registry);
  update_client::RegisterPrefs(registry);
  variations::VariationsService::RegisterPrefs(registry);
  new_tab_page_field_trial::RegisterLocalStatePrefs(registry);
  synced_segments_field_trial::RegisterLocalStatePrefs(registry);
  component_updater::RegisterComponentUpdateServicePrefs(registry);
  component_updater::AutofillStatesComponentInstallerPolicy::RegisterPrefs(
      registry);
  segmentation_platform::SegmentationPlatformService::RegisterLocalStatePrefs(
      registry);
  optimization_guide::prefs::RegisterLocalStatePrefs(registry);

  // Preferences related to the browser state manager.
  registry->RegisterStringPref(prefs::kBrowserStateLastUsed, std::string());
  registry->RegisterIntegerPref(prefs::kBrowserStatesNumCreated, 1);
  registry->RegisterListPref(prefs::kBrowserStatesLastActive);

  [MemoryDebuggerManager registerLocalState:registry];
  [IncognitoReauthSceneAgent registerLocalState:registry];
  [VariationsAppStateAgent registerLocalState:registry];

  registry->RegisterBooleanPref(prefs::kBrowsingDataMigrationHasBeenPossible,
                                false);

  // Preferences related to the application context.
  registry->RegisterStringPref(language::prefs::kApplicationLocale,
                               std::string());
  registry->RegisterBooleanPref(prefs::kEulaAccepted, false);
  registry->RegisterBooleanPref(metrics::prefs::kMetricsReportingEnabled,
                                false);

  registry->RegisterDictionaryPref(prefs::kIosPreRestoreAccountInfo);

  registry->RegisterListPref(prefs::kIosPromosManagerActivePromos);
  registry->RegisterListPref(prefs::kIosPromosManagerImpressions);
  registry->RegisterListPref(prefs::kIosPromosManagerSingleDisplayActivePromos);
  registry->RegisterDictionaryPref(
      prefs::kIosPromosManagerSingleDisplayPendingPromos);

  registry->RegisterBooleanPref(enterprise_reporting::kCloudReportingEnabled,
                                false);
  registry->RegisterTimePref(enterprise_reporting::kLastUploadTimestamp,
                             base::Time());
  registry->RegisterTimePref(
      enterprise_reporting::kLastUploadSucceededTimestamp, base::Time());
  registry->RegisterTimeDeltaPref(
      enterprise_reporting::kCloudReportingUploadFrequency, base::Hours(24));

  registry->RegisterDictionaryPref(prefs::kOverflowMenuDestinationUsageHistory,
                                   PrefRegistry::LOSSY_PREF);
  registry->RegisterListPref(prefs::kOverflowMenuNewDestinations,
                             PrefRegistry::LOSSY_PREF);
  registry->RegisterListPref(prefs::kOverflowMenuDestinationsOrder);

  // Preferences related to Enterprise policies.
  registry->RegisterListPref(prefs::kRestrictAccountsToPatterns);
  registry->RegisterIntegerPref(prefs::kBrowserSigninPolicy,
                                static_cast<int>(BrowserSigninMode::kEnabled));
  registry->RegisterBooleanPref(prefs::kAppStoreRatingPolicyEnabled, true);

  registry->RegisterBooleanPref(prefs::kLensCameraAssistedSearchPolicyAllowed,
                                true);

  registry->RegisterIntegerPref(kTrialGroupPrefName, 0);

  registry->RegisterIntegerPref(kSigninBottomSheetShownCount, 0);

  registry->RegisterIntegerPref(kFRETrialGroupPrefName, 0);

  registry->RegisterIntegerPref(kTrialGroupV3PrefName, 0);

  registry->RegisterDictionaryPref(kPrefPromoObject);

  // Registers prefs to count the remaining number of times autofill branding
  // animation should perform. Defaults to 2, which is the maximum number of
  // times a user should see autofill branding animation after installation.
  registry->RegisterIntegerPref(
      prefs::kAutofillBrandingIconAnimationRemainingCountPrefName, 2);

  registry->RegisterDictionaryPref(kLocalConsentsDictionary);

  registry->RegisterIntegerPref(kTrialGroupMICeAndDefaultBrowserVersionPrefName,
                                -1);

  registry->RegisterIntegerPref(
      prefs::kIosCredentialProviderPromoLastActionTaken, -1);

  registry->RegisterBooleanPref(prefs::kIosCredentialProviderPromoStopPromo,
                                false);

  registry->RegisterIntegerPref(prefs::kIosCredentialProviderPromoSource, 0);

  registry->RegisterBooleanPref(
      prefs::kIosCredentialProviderPromoHasRegisteredWithPromoManager, false);

  registry->RegisterBooleanPref(prefs::kIosCredentialProviderPromoPolicyEnabled,
                                true);
  // Preferences related to tab grid.
  // Default to 0 which is the unassigned value.
  registry->RegisterIntegerPref(prefs::kInactiveTabsTimeThreshold, 0);

  registry->RegisterIntegerPref(prefs::kIosSyncSegmentsNewTabPageDisplayCount,
                                0);
}

void RegisterBrowserStatePrefs(user_prefs::PrefRegistrySyncable* registry) {
  autofill::prefs::RegisterProfilePrefs(registry);
  commerce::RegisterPrefs(registry);
  dom_distiller::DistilledPagePrefs::RegisterProfilePrefs(registry);
  ios_feed::RegisterProfilePrefs(registry);
  FirstRun::RegisterProfilePrefs(registry);
  FontSizeTabHelper::RegisterBrowserStatePrefs(registry);
  HostContentSettingsMap::RegisterProfilePrefs(registry);
  invalidation::InvalidatorRegistrarWithMemory::RegisterProfilePrefs(registry);
  invalidation::PerUserTopicSubscriptionManager::RegisterProfilePrefs(registry);
  language::LanguagePrefs::RegisterProfilePrefs(registry);
  metrics::RegisterDemographicsProfilePrefs(registry);
  ntp_tiles::MostVisitedSites::RegisterProfilePrefs(registry);
  ntp_tiles::PopularSitesImpl::RegisterProfilePrefs(registry);
  optimization_guide::prefs::RegisterProfilePrefs(registry);
  password_manager::PasswordManager::RegisterProfilePrefs(registry);
  payments::RegisterProfilePrefs(registry);
  policy::URLBlocklistManager::RegisterProfilePrefs(registry);
  PrefProxyConfigTrackerImpl::RegisterProfilePrefs(registry);
  PushNotificationService::RegisterBrowserStatePrefs(registry);
  RegisterVoiceSearchBrowserStatePrefs(registry);
  safe_browsing::RegisterProfilePrefs(registry);
  segmentation_platform::SegmentationPlatformService::RegisterProfilePrefs(
      registry);
  segmentation_platform::DeviceSwitcherResultDispatcher::RegisterProfilePrefs(
      registry);
  sync_sessions::SessionSyncPrefs::RegisterProfilePrefs(registry);
  syncer::DeviceInfoPrefs::RegisterProfilePrefs(registry);
  syncer::SyncPrefs::RegisterProfilePrefs(registry);
  syncer::SyncTransportDataPrefs::RegisterProfilePrefs(registry);
  TemplateURLPrepopulateData::RegisterProfilePrefs(registry);
  translate::TranslatePrefs::RegisterProfilePrefs(registry);
  unified_consent::UnifiedConsentService::RegisterPrefs(registry);
  variations::VariationsService::RegisterProfilePrefs(registry);
  ZeroSuggestProvider::RegisterProfilePrefs(registry);

  [BookmarkMediator registerBrowserStatePrefs:registry];
  [BookmarkPathCache registerBrowserStatePrefs:registry];
  [ContentSuggestionsMediator registerBrowserStatePrefs:registry];
  [HandoffManager registerBrowserStatePrefs:registry];
  [SigninCoordinator registerBrowserStatePrefs:registry];
  [SigninPromoViewMediator registerBrowserStatePrefs:registry];

  registry->RegisterBooleanPref(prefs::kBottomOmnibox, true);
  registry->RegisterBooleanPref(kDataSaverEnabled, false);
  registry->RegisterBooleanPref(
      prefs::kEnableDoNotTrack, false,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterBooleanPref(
      translate::prefs::kOfferTranslateEnabled, true,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterBooleanPref(
      prefs::kTrackPricesOnTabsEnabled, true,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterBooleanPref(
      prefs::kNTPContentSuggestionsEnabled, true,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterBooleanPref(
      prefs::kArticlesForYouEnabled, true,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterBooleanPref(
      prefs::kNTPContentSuggestionsForSupervisedUserEnabled, true,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);

  registry->RegisterStringPref(prefs::kDefaultCharset,
                               l10n_util::GetStringUTF8(IDS_DEFAULT_ENCODING),
                               user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterStringPref(prefs::kContextualSearchEnabled, std::string(),
                               user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterBooleanPref(
      prefs::kSearchSuggestEnabled, true,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterBooleanPref(prefs::kSavingBrowserHistoryDisabled, false);

  // Register pref used to show the link preview.
  registry->RegisterBooleanPref(prefs::kLinkPreviewEnabled, true);

  // This comes from components/bookmarks/core/browser/bookmark_model.h
  // Defaults to 3, which is the id of bookmarkModel_->mobile_node()
  registry->RegisterInt64Pref(prefs::kNtpShownBookmarksFolder, 3);

  // The Following feed sort type comes from
  // ios/chrome/browser/discover_feed/feed_constants.h Defaults to 1, which is
  // grouped by publisher.
  registry->RegisterIntegerPref(prefs::kNTPFollowingFeedSortType, 1);

  // Register pref to determine if the user changed the Following sort type.
  registry->RegisterBooleanPref(prefs::kDefaultFollowingFeedSortTypeChanged,
                                false);

  // Register prefs used by Clear Browsing Data UI.
  browsing_data::prefs::RegisterBrowserUserPrefs(registry);

  registry->RegisterStringPref(prefs::kNewTabPageLocationOverride,
                               std::string());

  registry->RegisterIntegerPref(
      policy::policy_prefs::kIncognitoModeAvailability,
      static_cast<int>(IncognitoModePrefs::kEnabled));

  registry->RegisterBooleanPref(prefs::kPrintingEnabled, true);

  registry->RegisterBooleanPref(prefs::kAllowChromeDataInBackups, true);

  registry->RegisterBooleanPref(kShowReadingListInBookmarkBar, true);

  registry->RegisterBooleanPref(kOptimizationGuideRemoteFetchingEnabled, true);

  // Register HTTPS related settings.
  registry->RegisterBooleanPref(prefs::kHttpsOnlyModeEnabled, false);
  registry->RegisterBooleanPref(prefs::kMixedContentAutoupgradeEnabled, true);

  // Register pref storing whether the Incognito interstitial for third-party
  // intents is enabled.
  registry->RegisterBooleanPref(prefs::kIncognitoInterstitialEnabled, false);

  // Register pref used to determine whether the User Policy notification was
  // already shown.
  registry->RegisterBooleanPref(
      policy::policy_prefs::kUserPolicyNotificationWasShown, false);

  registry->RegisterIntegerPref(kAccountIdMigrationState, 0);

  registry->RegisterIntegerPref(prefs::kIosShareChromeCount, 0,
                                PrefRegistry::LOSSY_PREF);
  registry->RegisterTimePref(prefs::kIosShareChromeLastShare, base::Time(),
                             PrefRegistry::LOSSY_PREF);

  registry->RegisterDictionaryPref(kPrefPromoObject);

  // Register pref storing whether Web Inspector support is enabled.
  registry->RegisterBooleanPref(prefs::kWebInspectorEnabled, false);

  // Register prerender network prediction preferences.
  registry->RegisterIntegerPref(
      prefs::kNetworkPredictionSetting,
      base::to_underlying(
          prerender_prefs::NetworkPredictionSetting::kEnabledWifiOnly),
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);

  // Register pref used to determine if the Price Tracking UI has been shown.
  registry->RegisterBooleanPref(prefs::kPriceNotificationsHasBeenShown, false);

  // Register pref used to count the number of consecutive times the password
  // bottom sheet has been dismissed.
  registry->RegisterIntegerPref(prefs::kIosPasswordBottomSheetDismissCount, 0);

  // Register pref used to determine if Browser Lockdown Mode is enabled.
  registry->RegisterBooleanPref(prefs::kBrowserLockdownModeEnabled, false);

  ntp_snippets::prefs::RegisterProfilePrefsForMigrationApril2023(registry);

  registry->RegisterBooleanPref(kDeprecatedReadingListHasUnseenEntries, false);
}

// This method should be periodically pruned of year+ old migrations.
void MigrateObsoleteLocalStatePrefs(PrefService* prefs) {
  // Added 09/2021
  prefs->ClearPref(kTrialGroupPrefName);

  // Added 10/2021
  prefs->ClearPref(kSigninBottomSheetShownCount);

  // Added 04/2022
  prefs->ClearPref(kFRETrialGroupPrefName);

  // Added 05/2022
  prefs->ClearPref(kTrialGroupV3PrefName);

  // Added 09/2022
  prefs->ClearPref(kPrefPromoObject);

  // Added 11/2022.
  prefs->ClearPref(kLocalConsentsDictionary);

  // Added 01/2023
  prefs->ClearPref(kTrialGroupMICeAndDefaultBrowserVersionPrefName);

  // Added 04/2023
  if (prefs->FindPreference(kTrialPrefName)) {
    prefs->ClearPref(kTrialPrefName);
  }
}

// This method should be periodically pruned of year+ old migrations.
void MigrateObsoleteBrowserStatePrefs(PrefService* prefs) {
  // Check MigrateDeprecatedAutofillPrefs() to see if this is safe to remove.
  autofill::prefs::MigrateDeprecatedAutofillPrefs(prefs);

  // Added 03/2022
  prefs->ClearPref(kShowReadingListInBookmarkBar);

  // Added 3/2022.
  if (prefs->FindPreference(kPrefReadingListMessagesNeverShow)) {
    prefs->ClearPref(kPrefReadingListMessagesNeverShow);
  }

  // Added 4/2022.
  prefs->ClearPref(kOptimizationGuideRemoteFetchingEnabled);

  // Added 05/2022
  prefs->ClearPref(kAccountIdMigrationState);

  // Added 09/2022
  prefs->ClearPref(kPrefPromoObject);

  // Added 06/2022.
  syncer::SyncPrefs::MigrateSyncRequestedPrefPostMice(prefs);

  // Added 09/2022
  prefs->ClearPref(kDataSaverEnabled);

  // Added 10/2022.
  if (prefs->HasPrefPath(prefs::kGoogleServicesLastAccountIdDeprecated)) {
    std::string account_id =
        prefs->GetString(prefs::kGoogleServicesLastAccountIdDeprecated);
    prefs->ClearPref(prefs::kGoogleServicesLastAccountIdDeprecated);
    DCHECK_EQ(account_id.find('@'), std::string::npos)
        << "kGoogleServicesLastAccountId is not expected to be an email: "
        << account_id;
    if (!account_id.empty()) {
      prefs->SetString(prefs::kGoogleServicesLastGaiaId, account_id);
    }
  }

  // Added 12/2022.
  prefs->ClearPref(kDeprecatedReadingListHasUnseenEntries);

  // Added 04/2023.
  ntp_snippets::prefs::MigrateObsoleteProfilePrefsApril2023(prefs);
}
