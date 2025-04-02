// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/default_browser/model/default_status/default_status_helper_metrics.h"

#import "base/metrics/histogram_functions.h"
#import "base/strings/stringprintf.h"
#import "ios/chrome/browser/default_browser/model/default_status/default_status_helper_constants.h"
#import "ios/chrome/browser/default_browser/model/default_status/default_status_helper_types.h"
#import "ios/chrome/browser/default_browser/model/utils.h"

namespace default_status {

namespace internal {

DefaultStatusHeuristicAssessment AssessHeuristic(bool system_is_default,
                                                 bool heuristic_is_default) {
  if (heuristic_is_default) {
    if (system_is_default) {
      return DefaultStatusHeuristicAssessment::kTruePositive;
    } else {
      return DefaultStatusHeuristicAssessment::kFalsePositive;
    }
  } else {
    if (system_is_default) {
      return DefaultStatusHeuristicAssessment::kFalseNegative;
    } else {
      return DefaultStatusHeuristicAssessment::kTrueNegative;
    }
  }
}

}  // namespace internal

void RecordCooldownErrorDaysLeft(int days_left) {
  base::UmaHistogramCounts1000("IOS.DefaultStatusAPI.CooldownError.DaysLeft",
                               days_left);
}

void RecordDaysSinceLastSuccessfulCall(int days) {
  base::UmaHistogramCounts1000(
      "IOS.DefaultStatusAPI.DaysSinceLastSuccessfulCall", days);
}

void RecordDefaultStatusAPIOutcomeType(
    DefaultStatusAPIOutcomeType outcome_type) {
  base::UmaHistogramEnumeration("IOS.DefaultStatusAPI.OutcomeType",
                                outcome_type);
}

void RecordDefaultStatusAPIResult(bool is_default,
                                  int cohort_number,
                                  bool is_within_cohort_window) {
  DUMP_WILL_BE_CHECK(cohort_number >= 1 && cohort_number <= kCohortCount);

  base::UmaHistogramBoolean("IOS.DefaultStatusAPI.IsDefaultBrowser",
                            is_default);

  base::UmaHistogramBoolean(
      base::StringPrintf("IOS.DefaultStatusAPI.IsDefaultBrowser.Cohort%d",
                         cohort_number),
      is_default);
  if (is_within_cohort_window) {
    base::UmaHistogramBoolean(
        "IOS.DefaultStatusAPI.IsDefaultBrowser.StrictCohorts", is_default);
  }
}

void RecordDefaultStatusRetention(DefaultStatusRetention retention) {
  base::UmaHistogramEnumeration("IOS.DefaultStatusAPI.DefaultStatusRetention",
                                retention);
}

void RecordHeuristicAssessments(bool is_default_in_system_api) {
  DefaultStatusHeuristicAssessment assessment = internal::AssessHeuristic(
      is_default_in_system_api, IsChromeLikelyDefaultBrowserXDays(1));
  base::UmaHistogramEnumeration("IOS.DefaultStatusAPI.HeuristicAssessment1",
                                assessment);
  assessment = internal::AssessHeuristic(is_default_in_system_api,
                                         IsChromeLikelyDefaultBrowserXDays(3));
  base::UmaHistogramEnumeration("IOS.DefaultStatusAPI.HeuristicAssessment3",
                                assessment);
  assessment = internal::AssessHeuristic(is_default_in_system_api,
                                         IsChromeLikelyDefaultBrowserXDays(7));
  base::UmaHistogramEnumeration("IOS.DefaultStatusAPI.HeuristicAssessment7",
                                assessment);
  assessment = internal::AssessHeuristic(is_default_in_system_api,
                                         IsChromeLikelyDefaultBrowserXDays(14));
  base::UmaHistogramEnumeration("IOS.DefaultStatusAPI.HeuristicAssessment14",
                                assessment);
  assessment = internal::AssessHeuristic(is_default_in_system_api,
                                         IsChromeLikelyDefaultBrowserXDays(21));
  base::UmaHistogramEnumeration("IOS.DefaultStatusAPI.HeuristicAssessment21",
                                assessment);
  assessment = internal::AssessHeuristic(is_default_in_system_api,
                                         IsChromeLikelyDefaultBrowserXDays(28));
  base::UmaHistogramEnumeration("IOS.DefaultStatusAPI.HeuristicAssessment28",
                                assessment);
  assessment = internal::AssessHeuristic(is_default_in_system_api,
                                         IsChromeLikelyDefaultBrowserXDays(35));
  base::UmaHistogramEnumeration("IOS.DefaultStatusAPI.HeuristicAssessment35",
                                assessment);
  assessment = internal::AssessHeuristic(is_default_in_system_api,
                                         IsChromeLikelyDefaultBrowserXDays(42));
  base::UmaHistogramEnumeration("IOS.DefaultStatusAPI.HeuristicAssessment42",
                                assessment);
}

}  // namespace default_status
