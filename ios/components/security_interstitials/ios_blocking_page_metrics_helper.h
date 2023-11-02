// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_COMPONENTS_SECURITY_INTERSTITIALS_IOS_BLOCKING_PAGE_METRICS_HELPER_H_
#define IOS_COMPONENTS_SECURITY_INTERSTITIALS_IOS_BLOCKING_PAGE_METRICS_HELPER_H_

#include "components/security_interstitials/core/metrics_helper.h"
#include "url/gurl.h"

namespace web {
class WebState;
}

namespace security_interstitials {

// This class provides a concrete implementation for iOS to the
// security_interstitials::MetricsHelper class. Together, they record UMA and
// experience sampling metrics.
class IOSBlockingPageMetricsHelper
    : public security_interstitials::MetricsHelper {
 public:
  IOSBlockingPageMetricsHelper(
      web::WebState* web_state,
      const GURL& request_url,
      const security_interstitials::MetricsHelper::ReportDetails
          report_details);

  IOSBlockingPageMetricsHelper(const IOSBlockingPageMetricsHelper&) = delete;
  IOSBlockingPageMetricsHelper& operator=(const IOSBlockingPageMetricsHelper&) =
      delete;

  ~IOSBlockingPageMetricsHelper() override;

 protected:
  // security_interstitials::MetricsHelper implementation.
  void RecordExtraUserDecisionMetrics(
      security_interstitials::MetricsHelper::Decision decision) override;
  void RecordExtraUserInteractionMetrics(
      security_interstitials::MetricsHelper::Interaction interaction) override;
  void RecordExtraShutdownMetrics() override;
};

}  // namespace security_interstitials

#endif  // IOS_COMPONENTS_SECURITY_INTERSTITIALS_IOS_BLOCKING_PAGE_METRICS_HELPER_H_
