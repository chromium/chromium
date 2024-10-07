// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/profile/model/keyed_service_factories.h"

#import "ios/chrome/browser/affiliations/model/ios_chrome_affiliation_service_factory.h"
#import "ios/chrome/browser/autocomplete/model/autocomplete_classifier_factory.h"
#import "ios/chrome/browser/autocomplete/model/autocomplete_provider_client_impl.h"
#import "ios/chrome/browser/autocomplete/model/autocomplete_scoring_model_service_factory.h"
#import "ios/chrome/browser/autocomplete/model/in_memory_url_index_factory.h"
#import "ios/chrome/browser/autocomplete/model/provider_state_service_factory.h"
#import "ios/chrome/browser/autocomplete/model/remote_suggestions_service_factory.h"
#import "ios/chrome/browser/autocomplete/model/shortcuts_backend_factory.h"
#import "ios/chrome/browser/autocomplete/model/zero_suggest_cache_service_factory.h"
#import "ios/chrome/browser/autofill/model/autocomplete_history_manager_factory.h"
#import "ios/chrome/browser/autofill/model/autofill_image_fetcher_factory.h"
#import "ios/chrome/browser/autofill/model/autofill_log_router_factory.h"
#import "ios/chrome/browser/autofill/model/personal_data_manager_factory.h"
#import "ios/chrome/browser/autofill/model/strike_database_factory.h"
#import "ios/chrome/browser/bookmarks/model/account_bookmark_sync_service_factory.h"
#import "ios/chrome/browser/bookmarks/model/bookmark_model_factory.h"
#import "ios/chrome/browser/bookmarks/model/bookmark_undo_service_factory.h"
#import "ios/chrome/browser/bookmarks/model/local_or_syncable_bookmark_sync_service_factory.h"
#import "ios/chrome/browser/bookmarks/model/managed_bookmark_service_factory.h"
#import "ios/chrome/browser/bring_android_tabs/model/bring_android_tabs_to_ios_service_factory.h"
#import "ios/chrome/browser/browsing_data/model/browsing_data_remover_factory.h"
#import "ios/chrome/browser/commerce/model/session_proto_db_factory.h"
#import "ios/chrome/browser/commerce/model/shopping_service_factory.h"
#import "ios/chrome/browser/consent_auditor/model/consent_auditor_factory.h"
#import "ios/chrome/browser/content_notification/model/content_notification_service_factory.h"
#import "ios/chrome/browser/content_settings/model/cookie_settings_factory.h"
#import "ios/chrome/browser/content_settings/model/host_content_settings_map_factory.h"
#import "ios/chrome/browser/contextual_panel/model/contextual_panel_model_service_factory.h"
#import "ios/chrome/browser/contextual_panel/sample/model/sample_panel_model_factory.h"
#import "ios/chrome/browser/crash_report/model/breadcrumbs/breadcrumb_manager_keyed_service_factory.h"
#import "ios/chrome/browser/credential_provider/model/credential_provider_buildflags.h"
#import "ios/chrome/browser/data_sharing/model/data_sharing_service_factory.h"
#import "ios/chrome/browser/device_reauth/ios_device_authenticator_factory.h"
#import "ios/chrome/browser/device_sharing/model/device_sharing_manager_factory.h"
#import "ios/chrome/browser/discover_feed/model/discover_feed_service_factory.h"
#import "ios/chrome/browser/dom_distiller/model/dom_distiller_service_factory.h"
#import "ios/chrome/browser/download/model/background_service/background_download_service_factory.h"
#import "ios/chrome/browser/download/model/browser_download_service_factory.h"
#import "ios/chrome/browser/drive/model/drive_service_factory.h"
#import "ios/chrome/browser/enterprise/connectors/connectors_service_factory.h"
#import "ios/chrome/browser/enterprise/model/idle/idle_service_factory.h"
#import "ios/chrome/browser/external_files/model/external_file_remover_factory.h"
#import "ios/chrome/browser/favicon/model/favicon_service_factory.h"
#import "ios/chrome/browser/favicon/model/ios_chrome_favicon_loader_factory.h"
#import "ios/chrome/browser/favicon/model/ios_chrome_large_icon_cache_factory.h"
#import "ios/chrome/browser/favicon/model/ios_chrome_large_icon_service_factory.h"
#import "ios/chrome/browser/feature_engagement/model/tracker_factory.h"
#import "ios/chrome/browser/follow/model/follow_service_factory.h"
#import "ios/chrome/browser/gcm/model/instance_id/ios_chrome_instance_id_profile_service_factory.h"
#import "ios/chrome/browser/gcm/model/ios_chrome_gcm_profile_service_factory.h"
#import "ios/chrome/browser/google/model/google_logo_service_factory.h"
#import "ios/chrome/browser/history/model/domain_diversity_reporter_factory.h"
#import "ios/chrome/browser/history/model/history_service_factory.h"
#import "ios/chrome/browser/history/model/top_sites_factory.h"
#import "ios/chrome/browser/history/model/web_history_service_factory.h"
#import "ios/chrome/browser/https_upgrades/model/https_upgrade_service_factory.h"
#import "ios/chrome/browser/invalidation/model/ios_chrome_profile_invalidation_provider_factory.h"
#import "ios/chrome/browser/language/model/accept_languages_service_factory.h"
#import "ios/chrome/browser/language/model/language_model_manager_factory.h"
#import "ios/chrome/browser/language/model/url_language_histogram_factory.h"
#import "ios/chrome/browser/language_detection/model/language_detection_model_loader_service_ios_factory.h"
#import "ios/chrome/browser/language_detection/model/language_detection_model_service_factory.h"
#import "ios/chrome/browser/mailto_handler/model/mailto_handler_service_factory.h"
#import "ios/chrome/browser/metrics/model/google_groups_manager_factory.h"
#import "ios/chrome/browser/metrics/model/ios_profile_session_durations_service_factory.h"
#import "ios/chrome/browser/optimization_guide/model/optimization_guide_service_factory.h"
#import "ios/chrome/browser/page_content_annotations/model/page_content_annotations_service_factory.h"
#import "ios/chrome/browser/page_image/model/page_image_service_factory.h"
#import "ios/chrome/browser/page_info/about_this_site_service_factory.h"
#import "ios/chrome/browser/passwords/model/credentials_cleaner_runner_factory.h"
#import "ios/chrome/browser/passwords/model/ios_chrome_account_password_store_factory.h"
#import "ios/chrome/browser/passwords/model/ios_chrome_bulk_leak_check_service_factory.h"
#import "ios/chrome/browser/passwords/model/ios_chrome_password_check_manager_factory.h"
#import "ios/chrome/browser/passwords/model/ios_chrome_password_receiver_service_factory.h"
#import "ios/chrome/browser/passwords/model/ios_chrome_password_reuse_manager_factory.h"
#import "ios/chrome/browser/passwords/model/ios_chrome_password_sender_service_factory.h"
#import "ios/chrome/browser/passwords/model/ios_chrome_profile_password_store_factory.h"
#import "ios/chrome/browser/passwords/model/ios_password_manager_settings_service_factory.h"
#import "ios/chrome/browser/passwords/model/ios_password_requirements_service_factory.h"
#import "ios/chrome/browser/passwords/model/password_manager_log_router_factory.h"
#import "ios/chrome/browser/photos/model/photos_service_factory.h"
#import "ios/chrome/browser/plus_addresses/model/plus_address_service_factory.h"
#import "ios/chrome/browser/plus_addresses/model/plus_address_setting_service_factory.h"
#import "ios/chrome/browser/policy/model/cloud/user_policy_signin_service_factory.h"
#import "ios/chrome/browser/policy_url_blocking/model/policy_url_blocking_service.h"
#import "ios/chrome/browser/power_bookmarks/model/power_bookmark_service_factory.h"
#import "ios/chrome/browser/prerender/model/prerender_service_factory.h"
#import "ios/chrome/browser/price_insights/model/price_insights_model_factory.h"
#import "ios/chrome/browser/promos_manager/model/promos_manager_factory.h"
#import "ios/chrome/browser/push_notification/model/push_notification_profile_service_factory.h"
#import "ios/chrome/browser/reading_list/model/reading_list_download_service_factory.h"
#import "ios/chrome/browser/reading_list/model/reading_list_model_factory.h"
#import "ios/chrome/browser/safe_browsing/model/chrome_password_protection_service_factory.h"
#import "ios/chrome/browser/safe_browsing/model/hash_realtime_service_factory.h"
#import "ios/chrome/browser/safe_browsing/model/ohttp_key_service_factory.h"
#import "ios/chrome/browser/safe_browsing/model/real_time_url_lookup_service_factory.h"
#import "ios/chrome/browser/safe_browsing/model/safe_browsing_client_factory.h"
#import "ios/chrome/browser/safe_browsing/model/safe_browsing_metrics_collector_factory.h"
#import "ios/chrome/browser/safe_browsing/model/tailored_security/tailored_security_service_factory.h"
#import "ios/chrome/browser/safe_browsing/model/verdict_cache_manager_factory.h"
#import "ios/chrome/browser/safety_check/model/ios_chrome_safety_check_manager_factory.h"
#import "ios/chrome/browser/saved_tab_groups/model/tab_group_sync_service_factory.h"
#import "ios/chrome/browser/screen_time/model/screen_time_buildflags.h"
#import "ios/chrome/browser/search_engines/model/search_engine_choice_service_factory.h"
#import "ios/chrome/browser/search_engines/model/template_url_fetcher_factory.h"
#import "ios/chrome/browser/search_engines/model/template_url_service_factory.h"
#import "ios/chrome/browser/segmentation_platform/model/segmentation_platform_service_factory.h"
#import "ios/chrome/browser/sessions/model/ios_chrome_tab_restore_service_factory.h"
#import "ios/chrome/browser/sessions/model/session_restoration_service_factory.h"
#import "ios/chrome/browser/sessions/model/web_session_state_cache_factory.h"
#import "ios/chrome/browser/share_extension/model/share_extension_service_factory.h"
#import "ios/chrome/browser/share_kit/model/share_kit_service_factory.h"
#import "ios/chrome/browser/shared/model/browser/browser_list_factory.h"
#import "ios/chrome/browser/sharing_message/model/ios_sharing_message_bridge_factory.h"
#import "ios/chrome/browser/sharing_message/model/ios_sharing_service_factory.h"
#import "ios/chrome/browser/signin/model/about_signin_internals_factory.h"
#import "ios/chrome/browser/signin/model/account_consistency_service_factory.h"
#import "ios/chrome/browser/signin/model/account_investigator_factory.h"
#import "ios/chrome/browser/signin/model/account_reconcilor_factory.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service_factory.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/signin/model/signin_client_factory.h"
#import "ios/chrome/browser/signin/model/signin_error_controller_factory.h"
#import "ios/chrome/browser/signin/model/signin_metrics_service_factory.h"
#import "ios/chrome/browser/signin/model/signin_profile_info_updater_factory.h"
#import "ios/chrome/browser/signin/model/trusted_vault_client_backend_factory.h"
#import "ios/chrome/browser/supervised_user/model/child_account_service_factory.h"
#import "ios/chrome/browser/supervised_user/model/list_family_members_service_factory.h"
#import "ios/chrome/browser/supervised_user/model/supervised_user_metrics_service_factory.h"
#import "ios/chrome/browser/supervised_user/model/supervised_user_service_factory.h"
#import "ios/chrome/browser/supervised_user/model/supervised_user_settings_service_factory.h"
#import "ios/chrome/browser/sync/model/data_type_store_service_factory.h"
#import "ios/chrome/browser/sync/model/device_info_sync_service_factory.h"
#import "ios/chrome/browser/sync/model/ios_user_event_service_factory.h"
#import "ios/chrome/browser/sync/model/send_tab_to_self_sync_service_factory.h"
#import "ios/chrome/browser/sync/model/session_sync_service_factory.h"
#import "ios/chrome/browser/sync/model/sync_invalidations_service_factory.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
#import "ios/chrome/browser/tabs_search/model/tabs_search_service_factory.h"
#import "ios/chrome/browser/text_selection/model/text_classifier_model_service_factory.h"
#import "ios/chrome/browser/tips_manager/model/tips_manager_ios_factory.h"
#import "ios/chrome/browser/translate/model/translate_ranker_factory.h"
#import "ios/chrome/browser/trusted_vault/model/ios_trusted_vault_service_factory.h"
#import "ios/chrome/browser/unified_consent/model/unified_consent_service_factory.h"
#import "ios/chrome/browser/unit_conversion/unit_conversion_service_factory.h"
#import "ios/chrome/browser/visited_url_ranking/model/visited_url_ranking_service_factory.h"
#import "ios/chrome/browser/voice/ui_bundled/text_to_speech_playback_controller_factory.h"
#import "ios/chrome/browser/web/model/java_script_console/java_script_console_feature_factory.h"
#import "ios/chrome/browser/webauthn/model/ios_passkey_model_factory.h"
#import "ios/chrome/browser/webdata_services/model/web_data_service_factory.h"

#if BUILDFLAG(IOS_CREDENTIAL_PROVIDER_ENABLED)
#import "ios/chrome/browser/credential_provider/model/credential_provider_service_factory.h"
#endif

#if BUILDFLAG(IOS_SCREEN_TIME_ENABLED)
#import "ios/chrome/browser/screen_time/model/screen_time_history_deleter_factory.h"
#endif

// This method gets the instance of each ServiceFactory. We do this so that
// each ServiceFactory initializes itself and registers its dependencies with
// the global BrowserStateDependencyManager. We need to have a complete
// dependency graph when we create a profile so we can dispatch the profile
// creation message to the services that want to create their services at
// profile creation time.
void EnsureProfileKeyedServiceFactoriesBuilt() {
  // Keep this list alphabetized -- namespaced factories first, followed by
  // non-namespaced factories.
  autofill::AutocompleteHistoryManagerFactory::GetInstance();
  autofill::AutofillImageFetcherFactory::GetInstance();
  autofill::AutofillLogRouterFactory::GetInstance();
  autofill::PersonalDataManagerFactory::GetInstance();
  autofill::StrikeDatabaseFactory::GetInstance();
  commerce::ShoppingServiceFactory::GetInstance();
  data_sharing::DataSharingServiceFactory::GetInstance();
  dom_distiller::DomDistillerServiceFactory::GetInstance();
  drive::DriveServiceFactory::GetInstance();
  enterprise_connectors::ConnectorsServiceFactory::GetInstance();
  enterprise_idle::IdleServiceFactory::GetInstance();
  feature_engagement::TrackerFactory::GetInstance();
  ios::AboutSigninInternalsFactory::GetInstance();
  ios::AccountBookmarkSyncServiceFactory::GetInstance();
  ios::AccountConsistencyServiceFactory::GetInstance();
  ios::AccountInvestigatorFactory::GetInstance();
  ios::AccountReconcilorFactory::GetInstance();
  ios::AutocompleteClassifierFactory::GetInstance();
  ios::AutocompleteScoringModelServiceFactory::GetInstance();
  ios::BookmarkModelFactory::GetInstance();
  ios::BookmarkUndoServiceFactory::GetInstance();
  ios::CookieSettingsFactory::GetInstance();
  ios::FaviconServiceFactory::GetInstance();
  ios::HistoryServiceFactory::GetInstance();
  ios::HostContentSettingsMapFactory::GetInstance();
  ios::InMemoryURLIndexFactory::GetInstance();
  ios::LocalOrSyncableBookmarkSyncServiceFactory::GetInstance();
  ios::PasswordManagerLogRouterFactory::GetInstance();
  ios::ProviderStateServiceFactory::GetInstance();
  ios::SearchEngineChoiceServiceFactory::GetInstance();
  ios::ShortcutsBackendFactory::GetInstance();
  ios::SigninErrorControllerFactory::GetInstance();
  ios::TemplateURLFetcherFactory::GetInstance();
  ios::TemplateURLServiceFactory::GetInstance();
  ios::TopSitesFactory::GetInstance();
  ios::WebDataServiceFactory::GetInstance();
  ios::WebHistoryServiceFactory::GetInstance();
  ios::ZeroSuggestCacheServiceFactory::GetInstance();
  policy::UserPolicySigninServiceFactory::GetInstance();
  segmentation_platform::SegmentationPlatformServiceFactory::GetInstance();
  tab_groups::TabGroupSyncServiceFactory::GetInstance();
  translate::TranslateRankerFactory::GetInstance();

  AboutThisSiteServiceFactory::GetInstance();
  AcceptLanguagesServiceFactory::GetInstance();
  AuthenticationServiceFactory::GetInstance();
  BackgroundDownloadServiceFactory::GetInstance();
  BreadcrumbManagerKeyedServiceFactory::GetInstance();
  BringAndroidTabsToIOSServiceFactory::GetInstance();
  BrowserDownloadServiceFactory::GetInstance();
  BrowserListFactory::GetInstance();
  BrowsingDataRemoverFactory::GetInstance();
  ChildAccountServiceFactory::GetInstance();
  ChromeAccountManagerServiceFactory::GetInstance();
  ChromePasswordProtectionServiceFactory::GetInstance();
  ConsentAuditorFactory::GetInstance();
  ContentNotificationServiceFactory::GetInstance();
  ContextualPanelModelServiceFactory::GetInstance();
  CredentialsCleanerRunnerFactory::GetInstance();
  DataTypeStoreServiceFactory::GetInstance();
  DeviceAuthenticatorProxyFactory::GetInstance();
  DeviceInfoSyncServiceFactory::GetInstance();
  DeviceSharingManagerFactory::GetInstance();
  DiscoverFeedServiceFactory::GetInstance();
  DomainDiversityReporterFactory::GetInstance();
  ExternalFileRemoverFactory::GetInstance();
  FollowServiceFactory::GetInstance();
  GoogleGroupsManagerFactory::GetInstance();
  GoogleLogoServiceFactory::GetInstance();
  HashRealTimeServiceFactory::GetInstance();
  HttpsUpgradeServiceFactory::GetInstance();
  IdentityManagerFactory::GetInstance();
  IOSChromeAccountPasswordStoreFactory::GetInstance();
  IOSChromeAffiliationServiceFactory::GetInstance();
  IOSChromeBulkLeakCheckServiceFactory::GetInstance();
  IOSChromeFaviconLoaderFactory::GetInstance();
  IOSChromeGCMProfileServiceFactory::GetInstance();
  IOSChromeInstanceIDProfileServiceFactory::GetInstance();
  IOSChromeLargeIconCacheFactory::GetInstance();
  IOSChromeLargeIconServiceFactory::GetInstance();
  IOSChromePasswordCheckManagerFactory::GetInstance();
  IOSChromePasswordReceiverServiceFactory::GetInstance();
  IOSChromePasswordReuseManagerFactory::GetInstance();
  IOSChromePasswordSenderServiceFactory::GetInstance();
  IOSChromeProfileInvalidationProviderFactory::GetInstance();
  IOSChromeProfilePasswordStoreFactory::GetInstance();
  IOSChromeSafetyCheckManagerFactory::GetInstance();
  IOSChromeTabRestoreServiceFactory::GetInstance();
  IOSPasskeyModelFactory::GetInstance();
  IOSPasswordManagerSettingsServiceFactory::GetInstance();
  IOSPasswordRequirementsServiceFactory::GetInstance();
  IOSProfileSessionDurationsServiceFactory::GetInstance();
  IOSSharingMessageBridgeFactory::GetInstance();
  IOSSharingServiceFactory::GetInstance();
  IOSTrustedVaultServiceFactory::GetInstance();
  IOSUserEventServiceFactory::GetInstance();
  JavaScriptConsoleFeatureFactory::GetInstance();
  LanguageDetectionModelLoaderServiceIOSFactory::GetInstance();
  LanguageModelManagerFactory::GetInstance();
  ListFamilyMembersServiceFactory::GetInstance();
  MailtoHandlerServiceFactory::GetInstance();
  ManagedBookmarkServiceFactory::GetInstance();
  OhttpKeyServiceFactory::GetInstance();
  OptimizationGuideServiceFactory::GetInstance();
  PageContentAnnotationsServiceFactory::GetInstance();
  PageImageServiceFactory::GetInstance();
  PhotosServiceFactory::GetInstance();
  PlusAddressServiceFactory::GetInstance();
  PlusAddressSettingServiceFactory::GetInstance();
  PolicyBlocklistServiceFactory::GetInstance();
  PowerBookmarkServiceFactory::GetInstance();
  PrerenderServiceFactory::GetInstance();
  PriceInsightsModelFactory::GetInstance();
  PromosManagerFactory::GetInstance();
  PushNotificationProfileServiceFactory::GetInstance();
  ReadingListDownloadServiceFactory::GetInstance();
  ReadingListModelFactory::GetInstance();
  RealTimeUrlLookupServiceFactory::GetInstance();
  RemoteSuggestionsServiceFactory::GetInstance();
  SafeBrowsingClientFactory::GetInstance();
  SafeBrowsingMetricsCollectorFactory::GetInstance();
  SamplePanelModelFactory::GetInstance();
  SendTabToSelfSyncServiceFactory::GetInstance();
  SessionRestorationServiceFactory::GetInstance();
  SessionSyncServiceFactory::GetInstance();
  ShareExtensionServiceFactory::GetInstance();
  ShareKitServiceFactory::GetInstance();
  SigninClientFactory::GetInstance();
  SigninMetricsServiceFactory::GetInstance();
  SigninProfileInfoUpdaterFactory::GetInstance();
  SupervisedUserMetricsServiceFactory::GetInstance();
  SupervisedUserServiceFactory::GetInstance();
  SupervisedUserSettingsServiceFactory::GetInstance();
  SyncInvalidationsServiceFactory::GetInstance();
  SyncServiceFactory::GetInstance();
  TabsSearchServiceFactory::GetInstance();
  TailoredSecurityServiceFactory::GetInstance();
  TextClassifierModelServiceFactory::GetInstance();
  TextToSpeechPlaybackControllerFactory::GetInstance();
  TipsManagerIOSFactory::GetInstance();
  LanguageDetectionModelServiceFactory::GetInstance();
  TrustedVaultClientBackendFactory::GetInstance();
  UnifiedConsentServiceFactory::GetInstance();
  UnitConversionServiceFactory::GetInstance();
  UrlLanguageHistogramFactory::GetInstance();
  VerdictCacheManagerFactory::GetInstance();
  VisitedURLRankingServiceFactory::GetInstance();
  WebSessionStateCacheFactory::GetInstance();
  // Keep the above list alphabetized! Don't just add new entries at the end.

#if BUILDFLAG(IOS_CREDENTIAL_PROVIDER_ENABLED)
  CredentialProviderServiceFactory::GetInstance();
#endif

#if BUILDFLAG(IOS_SCREEN_TIME_ENABLED)
  ScreenTimeHistoryDeleterFactory::GetInstance();
#endif

  // Call other "Ensure...FactoriesBuilt" functions as necessary.
  EnsureSessionProtoDBFactoriesBuilt();
}
