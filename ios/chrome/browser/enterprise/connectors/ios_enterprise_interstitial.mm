// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/enterprise/connectors/ios_enterprise_interstitial.h"

#import "components/grit/components_resources.h"
#import "components/safe_browsing/core/browser/db/v4_protocol_manager_util.h"
#import "components/safe_browsing/core/browser/safe_browsing_metrics_collector.h"
#import "components/safe_browsing/core/common/proto/realtimeapi.pb.h"
#import "components/safe_browsing/ios/browser/safe_browsing_url_allow_list.h"
#import "components/security_interstitials/core/urls.h"
#import "ios/chrome/browser/enterprise/connectors/reporting/reporting_util.h"
#import "ios/chrome/browser/safe_browsing/model/safe_browsing_metrics_collector_factory.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/components/security_interstitials/ios_blocking_page_metrics_helper.h"
#import "ui/base/resource/resource_bundle.h"
#import "ui/base/webui/web_ui_util.h"

namespace enterprise_connectors {

namespace {

// Creates a metrics helper for `resource`.
std::unique_ptr<security_interstitials::IOSBlockingPageMetricsHelper>
CreateMetricsHelper(const security_interstitials::UnsafeResource& resource) {
  security_interstitials::MetricsHelper::ReportDetails reporting_info;
  switch (resource.threat_type) {
    case safe_browsing::SBThreatType::SB_THREAT_TYPE_MANAGED_POLICY_WARN:
      reporting_info.metric_prefix = "enterprise_warn";
      break;
    case safe_browsing::SBThreatType::SB_THREAT_TYPE_MANAGED_POLICY_BLOCK:
      reporting_info.metric_prefix = "enterprise_block";
      break;
    default:
      NOTREACHED();
  }

  return std::make_unique<security_interstitials::IOSBlockingPageMetricsHelper>(
      resource.weak_web_state.get(), resource.url, reporting_info);
}

class IOSEnterpriseBlockInterstitial : public IOSEnterpriseInterstitial {
 public:
  IOSEnterpriseBlockInterstitial(
      const security_interstitials::UnsafeResource& resource,
      std::unique_ptr<EnterprisePageControllerClient> client)
      : IOSEnterpriseInterstitial(resource, std::move(client)) {
    DCHECK_EQ(resource.threat_type,
              safe_browsing::SBThreatType::SB_THREAT_TYPE_MANAGED_POLICY_BLOCK);

    ReportEnterpriseUrlFilteringEvent(UrlFilteringEventType::kBlockedSeen,
                                      request_url(), web_state());
  }

  // EnterpriseInterstitialBase:
  EnterpriseInterstitialBase::Type type() const override {
    return EnterpriseInterstitialBase::Type::kBlock;
  }
};

class IOSEnterpriseWarnInterstitial : public IOSEnterpriseInterstitial {
 public:
  IOSEnterpriseWarnInterstitial(
      const security_interstitials::UnsafeResource& resource,
      std::unique_ptr<EnterprisePageControllerClient> client)
      : IOSEnterpriseInterstitial(resource, std::move(client)) {
    DCHECK_EQ(resource.threat_type,
              safe_browsing::SBThreatType::SB_THREAT_TYPE_MANAGED_POLICY_WARN);
    ReportEnterpriseUrlFilteringEvent(UrlFilteringEventType::kWarnedSeen,
                                      request_url(), web_state());
  }

  // EnterpriseInterstitialBase:
  EnterpriseInterstitialBase::Type type() const override {
    return EnterpriseInterstitialBase::Type::kWarn;
  }
};

}  // namespace

#pragma mark - IOSEnterpriseInterstitial

// static
std::unique_ptr<IOSEnterpriseInterstitial>
IOSEnterpriseInterstitial::CreateBlockingPage(
    const security_interstitials::UnsafeResource& resource) {
  return std::make_unique<IOSEnterpriseBlockInterstitial>(
      resource, std::make_unique<EnterprisePageControllerClient>(resource));
}

// static
std::unique_ptr<IOSEnterpriseInterstitial>
IOSEnterpriseInterstitial::CreateWarningPage(
    const security_interstitials::UnsafeResource& resource) {
  return std::make_unique<IOSEnterpriseWarnInterstitial>(
      resource, std::make_unique<EnterprisePageControllerClient>(resource));
}

IOSEnterpriseInterstitial::IOSEnterpriseInterstitial(
    const security_interstitials::UnsafeResource& resource,
    std::unique_ptr<IOSEnterpriseInterstitial::EnterprisePageControllerClient>
        client)
    : IOSSecurityInterstitialPage(resource.weak_web_state.get(),
                                  resource.url,
                                  client.get()),
      unsafe_resources_({resource}),
      // Always do the `client` std::move after it's been passed to the
      // `IOSSecurityInterstitialPage` constructor or other members.
      client_(std::move(client)) {
  if (web_state()) {
    // The EnterpriseWarnPage requires the allow list to be instantiated. The
    // allow list is not intantiated when opening the interstitial directly
    // through the chrome://interstitials WebUI page.
    SafeBrowsingUrlAllowList::CreateForWebState(web_state());
  }

  client_->metrics_helper()->RecordUserDecision(
      security_interstitials::MetricsHelper::SHOW);
  client_->metrics_helper()->RecordUserInteraction(
      security_interstitials::MetricsHelper::TOTAL_VISITS);
}

IOSEnterpriseInterstitial::~IOSEnterpriseInterstitial() = default;

std::string IOSEnterpriseInterstitial::GetHtmlContents() const {
  base::Value::Dict load_time_data;
  PopulateInterstitialStrings(load_time_data);
  webui::SetLoadTimeDataDefaults(client_->GetApplicationLocale(),
                                 &load_time_data);
  std::string html =
      ui::ResourceBundle::GetSharedInstance().LoadDataResourceString(
          IDR_SECURITY_INTERSTITIAL_HTML);
  webui::AppendWebUiCssTextDefaults(&html);
  return webui::GetLocalizedHtml(html, load_time_data);
}

void IOSEnterpriseInterstitial::HandleCommand(
    security_interstitials::SecurityInterstitialCommand command) {
  // Delegate the primary command handling logic to the client.
  client_->HandleCommand(command);
}

bool IOSEnterpriseInterstitial::ShouldCreateNewNavigation() const {
  return true;
}

void IOSEnterpriseInterstitial::WasDismissed() {
  // Record do not proceed when tab is closed but not via page commands. For
  // example: tapping the back button or closing the tab.
  client_->metrics_helper()->RecordUserDecision(
      security_interstitials::MetricsHelper::DONT_PROCEED);
}

void IOSEnterpriseInterstitial::PopulateInterstitialStrings(
    base::Value::Dict& load_time_data) const {
  PopulateStrings(load_time_data);
}

const std::vector<security_interstitials::UnsafeResource>&
IOSEnterpriseInterstitial::unsafe_resources() const {
  return unsafe_resources_;
}

GURL IOSEnterpriseInterstitial::request_url() const {
  return security_interstitials::IOSSecurityInterstitialPage::request_url();
}

#pragma mark - IOSEnterpriseInterstitial::EnterprisePageControllerClient

IOSEnterpriseInterstitial::EnterprisePageControllerClient::
    EnterprisePageControllerClient(
        const security_interstitials::UnsafeResource& resource)
    : IOSBlockingPageControllerClient(
          resource.weak_web_state.get(),
          CreateMetricsHelper(resource),
          GetApplicationContext()->GetApplicationLocale()),
      request_url_(resource.url),
      threat_type_(resource.threat_type),
      threat_source_(resource.threat_source) {}

IOSEnterpriseInterstitial::EnterprisePageControllerClient::
    ~EnterprisePageControllerClient() {
  RemovePendingUnsafeNavigationDecisionsFromAllowList();
}

void IOSEnterpriseInterstitial::EnterprisePageControllerClient::HandleCommand(
    security_interstitials::SecurityInterstitialCommand command) {
  switch (command) {
    case security_interstitials::CMD_DONT_PROCEED:
      metrics_helper()->RecordUserDecision(
          security_interstitials::MetricsHelper::DONT_PROCEED);
      RemovePendingUnsafeNavigationDecisionsFromAllowList();
      GoBack();
      break;

    case security_interstitials::CMD_OPEN_HELP_CENTER:
      metrics_helper()->RecordUserInteraction(
          security_interstitials::MetricsHelper::SHOW_LEARN_MORE);
      OpenUrlInNewForegroundTab(
          GURL(security_interstitials::kEnterpriseInterstitialHelpLink));
      break;

    case security_interstitials::CMD_PROCEED:
      // Proceed is only valid for Warning pages.
      CHECK_EQ(threat_type_,
               safe_browsing::SBThreatType::SB_THREAT_TYPE_MANAGED_POLICY_WARN);

      if (!web_state()) {
        return;
      }

      // Report that the user bypassed the warning.
      ReportEnterpriseUrlFilteringEvent(UrlFilteringEventType::kBypassed,
                                        request_url_, web_state());

      // Add the URL to the allowlist for this specific threat type.
      if (SafeBrowsingUrlAllowList* allow_list =
              SafeBrowsingUrlAllowList::FromWebState(web_state())) {
        allow_list->AllowUnsafeNavigations(request_url_, threat_type_);
      }

      // Record bypass metrics.
      if (ProfileIOS* profile =
              ProfileIOS::FromBrowserState(web_state()->GetBrowserState())) {
        if (safe_browsing::SafeBrowsingMetricsCollector* metrics_collector =
                SafeBrowsingMetricsCollectorFactory::GetForProfile(profile)) {
          metrics_collector->AddBypassEventToPref(threat_source_);
        }
      }

      metrics_helper()->RecordUserDecision(
          security_interstitials::MetricsHelper::PROCEED);
      // Trigger the actual navigation/reload.
      Proceed();
      break;

    default:
      NOTREACHED() << "Unsupported command received: " << command;
  }
}

void IOSEnterpriseInterstitial::EnterprisePageControllerClient::Proceed() {
  Reload();
}

void IOSEnterpriseInterstitial::EnterprisePageControllerClient::GoBack() {
  security_interstitials::IOSBlockingPageControllerClient::GoBack();
}

void IOSEnterpriseInterstitial::EnterprisePageControllerClient::
    GoBackAfterNavigationCommitted() {
  GoBack();
}

void IOSEnterpriseInterstitial::EnterprisePageControllerClient::
    RemovePendingUnsafeNavigationDecisionsFromAllowList() {
  if (web_state()) {
    SafeBrowsingUrlAllowList::FromWebState(web_state())
        ->RemovePendingUnsafeNavigationDecisions(request_url_);
  }
}

}  // namespace enterprise_connectors
