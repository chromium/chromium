// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/metrics/model/ios_chrome_default_browser_metrics_provider.h"

#import "base/check.h"
#import "base/metrics/histogram_functions.h"
#import "base/metrics/histogram_macros.h"
#import "base/not_fatal_until.h"
#import "components/metrics/metrics_log_uploader.h"
#import "components/ukm/ios/ukm_url_recorder.h"
#import "ios/chrome/browser/default_browser/model/utils.h"
#import "services/metrics/public/cpp/ukm_builders.h"

namespace {

void ProvideUmaHistograms() {
  base::UmaHistogramBoolean("IOS.IsDefaultBrowser",
                            IsChromeLikelyDefaultBrowser7Days());
  base::UmaHistogramBoolean("IOS.IsDefaultBrowser21",
                            IsChromeLikelyDefaultBrowser());
  base::UmaHistogramBoolean(
      "IOS.IsEligibleDefaultBrowserPromoUser",
      IsLikelyInterestedDefaultBrowserUser(DefaultPromoTypeGeneral));

  base::UmaHistogramBoolean("IOS.IsDefaultBrowser1",
                            IsChromeLikelyDefaultBrowserXDays(1));
  base::UmaHistogramBoolean("IOS.IsDefaultBrowser3",
                            IsChromeLikelyDefaultBrowserXDays(3));
  base::UmaHistogramBoolean("IOS.IsDefaultBrowser14",
                            IsChromeLikelyDefaultBrowserXDays(14));
  base::UmaHistogramBoolean("IOS.IsDefaultBrowser28",
                            IsChromeLikelyDefaultBrowserXDays(28));
  base::UmaHistogramBoolean("IOS.IsDefaultBrowser35",
                            IsChromeLikelyDefaultBrowserXDays(35));
  base::UmaHistogramBoolean("IOS.IsDefaultBrowser42",
                            IsChromeLikelyDefaultBrowserXDays(42));

  base::UmaHistogramBoolean("IOS.DefaultBrowserAbandonment21To7",
                            IsChromePotentiallyNoLongerDefaultBrowser(21, 7));
  base::UmaHistogramBoolean("IOS.DefaultBrowserAbandonment28To14",
                            IsChromePotentiallyNoLongerDefaultBrowser(28, 14));
  base::UmaHistogramBoolean("IOS.DefaultBrowserAbandonment35To14",
                            IsChromePotentiallyNoLongerDefaultBrowser(35, 14));
  base::UmaHistogramBoolean("IOS.DefaultBrowserAbandonment42To21",
                            IsChromePotentiallyNoLongerDefaultBrowser(42, 21));
}

}  // namespace

IOSChromeDefaultBrowserMetricsProvider::IOSChromeDefaultBrowserMetricsProvider(
    metrics::MetricsLogUploader::MetricServiceType metrics_service_type)
    : metrics_service_type_(metrics_service_type) {}

IOSChromeDefaultBrowserMetricsProvider::
    ~IOSChromeDefaultBrowserMetricsProvider() {}

void IOSChromeDefaultBrowserMetricsProvider::OnDidCreateMetricsLog() {
  if (metrics_service_type_ ==
      metrics::MetricsLogUploader::MetricServiceType::UMA) {
    ProvideUmaHistograms();
  }

  emitted_ = true;
}

void IOSChromeDefaultBrowserMetricsProvider::ProvideCurrentSessionData(
    metrics::ChromeUserMetricsExtension* uma_proto) {
  switch (metrics_service_type_) {
    case metrics::MetricsLogUploader::MetricServiceType::UMA:
      if (!emitted_) {
        ProvideUmaHistograms();
      }
      return;
    case metrics::MetricsLogUploader::MetricServiceType::UKM:
      ukm::builders::IOS_IsDefaultBrowser(ukm::NoURLSourceId())
          .SetIsDefaultBrowser(IsChromeLikelyDefaultBrowser())
          .Record(ukm::UkmRecorder::Get());
      return;
    case metrics::MetricsLogUploader::MetricServiceType::STRUCTURED_METRICS:
      // `this` should never be instantiated with this service type.
      CHECK(false);
      return;
    case metrics::MetricsLogUploader::MetricServiceType::DWA:
      // `this` should never be instantiated with this service type.
      CHECK(false, base::NotFatalUntil::M134);
      return;
  }
  NOTREACHED_IN_MIGRATION();
}
