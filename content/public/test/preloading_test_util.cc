// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/preloading_test_util.h"

#include "base/strings/stringprintf.h"
#include "services/metrics/public/cpp/ukm_builders.h"

namespace content::test {

using UkmEntry = ukm::TestUkmRecorder::HumanReadableUkmEntry;
using Preloading_Attempt = ukm::builders::Preloading_Attempt;

const std::vector<std::string> kPreloadingAttemptUkmMetrics{
    Preloading_Attempt::kPreloadingTypeName,
    Preloading_Attempt::kPreloadingPredictorName,
    Preloading_Attempt::kEligibilityName,
    Preloading_Attempt::kHoldbackStatusName,
    Preloading_Attempt::kTriggeringOutcomeName,
    Preloading_Attempt::kAccurateTriggeringName,
};

PreloadingAttemptUkmEntryBuilder::PreloadingAttemptUkmEntryBuilder(
    PreloadingType preloading_type,
    PreloadingPredictor predictor)
    : preloading_type_(preloading_type), predictor_(predictor) {}

UkmEntry PreloadingAttemptUkmEntryBuilder::BuildEntry(
    ukm::SourceId source_id,
    PreloadingEligibility eligibility,
    PreloadingHoldbackStatus holdback_status,
    PreloadingTriggeringOutcome triggering_outcome,
    bool accurate) const {
  return UkmEntry{
      source_id,
      {
          {Preloading_Attempt::kPreloadingTypeName,
           static_cast<int64_t>(preloading_type_)},
          {Preloading_Attempt::kPreloadingPredictorName,
           static_cast<int64_t>(predictor_)},
          {Preloading_Attempt::kEligibilityName,
           static_cast<int64_t>(eligibility)},
          {Preloading_Attempt::kHoldbackStatusName,
           static_cast<int64_t>(holdback_status)},
          {Preloading_Attempt::kTriggeringOutcomeName,
           static_cast<int64_t>(triggering_outcome)},
          {Preloading_Attempt::kAccurateTriggeringName, accurate ? 1 : 0},
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

}  // namespace content::test
