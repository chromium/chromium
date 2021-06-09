// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/metrics/ios_chrome_default_browser_metrics_provider.h"

#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "components/metrics/metrics_log_uploader.h"
#include "components/ukm/ios/ukm_url_recorder.h"
#import "ios/chrome/browser/ui/default_promo/default_browser_utils.h"
#include "services/metrics/public/cpp/ukm_builders.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

IOSChromeDefaultBrowserMetricsProvider::IOSChromeDefaultBrowserMetricsProvider(
    metrics::MetricsLogUploader::MetricServiceType metrics_service_type)
    : metrics_service_type_(metrics_service_type) {}

IOSChromeDefaultBrowserMetricsProvider::
    ~IOSChromeDefaultBrowserMetricsProvider() {}

void IOSChromeDefaultBrowserMetricsProvider::ProvideCurrentSessionData(
    metrics::ChromeUserMetricsExtension* uma_proto) {
  bool is_default = IsChromeLikelyDefaultBrowser();

  switch (metrics_service_type_) {
    case metrics::MetricsLogUploader::MetricServiceType::UMA:
      base::UmaHistogramBoolean("IOS.IsDefaultBrowser",
                                IsChromeLikelyDefaultBrowser7Days());
      base::UmaHistogramBoolean("IOS.IsDefaultBrowser21", is_default);
      base::UmaHistogramBoolean(
          "IOS.IsEligibleDefaultBrowserPromoUser",
          IsLikelyInterestedDefaultBrowserUser(DefaultPromoTypeGeneral));
      return;
    case metrics::MetricsLogUploader::MetricServiceType::UKM:
      ukm::builders::IOS_IsDefaultBrowser(ukm::NoURLSourceId())
          .SetIsDefaultBrowser(is_default)
          .Record(ukm::UkmRecorder::Get());
      return;
  }
  NOTREACHED();
}
