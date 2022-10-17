// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/preloading_test_util.h"

#include "base/strings/stringprintf.h"
#include "content/browser/preloading/preloading_attempt_impl.h"
#include "services/metrics/public/cpp/ukm_builders.h"

namespace content::test {

using UkmEntry = ukm::TestUkmRecorder::HumanReadableUkmEntry;
using Preloading_Attempt = ukm::builders::Preloading_Attempt;
using Preloading_Prediction = ukm::builders::Preloading_Prediction;

const std::vector<std::string> kPreloadingAttemptUkmMetrics{
    Preloading_Attempt::kPreloadingTypeName,
    Preloading_Attempt::kPreloadingPredictorName,
    Preloading_Attempt::kEligibilityName,
    Preloading_Attempt::kHoldbackStatusName,
    Preloading_Attempt::kTriggeringOutcomeName,
    Preloading_Attempt::kFailureReasonName,
    Preloading_Attempt::kAccurateTriggeringName,
};

const std::vector<std::string> kPreloadingPredictionUkmMetrics{
    Preloading_Prediction::kPreloadingPredictorName,
    Preloading_Prediction::kConfidenceName,
    Preloading_Prediction::kAccuratePredictionName,
};

PreloadingAttemptUkmEntryBuilder::PreloadingAttemptUkmEntryBuilder(
    PreloadingPredictor predictor)
    : predictor_(predictor) {}

UkmEntry PreloadingAttemptUkmEntryBuilder::BuildEntry(
    ukm::SourceId source_id,
    PreloadingType preloading_type,
    PreloadingEligibility eligibility,
    PreloadingHoldbackStatus holdback_status,
    PreloadingTriggeringOutcome triggering_outcome,
    PreloadingFailureReason failure_reason,
    bool accurate) const {
  return UkmEntry{
      source_id,
      {
          {Preloading_Attempt::kPreloadingTypeName,
           static_cast<int64_t>(preloading_type)},
          {Preloading_Attempt::kPreloadingPredictorName,
           static_cast<int64_t>(predictor_)},
          {Preloading_Attempt::kEligibilityName,
           static_cast<int64_t>(eligibility)},
          {Preloading_Attempt::kHoldbackStatusName,
           static_cast<int64_t>(holdback_status)},
          {Preloading_Attempt::kTriggeringOutcomeName,
           static_cast<int64_t>(triggering_outcome)},
          {Preloading_Attempt::kFailureReasonName,
           static_cast<int64_t>(failure_reason)},
          {Preloading_Attempt::kAccurateTriggeringName, accurate ? 1 : 0},
      }};
}

PreloadingPredictionUkmEntryBuilder::PreloadingPredictionUkmEntryBuilder(
    PreloadingPredictor predictor)
    : predictor_(predictor) {}

UkmEntry PreloadingPredictionUkmEntryBuilder::BuildEntry(
    ukm::SourceId source_id,
    int64_t confidence,
    bool accurate_prediction) const {
  return UkmEntry{source_id,
                  {
                      {Preloading_Prediction::kPreloadingPredictorName,
                       static_cast<int64_t>(predictor_)},
                      {Preloading_Prediction::kConfidenceName, confidence},
                      {Preloading_Prediction::kAccuratePredictionName,
                       accurate_prediction ? 1 : 0},
                  }};
}

std::string UkmEntryToString(const UkmEntry& entry) {
  std::string result;
  result +=
      base::StringPrintf("Source ID: %d\n", static_cast<int>(entry.source_id));
  for (const auto& metric : entry.metrics) {
    result += base::StringPrintf("Metric '%s' = %d\n", metric.first.c_str(),
                                 static_cast<int>(metric.second));
  }
  result += "\n";
  return result;
}

std::string ActualVsExpectedUkmEntryToString(const UkmEntry& actual,
                                             const UkmEntry& expected) {
  std::string result = "Actual UKM entry:\n";
  result += UkmEntryToString(actual);
  result += "Expected UKM entry:\n";
  result += UkmEntryToString(expected);
  return result;
}

std::string ActualVsExpectedUkmEntriesToString(
    const std::vector<UkmEntry>& actual,
    const std::vector<UkmEntry>& expected) {
  std::string result = "Actual UKM entries:\n";
  for (auto entry : actual) {
    result += UkmEntryToString(entry);
  }
  result += "Expected UKM entries:\n";
  for (auto entry : expected) {
    result += UkmEntryToString(entry);
  }
  return result;
}

PreloadingAttemptAccessor::PreloadingAttemptAccessor(
    PreloadingAttempt* preloading_attempt)
    : preloading_attempt_(preloading_attempt) {}

PreloadingTriggeringOutcome PreloadingAttemptAccessor::GetTriggeringOutcome() {
  return static_cast<PreloadingAttemptImpl*>(preloading_attempt_)
      ->triggering_outcome_;
}

PreloadingFailureReason PreloadingAttemptAccessor::GetFailureReason() {
  return static_cast<PreloadingAttemptImpl*>(preloading_attempt_)
      ->failure_reason_;
}

}  // namespace content::test
