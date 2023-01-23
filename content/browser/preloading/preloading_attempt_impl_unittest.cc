// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/preloading/preloading_attempt_impl.h"

#include "base/strings/stringprintf.h"
#include "base/test/metrics/histogram_tester.h"
#include "content/browser/preloading/preloading.h"
#include "content/public/browser/preloading.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "testing/gtest/include/gtest/gtest-param-test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {
const PreloadingPredictor kPredictors[] = {
    preloading_predictor::kUnspecified,
    preloading_predictor::kUrlPointerDownOnAnchor,
    preloading_predictor::kUrlPointerHoverOnAnchor,
    preloading_predictor::kLinkRel,
    content_preloading_predictor::kSpeculationRules,
};

const PreloadingType kTypes[] = {
    PreloadingType::kUnspecified,     PreloadingType::kPreconnect,
    PreloadingType::kPrefetch,        PreloadingType::kPrerender,
    PreloadingType::kNoStatePrefetch,
};

const char* kUmaTriggerOutcome = "Preloading.%s.Attempt.%s.TriggeringOutcome";

static base::StringPiece PreloadingTypeToString(PreloadingType type) {
  switch (type) {
    case PreloadingType::kUnspecified:
      return "Unspecified";
    case PreloadingType::kPreconnect:
      return "Preconnect";
    case PreloadingType::kPrefetch:
      return "Prefetch";
    case PreloadingType::kPrerender:
      return "Prerender";
    case PreloadingType::kNoStatePrefetch:
      return "NoStatePrefetch";
    default:
      NOTREACHED();
      return "";
  }
}
}  // namespace

using PreloadingAttemptImplRecordUMATest = ::testing::TestWithParam<
    ::testing::tuple<PreloadingPredictor, PreloadingType>>;

TEST_P(PreloadingAttemptImplRecordUMATest, TestHistogramRecordedCorrectly) {
  const auto& test_param = GetParam();
  const auto predictor = ::testing::get<0>(test_param);
  const auto preloading_type = ::testing::get<1>(test_param);
  auto attempt = std::make_unique<PreloadingAttemptImpl>(
      predictor, preloading_type, /*triggered_primary_page_source_id=*/0,
      /*url_match_predicate=*/
      PreloadingData::GetSameURLMatcher(GURL("http://example.com/")));
  {
    base::HistogramTester histogram_tester;
    // Use `ukm::kInvalidSourceId` so we skip the UKM recording.
    attempt->RecordPreloadingAttemptMetrics(ukm::kInvalidSourceId);
    histogram_tester.ExpectUniqueSample(
        base::StringPrintf(kUmaTriggerOutcome,
                           PreloadingTypeToString(preloading_type).data(),
                           predictor.name().data()),
        PreloadingTriggeringOutcome::kUnspecified, 1);
  }
  {
    attempt->SetEligibility(PreloadingEligibility::kEligible);
    attempt->SetHoldbackStatus(PreloadingHoldbackStatus::kAllowed);
    attempt->SetTriggeringOutcome(PreloadingTriggeringOutcome::kRunning);
    base::HistogramTester histogram_tester;
    attempt->RecordPreloadingAttemptMetrics(ukm::kInvalidSourceId);
    histogram_tester.ExpectUniqueSample(
        base::StringPrintf(kUmaTriggerOutcome,
                           PreloadingTypeToString(preloading_type).data(),
                           predictor.name().data()),
        PreloadingTriggeringOutcome::kRunning, 1);
  }
}

INSTANTIATE_TEST_SUITE_P(PreloadingAttemptImplRecordUMATests,
                         PreloadingAttemptImplRecordUMATest,
                         ::testing::Combine(::testing::ValuesIn(kPredictors),
                                            ::testing::ValuesIn(kTypes)));
}  // namespace content
