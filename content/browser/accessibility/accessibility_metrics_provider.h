// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ACCESSIBILITY_ACCESSIBILITY_METRICS_PROVIDER_H_
#define CONTENT_BROWSER_ACCESSIBILITY_ACCESSIBILITY_METRICS_PROVIDER_H_

#include "components/metrics/metrics_provider.h"
#include "content/common/content_export.h"

////////////////////////////////////////////////////////////////////////////////
//
// AccessibilityMetricsProvider
//
// A class used to provide frequent signals for AT or accessibility usage
// histograms on Win, Mac and Android, enable accurate counting of unique users.
//
////////////////////////////////////////////////////////////////////////////////
class CONTENT_EXPORT AccessibilityMetricsProvider
    : public metrics::MetricsProvider {
 public:
  AccessibilityMetricsProvider();
  ~AccessibilityMetricsProvider() override;

  // MetricsProvider:
  void ProvideCurrentSessionData(
      metrics::ChromeUserMetricsExtension* uma_proto) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(AccessibilityMetricsProvider);
};

#endif  // CONTENT_BROWSER_ACCESSIBILITY_ACCESSIBILITY_METRICS_PROVIDER_H_
