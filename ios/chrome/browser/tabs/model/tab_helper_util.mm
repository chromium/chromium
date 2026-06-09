// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/tabs/model/tab_helper_util.h"

#import "base/feature_list.h"
#import "components/breadcrumbs/core/breadcrumbs_status.h"
#import "components/commerce/ios/browser/commerce_tab_helper.h"
#import "components/data_sharing/public/features.h"
#import "components/favicon/core/favicon_service.h"
#import "components/favicon/ios/web_favicon_driver.h"
#import "components/history/core/browser/top_sites.h"
#import "components/history/ios/browser/web_state_top_sites_observer.h"
#import "components/keyed_service/core/service_access_type.h"
#import "components/language/ios/browser/ios_language_detection_tab_helper.h"
#import "components/omnibox/common/omnibox_features.h"
#import "components/safe_browsing/core/common/features.h"
#import "components/safe_browsing/ios/browser/safe_browsing_url_allow_list.h"
#import "components/send_tab_to_self/features.h"
#import "components/supervised_user/core/common/features.h"
#import "components/ukm/ios/ukm_url_recorder.h"
#import "components/webauthn/ios/features.h"
#import "components/webauthn/ios/passkey_tab_helper.h"
#import "ios/chrome/browser/aim/model/aim_tab_helper.h"
#import "ios/chrome/browser/app_launcher/model/app_launcher_abuse_detector.h"
#import "ios/chrome/browser/app_launcher/model/app_launcher_tab_helper.h"
#import "ios/chrome/browser/autofill/model/autofill_tab_helper.h"
#import "ios/chrome/browser/autofill/model/bottom_sheet/autofill_bottom_sheet_tab_helper.h"
#import "ios/chrome/browser/autofill/model/form_suggestion_tab_helper.h"
#import "ios/chrome/browser/browser_content/model/edit_menu_tab_helper.h"
#import "ios/chrome/browser/cobrowse/model/assistant_aim_tab_helper.h"
#import "ios/chrome/browser/cobrowse/model/cobrowse_tab_helper.h"
#import "ios/chrome/browser/collaboration/model/data_sharing_tab_helper.h"
#import "ios/chrome/browser/commerce/model/price_alert_util.h"
#import "ios/chrome/browser/commerce/model/price_notifications/price_notifications_tab_helper.h"
#import "ios/chrome/browser/commerce/model/push_notification/push_notification_feature.h"
#import "ios/chrome/browser/commerce/model/shopping_persisted_data_tab_helper.h"
#import "ios/chrome/browser/commerce/model/shopping_service_factory.h"
#import "ios/chrome/browser/complex_tasks/model/ios_task_tab_helper.h"
#import "ios/chrome/browser/contextual_panel/model/contextual_panel_model_service.h"
#import "ios/chrome/browser/contextual_panel/model/contextual_panel_model_service_factory.h"
#import "ios/chrome/browser/contextual_panel/model/contextual_panel_tab_helper.h"
#import "ios/chrome/browser/crash_report/model/breadcrumbs/breadcrumb_manager_tab_helper.h"
#import "ios/chrome/browser/dom_distiller/model/distiller_service_factory.h"
#import "ios/chrome/browser/download/model/ar_quick_look_tab_helper.h"
#import "ios/chrome/browser/download/model/document_download_tab_helper.h"
#import "ios/chrome/browser/download/model/download_manager_tab_helper.h"
#import "ios/chrome/browser/download/model/pass_kit_tab_helper.h"
#import "ios/chrome/browser/download/model/safari_download_tab_helper.h"
#import "ios/chrome/browser/download/model/vcard_tab_helper.h"
#import "ios/chrome/browser/drive/model/drive_tab_helper.h"
#import "ios/chrome/browser/enterprise/data_controls/model/data_controls_tab_helper.h"
#import "ios/chrome/browser/enterprise/data_protection/model/data_protection_tab_helper.h"
#import "ios/chrome/browser/enterprise/data_protection/public/features.h"
#import "ios/chrome/browser/favicon/model/favicon_service_factory.h"
#import "ios/chrome/browser/find_in_page/model/find_tab_helper.h"
#import "ios/chrome/browser/history/model/history_service_factory.h"
#import "ios/chrome/browser/history/model/history_tab_helper.h"
#import "ios/chrome/browser/history/model/top_sites_factory.h"
#import "ios/chrome/browser/https_upgrades/model/https_only_mode_upgrade_tab_helper.h"
#import "ios/chrome/browser/https_upgrades/model/https_upgrade_service_factory.h"
#import "ios/chrome/browser/https_upgrades/model/typed_navigation_upgrade_tab_helper.h"
#import "ios/chrome/browser/infobars/model/infobar_badge_tab_helper.h"
#import "ios/chrome/browser/infobars/model/infobar_manager_impl.h"
#import "ios/chrome/browser/infobars/model/overlays/default_infobar_overlay_request_factory.h"
#import "ios/chrome/browser/infobars/model/overlays/infobar_overlay_request_inserter.h"
#import "ios/chrome/browser/infobars/model/overlays/infobar_overlay_tab_helper.h"
#import "ios/chrome/browser/infobars/model/overlays/translate_overlay_tab_helper.h"
#import "ios/chrome/browser/intelligence/actor/model/actor_tab_helper.h"
#import "ios/chrome/browser/intelligence/bwg/model/gemini_tab_helper.h"
#import "ios/chrome/browser/intelligence/features/features.h"
#import "ios/chrome/browser/itunes_urls/model/itunes_urls_handler_tab_helper.h"
#import "ios/chrome/browser/lens/model/lens_tab_helper.h"
#import "ios/chrome/browser/lens_overlay/coordinator/lens_overlay_availability.h"
#import "ios/chrome/browser/lens_overlay/model/lens_overlay_tab_helper.h"
#import "ios/chrome/browser/link_to_text/model/link_to_text_tab_helper.h"
#import "ios/chrome/browser/metrics/model/pageload_foreground_duration_tab_helper.h"
#import "ios/chrome/browser/mini_map/model/mini_map_tab_helper.h"
#import "ios/chrome/browser/ntp/model/new_tab_page_tab_helper.h"
#import "ios/chrome/browser/optimization_guide/model/optimization_guide_service.h"
#import "ios/chrome/browser/optimization_guide/model/optimization_guide_service_factory.h"
#import "ios/chrome/browser/optimization_guide/model/optimization_guide_tab_helper.h"
#import "ios/chrome/browser/optimization_guide/model/optimization_guide_validation_tab_helper.h"
#import "ios/chrome/browser/overlays/model/public/overlay_request_queue.h"
#import "ios/chrome/browser/overscroll_actions/model/overscroll_actions_tab_helper.h"
#import "ios/chrome/browser/page_info/features/features.h"
#import "ios/chrome/browser/page_info/model/about_this_site_tab_helper.h"
#import "ios/chrome/browser/passwords/model/ios_chrome_account_password_store_factory.h"
#import "ios/chrome/browser/passwords/model/password_controller.h"
#import "ios/chrome/browser/passwords/model/password_tab_helper.h"
#import "ios/chrome/browser/passwords/model/well_known_change_password_tab_helper.h"
#import "ios/chrome/browser/permissions/model/permissions_tab_helper.h"
#import "ios/chrome/browser/policy_url_blocking/model/policy_url_blocking_tab_helper.h"
#import "ios/chrome/browser/reader_mode/model/features.h"
#import "ios/chrome/browser/reader_mode/model/reader_mode_tab_helper.h"
#import "ios/chrome/browser/reading_list/model/offline_page_tab_helper.h"
#import "ios/chrome/browser/reading_list/model/reading_list_model_factory.h"
#import "ios/chrome/browser/reading_list/model/reading_list_web_state_observer.h"
#import "ios/chrome/browser/safe_browsing/model/safe_browsing_client_factory.h"
#import "ios/chrome/browser/safe_browsing/model/tailored_security/tailored_security_service_factory.h"
#import "ios/chrome/browser/safe_browsing/model/tailored_security/tailored_security_tab_helper.h"
#import "ios/chrome/browser/search_engines/model/search_engine_tab_helper.h"
#import "ios/chrome/browser/search_engines/model/template_url_service_factory.h"
#import "ios/chrome/browser/send_tab_to_self/model/send_tab_to_self_tab_helper.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/sharing/model/share_file_download_tab_helper.h"
#import "ios/chrome/browser/snapshots/model/snapshot_source_tab_helper.h"
#import "ios/chrome/browser/snapshots/model/snapshot_tab_helper.h"
#import "ios/chrome/browser/ssl/model/captive_portal_tab_helper.h"
#import "ios/chrome/browser/supervised_user/model/supervised_user_error_container.h"
#import "ios/chrome/browser/supervised_user/model/supervised_user_url_filter_tab_helper.h"
#import "ios/chrome/browser/tabs/model/ios_chrome_synced_tab_delegate.h"
#import "ios/chrome/browser/tabs/model/tab_helper_attacher.h"
#import "ios/chrome/browser/translate/model/chrome_ios_translate_client.h"
#import "ios/chrome/browser/translate/model/translate_pdf_metric_logger.h"
#import "ios/chrome/browser/voice/model/voice_search_navigations_tab_helper.h"
#import "ios/chrome/browser/web/model/annotations/annotations_tab_helper.h"
#import "ios/chrome/browser/web/model/blocked_popup_tab_helper.h"
#import "ios/chrome/browser/web/model/choose_file/choose_file_tab_helper.h"
#import "ios/chrome/browser/web/model/choose_file/last_tap_location_tab_helper.h"
#import "ios/chrome/browser/web/model/font_size/font_size_tab_helper.h"
#import "ios/chrome/browser/web/model/image_fetch/image_fetch_tab_helper.h"
#import "ios/chrome/browser/web/model/invalid_url_tab_helper.h"
#import "ios/chrome/browser/web/model/load_timing_tab_helper.h"
#import "ios/chrome/browser/web/model/page_placeholder_tab_helper.h"
#import "ios/chrome/browser/web/model/print/print_tab_helper.h"
#import "ios/chrome/browser/web/model/repost_form_tab_helper.h"
#import "ios/chrome/browser/web/model/sad_tab_tab_helper.h"
#import "ios/chrome/browser/web/model/web_performance_metrics/web_performance_metrics_tab_helper.h"
#import "ios/chrome/browser/web/model/web_view_proxy/web_view_proxy_tab_helper.h"
#import "ios/chrome/browser/web_selection/model/web_selection_tab_helper.h"
#import "ios/chrome/browser/webauthn/model/ios_chrome_passkey_client.h"
#import "ios/chrome/browser/webauthn/model/ios_passkey_model_factory.h"
#import "ios/chrome/browser/webui/model/net_export_tab_helper.h"
#import "ios/components/security_interstitials/https_only_mode/feature.h"
#import "ios/components/security_interstitials/https_only_mode/https_only_mode_container.h"
#import "ios/components/security_interstitials/ios_blocking_page_tab_helper.h"
#import "ios/components/security_interstitials/lookalikes/lookalike_url_container.h"
#import "ios/components/security_interstitials/lookalikes/lookalike_url_tab_allow_list.h"
#import "ios/components/security_interstitials/lookalikes/lookalike_url_tab_helper.h"
#import "ios/components/security_interstitials/safe_browsing/safe_browsing_client.h"
#import "ios/components/security_interstitials/safe_browsing/safe_browsing_query_manager.h"
#import "ios/components/security_interstitials/safe_browsing/safe_browsing_tab_helper.h"
#import "ios/components/security_interstitials/safe_browsing/safe_browsing_unsafe_resource_container.h"
#import "ios/public/provider/chrome/browser/cobalt/cobalt_api.h"
#import "ios/public/provider/chrome/browser/intelligence/classification_metrics_tab_helper_api.h"
#import "ios/public/provider/chrome/browser/text_zoom/text_zoom_api.h"
#import "ios/web/common/annotations_utils.h"
#import "ios/web/common/features.h"
#import "ios/web/public/web_state.h"

void AttachTabHelpers(web::WebState* web_state, TabHelperFilter filter_flags) {
  TabHelperAttacher attacher(web_state, filter_flags);
  ProfileIOS* const profile = attacher.GetProfile();
  ProfileIOS* original_profile = profile->GetOriginalProfile();

  // When adding a new tab helper, please consider whether it should be filtered
  // out when the web_state is presented in the following context:
  // - kPrerender: Tab helpers that are not required or not used for navigation
  // should be filtered out.
  // - kLensOverlay: Tab helpers that are required for Lens UI.
  // - kReaderMode: Tab helpers that are required for Reader Mode UI.
  //
  // When a web state is presented by the BVC, AttachTabHelpers is called to
  // attach all tab helpers. (the method is idempotent, so it is okay to call it
  // multiple times for the same WebState).

  attacher.Create<OverlayRequestQueue>();
  attacher.Create<VoiceSearchNavigationTabHelper>();
  attacher.Create<InfoBarManagerImpl>();
  attacher.Create<FindTabHelper>();

  bool should_create_history_tab_helper =
      !attacher.IsForReaderMode() &&
      (!attacher.IsForLensOverlay() ||
       base::FeatureList::IsEnabled(kLensOverlayNavigationHistory));
  attacher.CreateWhen<HistoryTabHelper>(should_create_history_tab_helper);
  if (should_create_history_tab_helper && attacher.IsForLensOverlay()) {
    HistoryTabHelper::FromWebState(web_state)->EnableLensURLProcessing();
  }

  attacher.Create<LoadTimingTabHelper>();
  attacher.Create<OverscrollActionsTabHelper>();
  attacher.Create<IOSTaskTabHelper>();

  attacher.CreateWhen<ShoppingPersistedDataTabHelper>(
      attacher.IsForStandardNavigation() &&
      IsPriceAlertsEligibleForWebState(web_state));

  attacher.Create<commerce::CommerceTabHelper>(
      attacher.IsOffTheRecord(),
      commerce::ShoppingServiceFactory::GetForProfile(profile));

  // Since LensTabHelper listens for a custom scheme, it needs to be
  // created before AppLauncherTabHelper, which will filter out
  // unhandled schemes.
  attacher.CreateWhen<LensTabHelper>(attacher.IsNotInTabHelperFilter());
  attacher.CreateWhen<LensOverlayTabHelper>(attacher.IsNotInTabHelperFilter());
  attacher
      .CreateDeferredWhen<AppLauncherTabHelper>(!attacher.IsForLensOverlay() &&
                                                !attacher.IsForPrerender())
      .With([&]() { return [[AppLauncherAbuseDetector alloc] init]; },
            [&]() { return attacher.IsOffTheRecord(); });
  attacher
      .CreateDeferredWhen<ReaderModeTabHelper>(
          attacher.IsNotInTabHelperFilter() && IsReaderModeAvailable())
      .WithFactory<DistillerServiceFactory>(profile);

  attacher.Create<security_interstitials::IOSBlockingPageTabHelper>();
  attacher.Create<WellKnownChangePasswordTabHelper>();
  attacher.Create<InvalidUrlTabHelper>();

  attacher.CreateWhen<InfobarOverlayRequestInserter>(
      attacher.IsForStandardNavigation(), &DefaultInfobarOverlayRequestFactory);
  attacher.CreateWhen<InfobarOverlayTabHelper>(
      attacher.IsForStandardNavigation());
  attacher.CreateWhen<TranslateOverlayTabHelper>(
      attacher.IsForStandardNavigation());

  attacher.CreateWhen<FontSizeTabHelper>(ios::provider::IsTextZoomEnabled());
  attacher.CreateWhen<BreadcrumbManagerTabHelper>(
      breadcrumbs::IsEnabled(GetApplicationContext()->GetLocalState()));

  attacher.Create<AnnotationsTabHelper>();
  attacher.CreateWhen<SendTabToSelfTabHelper>(
      base::FeatureList::IsEnabled(
          send_tab_to_self::kSendTabToSelfPropagateScrollPosition) ||
      base::FeatureList::IsEnabled(
          send_tab_to_self::kSendTabToSelfPropagateFormFields));

  SafeBrowsingClient* client =
      SafeBrowsingClientFactory::GetForProfile(profile);
  attacher.Create<SafeBrowsingQueryManager>(client);
  attacher.Create<SafeBrowsingTabHelper>(client);
  attacher.Create<SafeBrowsingUrlAllowList>();
  attacher.Create<SafeBrowsingUnsafeResourceContainer>();

  attacher.Create<TailoredSecurityTabHelper>(
      TailoredSecurityServiceFactory::GetForProfile(profile));
  attacher.Create<PolicyUrlBlockingTabHelper>();

  // Supervised user services are not supported for off-the-record.
  attacher.CreateWhen<SupervisedUserURLFilterTabHelper>(
      !attacher.IsOffTheRecord());
  attacher.CreateWhen<SupervisedUserErrorContainer>(!attacher.IsOffTheRecord());

  attacher.Create<ImageFetchTabHelper>();
  attacher.Create<NewTabPageTabHelper>();
  attacher.Create<ShareFileDownloadTabHelper>();
  attacher.Create<OptimizationGuideTabHelper>();
  attacher.Create<OptimizationGuideValidationTabHelper>();

  attacher.Create<favicon::WebFaviconDriver>(
      ios::FaviconServiceFactory::GetForProfile(
          original_profile, ServiceAccessType::IMPLICIT_ACCESS));
  attacher.Create<history::WebStateTopSitesObserver>(
      ios::TopSitesFactory::GetForProfile(original_profile).get());

  // Depends on favicon::WebFaviconDriver, must be created after it.
  attacher.Create<SearchEngineTabHelper>();

  ukm::InitializeSourceUrlRecorderForWebState(web_state);

  // Download tab helpers.
  attacher.Create<DownloadManagerTabHelper>();
  attacher.Create<SafariDownloadTabHelper>();
  attacher.Create<VcardTabHelper>();
  attacher.Create<DocumentDownloadTabHelper>();
  attacher.Create<ARQuickLookTabHelper>();
  attacher.Create<PassKitTabHelper>();
  attacher.Create<DriveTabHelper>();

  attacher.Create<ITunesUrlsHandlerTabHelper>();
  attacher.Create<PageloadForegroundDurationTabHelper>();

  attacher.Create<LookalikeUrlTabHelper>();
  attacher.Create<LookalikeUrlTabAllowList>();
  attacher.Create<LookalikeUrlContainer>();

  // TODO(crbug.com/41360476): pre-rendered WebState have lots of unnecessary
  // tab helpers for historical reasons. For the moment, AttachTabHelpers
  // allows to inhibit the creation of some of them.
  attacher.CreateWhen<SadTabTabHelper>(
      !attacher.IsForLensOverlay() && !attacher.IsForPrerender(),
      SadTabTabHelper::kDefaultRepeatFailureInterval);
  attacher.CreateWhen<SnapshotTabHelper>(!attacher.IsForLensOverlay() &&
                                         !attacher.IsForPrerender());
  attacher.CreateWhen<SnapshotSourceTabHelper>(!attacher.IsForLensOverlay() &&
                                               !attacher.IsForPrerender());
  attacher.CreateWhen<PagePlaceholderTabHelper>(!attacher.IsForLensOverlay() &&
                                                !attacher.IsForPrerender());
  attacher.CreateWhen<PasswordTabHelper>(attacher.IsNotInTabHelperFilter());
  attacher.CreateWhen<AutofillBottomSheetTabHelper>(
      attacher.IsNotInTabHelperFilter());
  attacher.CreateWhen<AutofillTabHelper>(attacher.IsNotInTabHelperFilter());

  attacher.CreateWhen<InfobarBadgeTabHelper>(
      attacher.IsForStandardNavigation());

  // Needs to be created after `InfobarBadgeTabHelper`.
  attacher.CreateWhen<ChromeIOSTranslateClient>(
      attacher.IsNotInTabHelperFilter(),
      InfoBarManagerImpl::FromWebState(web_state));

  attacher
      .CreateDeferredWhen<webauthn::PasskeyTabHelper>(
          attacher.IsForStandardNavigation() &&
          base::FeatureList::IsEnabled(kIOSPasskeyShim))
      .With([&]() { return IOSPasskeyModelFactory::GetForProfile(profile); },
            [&]() {
              return IOSChromeAccountPasswordStoreFactory::GetForProfile(
                  profile, ServiceAccessType::EXPLICIT_ACCESS);
            },
            [&]() {
              return std::make_unique<IOSChromePasskeyClient>(web_state);
            });

  attacher.CreateWhen<LinkToTextTabHelper>(
      base::FeatureList::IsEnabled(kSharedHighlightingIOS));

  attacher.Create<WebSelectionTabHelper>();
  attacher.Create<WebPerformanceMetricsTabHelper>();
  attacher.Create<OfflinePageTabHelper>(
      ReadingListModelFactory::GetForProfile(profile));
  attacher.Create<PermissionsTabHelper>();
  attacher.Create<RepostFormTabHelper>();
  attacher.Create<HttpsOnlyModeUpgradeTabHelper>(
      profile->GetPrefs(), HttpsUpgradeServiceFactory::GetForProfile(profile));
  attacher.Create<HttpsOnlyModeContainer>();

  attacher
      .CreateDeferredWhen<TypedNavigationUpgradeTabHelper>(
          !attacher.IsForPrerender() &&
          base::FeatureList::IsEnabled(
              omnibox::kDefaultTypedNavigationsToHttps))
      .WithFactory<HttpsUpgradeServiceFactory>(profile);

  attacher.CreateWhen<PriceNotificationsTabHelper>(
      attacher.IsForStandardNavigation() && !attacher.IsOffTheRecord());

  attacher
      .CreateDeferredWhen<ContextualPanelTabHelper>(
          attacher.IsForStandardNavigation())
      .With([&]() {
        return ContextualPanelModelServiceFactory::GetForProfile(profile)
            ->models();
      });

  auto* optimization_guide_decider =
      OptimizationGuideServiceFactory::GetForProfile(profile);
  attacher.CreateWhen<AboutThisSiteTabHelper>(
      attacher.IsForStandardNavigation() && !attacher.IsOffTheRecord() &&
          IsAboutThisSiteFeatureEnabled() && optimization_guide_decider,
      optimization_guide_decider);

  attacher.CreateWhen<DataSharingTabHelper>(
      !attacher.IsOffTheRecord() && !attacher.IsForPrerender() &&
      data_sharing::features::ShouldInterceptUrlForVersioning());

  attacher.Create<EditMenuTabHelper>();

  attacher.CreateWhen<MiniMapTabHelper>(
      (base::FeatureList::IsEnabled(kIOSMiniMapUniversalLink) ||
       base::FeatureList::IsEnabled(kIOSMiniMapUniversalLinkCounterfactual)) &&
      attacher.IsNotInTabHelperFilter());

  if (IsAimCobrowseEnabled()) {
    attacher.CreateWhen<CobrowseTabHelper>(
        attacher.IsNotInTabHelperFilter(),
        ios::TemplateURLServiceFactory::GetForProfile(profile));
    attacher.CreateWhen<AssistantAimTabHelper>(attacher.IsForAssistantAim());
  }

  if (IsComposeboxIOSEnabled()) {
    attacher.CreateWhen<AimTabHelper>(!attacher.IsForPrerender() &&
                                      !attacher.IsForReaderMode());
  }

  attacher.CreateWhen<GeminiTabHelper>(!attacher.IsOffTheRecord() &&
                                       !attacher.IsForPrerender() &&
                                       IsPageActionMenuEnabled());

  const bool is_actor_tab_helper_enabled =
      IsActorEnabled() && !attacher.IsForPrerender();
  attacher.CreateWhen<ActorTabHelper>(is_actor_tab_helper_enabled);

  attacher.Create<WebViewProxyTabHelper>();

  attacher.CreateWhen<ChooseFileTabHelper>(
      attacher.IsNotInTabHelperFilter() &&
      (base::FeatureList::IsEnabled(kIOSChooseFromDrive) ||
       base::FeatureList::IsEnabled(kIOSCustomFileUploadMenu)));
  attacher.CreateWhen<LastTapLocationTabHelper>(
      attacher.IsNotInTabHelperFilter() &&
      base::FeatureList::IsEnabled(kIOSCustomFileUploadMenu));

  if (!attacher.IsOffTheRecord() && !attacher.IsForPrerender() &&
      IsModelBasedPageClassificationEnabled()) {
    ios::provider::AttachClassificationMetricsTabHelper(web_state);
  }

  attacher.Create<data_controls::DataControlsTabHelper>();
  if (IsEnableScreenshotProtectionIOSEnabled()) {
    attacher.Create<DataProtectionTabHelper>();
  }
  attacher.Create<CaptivePortalTabHelper>();
  attacher.Create<PrintTabHelper>();
  attacher.Create<BlockedPopupTabHelper>();
  attacher.Create<NetExportTabHelper>();
  attacher.Create<TranslatePDFMetricLogger>();

  if (web::features::IsCobaltEnabled()) {
    ios::provider::AttachCobaltTabHelpers(attacher);
  }
}
