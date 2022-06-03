// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/components/security_interstitials/legacy_tls/legacy_tls_controller_client.h"

#include "components/security_interstitials/core/metrics_helper.h"
#include "ios/components/security_interstitials/ios_blocking_page_metrics_helper.h"
#include "ios/components/security_interstitials/legacy_tls/legacy_tls_tab_allow_list.h"
#import "ios/web/public/web_state.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Creates a metrics helper for |url|.
std::unique_ptr<security_interstitials::IOSBlockingPageMetricsHelper>
CreateMetricsHelper(web::WebState* web_state, const GURL& url) {
  security_interstitials::MetricsHelper::ReportDetails reporting_info;
  reporting_info.metric_prefix = "legacy_tls";
  return std::make_unique<security_interstitials::IOSBlockingPageMetricsHelper>(
      web_state, url, reporting_info);
}
}  // namespace

LegacyTLSControllerClient::LegacyTLSControllerClient(
    web::WebState* web_state,
    const GURL& request_url,
    const std::string& app_locale)
    : IOSBlockingPageControllerClient(
          web_state,
          CreateMetricsHelper(web_state, request_url),
          app_locale),
      request_url_(request_url) {}

LegacyTLSControllerClient::~LegacyTLSControllerClient() {}

void LegacyTLSControllerClient::Proceed() {
  LegacyTLSTabAllowList::FromWebState(web_state())
      ->AllowDomain(request_url_.host());
  Reload();
}
