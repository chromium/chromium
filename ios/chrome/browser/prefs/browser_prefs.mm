// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/prefs/browser_prefs.h"

#include "base/time/time.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/browsing_data/core/pref_names.h"
#include "components/component_updater/component_updater_service.h"
#include "components/component_updater/installer_policies/autofill_states_component_installer.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/dom_distiller/core/distilled_page_prefs.h"
#include "components/enterprise/browser/reporting/common_pref_names.h"
#include "components/feed/core/v2/public/ios/pref_names.h"
#include "components/flags_ui/pref_service_flags_storage.h"
#import "components/handoff/handoff_manager.h"
#include "components/history/core/common/pref_names.h"
#include "components/invalidation/impl/invalidator_registrar_with_memory.h"
#include "components/invalidation/impl/per_user_topic_subscription_manager.h"
#include "components/language/core/browser/language_prefs.h"
#include "components/language/core/browser/pref_names.h"
#include "components/metrics/demographics/user_demographics.h"
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
#import "components/optimization_guide/core/optimization_guide_prefs.h"
#include "components/password_manager/core/browser/password_manager.h"
#include "components/payments/core/payment_prefs.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/browser/url_blocklist_manager.h"
#import "components/policy/core/common/policy_pref_names.h"
#include "components/policy/core/common/policy_statistics_collector.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/proxy_config/pref_proxy_config_tracker_impl.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/search_engines/template_url_prepopulate_data.h"
#include "components/sessions/core/session_id_generator.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/strings/grit/components_locale_settings.h"
#include "components/sync/base/sync_prefs.h"
#include "components/sync/driver/glue/sync_transport_data_prefs.h"
#include "components/sync_device_info/device_info_prefs.h"
#include "components/sync_sessions/session_sync_prefs.h"
#include "components/translate/core/browser/translate_pref_names.h"
#include "components/translate/core/browser/translate_prefs.h"
#include "components/unified_consent/unified_consent_service.h"
#include "components/update_client/update_client.h"
#include "components/variations/service/variations_service.h"
#include "components/web_resource/web_resource_pref_names.h"
#include "ios/chrome/browser/browser_state/browser_state_info_cache.h"
#include "ios/chrome/browser/first_run/first_run.h"
#import "ios/chrome/browser/memory/memory_debugger_manager.h"
#import "ios/chrome/browser/metrics/ios_chrome_metrics_service_client.h"
#include "ios/chrome/browser/notification_promo.h"
#import "ios/chrome/browser/policy/policy_util.h"
#include "ios/chrome/browser/pref_names.h"
#include "ios/chrome/browser/prerender/prerender_pref.h"
#import "ios/chrome/browser/ui/authentication/signin/signin_coordinator.h"
#import "ios/chrome/browser/ui/authentication/signin_promo_view_mediator.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_mediator.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_path_cache.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_utils_ios.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_mediator.h"
#include "ios/chrome/browser/ui/first_run/fre_field_trial.h"
#import "ios/chrome/browser/ui/incognito_reauth/incognito_reauth_scene_agent.h"
#include "ios/chrome/browser/voice/voice_search_prefs_registration.h"
#import "ios/chrome/browser/web/font_size/font_size_tab_helper.h"
#import "ios/web/common/features.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
const char kLastKnownGoogleURL[] = "browser.last_known_google_url";
const char kLastPromptedGoogleURL[] = "browser.last_prompted_google_url";

// Deprecated 9/2019
const char kGoogleServicesUsername[] = "google.services.username";
const char kGoogleServicesUserAccountId[] = "google.services.user_account_id";

// Deprecated 1/2020
const char kGCMChannelStatus[] = "gcm.channel_status";
const char kGCMChannelPollIntervalSeconds[] = "gcm.poll_interval";
const char kGCMChannelLastCheckTime[] = "gcm.check_time";

// Deprecated 2/2020
const char kInvalidatorClientId[] = "invalidator.client_id";
const char kInvalidatorInvalidationState[] = "invalidator.invalidation_state";
const char kInvalidatorSavedInvalidations[] = "invalidator.saved_invalidations";

// Deprecated 9/2020
const char kPasswordManagerOnboardingState[] =
    "profile.password_manager_onboarding_state";
const char kWasOnboardingFeatureCheckedBefore[] =
    "profile.was_pwm_onboarding_feature_checked_before";

// Deprecated 12/2020
const char kDomainsWithCookiePref[] = "signin.domains_with_cookie";

// Deprecated 03/2021
const char kOmniboxGeolocationAuthorizationState[] =
    "ios.omnibox.geolocation_authorization_state";
const char kOmniboxGeolocationLastAuthorizationAlertVersion[] =
    "ios.omnibox.geolocation_last_authorization_alert_version";

// Deprecated 07/2021
const char kMetricsReportingWifiOnly[] =
    "ios.user_experience_metrics.wifi_only";

// Deprecated 07/2021
const char kLastSessionExitedCleanly[] =
    "ios.user_experience_metrics.last_session_exited_cleanly";

// Deprecated 08/2021
const char kSigninAllowedByPolicy[] = "signin.allowed_by_policy";

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
}  // namespace

void RegisterLocalStatePrefs(PrefRegistrySimple* registry) {
  BrowserStateInfoCache::RegisterPrefs(registry);
  flags_ui::PrefServiceFlagsStorage::RegisterPrefs(registry);
  signin::IdentityManager::RegisterLocalStatePrefs(registry);
  IOSChromeMetricsServiceClient::RegisterPrefs(registry);
  network_time::NetworkTimeTracker::RegisterPrefs(registry);
  ios::NotificationPromo::RegisterPrefs(registry);
  policy::BrowserPolicyConnector::RegisterPrefs(registry);
  policy::PolicyStatisticsCollector::RegisterPrefs(registry);
  PrefProxyConfigTrackerImpl::RegisterPrefs(registry);
  sessions::SessionIdGenerator::RegisterPrefs(registry);
  update_client::RegisterPrefs(registry);
  variations::VariationsService::RegisterPrefs(registry);
  fre_field_trial::RegisterLocalStatePrefs(registry);
  component_updater::RegisterComponentUpdateServicePrefs(registry);
  component_updater::AutofillStatesComponentInstallerPolicy::RegisterPrefs(
      registry);

  // Preferences related to the browser state manager.
  registry->RegisterStringPref(prefs::kBrowserStateLastUsed, std::string());
  registry->RegisterIntegerPref(prefs::kBrowserStatesNumCreated, 1);
  registry->RegisterListPref(prefs::kBrowserStatesLastActive);

  [MemoryDebuggerManager registerLocalState:registry];
  [IncognitoReauthSceneAgent registerLocalState:registry];

  registry->RegisterBooleanPref(prefs::kBrowsingDataMigrationHasBeenPossible,
                                false);

  // Preferences related to the application context.
  registry->RegisterStringPref(language::prefs::kApplicationLocale,
                               std::string());
  registry->RegisterBooleanPref(prefs::kEulaAccepted, false);
  registry->RegisterBooleanPref(metrics::prefs::kMetricsReportingEnabled,
                                false);
  registry->RegisterBooleanPref(kLastSessionExitedCleanly, true);
  registry->RegisterBooleanPref(kMetricsReportingWifiOnly, true);
  registry->RegisterBooleanPref(kGCMChannelStatus, true);
  registry->RegisterIntegerPref(kGCMChannelPollIntervalSeconds, 0);
  registry->RegisterInt64Pref(kGCMChannelLastCheckTime, 0);

  registry->RegisterListPref(kInvalidatorSavedInvalidations);
  registry->RegisterStringPref(kInvalidatorInvalidationState, std::string());
  registry->RegisterStringPref(kInvalidatorClientId, std::string());

  registry->RegisterBooleanPref(enterprise_reporting::kCloudReportingEnabled,
                                false);
  registry->RegisterTimePref(enterprise_reporting::kLastUploadTimestamp,
                             base::Time());
  registry->RegisterTimePref(
      enterprise_reporting::kLastUploadSucceededTimestamp, base::Time());
  registry->RegisterTimeDeltaPref(
      enterprise_reporting::kCloudReportingUploadFrequency, base::Hours(24));

  registry->RegisterIntegerPref(kOmniboxGeolocationAuthorizationState, 0);
  registry->RegisterStringPref(kOmniboxGeolocationLastAuthorizationAlertVersion,
                               "");

  // Preferences related to Enterprise policies.
  registry->RegisterListPref(prefs::kRestrictAccountsToPatterns);
  registry->RegisterIntegerPref(prefs::kBrowserSigninPolicy,
                                static_cast<int>(BrowserSigninMode::kEnabled));

  registry->RegisterIntegerPref(kTrialGroupPrefName, 0);

  registry->RegisterIntegerPref(kSigninBottomSheetShownCount, 0);

  registry->RegisterIntegerPref(kFRETrialGroupPrefName, 0);

  registry->RegisterIntegerPref(kTrialGroupV3PrefName, 0);
}

void RegisterBrowserStatePrefs(user_prefs::PrefRegistrySyncable* registry) {
  autofill::prefs::RegisterProfilePrefs(registry);
  dom_distiller::DistilledPagePrefs::RegisterProfilePrefs(registry);
  ios_feed::RegisterProfilePrefs(registry);
  FirstRun::RegisterProfilePrefs(registry);
  FontSizeTabHelper::RegisterBrowserStatePrefs(registry);
  HostContentSettingsMap::RegisterProfilePrefs(registry);
  invalidation::InvalidatorRegistrarWithMemory::RegisterProfilePrefs(registry);
  invalidation::PerUserTopicSubscriptionManager::RegisterProfilePrefs(registry);
  ios::NotificationPromo::RegisterProfilePrefs(registry);
  language::LanguagePrefs::RegisterProfilePrefs(registry);
  metrics::RegisterDemographicsProfilePrefs(registry);
  ntp_snippets::ClickBasedCategoryRanker::RegisterProfilePrefs(registry);
  ntp_snippets::ContentSuggestionsService::RegisterProfilePrefs(registry);
  ntp_snippets::RemoteSuggestionsProviderImpl::RegisterProfilePrefs(registry);
  ntp_snippets::RemoteSuggestionsSchedulerImpl::RegisterProfilePrefs(registry);
  ntp_snippets::RequestThrottler::RegisterProfilePrefs(registry);
  ntp_snippets::UserClassifier::RegisterProfilePrefs(registry);
  ntp_tiles::MostVisitedSites::RegisterProfilePrefs(registry);
  ntp_tiles::PopularSitesImpl::RegisterProfilePrefs(registry);
  optimization_guide::prefs::RegisterProfilePrefs(registry);
  password_manager::PasswordManager::RegisterProfilePrefs(registry);
  payments::RegisterProfilePrefs(registry);
  policy::URLBlocklistManager::RegisterProfilePrefs(registry);
  PrefProxyConfigTrackerImpl::RegisterProfilePrefs(registry);
  prerender_prefs::RegisterNetworkPredictionPrefs(registry);
  RegisterVoiceSearchBrowserStatePrefs(registry);
  safe_browsing::RegisterProfilePrefs(registry);
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

  registry->RegisterBooleanPref(prefs::kDataSaverEnabled, false);
  registry->RegisterBooleanPref(
      prefs::kEnableDoNotTrack, false,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterBooleanPref(
      translate::prefs::kOfferTranslateEnabled, true,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterBooleanPref(
      prefs::kTrackPricesOnTabsEnabled, true,
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

  // Register prefs used by Clear Browsing Data UI.
  browsing_data::prefs::RegisterBrowserUserPrefs(registry);

  registry->RegisterStringPref(kLastKnownGoogleURL, std::string());
  registry->RegisterStringPref(kLastPromptedGoogleURL, std::string());
  registry->RegisterStringPref(kGoogleServicesUsername, std::string());
  registry->RegisterStringPref(kGoogleServicesUserAccountId, std::string());
  registry->RegisterStringPref(prefs::kNewTabPageLocationOverride,
                               std::string());

  registry->RegisterBooleanPref(kGCMChannelStatus, true);
  registry->RegisterIntegerPref(kGCMChannelPollIntervalSeconds, 0);
  registry->RegisterInt64Pref(kGCMChannelLastCheckTime, 0);

  registry->RegisterIntegerPref(prefs::kIncognitoModeAvailability,
                                static_cast<int>(IncognitoModePrefs::kEnabled));

  registry->RegisterListPref(kInvalidatorSavedInvalidations);
  registry->RegisterStringPref(kInvalidatorInvalidationState, std::string());
  registry->RegisterStringPref(kInvalidatorClientId, std::string());

  registry->RegisterBooleanPref(prefs::kPrintingEnabled, true);

  registry->RegisterIntegerPref(kPasswordManagerOnboardingState, 0);
  registry->RegisterBooleanPref(kWasOnboardingFeatureCheckedBefore, false);
  registry->RegisterDictionaryPref(kDomainsWithCookiePref);

  registry->RegisterBooleanPref(prefs::kAllowChromeDataInBackups, true);

  // Preference related to the browser sign-in policy that is being deprecated.
  registry->RegisterBooleanPref(kSigninAllowedByPolicy, true);

  registry->RegisterBooleanPref(kShowReadingListInBookmarkBar, true);

  registry->RegisterBooleanPref(kOptimizationGuideRemoteFetchingEnabled, true);

  registry->RegisterBooleanPref(prefs::kHttpsOnlyModeEnabled, false);

  // Register pref used to determine whether the User Policy notification was
  // already shown.
  registry->RegisterBooleanPref(
      policy::policy_prefs::kUserPolicyNotificationWasShown, false);
}

// This method should be periodically pruned of year+ old migrations.
void MigrateObsoleteLocalStatePrefs(PrefService* prefs) {
  // Added 1/2020.
  prefs->ClearPref(kGCMChannelStatus);
  prefs->ClearPref(kGCMChannelPollIntervalSeconds);
  prefs->ClearPref(kGCMChannelLastCheckTime);

  // Added 2/2020.
  prefs->ClearPref(kInvalidatorSavedInvalidations);
  prefs->ClearPref(kInvalidatorInvalidationState);
  prefs->ClearPref(kInvalidatorClientId);

  // Added 2021/03.
  prefs->ClearPref(kOmniboxGeolocationAuthorizationState);
  prefs->ClearPref(kOmniboxGeolocationLastAuthorizationAlertVersion);

  // Added 7/2021
  prefs->ClearPref(kMetricsReportingWifiOnly);

  // Added 7/2021
  prefs->ClearPref(kLastSessionExitedCleanly);

  // Added 09/2021
  prefs->ClearPref(kTrialGroupPrefName);

  // Added 10/2021
  prefs->ClearPref(kSigninBottomSheetShownCount);

  // Added 04/2022
  prefs->ClearPref(kFRETrialGroupPrefName);

  // Added 05/2022
  prefs->ClearPref(kTrialGroupV3PrefName);
}

// This method should be periodically pruned of year+ old migrations.
void MigrateObsoleteBrowserStatePrefs(PrefService* prefs) {
  // Check MigrateDeprecatedAutofillPrefs() to see if this is safe to remove.
  autofill::prefs::MigrateDeprecatedAutofillPrefs(prefs);

  // Added 07/2019.
  syncer::MigrateSyncSuppressedPref(prefs);
  prefs->ClearPref(kLastKnownGoogleURL);
  prefs->ClearPref(kLastPromptedGoogleURL);

  // Added 09/2019
  prefs->ClearPref(kGoogleServicesUsername);
  prefs->ClearPref(kGoogleServicesUserAccountId);

  // Added 1/2020.
  prefs->ClearPref(kGCMChannelStatus);
  prefs->ClearPref(kGCMChannelPollIntervalSeconds);
  prefs->ClearPref(kGCMChannelLastCheckTime);

  // Added 2/2020.
  prefs->ClearPref(kInvalidatorSavedInvalidations);
  prefs->ClearPref(kInvalidatorInvalidationState);
  prefs->ClearPref(kInvalidatorClientId);

  // Added 9/2020.
  prefs->ClearPref(kPasswordManagerOnboardingState);
  prefs->ClearPref(kWasOnboardingFeatureCheckedBefore);
  prerender_prefs::MigrateNetworkPredictionPrefs(prefs);

  // Added 12/2020.
  prefs->ClearPref(kDomainsWithCookiePref);

  // Added 8/2021.
  prefs->ClearPref(kSigninAllowedByPolicy);

  // Added 03/2022
  prefs->ClearPref(kShowReadingListInBookmarkBar);

  // Added 3/2022.
  if (prefs->FindPreference(kPrefReadingListMessagesNeverShow)) {
    prefs->ClearPref(kPrefReadingListMessagesNeverShow);
  }

  // Added 4/2022.
  prefs->ClearPref(kOptimizationGuideRemoteFetchingEnabled);
}
