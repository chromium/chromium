// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/model/prefs/browser_prefs.h"

#import "base/apple/foundation_util.h"
#import "base/containers/contains.h"
#import "base/json/values_util.h"
#import "base/strings/sys_string_conversions.h"
#import "base/threading/thread_restrictions.h"
#import "base/time/time.h"
#import "base/types/cxx23_to_underlying.h"
#import "base/values.h"
#import "components/autofill/core/common/autofill_prefs.h"
#import "components/breadcrumbs/core/breadcrumbs_status.h"
#import "components/browser_sync/sync_to_signin_migration.h"
#import "components/browsing_data/core/pref_names.h"
#import "components/collaboration/public/pref_names.h"
#import "components/commerce/core/pref_names.h"
#import "components/component_updater/component_updater_service.h"
#import "components/component_updater/installer_policies/autofill_states_component_installer.h"
#import "components/content_settings/core/browser/host_content_settings_map.h"
#import "components/dom_distiller/core/distilled_page_prefs.h"
#import "components/enterprise/browser/identifiers/identifiers_prefs.h"
#import "components/enterprise/browser/reporting/common_pref_names.h"
#import "components/enterprise/connectors/core/connectors_prefs.h"
#import "components/enterprise/idle/idle_pref_names.h"
#import "components/feed/core/v2/public/ios/pref_names.h"
#import "components/handoff/handoff_manager.h"
#import "components/history/core/common/pref_names.h"
#import "components/image_fetcher/core/cache/image_cache.h"
#import "components/invalidation/impl/fcm_invalidation_service.h"
#import "components/invalidation/impl/invalidator_registrar_with_memory.h"
#import "components/invalidation/impl/per_user_topic_subscription_manager.h"
#import "components/language/core/browser/language_prefs.h"
#import "components/language/core/browser/pref_names.h"
#import "components/lens/lens_overlay_permission_utils.h"
#import "components/metrics/demographics/user_demographics.h"
#import "components/metrics/metrics_pref_names.h"
#import "components/network_time/network_time_tracker.h"
#import "components/ntp_tiles/most_visited_sites.h"
#import "components/ntp_tiles/popular_sites_impl.h"
#import "components/omnibox/browser/omnibox_prefs.h"
#import "components/omnibox/browser/zero_suggest_provider.h"
#import "components/optimization_guide/core/model_execution/model_execution_prefs.h"
#import "components/optimization_guide/core/optimization_guide_prefs.h"
#import "components/password_manager/core/browser/password_manager.h"
#import "components/password_manager/core/common/password_manager_pref_names.h"
#import "components/payments/core/payment_prefs.h"
#import "components/plus_addresses/plus_address_prefs.h"
#import "components/policy/core/browser/browser_policy_connector.h"
#import "components/policy/core/browser/url_blocklist_manager.h"
#import "components/policy/core/common/local_test_policy_provider.h"
#import "components/policy/core/common/policy_pref_names.h"
#import "components/policy/core/common/policy_statistics_collector.h"
#import "components/pref_registry/pref_registry_syncable.h"
#import "components/prefs/pref_service.h"
#import "components/proxy_config/pref_proxy_config_tracker_impl.h"
#import "components/regional_capabilities/regional_capabilities_prefs.h"
#import "components/safe_browsing/core/common/safe_browsing_prefs.h"
#import "components/saved_tab_groups/public/pref_names.h"
#import "components/search_engines/template_url_prepopulate_data.h"
#import "components/segmentation_platform/embedder/default_model/device_switcher_result_dispatcher.h"
#import "components/segmentation_platform/embedder/home_modules/tips_manager/tips_manager.h"
#import "components/segmentation_platform/public/segmentation_platform_service.h"
#import "components/send_tab_to_self/pref_names.h"
#import "components/sessions/core/session_id_generator.h"
#import "components/sharing_message/sharing_sync_preference.h"
#import "components/signin/core/browser/active_primary_accounts_metrics_recorder.h"
#import "components/signin/public/base/signin_pref_names.h"
#import "components/signin/public/base/signin_prefs.h"
#import "components/signin/public/identity_manager/identity_manager.h"
#import "components/strings/grit/components_locale_settings.h"
#import "components/supervised_user/core/browser/supervised_user_metrics_service.h"
#import "components/supervised_user/core/browser/supervised_user_preferences.h"
#import "components/sync/service/glue/sync_transport_data_prefs.h"
#import "components/sync/service/sync_prefs.h"
#import "components/sync_device_info/device_info_prefs.h"
#import "components/sync_sessions/session_sync_prefs.h"
#import "components/translate/core/browser/translate_pref_names.h"
#import "components/translate/core/browser/translate_prefs.h"
#import "components/unified_consent/unified_consent_service.h"
#import "components/update_client/update_client.h"
#import "components/variations/service/variations_service.h"
#import "components/web_resource/web_resource_pref_names.h"
#import "components/webui/chrome_urls/pref_names.h"
#import "components/webui/flags/pref_service_flags_storage.h"
#import "ios/chrome/app/spotlight/spotlight_util.h"
#import "ios/chrome/app/variations_app_state_agent.h"
#import "ios/chrome/browser/authentication/ui_bundled/history_sync/history_sync_utils.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin/signin_coordinator.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin_promo_view_mediator.h"
#import "ios/chrome/browser/bookmarks/ui_bundled/bookmark_mediator.h"
#import "ios/chrome/browser/bookmarks/ui_bundled/bookmark_path_cache.h"
#import "ios/chrome/browser/bookmarks/ui_bundled/home/bookmarks_home_mediator.h"
#import "ios/chrome/browser/content_suggestions/ui_bundled/content_suggestions_mediator.h"
#import "ios/chrome/browser/content_suggestions/ui_bundled/price_tracking_promo/price_tracking_promo_prefs.h"
#import "ios/chrome/browser/content_suggestions/ui_bundled/safety_check/safety_check_prefs.h"
#import "ios/chrome/browser/content_suggestions/ui_bundled/shop_card/shop_card_prefs.h"
#import "ios/chrome/browser/content_suggestions/ui_bundled/tips/tips_prefs.h"
#import "ios/chrome/browser/download/model/auto_deletion/auto_deletion_service.h"
#import "ios/chrome/browser/drive/model/drive_policy.h"
#import "ios/chrome/browser/first_run/model/first_run.h"
#import "ios/chrome/browser/first_run/ui_bundled/welcome_back/model/welcome_back_prefs.h"
#import "ios/chrome/browser/incognito_reauth/ui_bundled/incognito_reauth_scene_agent.h"
#import "ios/chrome/browser/memory/model/memory_debugger_manager.h"
#import "ios/chrome/browser/metrics/model/constants.h"
#import "ios/chrome/browser/metrics/model/ios_chrome_metrics_service_client.h"
#import "ios/chrome/browser/ntp/model/set_up_list_prefs.h"
#import "ios/chrome/browser/ntp/shared/metrics/feed_metrics_constants.h"
#import "ios/chrome/browser/ntp_tiles/model/tab_resumption/tab_resumption_prefs.h"
#import "ios/chrome/browser/parcel_tracking/parcel_tracking_prefs.h"
#import "ios/chrome/browser/photos/model/photos_policy.h"
#import "ios/chrome/browser/policy/model/policy_util.h"
#import "ios/chrome/browser/prerender/model/prerender_pref.h"
#import "ios/chrome/browser/push_notification/model/push_notification_prefs.h"
#import "ios/chrome/browser/push_notification/model/push_notification_service.h"
#import "ios/chrome/browser/safety_check/model/ios_chrome_safety_check_manager_constants.h"
#import "ios/chrome/browser/settings/ui_bundled/clear_browsing_data/features.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/profile/profile_attributes_storage_ios.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/tips_notifications/model/tips_notification_client.h"
#import "ios/chrome/browser/upgrade/model/upgrade_constants.h"
#import "ios/chrome/browser/voice/model/voice_search_prefs_registration.h"
#import "ios/chrome/browser/web/model/annotations/annotations_util.h"
#import "ios/chrome/browser/web/model/font_size/font_size_tab_helper.h"
#import "ios/components/cookie_util/cookie_constants.h"
#import "ios/web/common/features.h"
#import "ui/base/l10n/l10n_util.h"

#if !BUILDFLAG(IS_IOS_MACCATALYST)
#import "ios/chrome/browser/default_browser/model/default_status/default_status_helper_prefs.h"
#endif  // !BUILDFLAG(IS_IOS_MACCATALYST)

namespace {

// Deprecated 06/2024.
constexpr char kObsoletePasswordsPerAccountPrefMigrationDone[] =
    "sync.passwords_per_account_pref_migration_done";
constexpr char kObsoleteBookmarksAndReadingListAccountStorageOptIn[] =
    "sync.bookmarks_and_reading_list_account_storage_opt_in";

// Deprecated 08/2024.
const char kTrialPrefName[] = "trending_queries.trial_version";

// Deprecated 08/2024.
constexpr char kSafeBrowsingEsbOptInWithFriendlierSettings[] =
    "safebrowsing.esb_opt_in_with_friendlier_settings";

// Deprecated 09/2024.
constexpr char kContentSettingsWindowLastTabIndex[] =
    "content_settings_window.last_tab_index";
constexpr char kSyncPasswordHash[] = "profile.sync_password_hash";
constexpr char kSyncPasswordLengthAndHashSalt[] =
    "profile.sync_password_length_and_hash_salt";
constexpr char kContextualSearchEnabled[] = "search.contextual_search_enabled";
constexpr char kNtpShownBookmarksFolder[] = "ntp.shown_bookmarks_folder";
constexpr char kBrowsingDataMigrationHasBeenPossible[] =
    "ios.browsing_data_migration_controller.migration_has_been_possible";
constexpr char
    kIosMagicStackSegmentationPriceTrackingPromoImpressionsSinceFreshness[] =
        "ios.magic_stack_segmentation.price_tracking_promo_freshness";

// Deprecated 11/2024
constexpr char kEnableDoNotTrackIos[] = "enable_do_not_track";

// Deprecated 12/2024.
inline constexpr char kPageContentCollectionEnabled[] =
    "page_content_collection.enabled";

// Deprecated 02/2025.
inline constexpr char kNumberOfProfiles[] = "profile.profiles_created";
inline constexpr char kLastActiveProfiles[] = "profile.last_active_profiles";

// Deprecated 03/2025.
inline constexpr char kIosParcelTrackingOptInPromptDisplayLimitMet[] =
    "ios.parcel_tracking.opt_in_prompt_displayed";
inline constexpr char kIosParcelTrackingOptInStatus[] =
    "ios.parcel_tracking.opt_in_status";
inline constexpr char kIosParcelTrackingOptInPromptSwipedDown[] =
    "ios.parcel_tracking.opt_in_prompt_swiped_down";
inline constexpr char kIosParcelTrackingPolicyEnabled[] =
    "ios.parcel_tracking.policy_enabled";

// Deprecated 04/2025.
inline constexpr char kMixedContentAutoupgradeEnabled[] =
    "ios.mixed_content_autoupgrade_enabled";

// Deprecated 04/2025.
inline constexpr char kAutologinEnabled[] = "autologin.enabled";

// Deprecated 04/2025.
inline constexpr char kSuggestionGroupVisibility[] =
    "omnibox.suggestionGroupVisibility";

// Deprecated 05/2025.
inline constexpr char kSyncCacheGuid[] = "sync.cache_guid";
inline constexpr char kSyncBirthday[] = "sync.birthday";
inline constexpr char kSyncBagOfChips[] = "sync.bag_of_chips";
inline constexpr char kSyncLastSyncedTime[] = "sync.last_synced_time";
inline constexpr char kSyncLastPollTime[] = "sync.last_poll_time";
inline constexpr char kSyncPollInterval[] = "sync.short_poll_interval";

// Migrates a boolean pref from source to target PrefService.
void MigrateBooleanPref(std::string_view pref_name,
                        PrefService* target_pref_service,
                        PrefService* source_pref_service) {
  const PrefService::Preference* target_pref =
      target_pref_service->FindPreference(pref_name);
  CHECK(target_pref);

  const PrefService::Preference* source_pref =
      source_pref_service->FindPreference(pref_name);
  CHECK(source_pref);

  // Only migrate the pref if 1. it is not set in target,
  // 2. it is not the default in source.
  if (target_pref->IsDefaultValue() && !source_pref->IsDefaultValue()) {
    target_pref_service->SetBoolean(pref_name,
                                    source_pref_service->GetBoolean(pref_name));
  }

  // In all cases, clear the pref from source.
  source_pref_service->ClearPref(pref_name);
}

// Migrates a integer pref from source to target PrefService.
void MigrateIntegerPref(std::string_view pref_name,
                        PrefService* target_pref_service,
                        PrefService* source_pref_service) {
  const PrefService::Preference* target_pref =
      target_pref_service->FindPreference(pref_name);
  CHECK(target_pref);

  const PrefService::Preference* source_pref =
      source_pref_service->FindPreference(pref_name);
  CHECK(source_pref);

  // Only migrate the pref if 1. it is not set in target,
  // 2. it is not the default in source.
  if (target_pref->IsDefaultValue() && !source_pref->IsDefaultValue()) {
    target_pref_service->SetInteger(pref_name,
                                    source_pref_service->GetInteger(pref_name));
  }

  // In all cases, clear the pref from source.
  source_pref_service->ClearPref(pref_name);
}

// Migrates a string pref from source to target PrefService.
void MigrateStringPref(std::string_view pref_name,
                       PrefService* target_pref_service,
                       PrefService* source_pref_service) {
  const PrefService::Preference* target_pref =
      target_pref_service->FindPreference(pref_name);
  CHECK(target_pref);

  const PrefService::Preference* source_pref =
      source_pref_service->FindPreference(pref_name);
  CHECK(source_pref);

  // Only migrate the pref if 1. it is not set in target,
  // 2. it is not the default in source.
  if (target_pref->IsDefaultValue() && !source_pref->IsDefaultValue()) {
    target_pref_service->SetString(pref_name,
                                   source_pref_service->GetString(pref_name));
  }

  // In all cases, clear the pref from source.
  source_pref_service->ClearPref(pref_name);
}

// Migrates a Dict pref from source to target PrefService.
void MigrateDictPref(std::string_view pref_name,
                     PrefService* target_pref_service,
                     PrefService* source_pref_service) {
  const PrefService::Preference* target_pref =
      target_pref_service->FindPreference(pref_name);
  CHECK(target_pref);

  const PrefService::Preference* source_pref =
      source_pref_service->FindPreference(pref_name);
  CHECK(source_pref);

  // Only migrate the pref if 1. it is not set in target,
  // 2. it is not the default in source.
  if (target_pref->IsDefaultValue() && !source_pref->IsDefaultValue()) {
    target_pref_service->SetDict(
        pref_name, source_pref_service->GetDict(pref_name).Clone());
  }

  // In all cases, clear the pref from source.
  source_pref_service->ClearPref(pref_name);
}

// Migrates a Time pref from source to target PrefService.
void MigrateTimePref(std::string_view pref_name,
                     PrefService* target_pref_service,
                     PrefService* source_pref_service) {
  const PrefService::Preference* target_pref =
      target_pref_service->FindPreference(pref_name);
  CHECK(target_pref);

  const PrefService::Preference* source_pref =
      source_pref_service->FindPreference(pref_name);
  CHECK(source_pref);

  // Only migrate the pref if 1. it is not set in target,
  // 2. it is not the default in source.
  if (target_pref->IsDefaultValue() && !source_pref->IsDefaultValue()) {
    target_pref_service->SetTime(pref_name,
                                 source_pref_service->GetTime(pref_name));
  }

  // In all cases, clear the pref from source.
  source_pref_service->ClearPref(pref_name);
}

// Helper function migrating the `string` preference from LocalState prefs to
// Profile prefs.
void MigrateStringPrefFromLocalStatePrefsToProfilePrefs(
    std::string_view pref_name,
    PrefService* profile_pref_service) {
  MigrateStringPref(pref_name, profile_pref_service,
                    GetApplicationContext()->GetLocalState());
}

// Helper function migrating the `int` preference from LocalState prefs to
// Profile prefs.
void MigrateIntegerPrefFromLocalStatePrefsToProfilePrefs(
    std::string_view pref_name,
    PrefService* profile_pref_service) {
  MigrateIntegerPref(pref_name, profile_pref_service,
                     GetApplicationContext()->GetLocalState());
}

// Helper function migrating the `int` preference from Profile prefs to
// LocalState prefs.
void MigrateIntegerPrefFromProfilePrefsToLocalStatePrefs(
    std::string_view pref_name,
    PrefService* profile_pref_service) {
  MigrateIntegerPref(pref_name, GetApplicationContext()->GetLocalState(),
                     profile_pref_service);
}

// Helper function migrating the `bool` preference from LocalState prefs to
// Profile prefs.
void MigrateBooleanPrefFromLocalStatePrefsToProfilePrefs(
    std::string_view pref_name,
    PrefService* profile_pref_service) {
  MigrateBooleanPref(pref_name, profile_pref_service,
                     GetApplicationContext()->GetLocalState());
}

// Helper function migrating the `bool` preference from Profile prefs to
// LocalState prefs.
void MigrateBooleanPrefFromProfilePrefsToLocalStatePrefs(
    std::string_view pref_name,
    PrefService* profile_pref_service) {
  MigrateBooleanPref(pref_name, GetApplicationContext()->GetLocalState(),
                     profile_pref_service);
}

// Helper function migrating the `Value::Dict` preference from LocalState prefs
// to Profile prefs.
void MigrateDictionaryPrefFromLocalStatePrefsToProfilePrefs(
    std::string_view pref_name,
    PrefService* profile_pref_service) {
  MigrateDictPref(pref_name, profile_pref_service,
                  GetApplicationContext()->GetLocalState());
}

// Helper function migrating the `base::Time` preference from Profile prefs
// to LocalState prefs.
void MigrateTimePrefFromProfilePrefsToLocalStatePrefs(
    std::string_view pref_name,
    PrefService* profile_pref_service) {
  MigrateTimePref(pref_name, GetApplicationContext()->GetLocalState(),
                  profile_pref_service);
}

void MigrateBooleanFromUserDefaultsToProfilePrefs(
    NSString* user_defaults_key,
    std::string_view pref_name,
    PrefService* profile_pref_service) {
  auto* pref = profile_pref_service->FindPreference(pref_name);
  CHECK(pref);
  NSUserDefaults* defaults = [NSUserDefaults standardUserDefaults];
  // Only migrate if the pref is not set in the prefs.
  if (pref->IsDefaultValue()) {
    profile_pref_service->SetBoolean(pref_name,
                                     [defaults boolForKey:user_defaults_key]);
  }
  [defaults removeObjectForKey:user_defaults_key];
}

}  // namespace

void RegisterLocalStatePrefs(PrefRegistrySimple* registry) {
  autofill::prefs::RegisterLocalStatePrefs(registry);
  breadcrumbs::RegisterPrefs(registry);
  ProfileAttributesStorageIOS::RegisterPrefs(registry);
  chrome_urls::RegisterPrefs(registry);
  flags_ui::PrefServiceFlagsStorage::RegisterPrefs(registry);
  signin::IdentityManager::RegisterLocalStatePrefs(registry);
  IOSChromeMetricsServiceClient::RegisterPrefs(registry);
  metrics::RegisterDemographicsLocalStatePrefs(registry);
  network_time::NetworkTimeTracker::RegisterPrefs(registry);
  policy::BrowserPolicyConnector::RegisterPrefs(registry);
  policy::LocalTestPolicyProvider::RegisterLocalStatePrefs(registry);
  policy::PolicyStatisticsCollector::RegisterPrefs(registry);
  PrefProxyConfigTrackerImpl::RegisterPrefs(registry);
  segmentation_platform::TipsManager::RegisterLocalPrefs(registry);
  sessions::SessionIdGenerator::RegisterPrefs(registry);
  set_up_list_prefs::RegisterPrefs(registry);
  signin::ActivePrimaryAccountsMetricsRecorder::RegisterLocalStatePrefs(
      registry);
  tab_resumption_prefs::RegisterLocalStatePrefs(registry);
  RegisterParcelTrackingPrefs(registry);
  update_client::RegisterPrefs(registry);
  variations::VariationsService::RegisterPrefs(registry);
  component_updater::RegisterComponentUpdateServicePrefs(registry);
  component_updater::AutofillStatesComponentInstallerPolicy::RegisterPrefs(
      registry);
  segmentation_platform::SegmentationPlatformService::RegisterLocalStatePrefs(
      registry);
  optimization_guide::prefs::RegisterLocalStatePrefs(registry);
  optimization_guide::model_execution::prefs::RegisterLocalStatePrefs(registry);
  PushNotificationService::RegisterLocalStatePrefs(registry);
  TipsNotificationClient::RegisterLocalStatePrefs(registry);
  auto_deletion::AutoDeletionService::RegisterLocalStatePrefs(registry);
  push_notification_prefs::RegisterLocalStatePrefs(registry);
  RegisterWelcomeBackLocalStatePrefs(registry);

#if !BUILDFLAG(IS_IOS_MACCATALYST)
  default_status::RegisterDefaultStatusPrefs(registry);
#endif  // !BUILDFLAG(IS_IOS_MACCATALYST)

  // Preferences related to the profile manager.
  registry->RegisterStringPref(prefs::kLastUsedProfile, std::string());
  registry->RegisterBooleanPref(prefs::kLegacyProfileHidden, false);
  registry->RegisterDictionaryPref(prefs::kLegacyProfileMap);

  [MemoryDebuggerManager registerLocalState:registry];
  [IncognitoReauthSceneAgent registerLocalState:registry];
  [VariationsAppStateAgent registerLocalState:registry];

  // Preferences related to the application context.
  registry->RegisterStringPref(language::prefs::kApplicationLocale,
                               std::string());
  registry->RegisterBooleanPref(prefs::kEulaAccepted, false);
  registry->RegisterBooleanPref(metrics::prefs::kMetricsReportingEnabled,
                                false);

  registry->RegisterListPref(prefs::kIosPromosManagerActivePromos);
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
  registry->RegisterTimePref(enterprise_idle::prefs::kLastActiveTimestamp,
                             base::Time(), PrefRegistry::LOSSY_PREF);
  registry->RegisterListPref(prefs::kOverflowMenuNewDestinations,
                             PrefRegistry::LOSSY_PREF);
  registry->RegisterListPref(prefs::kOverflowMenuDestinationsOrder);
  registry->RegisterListPref(prefs::kOverflowMenuHiddenDestinations);
  registry->RegisterDictionaryPref(prefs::kOverflowMenuDestinationBadgeData);
  registry->RegisterDictionaryPref(prefs::kOverflowMenuActionsOrder);
  registry->RegisterBooleanPref(
      prefs::kOverflowMenuDestinationUsageHistoryEnabled, true);

  // Preferences related to Enterprise policies.
  registry->RegisterListPref(prefs::kRestrictAccountsToPatterns);
  registry->RegisterIntegerPref(prefs::kBrowserSigninPolicy,
                                static_cast<int>(BrowserSigninMode::kEnabled));
  registry->RegisterBooleanPref(prefs::kSigninAllowedOnDevice, true);
  registry->RegisterBooleanPref(prefs::kAppStoreRatingPolicyEnabled, true);
  registry->RegisterBooleanPref(kIosParcelTrackingPolicyEnabled, true);

  registry->RegisterBooleanPref(prefs::kLensCameraAssistedSearchPolicyAllowed,
                                true);

  // Registers prefs to count the remaining number of times autofill branding
  // animation should perform. Defaults to 2, which is the maximum number of
  // times a user should see autofill branding animation after installation.
  registry->RegisterIntegerPref(
      prefs::kAutofillBrandingIconAnimationRemainingCount, 2);
  // Register other autofill branding prefs.
  registry->RegisterIntegerPref(prefs::kAutofillBrandingIconDisplayCount, 0);

  registry->RegisterIntegerPref(
      prefs::kIosCredentialProviderPromoLastActionTaken, -1);
  registry->RegisterTimePref(prefs::kIosCredentialProviderPromoDisplayTime,
                             base::Time());

  registry->RegisterBooleanPref(prefs::kIosCredentialProviderPromoStopPromo,
                                false);

  registry->RegisterIntegerPref(prefs::kIosCredentialProviderPromoSource, 0);

  registry->RegisterBooleanPref(
      prefs::kIosCredentialProviderPromoHasRegisteredWithPromoManager, false);

  registry->RegisterBooleanPref(prefs::kIosCredentialProviderPromoPolicyEnabled,
                                true);

  registry->RegisterTimePref(prefs::kIosSuccessfulLoginWithExistingPassword,
                             base::Time());

  registry->RegisterTimePref(prefs::kIosDefaultBrowserBlueDotPromoFirstDisplay,
                             base::Time());

  registry->RegisterIntegerPref(prefs::kIosDefaultBrowserPromoLastAction, -1);

  // Preference related to the tab pickup feature.
  registry->RegisterBooleanPref(prefs::kTabPickupEnabled, true);

  // Preferences related to the new Safety Check Manager.
  registry->RegisterStringPref(
      prefs::kIosSafetyCheckManagerUpdateCheckResult,
      NameForSafetyCheckState(UpdateChromeSafetyCheckState::kDefault),
      PrefRegistry::LOSSY_PREF);
  registry->RegisterStringPref(
      prefs::kIosSafetyCheckManagerSafeBrowsingCheckResult,
      NameForSafetyCheckState(SafeBrowsingSafetyCheckState::kDefault),
      PrefRegistry::LOSSY_PREF);
  registry->RegisterTimePref(prefs::kIosSafetyCheckManagerLastRunTime,
                             base::Time(), PrefRegistry::LOSSY_PREF);
  // TODO(crbug.com/40930653): Remove this Pref when Settings Safety Check is
  // refactored to use the new Safety Check Manager.
  registry->RegisterTimePref(prefs::kIosSettingsSafetyCheckLastRunTime,
                             base::Time());

  registry->RegisterStringPref(kIOSChromeNextVersionKey, std::string());
  registry->RegisterStringPref(kIOSChromeUpgradeURLKey, std::string());
  registry->RegisterTimePref(kLastInfobarDisplayTimeKey, base::Time());

  // Bottom omnibox preferences.
  registry->RegisterBooleanPref(prefs::kBottomOmnibox, false);
  registry->RegisterBooleanPref(prefs::kBottomOmniboxByDefault, false);

  // Preferences related to the Docking Promo feature (used only if
  // `kIOSDockingPromoForEligibleUsersOnly` is enabled).
  registry->RegisterBooleanPref(prefs::kIosDockingPromoEligibilityMet, false);

  // Pref related to the Enhanced Safe Browsing Opt-in with new friendlier
  // settings UI on chrome://settings/security.
  registry->RegisterBooleanPref(kSafeBrowsingEsbOptInWithFriendlierSettings,
                                false);

  registry->RegisterTimePref(prefs::kLensLastOpened, base::Time());

  // Register pref used to determine if OS Lockdown Mode is enabled.
  registry->RegisterBooleanPref(prefs::kOSLockdownModeEnabled, false);

  // Register pref used to determine if Browser Lockdown Mode is enabled.
  registry->RegisterBooleanPref(prefs::kBrowserLockdownModeEnabled, false);

  registry->RegisterTimePref(
      prefs::kIosSafetyCheckNotificationFirstPresentTimestamp, base::Time());

  // Preferences related to the Safety Check Notifications feature.
  registry->RegisterIntegerPref(prefs::kIosSafetyCheckNotificationsLastSent,
                                -1);
  registry->RegisterIntegerPref(
      prefs::kIosSafetyCheckNotificationsLastTriggered, -1);

  // List pref that stores the positions of the Safety Check module (with
  // notifications opt-in) within the Magic Stack.
  registry->RegisterListPref(prefs::kMagicStackSafetyCheckNotificationsShown);

  password_manager::PasswordManager::RegisterLocalPrefs(registry);

  // Prefs used to skip too frequent identity confirmation snackbar prompt.
  registry->RegisterTimePref(prefs::kIdentityConfirmationSnackbarLastPromptTime,
                             base::Time());

  registry->RegisterIntegerPref(
      prefs::kIdentityConfirmationSnackbarDisplayCount, 0);

  // Register pref storing whether the Incognito interstitial for third-party
  // intents is enabled.
  registry->RegisterBooleanPref(prefs::kIncognitoInterstitialEnabled, false);

  registry->RegisterIntegerPref(prefs::kAddressBarSettingsNewBadgeShownCount,
                                0);
  registry->RegisterIntegerPref(prefs::kNTPLensEntryPointNewBadgeShownCount, 0);

  registry->RegisterIntegerPref(
      prefs::kProminenceNotificationAlertImpressionCount, 0);

  registry->RegisterIntegerPref(prefs::kChromeDataRegionSetting, 0);

  registry->RegisterBooleanPref(prefs::kYoutubeIncognitoHasBeenShown, false);

  registry->RegisterIntegerPref(
      prefs::kNTPHomeCustomizationNewBadgeImpressionCount, 0);

  registry->RegisterBooleanPref(prefs::kHasSwitchedAccountsViaWebFlow, false);

  // Prefs used to force multi-profile migration.
  registry->RegisterTimePref(
      prefs::kWaitingForMultiProfileForcedMigrationTimestamp, base::Time());

  // Deprecated 07/2024 (migrated to profile prefs).
  registry->RegisterTimePref(prefs::kTabPickupLastDisplayedTime, base::Time());
  registry->RegisterStringPref(prefs::kTabPickupLastDisplayedURL,
                               std::string());
  registry->RegisterIntegerPref(prefs::kIosSyncSegmentsNewTabPageDisplayCount,
                                0);

  // Deprecated 07/2024.
  registry->RegisterDictionaryPref(
      prefs::kIosSafetyCheckManagerInsecurePasswordCounts,
      PrefRegistry::LOSSY_PREF);

  // Deprecated 07/2024 (migrated to profile pref).
  registry->RegisterStringPref(
      prefs::kIosSafetyCheckManagerPasswordCheckResult,
      NameForSafetyCheckState(PasswordSafetyCheckState::kDefault),
      PrefRegistry::LOSSY_PREF);

  // Deprecated 08/2024.
  registry->RegisterIntegerPref(kTrialPrefName, 0);

  // Deprecated 09/2024.
  registry->RegisterBooleanPref(kBrowsingDataMigrationHasBeenPossible, false);

  // Deprecated 09/2024 (migrated to profile pref).
  registry->RegisterDictionaryPref(prefs::kIosPreRestoreAccountInfo);

  // Deprecated 02/2025.
  registry->RegisterIntegerPref(kNumberOfProfiles, 0);
  registry->RegisterListPref(kLastActiveProfiles);

  // Deprecated 02/2025 (migrated to profile prefs).
  safety_check_prefs::RegisterPrefs(registry);

  // Deprecated 02/2025 (migrated to profile prefs)
  registry->RegisterIntegerPref(
      prefs::kIosMagicStackSegmentationMVTImpressionsSinceFreshness, -1);
  registry->RegisterIntegerPref(
      prefs::kIosMagicStackSegmentationShortcutsImpressionsSinceFreshness, -1);
  registry->RegisterIntegerPref(
      prefs::kIosMagicStackSegmentationSafetyCheckImpressionsSinceFreshness,
      -1);
  registry->RegisterIntegerPref(
      prefs::kIosMagicStackSegmentationTabResumptionImpressionsSinceFreshness,
      -1);
  registry->RegisterIntegerPref(
      prefs::kIosMagicStackSegmentationParcelTrackingImpressionsSinceFreshness,
      -1);
  registry->RegisterIntegerPref(
      kIosMagicStackSegmentationPriceTrackingPromoImpressionsSinceFreshness,
      -1);

  // Deprecated 03/2025 (migrated to profile pref).
  registry->RegisterBooleanPref(prefs::kMigrateWidgetsPrefs, false);

  // Deprecated 03/2025 (migrated to profile pref).
  registry->RegisterIntegerPref(prefs::kInactiveTabsTimeThreshold, 0);

  // Deprecated 03/2025, migrated to profile pref.
  registry->RegisterIntegerPref(
      prefs::kHomeCustomizationMagicStackSafetyCheckIssuesCount, 0);
}

void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry) {
  autofill::prefs::RegisterProfilePrefs(registry);
  collaboration::prefs::RegisterProfilePrefs(registry);
  commerce::RegisterPrefs(registry);
  dom_distiller::DistilledPagePrefs::RegisterProfilePrefs(registry);
  enterprise::RegisterIdentifiersProfilePrefs(registry);
  enterprise_connectors::RegisterProfilePrefs(registry);
  ios_feed::RegisterProfilePrefs(registry);
  FirstRun::RegisterProfilePrefs(registry);
  FontSizeTabHelper::RegisterBrowserStatePrefs(registry);
  HostContentSettingsMap::RegisterProfilePrefs(registry);
  invalidation::InvalidatorRegistrarWithMemory::RegisterProfilePrefs(registry);
  invalidation::PerUserTopicSubscriptionManager::RegisterProfilePrefs(registry);
  image_fetcher::ImageCache::RegisterProfilePrefs(registry);
  language::LanguagePrefs::RegisterProfilePrefs(registry);
  metrics::RegisterDemographicsProfilePrefs(registry);
  ntp_tiles::MostVisitedSites::RegisterProfilePrefs(registry);
  ntp_tiles::PopularSitesImpl::RegisterProfilePrefs(registry);
  optimization_guide::prefs::RegisterProfilePrefs(registry);
  optimization_guide::model_execution::prefs::RegisterProfilePrefs(registry);
  password_manager::PasswordManager::RegisterProfilePrefs(registry);
  payments::RegisterProfilePrefs(registry);
  plus_addresses::prefs::RegisterProfilePrefs(registry);
  policy::URLBlocklistManager::RegisterProfilePrefs(registry);
  PrefProxyConfigTrackerImpl::RegisterProfilePrefs(registry);
  PushNotificationService::RegisterProfilePrefs(registry);
  RegisterPriceTrackingPromoPrefs(registry);
  regional_capabilities::prefs::RegisterProfilePrefs(registry);
  shop_card_prefs::RegisterPrefs(registry);
  tips_prefs::RegisterPrefs(registry);
  RegisterVoiceSearchBrowserStatePrefs(registry);
  safe_browsing::RegisterProfilePrefs(registry);
  segmentation_platform::SegmentationPlatformService::RegisterProfilePrefs(
      registry);
  segmentation_platform::DeviceSwitcherResultDispatcher::RegisterProfilePrefs(
      registry);
  SharingSyncPreference::RegisterProfilePrefs(registry);
  SigninPrefs::RegisterProfilePrefs(registry);
  supervised_user::RegisterProfilePrefs(registry);
  supervised_user::SupervisedUserMetricsService::RegisterProfilePrefs(registry);
  sync_sessions::SessionSyncPrefs::RegisterProfilePrefs(registry);
  syncer::DeviceInfoPrefs::RegisterProfilePrefs(registry);
  syncer::SyncPrefs::RegisterProfilePrefs(registry);
  syncer::SyncTransportDataPrefs::RegisterProfilePrefs(registry);
  TemplateURLPrepopulateData::RegisterProfilePrefs(registry);
  translate::TranslatePrefs::RegisterProfilePrefs(registry);
  unified_consent::UnifiedConsentService::RegisterPrefs(registry);
  variations::VariationsService::RegisterProfilePrefs(registry);
  ZeroSuggestProvider::RegisterProfilePrefs(registry);
  tab_resumption_prefs::RegisterProfilePrefs(registry);

  [BookmarkMediator registerBrowserStatePrefs:registry];
  [BookmarkPathCache registerBrowserStatePrefs:registry];
  [BookmarksHomeMediator registerBrowserStatePrefs:registry];
  [ContentSuggestionsMediator registerProfilePrefs:registry];
  [HandoffManager registerBrowserStatePrefs:registry];
  [SigninCoordinator registerProfilePrefs:registry];
  [SigninPromoViewMediator registerProfilePrefs:registry];

  tab_groups::prefs::RegisterProfilePrefs(registry);


  registry->RegisterBooleanPref(policy::policy_prefs::kPolicyTestPageEnabled,
                                true);
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
  registry->RegisterBooleanPref(
      prefs::kSearchSuggestEnabled, true,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterBooleanPref(prefs::kSavingBrowserHistoryDisabled, false);

  // Register pref used to show the link preview.
  registry->RegisterBooleanPref(prefs::kLinkPreviewEnabled, true);

  // The Following feed sort type comes from
  // ios/chrome/browser/discover_feed/model/feed_constants.h Defaults to 2,
  // which is sort by latest.
  registry->RegisterIntegerPref(prefs::kNTPFollowingFeedSortType, 2);

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

  // Register HTTPS related settings.
  registry->RegisterBooleanPref(prefs::kHttpsOnlyModeEnabled, false);
  registry->RegisterBooleanPref(kMixedContentAutoupgradeEnabled, false);

  // Register pref used to determine whether the User Policy notification was
  // already shown.
  registry->RegisterBooleanPref(
      policy::policy_prefs::kUserPolicyNotificationWasShown, false);

  registry->RegisterBooleanPref(policy::policy_prefs::kSyncDisabledAlertShown,
                                false);

  registry->RegisterIntegerPref(prefs::kIosShareChromeCount, 0,
                                PrefRegistry::LOSSY_PREF);
  registry->RegisterTimePref(prefs::kIosShareChromeLastShare, base::Time(),
                             PrefRegistry::LOSSY_PREF);

  // Register pref storing whether Web Inspector support is enabled.
#if BUILDFLAG(CHROMIUM_BRANDING) && !defined(NDEBUG)
  // Enable it by default on debug builds
  registry->RegisterBooleanPref(prefs::kWebInspectorEnabled, true);
#else
  registry->RegisterBooleanPref(prefs::kWebInspectorEnabled, false);
#endif

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

  // Register pref used to count the number of consecutive times the password
  // generation bottom sheet has been dismissed.
  registry->RegisterIntegerPref(
      prefs::kIosPasswordGenerationBottomSheetDismissCount, 0);

  // Register pref used to detect addresses in web page
  registry->RegisterBooleanPref(prefs::kDetectAddressesEnabled, true);
  registry->RegisterBooleanPref(prefs::kDetectAddressesAccepted, false);

  // Preferences related to Save to Photos settings.
  registry->RegisterStringPref(prefs::kIosSaveToPhotosDefaultGaiaId,
                               std::string());
  registry->RegisterBooleanPref(prefs::kIosSaveToPhotosSkipAccountPicker,
                                false);
  registry->RegisterIntegerPref(
      prefs::kIosSaveToPhotosContextMenuPolicySettings,
      static_cast<int>(SaveToPhotosPolicySettings::kEnabled));

  // Preferences related to Save to Drive settings.
  registry->RegisterStringPref(prefs::kIosSaveToDriveDefaultGaiaId,
                               std::string());
  registry->RegisterIntegerPref(
      prefs::kIosSaveToDriveDownloadManagerPolicySettings,
      static_cast<int>(SaveToDrivePolicySettings::kEnabled));
  // Preferences related to Choose from Drive settings.
  registry->RegisterIntegerPref(
      prefs::kIosChooseFromDriveFilePickerPolicySettings,
      static_cast<int>(ChooseFromDrivePolicySettings::kEnabled));

  // Preferences related to download restrictions enterprise policy.
  registry->RegisterIntegerPref(policy::policy_prefs::kDownloadRestrictions, 0);

  // Preferences related to parcel tracking.
  // Deprecated 03/2025.
  registry->RegisterBooleanPref(kIosParcelTrackingOptInPromptDisplayLimitMet,
                                false);
  registry->RegisterIntegerPref(kIosParcelTrackingOptInStatus, -1);
  registry->RegisterBooleanPref(kIosParcelTrackingOptInPromptSwipedDown, false);

  // Register prefs used to skip too frequent History Sync Opt-In prompt.
  history_sync::RegisterProfilePrefs(registry);

  registry->RegisterBooleanPref(prefs::kPasswordSharingFlowHasBeenEntered,
                                false);
  // Preference related to feed.
  registry->RegisterTimePref(kActivityBucketLastReportedDateKey, base::Time());
  registry->RegisterIntegerPref(kActivityBucketKey, 0);
  registry->RegisterDoublePref(kTimeSpentInFeedAggregateKey, 0.0);
  registry->RegisterTimePref(kLastDayTimeInFeedReportedKey, base::Time());
  registry->RegisterTimePref(kLastInteractionTimeForFollowingGoodVisits,
                             base::Time());
  registry->RegisterTimePref(kLastInteractionTimeForDiscoverGoodVisits,
                             base::Time());
  registry->RegisterTimePref(kLastInteractionTimeForGoodVisits, base::Time());
  registry->RegisterDoublePref(kLongDiscoverFeedVisitTimeAggregateKey, 0.0);
  registry->RegisterDoublePref(kLongFollowingFeedVisitTimeAggregateKey, 0.0);
  registry->RegisterDoublePref(kLongFeedVisitTimeAggregateKey, 0.0);
  registry->RegisterTimePref(kArticleVisitTimestampKey, base::Time());
  registry->RegisterIntegerPref(kLastUsedFeedForGoodVisitsKey, 0);
  registry->RegisterListPref(kActivityBucketLastReportedDateArrayKey);

  registry->RegisterBooleanPref(prefs::kDetectUnitsEnabled, true);

  registry->RegisterTimePref(prefs::kLastSigninTimestamp, base::Time());

  // Preferences related to Content Notifications.
  registry->RegisterTimePref(prefs::kNotificationsPromoLastDismissed,
                             base::Time());
  registry->RegisterTimePref(prefs::kNotificationsPromoLastShown, base::Time());
  registry->RegisterIntegerPref(prefs::kNotificationsPromoTimesShown, 0);
  registry->RegisterIntegerPref(prefs::kNotificationsPromoTimesDismissed, 0);

  registry->RegisterBooleanPref(prefs::kInsecureFormWarningsEnabled, true);

  registry->RegisterTimePref(kLastCookieDeletionDate, base::Time());

  registry->RegisterDictionaryPref(prefs::kWebAnnotationsPolicy);

  // Preferences related to the tab pickup feature.
  registry->RegisterTimePref(prefs::kTabPickupLastDisplayedTime, base::Time());
  registry->RegisterStringPref(prefs::kTabPickupLastDisplayedURL,
                               std::string());

  // Pref used to store the latest Most Visited Sites to detect changes
  // to the top Most Visited Sites.
  registry->RegisterListPref(prefs::kIosLatestMostVisitedSites,
                             PrefRegistry::LOSSY_PREF);

  registry->RegisterBooleanPref(prefs::kUserAgentWasChanged, false);

  registry->RegisterTimePref(prefs::kLastApplicationStorageMetricsLogTime,
                             base::Time());

  registry->RegisterIntegerPref(spotlight::kSpotlightLastIndexingVersionKey, 0);
  registry->RegisterTimePref(spotlight::kSpotlightLastIndexingDateKey,
                             base::Time());

  registry->RegisterDictionaryPref(
      prefs::kContentNotificationsEnrollmentEligibility);

  // Registers the Home customization visibility prefs.
  registry->RegisterBooleanPref(prefs::kHomeCustomizationMostVisitedEnabled,
                                true);
  registry->RegisterBooleanPref(prefs::kHomeCustomizationMagicStackEnabled,
                                true);

  // Registers the Magic Stack module visibility prefs.
  registry->RegisterBooleanPref(
      prefs::kHomeCustomizationMagicStackSetUpListEnabled, true);
  registry->RegisterBooleanPref(
      prefs::kHomeCustomizationMagicStackSafetyCheckEnabled, true);
  registry->RegisterBooleanPref(
      prefs::kHomeCustomizationMagicStackTabResumptionEnabled, true);
  registry->RegisterBooleanPref(
      prefs::kHomeCustomizationMagicStackParcelTrackingEnabled, true);

  safety_check_prefs::RegisterPrefs(registry);

  registry->RegisterIntegerPref(
      prefs::kIosMagicStackSegmentationMVTImpressionsSinceFreshness, -1);
  registry->RegisterIntegerPref(
      prefs::kIosMagicStackSegmentationShortcutsImpressionsSinceFreshness, -1);
  registry->RegisterIntegerPref(
      prefs::kIosMagicStackSegmentationSafetyCheckImpressionsSinceFreshness,
      -1);
  registry->RegisterIntegerPref(
      prefs::kIosMagicStackSegmentationTabResumptionImpressionsSinceFreshness,
      -1);
  // Pref used to store the number of impressions of the shop card
  // module in the Home Surface since a shop card freshness signal.
  registry->RegisterIntegerPref(
      prefs::kIosMagicStackSegmentationShopCardImpressionsSinceFreshness, -1);

  // Registers a preference to store the count of displayed Safety Check issues.
  // This count determines if the Safety Check module remains in the Magic
  // Stack.
  registry->RegisterIntegerPref(
      prefs::kHomeCustomizationMagicStackSafetyCheckIssuesCount, 0);

  registry->RegisterIntegerPref(
      lens::prefs::kLensOverlaySettings,
      static_cast<int>(lens::prefs::LensOverlaySettingsPolicyValue::kEnabled));

  registry->RegisterIntegerPref(prefs::kIosSyncSegmentsNewTabPageDisplayCount,
                                0);

  registry->RegisterBooleanPref(kObsoletePasswordsPerAccountPrefMigrationDone,
                                false);

  registry->RegisterStringPref(prefs::kBrowserStateStorageIdentifier,
                               std::string());

  registry->RegisterBooleanPref(policy::policy_prefs::kForceGoogleSafeSearch,
                                false);

  registry->RegisterBooleanPref(
      kObsoleteBookmarksAndReadingListAccountStorageOptIn, false);

  // Preferences related to the new Safety Check Manager.
  registry->RegisterStringPref(
      prefs::kIosSafetyCheckManagerPasswordCheckResult,
      NameForSafetyCheckState(PasswordSafetyCheckState::kDefault),
      PrefRegistry::LOSSY_PREF);
  registry->RegisterDictionaryPref(
      prefs::kIosSafetyCheckManagerInsecurePasswordCounts,
      PrefRegistry::LOSSY_PREF);

  // Prefs migrated to localState prefs.
  registry->RegisterBooleanPref(prefs::kBottomOmnibox, false);
  registry->RegisterBooleanPref(prefs::kBottomOmniboxByDefault, false);

  // Preferences related to Lens Overlay.
  registry->RegisterBooleanPref(prefs::kLensOverlayConditionsAccepted, false);

  // Prefs related to Reminder Notifications.
  registry->RegisterDictionaryPref(prefs::kReminderNotifications);

  // Preferences related to tab grid.
  // Default to 0 which is the unassigned value.
  registry->RegisterIntegerPref(prefs::kInactiveTabsTimeThreshold, 0);

  registry->RegisterDictionaryPref(prefs::kIosPreRestoreAccountInfo);

  registry->RegisterStringPref(
      send_tab_to_self::prefs::kIOSSendTabToSelfLastReceivedTabURLPref,
      std::string());

  registry->RegisterIntegerPref(prefs::kIOSLastKnownNTPWebStateIndex, -1);

  registry->RegisterIntegerPref(prefs::kChromeDataRegionSetting, 0);

  registry->RegisterBooleanPref(prefs::kProvisionalNotificationsAllowedByPolicy,
                                true);

  registry->RegisterBooleanPref(prefs::kIOSGLICConsent, false);

  registry->RegisterTimePref(prefs::kIosSyncInfobarErrorLastDismissedTimestamp,
                             base::Time());

  // TODO(crbug.com/422744656): Remove `kAIModeSearchSuggestSettings` pref once
  // `kAIModeSettings` is implemented.
  registry->RegisterIntegerPref(omnibox::kAIModeSearchSuggestSettings, 0);
  registry->RegisterIntegerPref(omnibox::kAIModeSettings, 0);

  // Deprecated 09/2024 (migrated to localState prefs).
  registry->RegisterBooleanPref(prefs::kIncognitoInterstitialEnabled, false);

  // Deprecated 09/2024 (migrated to localState prefs).
  registry->RegisterTimePref(prefs::kIdentityConfirmationSnackbarLastPromptTime,
                             base::Time());

  // Deprecated 09/2024 (migrated to localState prefs).
  registry->RegisterIntegerPref(
      prefs::kIdentityConfirmationSnackbarDisplayCount, 0);

  // Deprecated 09/2024.
  registry->RegisterBooleanPref(prefs::kBrowserLockdownModeEnabled, false);

  // Deprecated 09/2024.
  registry->RegisterBooleanPref(prefs::kOSLockdownModeEnabled, false);

  // Deprecated 09/2024 (migrated to LocalState pref).
  registry->RegisterIntegerPref(prefs::kAddressBarSettingsNewBadgeShownCount,
                                0);

  // Deprecated 09/2024.
  registry->RegisterIntegerPref(kContentSettingsWindowLastTabIndex, 0);
  registry->RegisterStringPref(kSyncPasswordHash, std::string());
  registry->RegisterStringPref(kSyncPasswordLengthAndHashSalt, std::string());
  registry->RegisterStringPref(kContextualSearchEnabled, std::string());
  registry->RegisterInt64Pref(kNtpShownBookmarksFolder, 3);

  // Deprecated 11/2024
  registry->RegisterBooleanPref(kEnableDoNotTrackIos, false);

  // Deprecated 12/2024.
  registry->RegisterBooleanPref(kPageContentCollectionEnabled, false);

  // Deprecated 02/2025 (migrated to LocalState pref).
  registry->RegisterIntegerPref(prefs::kNTPLensEntryPointNewBadgeShownCount, 0);

  // Deprecated 02/2025 (migrated to localState prefs).
  registry->RegisterIntegerPref(
      prefs::kNTPHomeCustomizationNewBadgeImpressionCount, 0);

  // Deprecated 04/2025.
  registry->RegisterBooleanPref(kAutologinEnabled, false);

  // Deprecated 04/2025.
  registry->RegisterDictionaryPref(kSuggestionGroupVisibility);

  // Deprecated 05/2025.
  registry->RegisterStringPref(kSyncCacheGuid, std::string());
  registry->RegisterStringPref(kSyncBirthday, std::string());
  registry->RegisterStringPref(kSyncBagOfChips, std::string());
  registry->RegisterTimePref(kSyncLastSyncedTime, base::Time());
  registry->RegisterTimePref(kSyncLastPollTime, base::Time());
  registry->RegisterTimeDeltaPref(kSyncPollInterval, base::TimeDelta());
}

// This method should be periodically pruned of year+ old migrations.
void MigrateObsoleteLocalStatePrefs(PrefService* prefs) {
  // This function is not allowed to block.
  base::ScopedDisallowBlocking disallow_blocking;

  // Added 07/2024.
  prefs->ClearPref(prefs::kTabPickupEnabled);
  prefs->ClearPref(prefs::kTabPickupLastDisplayedTime);
  prefs->ClearPref(prefs::kTabPickupLastDisplayedURL);

  // Added 08/2024.
  prefs->ClearPref(kTrialPrefName);

  // Added 08/2024.
  prefs->ClearPref(kSafeBrowsingEsbOptInWithFriendlierSettings);

  // Added 09/2024.
  prefs->ClearPref(kBrowsingDataMigrationHasBeenPossible);

  // Added 09/2024
  prefs->ClearPref(
      kIosMagicStackSegmentationPriceTrackingPromoImpressionsSinceFreshness);

  // Added 02/2025
  prefs->ClearPref(kNumberOfProfiles);
  prefs->ClearPref(kLastActiveProfiles);

  // Added 02/2025
  prefs->ClearPref(
      prefs::kIosMagicStackSegmentationParcelTrackingImpressionsSinceFreshness);

  // Added 02/2025.
  prefs->ClearPref(set_up_list_prefs::kDisabled);

  // Added 03/2025.
  prefs->ClearPref(kIosParcelTrackingPolicyEnabled);

  // Added 04/2025.
  prefs->ClearPref("set_up_list.disabled");
}

// This method should be periodically pruned of year+ old migrations.
void MigrateObsoleteProfilePrefs(PrefService* prefs) {
  // This function is not allowed to block.
  base::ScopedDisallowBlocking disallow_blocking;

  // Check MigrateDeprecatedAutofillPrefs() to see if this is safe to remove.
  autofill::prefs::MigrateDeprecatedAutofillPrefs(prefs);

  // Added 06/2024.
  MigrateIntegerPrefFromLocalStatePrefsToProfilePrefs(
      prefs::kIosSyncSegmentsNewTabPageDisplayCount, prefs);

  // Added 06/2024.
  MigrateBooleanPrefFromProfilePrefsToLocalStatePrefs(prefs::kBottomOmnibox,
                                                      prefs);

  // Added 06/2024.
  MigrateBooleanPrefFromProfilePrefsToLocalStatePrefs(
      prefs::kBottomOmniboxByDefault, prefs);

  // Added 06/2024.
  prefs->ClearPref(kObsoletePasswordsPerAccountPrefMigrationDone);

  // Added 06/2024.
  prefs->ClearPref(kObsoleteBookmarksAndReadingListAccountStorageOptIn);

  // Added 07/2024.
  // Note that this key is an obsolete LocalState pref, it's here because it was
  // moved from LocalState pref to Profile pref and before clearing it the
  // Profile pref needs to be updated.
  MigrateStringPrefFromLocalStatePrefsToProfilePrefs(
      prefs::kIosSafetyCheckManagerPasswordCheckResult, prefs);

  // Added 07/2024.
  // Note that this key is an obsolete LocalState pref, it's here because it was
  // moved from LocalState pref to Profile pref and before clearing it the
  // Profile pref needs to be updated.
  MigrateDictionaryPrefFromLocalStatePrefsToProfilePrefs(
      prefs::kIosSafetyCheckManagerInsecurePasswordCounts, prefs);

  // Added 07/2024.
  prefs->ClearPref(prefs::kTabPickupLastDisplayedTime);
  prefs->ClearPref(prefs::kTabPickupLastDisplayedURL);

  // Added 09/2024.
  prefs->ClearPref(kContentSettingsWindowLastTabIndex);
  prefs->ClearPref(kSyncPasswordHash);
  prefs->ClearPref(kSyncPasswordLengthAndHashSalt);
  prefs->ClearPref(kContextualSearchEnabled);
  prefs->ClearPref(kNtpShownBookmarksFolder);

  // Added 09/2024.
  MigrateDictionaryPrefFromLocalStatePrefsToProfilePrefs(
      prefs::kIosPreRestoreAccountInfo, prefs);

  // Added 09/2024.
  MigrateBooleanPrefFromProfilePrefsToLocalStatePrefs(
      prefs::kOSLockdownModeEnabled, prefs);

  // Added 09/2024.
  MigrateBooleanPrefFromProfilePrefsToLocalStatePrefs(
      prefs::kBrowserLockdownModeEnabled, prefs);

  // Added 09/2024.
  MigrateBooleanPrefFromProfilePrefsToLocalStatePrefs(
      password_manager::prefs::kCredentialProviderEnabledOnStartup, prefs);

  // Added 09/2024.
  MigrateTimePrefFromProfilePrefsToLocalStatePrefs(
      prefs::kIdentityConfirmationSnackbarLastPromptTime, prefs);

  // Added 09/2024.
  MigrateIntegerPrefFromProfilePrefsToLocalStatePrefs(
      prefs::kIdentityConfirmationSnackbarDisplayCount, prefs);

  // Added 09/2024.
  MigrateBooleanPrefFromProfilePrefsToLocalStatePrefs(
      prefs::kIncognitoInterstitialEnabled, prefs);

  // Added 09/2024.
  MigrateIntegerPrefFromProfilePrefsToLocalStatePrefs(
      prefs::kAddressBarSettingsNewBadgeShownCount, prefs);

  // Added 09/2024.
  if (IsIosQuickDeleteEnabled()) {
    browsing_data::prefs::MaybeMigrateToQuickDeletePrefValues(prefs);
  }

  // Added 11/2024
  prefs->ClearPref(kEnableDoNotTrackIos);

  // Added 12/2024.
  prefs->ClearPref(kPageContentCollectionEnabled);

  // Added 02/2025
  // TODO(crbug.com/395840121): Remove migration call below after successfully
  // migrating `kSafetyCheckInMagicStackDisabledPref` from local-state to
  // profile Prefs.
  MigrateBooleanPrefFromLocalStatePrefsToProfilePrefs(
      safety_check_prefs::kSafetyCheckInMagicStackDisabledPref, prefs);

  // Added 02/2025
  // TODO(crbug.com/395840133): Remove migration call below after successfully
  // migrating `tab_resumption_prefs::kTabResumptionDisabledPref` from
  // local-state to profile Prefs.
  MigrateBooleanPrefFromLocalStatePrefsToProfilePrefs(
      tab_resumption_prefs::kTabResumptionDisabledPref, prefs);

  // Added 02/2025
  // TODO(crbug.com/398173021): Remove these Magic Stack freshness pref
  // migrations after successfully migrating from local state to profile Prefs.
  // These migrations were added Feb 2025 - approximately remove them Feb 2026.
  MigrateIntegerPrefFromLocalStatePrefsToProfilePrefs(
      prefs::kIosMagicStackSegmentationMVTImpressionsSinceFreshness, prefs);
  MigrateIntegerPrefFromLocalStatePrefsToProfilePrefs(
      prefs::kIosMagicStackSegmentationShortcutsImpressionsSinceFreshness,
      prefs);
  MigrateIntegerPrefFromLocalStatePrefsToProfilePrefs(
      prefs::kIosMagicStackSegmentationSafetyCheckImpressionsSinceFreshness,
      prefs);
  MigrateIntegerPrefFromLocalStatePrefsToProfilePrefs(
      prefs::kIosMagicStackSegmentationTabResumptionImpressionsSinceFreshness,
      prefs);

  // Added 02/2025.
  MigrateIntegerPrefFromProfilePrefsToLocalStatePrefs(
      prefs::kNTPLensEntryPointNewBadgeShownCount, prefs);

  // Added 02/2025.
  MigrateIntegerPrefFromProfilePrefsToLocalStatePrefs(
      prefs::kNTPHomeCustomizationNewBadgeImpressionCount, prefs);

  // Added 03/2025.
  MigrateIntegerPrefFromLocalStatePrefsToProfilePrefs(
      prefs::kInactiveTabsTimeThreshold, prefs);

  // Added 03/2025.
  MigrateIntegerPrefFromLocalStatePrefsToProfilePrefs(
      prefs::kHomeCustomizationMagicStackSafetyCheckIssuesCount, prefs);

  // Added 03/2025.
  prefs->ClearPref(kIosParcelTrackingOptInPromptDisplayLimitMet);
  prefs->ClearPref(kIosParcelTrackingOptInStatus);
  prefs->ClearPref(kIosParcelTrackingOptInPromptSwipedDown);

  // Added 04/2025.
  prefs->ClearPref(kMixedContentAutoupgradeEnabled);

  // Added 04/2025
  MigrateBooleanFromUserDefaultsToProfilePrefs(
      @"SyncDisabledAlertShown", policy::policy_prefs::kSyncDisabledAlertShown,
      prefs);

  // Added 04/2025.
  prefs->ClearPref(kAutologinEnabled);

  // Added 04/2025.
  prefs->ClearPref(kSuggestionGroupVisibility);

  // Added 05/2025.
  prefs->ClearPref(kSyncCacheGuid);
  prefs->ClearPref(kSyncBirthday);
  prefs->ClearPref(kSyncBagOfChips);
  prefs->ClearPref(kSyncLastSyncedTime);
  prefs->ClearPref(kSyncLastPollTime);
  prefs->ClearPref(kSyncPollInterval);
}

void MigrateObsoleteUserDefault() {
  NSUserDefaults* defaults = [NSUserDefaults standardUserDefaults];

  // Added 06/2024.
  [defaults removeObjectForKey:@"TimestampAppLastOpenedViaFirstPartyIntent"];
  [defaults removeObjectForKey:@"TimestampLastValidURLPasted"];

  // Added 07/2024.
  [defaults
      removeObjectForKey:@"MostRecentTimestampBlueDotPromoShownInOverflowMenu"];
  [defaults
      removeObjectForKey:@"MostRecentTimestampBlueDotPromoShownInSettingsMenu"];

  // Added 08/2024.
  [defaults removeObjectForKey:@"userHasInteractedWithWhatsNew"];

  // Added 11/2024.
  [defaults removeObjectForKey:@"DisplaySwitchProfile"];

  // Added 01/2025.
  [defaults removeObjectForKey:@"ChromeRecentTabsCollapsedSections"];

  // Added 03/2025.
  [defaults removeObjectForKey:@"FeedLastBackgroundRefreshTimestamp"];

  // Added 03/2025.
  [defaults removeObjectForKey:@"PreviousSessionInfoConnectedSceneSessionIDs"];
}
