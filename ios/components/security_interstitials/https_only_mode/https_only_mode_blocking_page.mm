// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/components/security_interstitials/https_only_mode/https_only_mode_blocking_page.h"

#include <utility>

#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "components/security_interstitials/core/common_string_util.h"
#include "components/security_interstitials/core/https_only_mode_ui_util.h"
#include "components/security_interstitials/core/metrics_helper.h"
#include "ios/components/security_interstitials/ios_blocking_page_controller_client.h"
#include "ios/components/security_interstitials/ios_blocking_page_metrics_helper.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

HttpsOnlyModeBlockingPage::HttpsOnlyModeBlockingPage(
    web::WebState* web_state,
    const GURL& request_url,
    std::unique_ptr<HttpsOnlyModeControllerClient> client)
    : security_interstitials::IOSSecurityInterstitialPage(web_state,
                                                          request_url,
                                                          client.get()),
      web_state_(web_state),
      controller_(std::move(client)) {
  DCHECK(web_state_);
  // Creating an interstitial without showing it (e.g. from
  // chrome://interstitials) leaks memory, so don't create it here.
}

HttpsOnlyModeBlockingPage::~HttpsOnlyModeBlockingPage() {
  // TODO(crbug.com/1302509): Update metrics when the interstitial is closed
  // or user navigates away.
}

bool HttpsOnlyModeBlockingPage::ShouldCreateNewNavigation() const {
  return true;
}

void HttpsOnlyModeBlockingPage::PopulateInterstitialStrings(
    base::Value* load_time_data) const {
  CHECK(load_time_data);

  // Set a value if backwards navigation is not available, used
  // to change the button text to 'Close page' when there is no
  // suggested URL.
  if (!controller_->CanGoBack()) {
    load_time_data->SetBoolKey("cant_go_back", true);
  }

  PopulateHttpsOnlyModeStringsForBlockingPage(load_time_data, request_url());
}

bool HttpsOnlyModeBlockingPage::ShouldDisplayURL() const {
  return false;
}

void HttpsOnlyModeBlockingPage::HandleCommand(
    security_interstitials::SecurityInterstitialCommand command,
    const GURL& origin_url,
    bool user_is_interacting,
    web::WebFrame* sender_frame) {
  if (command == security_interstitials::CMD_DONT_PROCEED) {
    controller_->metrics_helper()->RecordUserDecision(
        security_interstitials::MetricsHelper::DONT_PROCEED);
    // TODO(crbug.com/1302509): Record metrics here.
    controller_->GoBack();
  } else if (command == security_interstitials::CMD_PROCEED) {
    controller_->metrics_helper()->RecordUserDecision(
        security_interstitials::MetricsHelper::PROCEED);
    // TODO(crbug.com/1302509): Record metrics here.
    controller_->Proceed();
  }
}
