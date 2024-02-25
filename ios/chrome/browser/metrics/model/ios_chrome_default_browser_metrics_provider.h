// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_METRICS_MODEL_IOS_CHROME_DEFAULT_BROWSER_METRICS_PROVIDER_H_
#define IOS_CHROME_BROWSER_METRICS_MODEL_IOS_CHROME_DEFAULT_BROWSER_METRICS_PROVIDER_H_

#include "components/metrics/metrics_log_uploader.h"
#include "components/metrics/metrics_provider.h"

// IOSChromeStabilityMetricsProvider records iOS default-browser related
// metrics.
class IOSChromeDefaultBrowserMetricsProvider : public metrics::MetricsProvider {
 public:
  explicit IOSChromeDefaultBrowserMetricsProvider(
      metrics::MetricsLogUploader::MetricServiceType metrics_service_type);

  IOSChromeDefaultBrowserMetricsProvider(
      const IOSChromeDefaultBrowserMetricsProvider&) = delete;
  IOSChromeDefaultBrowserMetricsProvider& operator=(
      const IOSChromeDefaultBrowserMetricsProvider&) = delete;

  ~IOSChromeDefaultBrowserMetricsProvider() override;

  // metrics::MetricsProvider:
  void OnDidCreateMetricsLog() override;
  void ProvideCurrentSessionData(
      metrics::ChromeUserMetricsExtension* uma_proto) override;

 private:
  // The type of the metrics service for which to emit the user demographics
  // status histogram (e.g., UMA).
  const metrics::MetricsLogUploader::MetricServiceType metrics_service_type_;
};

#endif  // IOS_CHROME_BROWSER_METRICS_MODEL_IOS_CHROME_DEFAULT_BROWSER_METRICS_PROVIDER_H_
