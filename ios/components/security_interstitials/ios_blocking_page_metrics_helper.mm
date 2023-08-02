// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/components/security_interstitials/ios_blocking_page_metrics_helper.h"

#import "ios/web/public/web_state.h"

namespace security_interstitials {

IOSBlockingPageMetricsHelper::IOSBlockingPageMetricsHelper(
    web::WebState* web_state,
    const GURL& request_url,
    const security_interstitials::MetricsHelper::ReportDetails report_details)
    : security_interstitials::MetricsHelper(request_url,
                                            report_details,
                                            nullptr) {}

IOSBlockingPageMetricsHelper::~IOSBlockingPageMetricsHelper() {}

void IOSBlockingPageMetricsHelper::RecordExtraUserDecisionMetrics(
    security_interstitials::MetricsHelper::Decision decision) {}

void IOSBlockingPageMetricsHelper::RecordExtraUserInteractionMetrics(
    security_interstitials::MetricsHelper::Interaction interaction) {}

void IOSBlockingPageMetricsHelper::RecordExtraShutdownMetrics() {}

}  // namespace security_interstitials
