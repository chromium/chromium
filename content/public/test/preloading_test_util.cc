// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/preloading_test_util.h"

#include "base/strings/stringprintf.h"
#include "content/browser/preloading/preloading_attempt_impl.h"
#include "content/browser/preloading/preloading_config.h"
#include "content/browser/preloading/preloading_data_impl.h"
#include "preloading_test_util.h"
#include "services/metrics/public/cpp/metrics_utils.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

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
    Preloading_Attempt::kReadyTimeName,
    Preloading_Attempt::kTimeToNextNavigationName,
    Preloading_Attempt::kSpeculationEagernessName,
};

const std::vector<std::string> kPreloadingPredictionUkmMetrics{
    Preloading_Prediction::kPreloadingPredictorName,
    Preloading_Prediction::kConfidenceName,
    Preloading_Prediction::kAccuratePredictionName,
    Preloading_Prediction::kTimeToNextNavigationName,
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
    bool accurate,
    std::optional<base::TimeDelta> ready_time,
    std::optional<blink::mojom::SpeculationEagerness> eagerness) const {
  std::map<std::string, int64_t> metrics = {
      {Preloading_Attempt::kPreloadingTypeName,
       static_cast<int64_t>(preloading_type)},
      {Preloading_Attempt::kPreloadingPredictorName, predictor_.ukm_value()},
      {Preloading_Attempt::kEligibilityName, static_cast<int64_t>(eligibility)},
      {Preloading_Attempt::kHoldbackStatusName,
       static_cast<int64_t>(holdback_status)},
      {Preloading_Attempt::kTriggeringOutcomeName,
       static_cast<int64_t>(triggering_outcome)},
      {Preloading_Attempt::kFailureReasonName,
       static_cast<int64_t>(failure_reason)},
      {Preloading_Attempt::kAccurateTriggeringName, accurate ? 1 : 0},
      {Preloading_Attempt::kTimeToNextNavigationName,
       ukm::GetExponentialBucketMinForCounts1000(
           base::ScopedMockElapsedTimersForTest::kMockElapsedTime
               .InMilliseconds())}};
  if (ready_time) {
    metrics.insert({Preloading_Attempt::kReadyTimeName,
                    ukm::GetExponentialBucketMinForCounts1000(
                        ready_time->InMilliseconds())});
  }
  if (eagerness) {
    metrics.insert({Preloading_Attempt::kSpeculationEagernessName,
                    static_cast<int64_t>(eagerness.value())});
  }
  return UkmEntry{source_id, std::move(metrics)};
}

PreloadingPredictionUkmEntryBuilder::PreloadingPredictionUkmEntryBuilder(
    PreloadingPredictor predictor)
    : predictor_(predictor) {}

UkmEntry PreloadingPredictionUkmEntryBuilder::BuildEntry(
    ukm::SourceId source_id,
    int64_t confidence,
    bool accurate_prediction) const {
  return UkmEntry{source_id,
                  {{Preloading_Prediction::kPreloadingPredictorName,
                    predictor_.ukm_value()},
                   {Preloading_Prediction::kConfidenceName, confidence},
                   {Preloading_Prediction::kAccuratePredictionName,
                    accurate_prediction ? 1 : 0},
                   {Preloading_Prediction::kTimeToNextNavigationName,
                    ukm::GetExponentialBucketMinForCounts1000(
                        base::ScopedMockElapsedTimersForTest::kMockElapsedTime
                            .InMilliseconds())}}};
}

void ExpectPreloadingAttemptUkm(
    ukm::TestAutoSetUkmRecorder& ukm_recorder,
    const std::vector<ukm::TestUkmRecorder::HumanReadableUkmEntry>&
        expected_attempt_entries) {
  auto attempt_entries = ukm_recorder.GetEntries(
      Preloading_Attempt::kEntryName, test::kPreloadingAttemptUkmMetrics);
  EXPECT_EQ(attempt_entries.size(), expected_attempt_entries.size());
  EXPECT_THAT(attempt_entries,
              testing::UnorderedElementsAreArray(expected_attempt_entries))
      << test::ActualVsExpectedUkmEntriesToString(attempt_entries,
                                                  expected_attempt_entries);
}

void ExpectPreloadingPredictionUkm(
    ukm::TestAutoSetUkmRecorder& ukm_recorder,
    const std::vector<ukm::TestUkmRecorder::HumanReadableUkmEntry>&
        expected_prediction_entries) {
  auto prediction_entries = ukm_recorder.GetEntries(
      Preloading_Prediction::kEntryName, test::kPreloadingPredictionUkmMetrics);
  EXPECT_EQ(prediction_entries.size(), expected_prediction_entries.size());
  EXPECT_THAT(prediction_entries,
              testing::UnorderedElementsAreArray(expected_prediction_entries))
      << test::ActualVsExpectedUkmEntriesToString(prediction_entries,
                                                  expected_prediction_entries);
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

PreloadingConfigOverride::PreloadingConfigOverride() {
  preloading_config_ = std::make_unique<PreloadingConfig>();
  overridden_config_ =
      PreloadingConfig::OverrideForTesting(preloading_config_.get());
}

PreloadingConfigOverride::~PreloadingConfigOverride() {
  raw_ptr<PreloadingConfig> uninstalled_override =
      PreloadingConfig::OverrideForTesting(overridden_config_);
  // Make sure the override we uninstalled is the one we installed in the
  // constructor.
  CHECK_EQ(uninstalled_override.get(), preloading_config_.get());
}

void PreloadingConfigOverride::SetHoldback(PreloadingType preloading_type,
                                           PreloadingPredictor predictor,
                                           bool holdback) {
  preloading_config_->SetHoldbackForTesting(preloading_type, predictor,
                                            holdback);
}

void PreloadingConfigOverride::SetHoldback(std::string_view preloading_type,
                                           std::string_view predictor,
                                           bool holdback) {
  preloading_config_->SetHoldbackForTesting(preloading_type, predictor,
                                            holdback);
}

void SetHasSpeculationRulesPrerender(PreloadingData* preloading_data) {
  static_cast<PreloadingDataImpl*>(preloading_data)
      ->SetHasSpeculationRulesPrerender();
}

}  // namespace content::test
