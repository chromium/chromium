// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_METRICS_MODEL_IOS_FAMILY_LINK_USER_METRICS_PROVIDER_H_
#define IOS_CHROME_BROWSER_METRICS_MODEL_IOS_FAMILY_LINK_USER_METRICS_PROVIDER_H_

#import "components/metrics/metrics_provider.h"

// Categorizes the primary account of each profile into a FamilyLink
// supervision type to segment the Chrome user population.
class IOSFamilyLinkUserMetricsProvider : public metrics::MetricsProvider {
 public:
  IOSFamilyLinkUserMetricsProvider();
  ~IOSFamilyLinkUserMetricsProvider() override;

  // metrics::MetricsProvider
  bool ProvideHistograms() override;
};

#endif  // IOS_CHROME_BROWSER_METRICS_MODEL_IOS_FAMILY_LINK_USER_METRICS_PROVIDER_H_
