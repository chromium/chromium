// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/enterprise/connectors/ios_enterprise_interstitial.h"

#import "components/grit/components_resources.h"
#import "components/safe_browsing/core/browser/db/v4_protocol_manager_util.h"
#import "components/security_interstitials/core/urls.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
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
      client_(std::move(client)) {}

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
  if (command == security_interstitials::CMD_DONT_PROCEED) {
    client_->metrics_helper()->RecordUserDecision(
        security_interstitials::MetricsHelper::DONT_PROCEED);
  } else if (command == security_interstitials::CMD_OPEN_HELP_CENTER) {
    client_->metrics_helper()->RecordUserInteraction(
        security_interstitials::MetricsHelper::SHOW_LEARN_MORE);
    client_->OpenUrlInNewForegroundTab(
        GURL(security_interstitials::kEnterpriseInterstitialHelpLink));
  } else if (command == security_interstitials::CMD_PROCEED &&
             type() == EnterpriseInterstitialBase::Type::kWarn) {
    client_->metrics_helper()->RecordUserDecision(
        security_interstitials::MetricsHelper::PROCEED);
  } else {
    // Not supported by the URL blocking page.
    NOTREACHED() << "Unsupported command: " << command;
  }
}

bool IOSEnterpriseInterstitial::ShouldCreateNewNavigation() const {
  return true;
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
          GetApplicationContext()->GetApplicationLocale()) {}

IOSEnterpriseInterstitial::EnterprisePageControllerClient::
    ~EnterprisePageControllerClient() = default;

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

}  // namespace enterprise_connectors
