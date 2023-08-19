// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/components/security_interstitials/lookalikes/lookalike_url_controller_client.h"

#import "components/security_interstitials/core/metrics_helper.h"
#import "ios/components/security_interstitials/ios_blocking_page_metrics_helper.h"
#import "ios/components/security_interstitials/lookalikes/lookalike_url_tab_allow_list.h"
#import "ios/web/public/web_state.h"

namespace {
// Creates a metrics helper for `url`.
std::unique_ptr<security_interstitials::IOSBlockingPageMetricsHelper>
CreateMetricsHelper(web::WebState* web_state, const GURL& url) {
  security_interstitials::MetricsHelper::ReportDetails reporting_info;
  reporting_info.metric_prefix = "lookalike";
  return std::make_unique<security_interstitials::IOSBlockingPageMetricsHelper>(
      web_state, url, reporting_info);
}
}  // namespace

LookalikeUrlControllerClient::LookalikeUrlControllerClient(
    web::WebState* web_state,
    const GURL& safe_url,
    const GURL& request_url,
    const std::string& app_locale)
    : IOSBlockingPageControllerClient(
          web_state,
          CreateMetricsHelper(web_state, request_url),
          app_locale),
      safe_url_(safe_url),
      request_url_(request_url) {}

LookalikeUrlControllerClient::~LookalikeUrlControllerClient() {}

void LookalikeUrlControllerClient::GoBack() {
  // If the interstitial doesn't have a suggested URL (e.g. punycode
  // interstitial), either go back or close the tab. If there is a
  // suggested URL, instead of a 'go back' option, redirect to the
  // legitimate site.
  if (!safe_url_.is_valid()) {
    IOSBlockingPageControllerClient::GoBack();
  } else {
    // For simplicity and because replacement doesn't always work, the
    // navigation to the safe URL does not replace the navigation to
    // the interstitial. However, this is acceptable since if a user
    // navigates back to the lookalike, the interstitial will be shown.
    OpenUrlInCurrentTab(safe_url_);
  }
}

void LookalikeUrlControllerClient::Proceed() {
  LookalikeUrlTabAllowList::FromWebState(web_state())
      ->AllowDomain(request_url_.host());
  Reload();
}
