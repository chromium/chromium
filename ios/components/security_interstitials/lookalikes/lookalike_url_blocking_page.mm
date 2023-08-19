// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/components/security_interstitials/lookalikes/lookalike_url_blocking_page.h"

#import <utility>

#import "base/strings/string_number_conversions.h"
#import "base/values.h"
#import "components/lookalikes/core/lookalike_url_ui_util.h"
#import "components/lookalikes/core/lookalike_url_util.h"
#import "components/security_interstitials/core/common_string_util.h"
#import "components/security_interstitials/core/metrics_helper.h"
#import "ios/components/security_interstitials/ios_blocking_page_controller_client.h"
#import "ios/components/security_interstitials/ios_blocking_page_metrics_helper.h"

LookalikeUrlBlockingPage::LookalikeUrlBlockingPage(
    web::WebState* web_state,
    const GURL& safe_url,
    const GURL& request_url,
    ukm::SourceId source_id,
    lookalikes::LookalikeUrlMatchType match_type,
    std::unique_ptr<LookalikeUrlControllerClient> client)
    : security_interstitials::IOSSecurityInterstitialPage(web_state,
                                                          request_url,
                                                          client.get()),
      web_state_(web_state),
      controller_(std::move(client)),
      safe_url_(safe_url),
      source_id_(source_id),
      match_type_(match_type) {
  DCHECK(web_state_);
  controller_->metrics_helper()->RecordUserDecision(
      security_interstitials::MetricsHelper::SHOW);
  controller_->metrics_helper()->RecordUserInteraction(
      security_interstitials::MetricsHelper::TOTAL_VISITS);

  // Creating an interstitial without showing it (e.g. from
  // chrome://interstitials) leaks memory, so don't create it here.
}

LookalikeUrlBlockingPage::~LookalikeUrlBlockingPage() {
  // Update metrics when the interstitial is closed or user navigates away.
  ReportUkmForLookalikeUrlBlockingPageIfNeeded(
      source_id_, match_type_,
      lookalikes::LookalikeUrlBlockingPageUserAction::kCloseOrBack,
      /*triggered_by_initial_url=*/false);
}

bool LookalikeUrlBlockingPage::ShouldCreateNewNavigation() const {
  return true;
}

void LookalikeUrlBlockingPage::PopulateInterstitialStrings(
    base::Value::Dict& load_time_data) const {
  // Set a value if backwards navigation is not available, used
  // to change the button text to 'Close page' when there is no
  // suggested URL.
  if (!controller_->CanGoBack()) {
    load_time_data.Set("cant_go_back", true);
  }

  lookalikes::PopulateLookalikeUrlBlockingPageStrings(load_time_data, safe_url_,
                                                      request_url());
}

bool LookalikeUrlBlockingPage::ShouldDisplayURL() const {
  return false;
}

void LookalikeUrlBlockingPage::HandleCommand(
    security_interstitials::SecurityInterstitialCommand command) {
  if (command == security_interstitials::CMD_DONT_PROCEED) {
    controller_->metrics_helper()->RecordUserDecision(
        security_interstitials::MetricsHelper::DONT_PROCEED);
    ReportUkmForLookalikeUrlBlockingPageIfNeeded(
        source_id_, match_type_,
        lookalikes::LookalikeUrlBlockingPageUserAction::kAcceptSuggestion,
        /*triggered_by_initial_url=*/false);
    controller_->GoBack();
  } else if (command == security_interstitials::CMD_PROCEED) {
    controller_->metrics_helper()->RecordUserDecision(
        security_interstitials::MetricsHelper::PROCEED);
    ReportUkmForLookalikeUrlBlockingPageIfNeeded(
        source_id_, match_type_,
        lookalikes::LookalikeUrlBlockingPageUserAction::kClickThrough,
        /*triggered_by_initial_url=*/false);
    controller_->Proceed();
  }
}
