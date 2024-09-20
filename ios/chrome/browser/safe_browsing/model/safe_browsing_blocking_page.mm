// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/safe_browsing/model/safe_browsing_blocking_page.h"

#import "base/logging.h"
#import "base/memory/ptr_util.h"
#import "base/strings/string_number_conversions.h"
#import "base/time/time.h"
#import "components/feature_engagement/public/event_constants.h"
#import "components/feature_engagement/public/feature_list.h"
#import "components/feature_engagement/public/tracker.h"
#import "components/prefs/pref_service.h"
#import "components/safe_browsing/core/browser/safe_browsing_metrics_collector.h"
#import "components/safe_browsing/core/common/features.h"
#import "components/safe_browsing/core/common/safe_browsing_prefs.h"
#import "components/safe_browsing/core/common/utils.h"
#import "components/safe_browsing/ios/browser/safe_browsing_url_allow_list.h"
#import "components/security_interstitials/core/base_safe_browsing_error_ui.h"
#import "components/security_interstitials/core/metrics_helper.h"
#import "components/security_interstitials/core/safe_browsing_loud_error_ui.h"
#import "ios/chrome/browser/feature_engagement/model/tracker_factory.h"
#import "ios/chrome/browser/safe_browsing/model/safe_browsing_metrics_collector_factory.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/components/security_interstitials/ios_blocking_page_metrics_helper.h"
#import "ios/components/security_interstitials/safe_browsing/safe_browsing_tab_helper.h"
#import "ios/components/security_interstitials/safe_browsing/unsafe_resource_util.h"
#import "ios/web/public/web_state.h"
#import "ui/base/resource/resource_bundle.h"
#import "ui/base/webui/web_ui_util.h"

using security_interstitials::BaseSafeBrowsingErrorUI;
using security_interstitials::IOSBlockingPageMetricsHelper;
using security_interstitials::SafeBrowsingLoudErrorUI;
using security_interstitials::SecurityInterstitialCommand;
using security_interstitials::UnsafeResource;

namespace {
// Creates a metrics helper for `resource`.
std::unique_ptr<IOSBlockingPageMetricsHelper> CreateMetricsHelper(
    const UnsafeResource& resource) {
  security_interstitials::MetricsHelper::ReportDetails reporting_info;
  reporting_info.metric_prefix = GetUnsafeResourceMetricPrefix(resource);
  reporting_info.extra_suffix = safe_browsing::GetExtraMetricsSuffix(resource);
  return std::make_unique<IOSBlockingPageMetricsHelper>(
      resource.weak_web_state.get(), resource.url, reporting_info);
}
// Returns the default safe browsing error display options.
BaseSafeBrowsingErrorUI::SBErrorDisplayOptions GetDefaultDisplayOptions(
    const UnsafeResource& resource) {
  web::WebState* web_state = resource.weak_web_state.get();
  ProfileIOS* profile =
      ProfileIOS::FromBrowserState(web_state->GetBrowserState());
  PrefService* prefs = profile->GetPrefs();
  safe_browsing::SafeBrowsingMetricsCollector* metrics_collector =
      SafeBrowsingMetricsCollectorFactory::GetForProfile(profile);
  if (metrics_collector) {
    metrics_collector->AddSafeBrowsingEventToPref(
        safe_browsing::SafeBrowsingMetricsCollector::
            SECURITY_SENSITIVE_SAFE_BROWSING_INTERSTITIAL);
  }
  return BaseSafeBrowsingErrorUI::SBErrorDisplayOptions(
      resource.IsMainPageLoadPendingWithSyncCheck(),
      /*is_extended_reporting_opt_in_allowed=*/false,
      /*is_off_the_record=*/false,
      /*is_extended_reporting=*/false,
      /*is_sber_policy_managed=*/false,
      safe_browsing::IsEnhancedProtectionEnabled(*prefs),
      prefs->GetBoolean(prefs::kSafeBrowsingProceedAnywayDisabled),
      /*should_open_links_in_new_tab=*/false,
      /*always_show_back_to_safety=*/true,
      /*is_enhanced_protection_message_enabled=*/true,
      /*is_safe_browsing_managed=*/false, "cpn_safe_browsing");
}
}  // namespace

#pragma mark - SafeBrowsingBlockingPage

// static
std::unique_ptr<SafeBrowsingBlockingPage> SafeBrowsingBlockingPage::Create(
    const security_interstitials::UnsafeResource& resource) {
  std::unique_ptr<SafeBrowsingControllerClient> client =
      base::WrapUnique(new SafeBrowsingControllerClient(resource));
  std::unique_ptr<SafeBrowsingBlockingPage> blocking_page =
      base::WrapUnique(new SafeBrowsingBlockingPage(resource, client.get()));
  blocking_page->SetClient(std::move(client));
  return blocking_page;
}

SafeBrowsingBlockingPage::SafeBrowsingBlockingPage(
    const security_interstitials::UnsafeResource& resource,
    SafeBrowsingControllerClient* client)
    : IOSSecurityInterstitialPage(resource.weak_web_state.get(),
                                  GetMainFrameUrl(resource),
                                  client),
      is_main_page_load_blocked_(resource.IsMainPageLoadPendingWithSyncCheck()),
      error_ui_(std::make_unique<SafeBrowsingLoudErrorUI>(
          resource.url,
          GetUnsafeResourceInterstitialReason(resource),
          GetDefaultDisplayOptions(resource),
          client->GetApplicationLocale(),
          base::Time::NowFromSystemTime(),
          client,
          is_main_page_load_blocked_)) {}

SafeBrowsingBlockingPage::~SafeBrowsingBlockingPage() = default;

void SafeBrowsingBlockingPage::SetClient(
    std::unique_ptr<SafeBrowsingControllerClient> client) {
  client_ = std::move(client);
}

std::string SafeBrowsingBlockingPage::GetHtmlContents() const {
  base::Value::Dict load_time_data;
  PopulateInterstitialStrings(load_time_data);
  webui::SetLoadTimeDataDefaults(client_->GetApplicationLocale(),
                                 &load_time_data);
  std::string html =
      ui::ResourceBundle::GetSharedInstance().LoadDataResourceString(
          error_ui_->GetHTMLTemplateId());
  webui::AppendWebUiCssTextDefaults(&html);
  return webui::GetLocalizedHtml(html, load_time_data);
}

void SafeBrowsingBlockingPage::HandleCommand(
    SecurityInterstitialCommand command) {
  error_ui_->HandleCommand(command);
  if (command == security_interstitials::CMD_DONT_PROCEED) {
    // `error_ui_` handles recording PROCEED and
    // OPEN_ENHANCED_PROTECTION_SETTINGS decisions.
    client_->metrics_helper()->RecordUserDecision(
        security_interstitials::MetricsHelper::DONT_PROCEED);
  }
}

bool SafeBrowsingBlockingPage::ShouldCreateNewNavigation() const {
  return is_main_page_load_blocked_;
}

void SafeBrowsingBlockingPage::PopulateInterstitialStrings(
    base::Value::Dict& load_time_data) const {
  load_time_data.Set("url_to_reload", request_url().spec());
  error_ui_->PopulateStringsForHtml(load_time_data);
}

void SafeBrowsingBlockingPage::ShowInfobar() {
  ProfileIOS* profile =
      ProfileIOS::FromBrowserState(web_state()->GetBrowserState());
  feature_engagement::Tracker* tracker =
      feature_engagement::TrackerFactory::GetForProfile(profile);
  tracker->NotifyEvent(
      feature_engagement::events::kEnhancedSafeBrowsingPromoCriterionMet);

  if (!base::FeatureList::IsEnabled(
          safe_browsing::kEnhancedSafeBrowsingPromo)) {
    return;
  }

  client_->ShowEnhancedSafeBrowsingInfobar();
}

#pragma mark - SafeBrowsingBlockingPage::SafeBrowsingControllerClient

SafeBrowsingBlockingPage::SafeBrowsingControllerClient::
    SafeBrowsingControllerClient(const UnsafeResource& resource)
    : IOSBlockingPageControllerClient(
          resource.weak_web_state.get(),
          CreateMetricsHelper(resource),
          GetApplicationContext()->GetApplicationLocale()),
      url_(SafeBrowsingUrlAllowList::GetDecisionUrl(resource)),
      threat_type_(resource.threat_type),
      threat_source_(resource.threat_source) {}

SafeBrowsingBlockingPage::SafeBrowsingControllerClient::
    ~SafeBrowsingControllerClient() {
  if (web_state()) {
    SafeBrowsingUrlAllowList::FromWebState(web_state())
        ->RemovePendingUnsafeNavigationDecisions(url_);
  }
}

void SafeBrowsingBlockingPage::SafeBrowsingControllerClient::Proceed() {
  if (web_state()) {
    SafeBrowsingUrlAllowList::FromWebState(web_state())
        ->AllowUnsafeNavigations(url_, threat_type_);
    ProfileIOS* profile =
        ProfileIOS::FromBrowserState(web_state()->GetBrowserState());
    safe_browsing::SafeBrowsingMetricsCollector* metrics_collector =
        SafeBrowsingMetricsCollectorFactory::GetForProfile(profile);
    if (metrics_collector) {
      metrics_collector->AddBypassEventToPref(threat_source_);
    }
  }
  Reload();
}

void SafeBrowsingBlockingPage::SafeBrowsingControllerClient::GoBack() {
  if (web_state()) {
    SafeBrowsingUrlAllowList::FromWebState(web_state())
        ->RemovePendingUnsafeNavigationDecisions(url_);
  }
  security_interstitials::IOSBlockingPageControllerClient::GoBack();
}

void SafeBrowsingBlockingPage::SafeBrowsingControllerClient::
    GoBackAfterNavigationCommitted() {
  // Safe browsing blocking pages are always committed, and should use
  // consistent "Return to safety" behavior.
  GoBack();
}

void SafeBrowsingBlockingPage::SafeBrowsingControllerClient::
    OpenEnhancedProtectionSettings() {
  if (web_state()) {
    SafeBrowsingTabHelper::FromWebState(web_state())
        ->OpenSafeBrowsingSettings();
  }
}

void SafeBrowsingBlockingPage::SafeBrowsingControllerClient::
    ShowEnhancedSafeBrowsingInfobar() {
  ProfileIOS* profile =
      ProfileIOS::FromBrowserState(web_state()->GetBrowserState());
  const PrefService* prefs = profile->GetPrefs();
  bool is_enterprise_managed =
      safe_browsing::IsSafeBrowsingPolicyManaged(*prefs);
  bool is_standard_safe_browsing_user =
      safe_browsing::GetSafeBrowsingState(*prefs) ==
      safe_browsing::SafeBrowsingState::STANDARD_PROTECTION;
  if (web_state() && !is_enterprise_managed && is_standard_safe_browsing_user) {
    SafeBrowsingTabHelper::FromWebState(web_state())
        ->ShowEnhancedSafeBrowsingInfobar();
  }
}
