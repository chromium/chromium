// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/components/security_interstitials/https_only_mode/https_only_mode_controller_client.h"

#import "components/security_interstitials/core/metrics_helper.h"
#import "ios/components/security_interstitials/ios_blocking_page_metrics_helper.h"
#import "ios/web/public/web_state.h"

namespace {
// Creates a metrics helper for `url`.
std::unique_ptr<security_interstitials::IOSBlockingPageMetricsHelper>
CreateMetricsHelper(web::WebState* web_state, const GURL& url) {
  security_interstitials::MetricsHelper::ReportDetails reporting_info;
  reporting_info.metric_prefix = "https_only_mode";
  return std::make_unique<security_interstitials::IOSBlockingPageMetricsHelper>(
      web_state, url, reporting_info);
}
}  // namespace

HttpsOnlyModeControllerClient::HttpsOnlyModeControllerClient(
    web::WebState* web_state,
    const GURL& request_url,
    const std::string& app_locale)
    : IOSBlockingPageControllerClient(
          web_state,
          CreateMetricsHelper(web_state, request_url),
          app_locale) {}

HttpsOnlyModeControllerClient::~HttpsOnlyModeControllerClient() {}

void HttpsOnlyModeControllerClient::GoBack() {
  IOSBlockingPageControllerClient::GoBack();
}

void HttpsOnlyModeControllerClient::Proceed() {
  // TODO(crbug.com/40825375): Remember the URL so that we don't block
  // again for a certain time.
  Reload();
}
