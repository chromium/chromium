// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DEFAULT_BROWSER_MODEL_DEFAULT_STATUS_DEFAULT_STATUS_HELPER_TYPES_H_
#define IOS_CHROME_BROWSER_DEFAULT_BROWSER_MODEL_DEFAULT_STATUS_DEFAULT_STATUS_HELPER_TYPES_H_

namespace default_status {

// The possible outcomes of querying the system default status API. This enum
// must match the UMA histogram enum IOSDefaultStatusAPIOutcomeType.
//
// LINT.IfChange(DefaultStatusAPIOutcomeType)
enum class DefaultStatusAPIOutcomeType {
  kSuccess = 1,
  kCooldownError = 2,
  kOtherError = 3,
  kMaxValue = kOtherError
};
// LINT.ThenChange(/tools/metrics/histograms/metadata/ios/enums.xml:IOSDefaultStatusAPIOutcomeType)

// The true / false result of querying the system default status API, with an
// added "unknown" value to distinguish cases where no successful call has been
// made yet.
enum class DefaultStatusAPIResult {
  kUnknown = 0,
  kIsDefault = 1,
  kIsNotDefault = 2,
  kMaxValue = kIsNotDefault
};

// The assessment of the value returned by the IsChromeLikelyDefaultBrowserXDays
// heuristic based on the actual result from the default status system API. This
// enum must match the UMA histogram enum IOSDefaultStatusHeuristicAssessment.
//
// LINT.IfChange(DefaultStatusHeuristicAssessment)
enum class DefaultStatusHeuristicAssessment {
  kTrueNegative = 0,
  kFalseNegative = 1,
  kTruePositive = 2,
  kFalsePositive = 3,
  kMaxValue = kFalsePositive
};
// LINT.ThenChange(/tools/metrics/histograms/metadata/ios/enums.xml:IOSDefaultStatusHeuristicAssessment)

// The possible changes in default status year over year. This enum must match
// the UMA histogram enum IOSDefaultStatusRetention.
//
// LINT.IfChange(DefaultStatusRetention)
enum class DefaultStatusRetention {
  kBecameDefault = 0,
  kBecameNonDefault = 1,
  kRemainedDefault = 2,
  kRemainedNonDefault = 3,
  kMaxValue = kRemainedNonDefault
};
// LINT.ThenChange(/tools/metrics/histograms/metadata/ios/enums.xml:IOSDefaultStatusRetention)

}  // namespace default_status

#endif  // IOS_CHROME_BROWSER_DEFAULT_BROWSER_MODEL_DEFAULT_STATUS_DEFAULT_STATUS_HELPER_TYPES_H_
