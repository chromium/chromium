// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/components/security_interstitials/ios_blocking_page_metrics_helper.h"

#import "ios/web/public/web_state.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

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
