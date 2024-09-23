// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_METRICS_MODEL_IOS_CHROME_METRICS_SERVICE_ACCESSOR_H_
#define IOS_CHROME_BROWSER_METRICS_MODEL_IOS_CHROME_METRICS_SERVICE_ACCESSOR_H_

#include <stdint.h>
#include <string>

#include "base/gtest_prod_util.h"
#include "components/metrics/metrics_service_accessor.h"
#include "components/variations/synthetic_trials.h"

class ApplicationBreadcrumbsLogger;
class DiscoverFeedExperimentsTracker;
class IOSChromeSyncClient;
class IOSChromeVariationsServiceClient;
class OptimizationGuideService;

namespace {
class CrashesDOMHandler;
}

namespace segmentation_platform {
class IOSFieldTrialRegisterImpl;
}

// This class limits and documents access to metrics service helper methods.
// Since these methods are private, each user has to be explicitly declared
// as a 'friend' below.
class IOSChromeMetricsServiceAccessor : public metrics::MetricsServiceAccessor {
 public:
  IOSChromeMetricsServiceAccessor() = delete;
  IOSChromeMetricsServiceAccessor(const IOSChromeMetricsServiceAccessor&) =
      delete;
  IOSChromeMetricsServiceAccessor& operator=(
      const IOSChromeMetricsServiceAccessor&) = delete;

  // If arg is non-null, the value will be returned from future calls to
  // IsMetricsAndCrashReportingEnabled(). Pointer must be valid until it is
  // reset to null here.
  static void SetMetricsAndCrashReportingForTesting(const bool* value);

 private:
  friend class IOSChromeMetricsServicesManagerClient;
  friend class IOSChromeVariationsServiceClient;

  friend class ApplicationBreadcrumbsLogger;
  friend class CrashesDOMHandler;
  friend class DiscoverFeedExperimentsTracker;
  friend class IOSChromeMainParts;
  friend class IOSChromeSyncClient;
  friend class OptimizationGuideService;
  friend class segmentation_platform::IOSFieldTrialRegisterImpl;

  FRIEND_TEST_ALL_PREFIXES(IOSChromeMetricsServiceAccessorTest,
                           MetricsReportingEnabled);

  // Returns true if metrics reporting is enabled.
  static bool IsMetricsAndCrashReportingEnabled();

  // Calls metrics::MetricsServiceAccessor::RegisterSyntheticFieldTrial() with
  // ApplicationContext's MetricsService. See that function's declaration for
  // details.
  static bool RegisterSyntheticFieldTrial(
      std::string_view trial_name,
      std::string_view group_name,
      variations::SyntheticTrialAnnotationMode annotation_mode =
          variations::SyntheticTrialAnnotationMode::kNextLog);
};

#endif  // IOS_CHROME_BROWSER_METRICS_MODEL_IOS_CHROME_METRICS_SERVICE_ACCESSOR_H_
