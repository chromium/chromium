// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/tabs/model/tab_helper_util.h"

#import "base/feature_list.h"
#import "components/breadcrumbs/core/breadcrumbs_status.h"
#import "components/commerce/ios/browser/commerce_tab_helper.h"
#import "components/favicon/core/favicon_service.h"
#import "components/favicon/ios/web_favicon_driver.h"
#import "components/history/core/browser/top_sites.h"
#import "components/history/ios/browser/web_state_top_sites_observer.h"
#import "components/keyed_service/core/service_access_type.h"
#import "components/language/ios/browser/ios_language_detection_tab_helper.h"
#import "components/omnibox/common/omnibox_features.h"
#import "components/safe_browsing/core/common/features.h"
#import "components/safe_browsing/ios/browser/safe_browsing_url_allow_list.h"
#import "components/supervised_user/core/common/features.h"
#import "components/ukm/ios/ukm_url_recorder.h"
#import "ios/chrome/browser/app_launcher/model/app_launcher_abuse_detector.h"
#import "ios/chrome/browser/app_launcher/model/app_launcher_tab_helper.h"
#import "ios/chrome/browser/autofill/model/autofill_tab_helper.h"
#import "ios/chrome/browser/autofill/model/bottom_sheet/autofill_bottom_sheet_tab_helper.h"
#import "ios/chrome/browser/autofill/model/form_suggestion_tab_helper.h"
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
#import "ios/chrome/browser/data_sharing/model/data_sharing_tab_helper.h"
#import "ios/chrome/browser/data_sharing/model/features.h"
#import "ios/chrome/browser/download/model/ar_quick_look_tab_helper.h"
#import "ios/chrome/browser/download/model/document_download_tab_helper.h"
#import "ios/chrome/browser/download/model/download_manager_tab_helper.h"
#import "ios/chrome/browser/download/model/pass_kit_tab_helper.h"
#import "ios/chrome/browser/download/model/safari_download_tab_helper.h"
#import "ios/chrome/browser/download/model/vcard_tab_helper.h"
#import "ios/chrome/browser/drive/model/drive_tab_helper.h"
#import "ios/chrome/browser/favicon/model/favicon_service_factory.h"
#import "ios/chrome/browser/find_in_page/model/find_tab_helper.h"
#import "ios/chrome/browser/find_in_page/model/java_script_find_tab_helper.h"
#import "ios/chrome/browser/find_in_page/model/util.h"
#import "ios/chrome/browser/follow/model/follow_tab_helper.h"
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
#import "ios/chrome/browser/itunes_urls/model/itunes_urls_handler_tab_helper.h"
#import "ios/chrome/browser/lens/model/lens_tab_helper.h"
#import "ios/chrome/browser/lens_overlay/coordinator/lens_overlay_availability.h"
#import "ios/chrome/browser/lens_overlay/model/lens_overlay_tab_helper.h"
#import "ios/chrome/browser/link_to_text/model/link_to_text_tab_helper.h"
#import "ios/chrome/browser/metrics/model/pageload_foreground_duration_tab_helper.h"
#import "ios/chrome/browser/ntp/model/new_tab_page_tab_helper.h"
#import "ios/chrome/browser/optimization_guide/model/optimization_guide_service.h"
#import "ios/chrome/browser/optimization_guide/model/optimization_guide_service_factory.h"
#import "ios/chrome/browser/optimization_guide/model/optimization_guide_tab_helper.h"
#import "ios/chrome/browser/optimization_guide/model/optimization_guide_validation_tab_helper.h"
#import "ios/chrome/browser/overlays/model/public/overlay_request_queue.h"
#import "ios/chrome/browser/overscroll_actions/model/overscroll_actions_tab_helper.h"
#import "ios/chrome/browser/page_info/about_this_site_tab_helper.h"
#import "ios/chrome/browser/passwords/model/password_controller.h"
#import "ios/chrome/browser/passwords/model/password_tab_helper.h"
#import "ios/chrome/browser/passwords/model/well_known_change_password_tab_helper.h"
#import "ios/chrome/browser/permissions/model/permissions_tab_helper.h"
#import "ios/chrome/browser/policy_url_blocking/model/policy_url_blocking_tab_helper.h"
#import "ios/chrome/browser/prerender/model/prerender_service_factory.h"
#import "ios/chrome/browser/reading_list/model/offline_page_tab_helper.h"
#import "ios/chrome/browser/reading_list/model/reading_list_model_factory.h"
#import "ios/chrome/browser/reading_list/model/reading_list_web_state_observer.h"
#import "ios/chrome/browser/safe_browsing/model/safe_browsing_client_factory.h"
#import "ios/chrome/browser/safe_browsing/model/tailored_security/tailored_security_service_factory.h"
#import "ios/chrome/browser/safe_browsing/model/tailored_security/tailored_security_tab_helper.h"
#import "ios/chrome/browser/search_engines/model/search_engine_tab_helper.h"
#import "ios/chrome/browser/sessions/model/ios_chrome_session_tab_helper.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/sharing/model/share_file_download_tab_helper.h"
#import "ios/chrome/browser/snapshots/model/snapshot_tab_helper.h"
#import "ios/chrome/browser/ssl/model/captive_portal_tab_helper.h"
#import "ios/chrome/browser/supervised_user/model/supervised_user_error_container.h"
#import "ios/chrome/browser/supervised_user/model/supervised_user_url_filter_tab_helper.h"
#import "ios/chrome/browser/tabs/model/ios_chrome_synced_tab_delegate.h"
#import "ios/chrome/browser/translate/model/chrome_ios_translate_client.h"
#import "ios/chrome/browser/ui/page_info/features.h"
#import "ios/chrome/browser/voice/model/voice_search_navigations_tab_helper.h"
#import "ios/chrome/browser/web/model/annotations/annotations_tab_helper.h"
#import "ios/chrome/browser/web/model/blocked_popup_tab_helper.h"
#import "ios/chrome/browser/web/model/font_size/font_size_tab_helper.h"
#import "ios/chrome/browser/web/model/image_fetch/image_fetch_tab_helper.h"
#import "ios/chrome/browser/web/model/invalid_url_tab_helper.h"
#import "ios/chrome/browser/web/model/load_timing_tab_helper.h"
#import "ios/chrome/browser/web/model/page_placeholder_tab_helper.h"
#import "ios/chrome/browser/web/model/print/print_tab_helper.h"
#import "ios/chrome/browser/web/model/repost_form_tab_helper.h"
#import "ios/chrome/browser/web/model/sad_tab_tab_helper.h"
#import "ios/chrome/browser/web/model/web_performance_metrics/web_performance_metrics_tab_helper.h"
#import "ios/chrome/browser/web_selection/model/web_selection_tab_helper.h"
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
#import "ios/public/provider/chrome/browser/text_zoom/text_zoom_api.h"
#import "ios/web/common/annotations_utils.h"
#import "ios/web/public/find_in_page/java_script_find_in_page_manager.h"
#import "ios/web/public/web_state.h"

namespace {

// Returns whether the `flag` is set in `mask`.
constexpr bool IsTabHelperFilterMaskSet(TabHelperFilter mask,
                                        TabHelperFilter flag) {
  return (mask & flag) == flag;
}

}  // namespace

void AttachTabHelpers(web::WebState* web_state, TabHelperFilter filter_flags) {
  ProfileIOS* const profile =
      ProfileIOS::FromBrowserState(web_state->GetBrowserState());
  const bool is_off_the_record = profile->IsOffTheRecord();
  const bool for_prerender =
      IsTabHelperFilterMaskSet(filter_flags, TabHelperFilter::kPrerender);
  const bool for_bottom_sheet =
      IsTabHelperFilterMaskSet(filter_flags, TabHelperFilter::kBottomSheet);

  // When adding a new tab helper, please consider whether it should be filtered
  // out when the web_state is presented in the following context:
  // - kPrerender: Tab helpers that are not required or not used for navigation
  // should be filtered out.
  // - kBottomSheet: The bottom sheet is overlayed on the BVC, tab helpers that
  // rely on BVC's toolbar entry points should be filtered out.
  //
  // When a web state is presented by the BVC, AttachTabHelpers is called to
  // attach all tab helpers. (the method is idempotent, so it is okay to call it
  // multiple times for the same WebState).

  // IOSChromeSessionTabHelper sets up the session ID used by other helpers,
  // so it needs to be created before them.
  IOSChromeSessionTabHelper::CreateForWebState(web_state);

  OverlayRequestQueue::CreateForWebState(web_state);

  VoiceSearchNavigationTabHelper::CreateForWebState(web_state);
  IOSChromeSyncedTabDelegate::CreateForWebState(web_state);
  InfoBarManagerImpl::CreateForWebState(web_state);

  if (IsNativeFindInPageAvailable()) {
    FindTabHelper::CreateForWebState(web_state);
  } else {
    web::JavaScriptFindInPageManager::CreateForWebState(web_state);
    JavaScriptFindTabHelper::CreateForWebState(web_state);
  }

  if (!for_bottom_sheet) {
    HistoryTabHelper::CreateForWebState(web_state);
  }
  LoadTimingTabHelper::CreateForWebState(web_state);
  OverscrollActionsTabHelper::CreateForWebState(web_state);
  IOSTaskTabHelper::CreateForWebState(web_state);
  if (!for_bottom_sheet &&
      IsPriceAlertsEligible(web_state->GetBrowserState())) {
    ShoppingPersistedDataTabHelper::CreateForWebState(web_state);
  }
  commerce::CommerceTabHelper::CreateForWebState(
      web_state, is_off_the_record,
      commerce::ShoppingServiceFactory::GetForProfile(profile));

  if (!for_bottom_sheet && !for_prerender) {
    // Since LensTabHelper listens for a custom scheme, it needs to be
    // created before AppLauncherTabHelper, which will filter out
    // unhandled schemes.
    LensTabHelper::CreateForWebState(web_state);
    if (IsLensOverlayAvailable()) {
      LensOverlayTabHelper::CreateForWebState(web_state);
    }
    AppLauncherTabHelper::CreateForWebState(
        web_state, [[AppLauncherAbuseDetector alloc] init], is_off_the_record);
  }
  security_interstitials::IOSBlockingPageTabHelper::CreateForWebState(
      web_state);
  password_manager::WellKnownChangePasswordTabHelper::CreateForWebState(
      web_state);

  InvalidUrlTabHelper::CreateForWebState(web_state);

  if (!for_bottom_sheet) {
    InfobarOverlayRequestInserter::CreateForWebState(
        web_state, &DefaultInfobarOverlayRequestFactory);
    InfobarOverlayTabHelper::CreateForWebState(web_state);
    TranslateOverlayTabHelper::CreateForWebState(web_state);
  }

  if (ios::provider::IsTextZoomEnabled()) {
    FontSizeTabHelper::CreateForWebState(web_state);
  }

  if (breadcrumbs::IsEnabled(GetApplicationContext()->GetLocalState())) {
    BreadcrumbManagerTabHelper::CreateForWebState(web_state);
  }

  AnnotationsTabHelper::CreateForWebState(web_state);

  SafeBrowsingClient* client =
      SafeBrowsingClientFactory::GetForProfile(profile);
  SafeBrowsingQueryManager::CreateForWebState(web_state, client);
  SafeBrowsingTabHelper::CreateForWebState(web_state, client);
  SafeBrowsingUrlAllowList::CreateForWebState(web_state);
  SafeBrowsingUnsafeResourceContainer::CreateForWebState(web_state);

  TailoredSecurityTabHelper::CreateForWebState(
      web_state, TailoredSecurityServiceFactory::GetForProfile(profile));

  PolicyUrlBlockingTabHelper::CreateForWebState(web_state);

  // Supervised user services are not supported for off-the-record browser
  // state.
  if (!is_off_the_record) {
    SupervisedUserURLFilterTabHelper::CreateForWebState(web_state);
    SupervisedUserErrorContainer::CreateForWebState(web_state);
  }

  ImageFetchTabHelper::CreateForWebState(web_state);

  NewTabPageTabHelper::CreateForWebState(web_state);
  ShareFileDownloadTabHelper::CreateForWebState(web_state);
  OptimizationGuideTabHelper::CreateForWebState(web_state);
  OptimizationGuideValidationTabHelper::CreateForWebState(web_state);
  ProfileIOS* original_profile = profile->GetOriginalProfile();
  favicon::WebFaviconDriver::CreateForWebState(
      web_state, ios::FaviconServiceFactory::GetForProfile(
                     original_profile, ServiceAccessType::IMPLICIT_ACCESS));
  history::WebStateTopSitesObserver::CreateForWebState(
      web_state, ios::TopSitesFactory::GetForProfile(original_profile).get());

  // Depends on favicon::WebFaviconDriver, must be created after it.
  SearchEngineTabHelper::CreateForWebState(web_state);

  ukm::InitializeSourceUrlRecorderForWebState(web_state);

  // Download tab helpers.
  DownloadManagerTabHelper::CreateForWebState(web_state);
  SafariDownloadTabHelper::CreateForWebState(web_state);
  VcardTabHelper::CreateForWebState(web_state);

  // Drive tab helper.
  if (base::FeatureList::IsEnabled(kIOSSaveToDrive)) {
    DocumentDownloadTabHelper::CreateForWebState(web_state);
  }

  PageloadForegroundDurationTabHelper::CreateForWebState(web_state);

  LookalikeUrlTabHelper::CreateForWebState(web_state);
  LookalikeUrlTabAllowList::CreateForWebState(web_state);
  LookalikeUrlContainer::CreateForWebState(web_state);

  // TODO(crbug.com/41360476): pre-rendered WebState have lots of unnecessary
  // tab helpers for historical reasons. For the moment, AttachTabHelpers
  // allows to inhibit the creation of some of them. Once PreloadController
  // has been refactored to only create the necessary tab helpers, this
  // condition can be removed.
  if (!for_bottom_sheet && !for_prerender) {
    SadTabTabHelper::CreateForWebState(
        web_state, SadTabTabHelper::kDefaultRepeatFailureInterval);
    SnapshotTabHelper::CreateForWebState(web_state);
    PagePlaceholderTabHelper::CreateForWebState(web_state);
    ChromeIOSTranslateClient::CreateForWebState(web_state);

    PasswordTabHelper::CreateForWebState(web_state);
    AutofillBottomSheetTabHelper::CreateForWebState(web_state);
    AutofillTabHelper::CreateForWebState(web_state);

    FormSuggestionTabHelper::CreateForWebState(web_state, @[
      PasswordTabHelper::FromWebState(web_state)->GetSuggestionProvider(),
      AutofillTabHelper::FromWebState(web_state)->GetSuggestionProvider(),
    ]);
  }

  if (!for_bottom_sheet) {
    InfobarBadgeTabHelper::GetOrCreateForWebState(web_state);
  }

  if (base::FeatureList::IsEnabled(kSharedHighlightingIOS)) {
    LinkToTextTabHelper::CreateForWebState(web_state);
  }

  WebSelectionTabHelper::CreateForWebState(web_state);

  WebPerformanceMetricsTabHelper::CreateForWebState(web_state);

  OfflinePageTabHelper::CreateForWebState(
      web_state, ReadingListModelFactory::GetForProfile(profile));
  PermissionsTabHelper::CreateForWebState(web_state);

  RepostFormTabHelper::CreateForWebState(web_state);

  if (base::FeatureList::IsEnabled(
          security_interstitials::features::kHttpsOnlyMode) ||
      base::FeatureList::IsEnabled(
          security_interstitials::features::kHttpsUpgrades)) {
    HttpsOnlyModeUpgradeTabHelper::CreateForWebState(
        web_state, profile->GetPrefs(),
        PrerenderServiceFactory::GetForProfile(profile),
        HttpsUpgradeServiceFactory::GetForProfile(profile));
    HttpsOnlyModeContainer::CreateForWebState(web_state);
  }

  if (base::FeatureList::IsEnabled(omnibox::kDefaultTypedNavigationsToHttps)) {
    TypedNavigationUpgradeTabHelper::CreateForWebState(
        web_state, PrerenderServiceFactory::GetForProfile(profile),
        HttpsUpgradeServiceFactory::GetForProfile(profile));
  }

  if (!is_off_the_record) {
    FollowTabHelper::CreateForWebState(web_state);
  }

  if (!for_bottom_sheet && !is_off_the_record) {
    PriceNotificationsTabHelper::CreateForWebState(web_state);
  }

  if (!for_bottom_sheet && !is_off_the_record && IsContextualPanelEnabled()) {
    ContextualPanelModelService* model_service =
        ContextualPanelModelServiceFactory::GetForProfile(
            ProfileIOS::FromBrowserState(profile));
    ContextualPanelTabHelper::CreateForWebState(web_state,
                                                model_service->models());
  }

  if (!for_bottom_sheet && !is_off_the_record &&
      IsAboutThisSiteFeatureEnabled()) {
    if (auto* optimization_guide_decider =
            OptimizationGuideServiceFactory::GetForProfile(profile)) {
      AboutThisSiteTabHelper::CreateForWebState(web_state,
                                                optimization_guide_decider);
    }
  }

  if (IsSharedTabGroupsJoinEnabled(profile) && !is_off_the_record &&
      !for_prerender) {
    DataSharingTabHelper::CreateForWebState(web_state);
  }
}
