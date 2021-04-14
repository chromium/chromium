// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/safe_browsing/safe_browsing_blocking_page.h"

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#import "components/safe_browsing/ios/browser/safe_browsing_url_allow_list.h"
#include "components/security_interstitials/core/base_safe_browsing_error_ui.h"
#include "components/security_interstitials/core/metrics_helper.h"
#include "components/security_interstitials/core/safe_browsing_loud_error_ui.h"
#include "ios/chrome/browser/application_context.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/safe_browsing/unsafe_resource_util.h"
#include "ios/components/security_interstitials/ios_blocking_page_metrics_helper.h"
#import "ios/web/public/web_state.h"
#include "services/network/public/mojom/fetch_api.mojom.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/webui/jstemplate_builder.h"
#include "ui/base/webui/web_ui_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using security_interstitials::SecurityInterstitialCommand;
using security_interstitials::BaseSafeBrowsingErrorUI;
using security_interstitials::UnsafeResource;
using security_interstitials::IOSBlockingPageMetricsHelper;
using security_interstitials::SafeBrowsingLoudErrorUI;

namespace {
// Retrieves the main frame URL for |resource| in |web_state|.
const GURL GetMainFrameUrl(const UnsafeResource& resource) {
  if (resource.request_destination ==
      network::mojom::RequestDestination::kDocument)
    return resource.url;
  return resource.web_state_getter.Run()->GetLastCommittedURL();
}
// Creates a metrics helper for |resource|.
std::unique_ptr<IOSBlockingPageMetricsHelper> CreateMetricsHelper(
    const UnsafeResource& resource) {
  security_interstitials::MetricsHelper::ReportDetails reporting_info;
  reporting_info.metric_prefix = GetUnsafeResourceMetricPrefix(resource);
  return std::make_unique<IOSBlockingPageMetricsHelper>(
      resource.web_state_getter.Run(), resource.url, reporting_info);
}
// Returns the default safe browsing error display options.
BaseSafeBrowsingErrorUI::SBErrorDisplayOptions GetDefaultDisplayOptions(
    const UnsafeResource& resource) {
  web::WebState* web_state = resource.web_state_getter.Run();
  ChromeBrowserState* browser_state =
      ChromeBrowserState::FromBrowserState(web_state->GetBrowserState());
  PrefService* prefs = browser_state->GetPrefs();
  return BaseSafeBrowsingErrorUI::SBErrorDisplayOptions(
      resource.IsMainPageLoadBlocked(),
      /*is_extended_reporting_opt_in_allowed=*/false,
      /*is_off_the_record=*/false,
      /*is_extended_reporting=*/false,
      /*is_sber_policy_managed=*/false,
      /*is_enhanced_protection_enabled=*/false,
      prefs->GetBoolean(prefs::kSafeBrowsingProceedAnywayDisabled),
      /*should_open_links_in_new_tab=*/false,
      /*always_show_back_to_safety=*/true,
      /*is_enhanced_protection_message_enabled=*/false,
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
    : IOSSecurityInterstitialPage(resource.web_state_getter.Run(),
                                  GetMainFrameUrl(resource),
                                  client),
      is_main_page_load_blocked_(resource.IsMainPageLoadBlocked()),
      error_ui_(std::make_unique<SafeBrowsingLoudErrorUI>(
          resource.url,
          GetMainFrameUrl(resource),
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
  base::DictionaryValue load_time_data;
  PopulateInterstitialStrings(&load_time_data);
  webui::SetLoadTimeDataDefaults(client_->GetApplicationLocale(),
                                 &load_time_data);
  std::string html =
      ui::ResourceBundle::GetSharedInstance().LoadDataResourceString(
          error_ui_->GetHTMLTemplateId());
  webui::AppendWebUiCssTextDefaults(&html);
  return webui::GetI18nTemplateHtml(html, &load_time_data);
}

void SafeBrowsingBlockingPage::HandleScriptCommand(
    const base::DictionaryValue& message,
    const GURL& origin_url,
    bool user_is_interacting,
    web::WebFrame* sender_frame) {
  // Fetch the command string from the message.
  std::string command_string;
  if (!message.GetString("command", &command_string)) {
    LOG(ERROR) << "JS message parameter not found: command";
    return;
  }

  // Remove the command prefix so that the string value can be converted to a
  // SecurityInterstitialCommand enum value.
  std::size_t delimiter = command_string.find(".");
  if (delimiter == std::string::npos)
    return;

  // Parse the command int value from the text after the delimiter.
  int command = 0;
  if (!base::StringToInt(command_string.substr(delimiter + 1), &command)) {
    NOTREACHED() << "Command cannot be parsed to an int : " << command_string;
    return;
  }

  error_ui_->HandleCommand(static_cast<SecurityInterstitialCommand>(command));
  if (command == security_interstitials::CMD_DONT_PROCEED) {
    // |error_ui_| handles recording PROCEED decisions.
    client_->metrics_helper()->RecordUserDecision(
        security_interstitials::MetricsHelper::DONT_PROCEED);
  }
}

bool SafeBrowsingBlockingPage::ShouldCreateNewNavigation() const {
  return is_main_page_load_blocked_;
}

void SafeBrowsingBlockingPage::PopulateInterstitialStrings(
    base::DictionaryValue* load_time_data) const {
  load_time_data->SetString("url_to_reload", request_url().spec());
  error_ui_->PopulateStringsForHtml(load_time_data);
}

#pragma mark - SafeBrowsingBlockingPage::SafeBrowsingControllerClient

SafeBrowsingBlockingPage::SafeBrowsingControllerClient::
    SafeBrowsingControllerClient(const UnsafeResource& resource)
    : IOSBlockingPageControllerClient(
          resource.web_state_getter.Run(),
          CreateMetricsHelper(resource),
          GetApplicationContext()->GetApplicationLocale()),
      url_(SafeBrowsingUrlAllowList::GetDecisionUrl(resource)),
      threat_type_(resource.threat_type) {}

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
