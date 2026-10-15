// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/backend_promo/model/backend_promo_service.h"

#import "components/variations/synthetic_trials.h"
#import "ios/chrome/browser/metrics/model/ios_chrome_metrics_service_accessor.h"

// static
bool BackendPromoService::RegisterSyntheticFieldTrial(
    std::string_view trial_name,
    std::string_view group_name) {
  return IOSChromeMetricsServiceAccessor::RegisterSyntheticFieldTrial(
      trial_name, group_name,
      variations::SyntheticTrialAnnotationMode::kCurrentLog);
}
