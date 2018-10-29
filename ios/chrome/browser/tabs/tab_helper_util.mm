// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/tabs/tab_helper_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

#include "base/feature_list.h"
#import "components/favicon/ios/web_favicon_driver.h"
#include "components/history/core/browser/top_sites.h"
#import "components/history/ios/browser/web_state_top_sites_observer.h"
#include "components/keyed_service/core/service_access_type.h"
#import "components/language/ios/browser/ios_language_detection_tab_helper.h"
#import "ios/chrome/browser/autofill/autofill_tab_helper.h"
#import "ios/chrome/browser/autofill/form_suggestion_tab_helper.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/favicon/favicon_service_factory.h"
#import "ios/chrome/browser/find_in_page/find_tab_helper.h"
#include "ios/chrome/browser/history/history_service_factory.h"
#include "ios/chrome/browser/history/history_tab_helper.h"
#include "ios/chrome/browser/history/top_sites_factory.h"
#import "ios/chrome/browser/infobars/infobar_manager_impl.h"
#include "ios/chrome/browser/itunes_urls/itunes_urls_flag.h"
#import "ios/chrome/browser/itunes_urls/itunes_urls_handler_tab_helper.h"
#import "ios/chrome/browser/metrics/ukm_url_recorder.h"
#import "ios/chrome/browser/passwords/password_tab_helper.h"
#include "ios/chrome/browser/reading_list/reading_list_model_factory.h"
#import "ios/chrome/browser/reading_list/reading_list_web_state_observer.h"
#import "ios/chrome/browser/search_engines/feature_flags.h"
#import "ios/chrome/browser/search_engines/search_engine_tab_helper.h"
#import "ios/chrome/browser/sessions/ios_chrome_session_tab_helper.h"
#import "ios/chrome/browser/snapshots/snapshot_tab_helper.h"
#include "ios/chrome/browser/ssl/captive_portal_features.h"
#import "ios/chrome/browser/ssl/captive_portal_metrics_tab_helper.h"
#import "ios/chrome/browser/ssl/insecure_input_tab_helper.h"
#import "ios/chrome/browser/ssl/ios_security_state_tab_helper.h"
#import "ios/chrome/browser/store_kit/store_kit_tab_helper.h"
#import "ios/chrome/browser/sync/ios_chrome_synced_tab_delegate.h"
#import "ios/chrome/browser/tabs/legacy_tab_helper.h"
#import "ios/chrome/browser/tabs/tab.h"
#import "ios/chrome/browser/translate/chrome_ios_translate_client.h"
#import "ios/chrome/browser/ui/ui_feature_flags.h"
#import "ios/chrome/browser/voice/voice_search_navigations_tab_helper.h"
#import "ios/chrome/browser/web/blocked_popup_tab_helper.h"
#import "ios/chrome/browser/web/features.h"
#import "ios/chrome/browser/web/font_size_tab_helper.h"
#import "ios/chrome/browser/web/image_fetch_tab_helper.h"
#import "ios/chrome/browser/web/load_timing_tab_helper.h"
#import "ios/chrome/browser/web/network_activity_indicator_tab_helper.h"
#import "ios/chrome/browser/web/page_placeholder_tab_helper.h"
#import "ios/chrome/browser/web/repost_form_tab_helper.h"
#import "ios/chrome/browser/web/tab_id_tab_helper.h"
#import "ios/public/provider/chrome/browser/chrome_browser_provider.h"
#import "ios/web/public/web_state/web_state.h"

void AttachTabHelpers(web::WebState* web_state, bool for_prerender) {
  // Tab's WebStateObserver callbacks expect VoiceSearchNavigationTabHelper's
  // callbacks to be executed first so that state stays in sync.
  // TODO(crbug.com/778416): Remove this ordering requirement by relying solely
  // on the tab helper without going through Tab.
  VoiceSearchNavigationTabHelper::CreateForWebState(web_state);

  // TabIdHelper sets up the tab ID which is required for the creation of the
  // Tab by LegacyTabHelper.
  TabIdTabHelper::CreateForWebState(web_state);
  LegacyTabHelper::CreateForWebState(web_state);
  Tab* tab = LegacyTabHelper::GetTabForWebState(web_state);
  DCHECK(tab);

  ios::ChromeBrowserState* browser_state =
      ios::ChromeBrowserState::FromBrowserState(web_state->GetBrowserState());

  // IOSChromeSessionTabHelper sets up the session ID used by other helpers,
  // so it needs to be created before them.
  IOSChromeSessionTabHelper::CreateForWebState(web_state);

  NSString* tab_id = TabIdTabHelper::FromWebState(web_state)->tab_id();
  NetworkActivityIndicatorTabHelper::CreateForWebState(web_state, tab_id);
  IOSChromeSyncedTabDelegate::CreateForWebState(web_state);
  InfoBarManagerImpl::CreateForWebState(web_state);
  IOSSecurityStateTabHelper::CreateForWebState(web_state);
  BlockedPopupTabHelper::CreateForWebState(web_state);
  FindTabHelper::CreateForWebState(web_state);
  StoreKitTabHelper::CreateForWebState(web_state);
  if (base::FeatureList::IsEnabled(kITunesUrlsStoreKitHandling)) {
    ITunesUrlsHandlerTabHelper::CreateForWebState(web_state);
  }
  HistoryTabHelper::CreateForWebState(web_state);
  LoadTimingTabHelper::CreateForWebState(web_state);

  if (base::FeatureList::IsEnabled(kCaptivePortalMetrics)) {
    CaptivePortalMetricsTabHelper::CreateForWebState(web_state);
  }

  if (base::FeatureList::IsEnabled(web::kWebPageTextAccessibility)) {
    FontSizeTabHelper::CreateForWebState(web_state);
  }

  if (base::FeatureList::IsEnabled(kCopyImage)) {
    ImageFetchTabHelper::CreateForWebState(web_state);
  }

  if (base::FeatureList::IsEnabled(kCustomSearchEngines)) {
    SearchEngineTabHelper::CreateForWebState(web_state);
  }

  ReadingListModel* model =
      ReadingListModelFactory::GetForBrowserState(browser_state);
  ReadingListWebStateObserver::CreateForWebState(web_state, model);

  ios::ChromeBrowserState* original_browser_state =
      browser_state->GetOriginalChromeBrowserState();
  favicon::WebFaviconDriver::CreateForWebState(
      web_state,
      ios::FaviconServiceFactory::GetForBrowserState(
          original_browser_state, ServiceAccessType::IMPLICIT_ACCESS));
  history::WebStateTopSitesObserver::CreateForWebState(
      web_state,
      ios::TopSitesFactory::GetForBrowserState(original_browser_state).get());

  PasswordTabHelper::CreateForWebState(web_state);

  AutofillTabHelper::CreateForWebState(web_state, nullptr);

  FormSuggestionTabHelper::CreateForWebState(web_state, @[
    PasswordTabHelper::FromWebState(web_state)->GetSuggestionProvider(),
    AutofillTabHelper::FromWebState(web_state)->GetSuggestionProvider(),
  ]);

  InsecureInputTabHelper::CreateForWebState(web_state);

  ukm::InitializeSourceUrlRecorderForWebState(web_state);

  // TODO(crbug.com/794115): pre-rendered WebState have lots of unnecessary
  // tab helpers for historical reasons. For the moment, AttachTabHelpers
  // allows to inhibit the creation of some of them. Once PreloadController
  // has been refactored to only create the necessary tab helpers, this
  // condition can be removed.
  if (!for_prerender) {
    SnapshotTabHelper::CreateForWebState(web_state, tab_id);
    PagePlaceholderTabHelper::CreateForWebState(web_state);
  }

  // Allow the embedder to attach tab helpers.
  ios::GetChromeBrowserProvider()->AttachTabHelpers(web_state, tab);
}
