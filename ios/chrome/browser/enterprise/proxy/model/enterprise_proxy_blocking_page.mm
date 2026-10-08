// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/enterprise/proxy/model/enterprise_proxy_blocking_page.h"

#import <memory>
#import <string>
#import <string_view>
#import <utility>

#import "base/memory/ptr_util.h"
#import "base/values.h"
#import "components/application_locale_storage/application_locale_storage.h"
#import "components/enterprise/net/core/enterprise_proxy_error_data.h"
#import "components/grit/components_resources.h"
#import "components/security_interstitials/core/controller_client.h"
#import "components/security_interstitials/core/metrics_helper.h"
#import "components/security_interstitials/core/utils.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/components/security_interstitials/ios_blocking_page_controller_client.h"
#import "ios/components/security_interstitials/ios_blocking_page_metrics_helper.h"
#import "ios/components/ui_util/dynamic_type_util.h"
#import "ui/base/resource/resource_bundle.h"
#import "ui/base/webui/web_ui_util.h"
#import "url/gurl.h"

namespace {

using ErrorCategory = enterprise_net::EnterpriseProxyErrorData::ErrorCategory;

// Prefix of the interstitial histograms. Required by the metrics helper, even
// though the page records no metrics yet.
// TODO(crbug.com/568816402): Record the interstitial metrics.
constexpr std::string_view kMetricPrefix = "enterprise_proxy";

// Template parameter of the URL to reload instead of loading from cache.
constexpr std::string_view kUrlToReloadKey = "url_to_reload";

// Creates the controller client of the page displayed in `web_state` for
// `request_url`.
std::unique_ptr<security_interstitials::IOSBlockingPageControllerClient>
CreateControllerClient(web::WebState* web_state, const GURL& request_url) {
  security_interstitials::MetricsHelper::ReportDetails report_details;
  report_details.metric_prefix = kMetricPrefix;
  return std::make_unique<
      security_interstitials::IOSBlockingPageControllerClient>(
      web_state,
      std::make_unique<security_interstitials::IOSBlockingPageMetricsHelper>(
          web_state, request_url, report_details),
      std::string(GetApplicationContext()
                      ->GetApplicationLocaleStorage()
                      ->GetTag()
                      .tag_string()));
}

}  // namespace

// static
std::unique_ptr<EnterpriseProxyBlockingPage>
EnterpriseProxyBlockingPage::Create(
    web::WebState* web_state,
    const enterprise_net::EnterpriseProxyErrorData& error_data,
    base::DictValue error_page_params) {
  // `base::WrapUnique` because the constructor is private.
  return base::WrapUnique(new EnterpriseProxyBlockingPage(
      web_state, error_data, std::move(error_page_params),
      CreateControllerClient(web_state, error_data.destination_url())));
}

EnterpriseProxyBlockingPage::EnterpriseProxyBlockingPage(
    web::WebState* web_state,
    const enterprise_net::EnterpriseProxyErrorData& error_data,
    base::DictValue error_page_params,
    std::unique_ptr<security_interstitials::IOSBlockingPageControllerClient>
        client)
    : IOSSecurityInterstitialPage(web_state,
                                  error_data.destination_url(),
                                  client.get()),
      category_(error_data.error_category()),
      error_page_params_(std::move(error_page_params)),
      // Always move `client` after it's been passed to the base class.
      client_(std::move(client)) {}

EnterpriseProxyBlockingPage::~EnterpriseProxyBlockingPage() = default;

std::string EnterpriseProxyBlockingPage::GetHtmlContents() const {
  // Similar to `IOSSecurityInterstitialPage::GetHtmlContents()`, but with the
  // Secure Gateway template, which the base class doesn't let subclasses pick.
  base::DictValue load_time_data;
  // Interstitial pages on iOS get reloaded to prevent loading from cache, since
  // loading from cache breaks JavaScript commands.
  load_time_data.Set(kUrlToReloadKey, request_url().spec());
  PopulateInterstitialStrings(load_time_data);
  webui::SetLoadTimeDataDefaults(client_->GetApplicationLocale(),
                                 &load_time_data);
  // Scales `$i18n{fontsize}`, which the template stylesheet applies to the
  // body, by the Dynamic Type multiplier.
  security_interstitials::AdjustFontSize(
      load_time_data, ui_util::SystemSuggestedFontSizeMultiplier());
  // Unlike the base class, don't append the WebUI text-default CSS: the
  // template stylesheet already sets the same `body` font rules, and the
  // appended rules, which come later with the same specificity, would override
  // the scaled font size with the unscaled default.
  std::string html =
      ui::ResourceBundle::GetSharedInstance().LoadDataResourceString(
          IDR_ENTERPRISE_PROXY_ERROR_PAGE_HTML);
  return webui::GetLocalizedHtml(html, load_time_data);
}

void EnterpriseProxyBlockingPage::HandleCommand(
    security_interstitials::SecurityInterstitialCommand command) {
  switch (command) {
    case security_interstitials::CMD_DONT_PROCEED:
      // Closes the tab when there is no page to go back to.
      client_->GoBack();
      return;
    case security_interstitials::CMD_OPEN_LOGIN:
      // Only the authentication page offers to sign in again.
      if (category_ != ErrorCategory::kAuthentication) {
        return;
      }
      // TODO(crbug.com/568444429): Forward the sign-in request to
      // `EnterpriseProxyTabHelper`.
      return;
    default:
      // The page sends no other command.
      return;
  }
}

bool EnterpriseProxyBlockingPage::ShouldCreateNewNavigation() const {
  return true;
}

void EnterpriseProxyBlockingPage::PopulateInterstitialStrings(
    base::DictValue& load_time_data) const {
  load_time_data.Merge(error_page_params_.Clone());
}
