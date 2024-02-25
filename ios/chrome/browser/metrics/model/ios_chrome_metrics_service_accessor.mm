// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/metrics/model/ios_chrome_metrics_service_accessor.h"

#import <stdint.h>

#import "components/prefs/pref_service.h"
#import "components/variations/synthetic_trials.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"

namespace {

const bool* g_metrics_consent_for_testing = nullptr;

}  // namespace

// static
void IOSChromeMetricsServiceAccessor::SetMetricsAndCrashReportingForTesting(
    const bool* value) {
  DCHECK_NE(g_metrics_consent_for_testing == nullptr, value == nullptr)
      << "Unpaired set/reset";

  g_metrics_consent_for_testing = value;
}

// static
bool IOSChromeMetricsServiceAccessor::IsMetricsAndCrashReportingEnabled() {
  if (g_metrics_consent_for_testing)
    return *g_metrics_consent_for_testing;

  return IsMetricsReportingEnabled(GetApplicationContext()->GetLocalState());
}

// static
bool IOSChromeMetricsServiceAccessor::RegisterSyntheticFieldTrial(
    std::string_view trial_name,
    std::string_view group_name,
    variations::SyntheticTrialAnnotationMode annotation_mode) {
  return metrics::MetricsServiceAccessor::RegisterSyntheticFieldTrial(
      GetApplicationContext()->GetMetricsService(), trial_name, group_name,
      annotation_mode);
}
