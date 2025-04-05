// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DEFAULT_BROWSER_MODEL_DEFAULT_STATUS_DEFAULT_STATUS_HELPER_METRICS_H_
#define IOS_CHROME_BROWSER_DEFAULT_BROWSER_MODEL_DEFAULT_STATUS_DEFAULT_STATUS_HELPER_METRICS_H_

namespace default_status {

enum class DefaultStatusAPIOutcomeType;
enum class DefaultStatusHeuristicAssessment;
enum class DefaultStatusRetention;

// Do not call these functions; they are exposed for unit testing only.
namespace internal {

// Helper function to compare the heuristic and the system results. Exposed for
// unit testing.
DefaultStatusHeuristicAssessment AssessHeuristic(bool system_is_default,
                                                 bool heuristic_is_default);
}  // namespace internal

// Records the number of days left in the system call cooldown period.
void RecordCooldownErrorDaysLeft(int days_left);

// Records how many days have passed since the last successful default status
// API call.
void RecordDaysSinceLastSuccessfulCall(int days);

// Helper to record the type of outcome (error or success) of the default status
// API call.
void RecordDefaultStatusAPIOutcomeType(
    DefaultStatusAPIOutcomeType outcome_type);

// Helper to record the result of a successful default status API call to
// various histograms based on the client's cohort.
void RecordDefaultStatusAPIResult(bool is_default,
                                  int cohort_number,
                                  bool is_within_cohort_window);

// Records the default status retention since the last successful default status
// API call.
void RecordDefaultStatusRetention(DefaultStatusRetention retention);

// Records the difference between the provided default status according to the
// system API, and Chrome's heuristics.
void RecordHeuristicAssessments(bool is_default_in_system_api);

}  // namespace default_status

#endif  // IOS_CHROME_BROWSER_DEFAULT_BROWSER_MODEL_DEFAULT_STATUS_DEFAULT_STATUS_HELPER_METRICS_H_
