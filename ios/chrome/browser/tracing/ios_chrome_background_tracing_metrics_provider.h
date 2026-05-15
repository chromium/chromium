// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TRACING_IOS_CHROME_BACKGROUND_TRACING_METRICS_PROVIDER_H_
#define IOS_CHROME_BROWSER_TRACING_IOS_CHROME_BACKGROUND_TRACING_METRICS_PROVIDER_H_

#include "base/functional/callback.h"
#include "components/tracing/common/background_tracing_metrics_provider.h"

namespace metrics {
class SystemProfileProto;
}  // namespace metrics

namespace tracing {

// iOS-specific independent MetricsProvider for background tracing reports.
class IOSChromeBackgroundTracingMetricsProvider
    : public BackgroundTracingMetricsProvider {
 public:
  IOSChromeBackgroundTracingMetricsProvider();
  ~IOSChromeBackgroundTracingMetricsProvider() override;

  IOSChromeBackgroundTracingMetricsProvider(
      const IOSChromeBackgroundTracingMetricsProvider&) = delete;
  IOSChromeBackgroundTracingMetricsProvider& operator=(
      const IOSChromeBackgroundTracingMetricsProvider&) = delete;

  // BackgroundTracingMetricsProvider:
  void Init() override;
  void RecordCoreSystemProfileMetrics(
      metrics::SystemProfileProto& system_profile_proto) override;
};

}  // namespace tracing

#endif  // IOS_CHROME_BROWSER_TRACING_IOS_CHROME_BACKGROUND_TRACING_METRICS_PROVIDER_H_
