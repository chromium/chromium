// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_PRELOADING_TEST_UTIL_H_
#define CONTENT_PUBLIC_TEST_PRELOADING_TEST_UTIL_H_

#include <vector>

#include "base/memory/raw_ptr.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/browser/preloading.h"
#include "content/public/browser/preloading_data.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "third_party/blink/public/mojom/speculation_rules/speculation_rules.mojom-shared.h"

namespace content {

class PreloadingConfig;

namespace test {

// The set of UKM metric names in the PreloadingAttempt and PreloadingPrediction
// UKM logs. This is useful for calling TestUkmRecorder::GetEntries.
extern const std::vector<std::string> kPreloadingAttemptUkmMetrics;
extern const std::vector<std::string> kPreloadingPredictionUkmMetrics;

// Utility class to make building expected
// TestUkmRecorder::HumanReadableUkmEntry for EXPECT_EQ for PreloadingAttempt.
class PreloadingAttemptUkmEntryBuilder {
 public:
  explicit PreloadingAttemptUkmEntryBuilder(PreloadingPredictor predictor);

  // This method assumes a navigation has occurred thus `TimeToNextNavigation`
  // is set. Install `base::ScopedMockElapsedTimersForTest` into the test
  // fixture to assert the entry's latency values' correctness.
  //
  // Optional `ready_time` should be set by the caller, if this attempt ever
  // reaches `PreloadingTriggeringOutcome::kReady` state, at the time of
  // reporting.
  ukm::TestUkmRecorder::HumanReadableUkmEntry BuildEntry(
      ukm::SourceId source_id,
      PreloadingType preloading_type,
      PreloadingEligibility eligibility,
      PreloadingHoldbackStatus holdback_status,
      PreloadingTriggeringOutcome triggering_outcome,
      PreloadingFailureReason failure_reason,
      bool accurate,
      std::optional<base::TimeDelta> ready_time = std::nullopt,
      std::optional<blink::mojom::SpeculationEagerness> eagerness =
          std::nullopt) const;

 private:
  PreloadingPredictor predictor_;
};

// Utility class to make building expected
// TestUkmRecorder::HumanReadableUkmEntry for EXPECT_EQ for
// PreloadingPrediction.
class PreloadingPredictionUkmEntryBuilder {
 public:
  explicit PreloadingPredictionUkmEntryBuilder(PreloadingPredictor predictor);

  // This method assumes a navigation has occurred thus `TimeToNextNavigation`
  // is set. Install `base::ScopedMockElapsedTimersForTest` into the test
  // fixture to assert the entry's latency values' correctness.
  ukm::TestUkmRecorder::HumanReadableUkmEntry BuildEntry(
      ukm::SourceId source_id,
      int64_t confidence,
      bool accurate_prediction) const;

 private:
  PreloadingPredictor predictor_;
};

// Checks if `ukm_recorder` recorded `expected_attempt_entries`. Doesn't care
// about the recording order.
void ExpectPreloadingAttemptUkm(
    ukm::TestAutoSetUkmRecorder& ukm_recorder,
    const std::vector<ukm::TestUkmRecorder::HumanReadableUkmEntry>&
        expected_attempt_entries);

// Checks if `ukm_recorder` recorded `expected_prediction_entries`. Doesn't care
// about the recording order.
void ExpectPreloadingPredictionUkm(
    ukm::TestAutoSetUkmRecorder& ukm_recorder,
    const std::vector<ukm::TestUkmRecorder::HumanReadableUkmEntry>&
        expected_prediction_entries);

// Turns a UKM entry into a human-readable string.
std::string UkmEntryToString(
    const ukm::TestUkmRecorder::HumanReadableUkmEntry& entry);

// Turns two UKM entries into a human-readable string.
std::string ActualVsExpectedUkmEntryToString(
    const ukm::TestUkmRecorder::HumanReadableUkmEntry& actual,
    const ukm::TestUkmRecorder::HumanReadableUkmEntry& expected);

// Turns two collections of UKM entries into human-readable strings.
std::string ActualVsExpectedUkmEntriesToString(
    const std::vector<ukm::TestUkmRecorder::HumanReadableUkmEntry>& actual,
    const std::vector<ukm::TestUkmRecorder::HumanReadableUkmEntry>& expected);

// Utility class to access internal state from a PreloadingAttempt.
class PreloadingAttemptAccessor {
 public:
  explicit PreloadingAttemptAccessor(PreloadingAttempt* preloading_attempt);

  PreloadingTriggeringOutcome GetTriggeringOutcome();
  PreloadingFailureReason GetFailureReason();

 private:
  raw_ptr<PreloadingAttempt> preloading_attempt_;
};

// Creating a PreloadingConfigOverride will override the current
// PreloadingConfig (which is normally configured via field trial) until
// PreloadingConfigOverride is destroyed. By default the configuration disables
// sampling UKM preloading logs (some log types are sampled by default, which
// can make preloading tests that verify UKM logs flaky) but enables (i.e. does
// not hold back) all preloading features. For testing holdbacks, SetHoldback
// can be called to disable a particular preloading feature.
class PreloadingConfigOverride {
 public:
  PreloadingConfigOverride();
  ~PreloadingConfigOverride();

  void SetHoldback(PreloadingType preloading_type,
                   PreloadingPredictor predictor,
                   bool holdback);

  void SetHoldback(std::string_view preloading_type,
                   std::string_view predictor,
                   bool holdback);

 private:
  std::unique_ptr<PreloadingConfig> preloading_config_;
  raw_ptr<PreloadingConfig> overridden_config_;
};

void SetHasSpeculationRulesPrerender(PreloadingData* preloading_data);

}  // namespace test
}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_PRELOADING_TEST_UTIL_H_
