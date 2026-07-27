// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SUBSCRIPTION_ELIGIBILITY_MODEL_IOS_SUBSCRIPTION_ELIGIBILITY_METRICS_PROVIDER_H_
#define IOS_CHROME_BROWSER_SUBSCRIPTION_ELIGIBILITY_MODEL_IOS_SUBSCRIPTION_ELIGIBILITY_METRICS_PROVIDER_H_

#import "components/metrics/metrics_provider.h"
#import "components/subscription_eligibility/subscription_eligibility_metrics_util.h"

namespace subscription_eligibility {

class IOSSubscriptionEligibilityMetricsProvider
    : public metrics::MetricsProvider {
 public:
  IOSSubscriptionEligibilityMetricsProvider();
  IOSSubscriptionEligibilityMetricsProvider(
      const IOSSubscriptionEligibilityMetricsProvider&) = delete;
  IOSSubscriptionEligibilityMetricsProvider& operator=(
      const IOSSubscriptionEligibilityMetricsProvider&) = delete;
  ~IOSSubscriptionEligibilityMetricsProvider() override;

  // metrics::MetricsProvider:
  void ProvideCurrentSessionData(
      metrics::ChromeUserMetricsExtension* uma_proto) override;
};

}  // namespace subscription_eligibility

#endif  // IOS_CHROME_BROWSER_SUBSCRIPTION_ELIGIBILITY_MODEL_IOS_SUBSCRIPTION_ELIGIBILITY_METRICS_PROVIDER_H_
