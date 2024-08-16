// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/components/security_interstitials/https_only_mode/https_only_mode_blocking_page.h"

#import <utility>

#import "base/strings/string_number_conversions.h"
#import "base/values.h"
#import "components/security_interstitials/core/common_string_util.h"
#import "components/security_interstitials/core/https_only_mode_metrics.h"
#import "components/security_interstitials/core/https_only_mode_ui_util.h"
#import "components/security_interstitials/core/metrics_helper.h"
#import "ios/components/security_interstitials/https_only_mode/https_upgrade_service.h"
#import "ios/components/security_interstitials/ios_blocking_page_controller_client.h"
#import "ios/components/security_interstitials/ios_blocking_page_metrics_helper.h"

namespace {

// Must match the value of kLearnMoreLink in
// components/security_interstitials/content/https_only_mode_blocking_page.cc
const char kLearnMoreLink[] = "https://support.google.com/chrome?p=first_mode";

}  // namespace

HttpsOnlyModeBlockingPage::HttpsOnlyModeBlockingPage(
    web::WebState* web_state,
    const GURL& request_url,
    HttpsUpgradeService* service,
    std::unique_ptr<HttpsOnlyModeControllerClient> client)
    : security_interstitials::IOSSecurityInterstitialPage(web_state,
                                                          request_url,
                                                          client.get()),
      web_state_(web_state),
      service_(service),
      controller_(std::move(client)) {
  DCHECK(web_state_);
  controller_->metrics_helper()->RecordUserDecision(
      security_interstitials::MetricsHelper::SHOW);
  controller_->metrics_helper()->RecordUserInteraction(
      security_interstitials::MetricsHelper::TOTAL_VISITS);

  // Creating an interstitial without showing it (e.g. from
  // chrome://interstitials) leaks memory, so don't create it here.
}

HttpsOnlyModeBlockingPage::~HttpsOnlyModeBlockingPage() {
  // TODO(crbug.com/40825375): Update metrics when the interstitial is closed
  // or user navigates away.
}

bool HttpsOnlyModeBlockingPage::ShouldCreateNewNavigation() const {
  return true;
}

void HttpsOnlyModeBlockingPage::PopulateInterstitialStrings(
    base::Value::Dict& load_time_data) const {
  // Set a value if backwards navigation is not available, used
  // to change the button text to 'Close page' when there is no
  // suggested URL.
  if (!controller_->CanGoBack()) {
    load_time_data.Set("cant_go_back", true);
  }

  PopulateHttpsOnlyModeStringsForSharedHTML(
      load_time_data,
      /*august2024_refresh_enabled=*/false);
  PopulateHttpsOnlyModeStringsForBlockingPage(
      load_time_data, request_url(),
      security_interstitials::https_only_mode::HttpInterstitialState{},
      /*august2024_refresh_enabled=*/false);
}

bool HttpsOnlyModeBlockingPage::ShouldDisplayURL() const {
  return true;
}

void HttpsOnlyModeBlockingPage::HandleCommand(
    security_interstitials::SecurityInterstitialCommand command) {
  if (command == security_interstitials::CMD_DONT_PROCEED) {
    controller_->metrics_helper()->RecordUserDecision(
        security_interstitials::MetricsHelper::DONT_PROCEED);
    controller_->GoBack();
  } else if (command == security_interstitials::CMD_PROCEED) {
    service_->AllowHttpForHost(request_url().host());

    controller_->metrics_helper()->RecordUserDecision(
        security_interstitials::MetricsHelper::PROCEED);
    controller_->Proceed();
  } else if (command == security_interstitials::CMD_OPEN_HELP_CENTER) {
    controller_->metrics_helper()->RecordUserInteraction(
        security_interstitials::MetricsHelper::SHOW_LEARN_MORE);
    controller_->OpenUrlInNewForegroundTab(GURL(kLearnMoreLink));
  }
}
