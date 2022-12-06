// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/metrics/ios_chrome_default_browser_metrics_provider.h"

#import "base/metrics/histogram_functions.h"
#import "base/metrics/histogram_macros.h"
#import "components/metrics/metrics_features.h"
#import "components/metrics/metrics_log_uploader.h"
#import "components/ukm/ios/ukm_url_recorder.h"
#import "ios/chrome/browser/ui/default_promo/default_browser_utils.h"
#import "services/metrics/public/cpp/ukm_builders.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

void ProvideUmaHistograms() {
  base::UmaHistogramBoolean("IOS.IsDefaultBrowser",
                            IsChromeLikelyDefaultBrowser7Days());
  base::UmaHistogramBoolean("IOS.IsDefaultBrowser21",
                            IsChromeLikelyDefaultBrowser());
  base::UmaHistogramBoolean(
      "IOS.IsEligibleDefaultBrowserPromoUser",
      IsLikelyInterestedDefaultBrowserUser(DefaultPromoTypeGeneral));
}

}  // namespace

IOSChromeDefaultBrowserMetricsProvider::IOSChromeDefaultBrowserMetricsProvider(
    metrics::MetricsLogUploader::MetricServiceType metrics_service_type)
    : metrics_service_type_(metrics_service_type) {}

IOSChromeDefaultBrowserMetricsProvider::
    ~IOSChromeDefaultBrowserMetricsProvider() {}

void IOSChromeDefaultBrowserMetricsProvider::OnDidCreateMetricsLog() {
  if (!base::FeatureList::IsEnabled(
          metrics::features::kEmitHistogramsEarlier)) {
    return;
  }

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
      if (!base::FeatureList::IsEnabled(
              metrics::features::kEmitHistogramsEarlier) ||
          !emitted_) {
        ProvideUmaHistograms();
      }
      return;
    case metrics::MetricsLogUploader::MetricServiceType::UKM:
      ukm::builders::IOS_IsDefaultBrowser(ukm::NoURLSourceId())
          .SetIsDefaultBrowser(IsChromeLikelyDefaultBrowser())
          .Record(ukm::UkmRecorder::Get());
      return;
  }
  NOTREACHED();
}
