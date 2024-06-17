// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/preloading/preloading_attempt_impl.h"

#include "base/strings/stringprintf.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/browser/preloading/preloading.h"
#include "content/browser/preloading/preloading_config.h"
#include "content/common/features.h"
#include "content/public/browser/preloading.h"
#include "services/metrics/public/cpp/ukm_builders.h"
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
    preloading_predictor::kBackGestureNavigation,
    preloading_predictor::kPreloadingHeuristicsMLModel,
    content_preloading_predictor::kSpeculationRules,
    content_preloading_predictor::kMouseBackButton,
    content_preloading_predictor::kSpeculationRulesFromIsolatedWorld,
    content_preloading_predictor::kSpeculationRulesFromAutoSpeculationRules,
};

const PreloadingType kTypes[] = {
    PreloadingType::kUnspecified,     PreloadingType::kPreconnect,
    PreloadingType::kPrefetch,        PreloadingType::kPrerender,
    PreloadingType::kNoStatePrefetch,
};

constexpr char kUmaTriggerOutcome[] =
    "Preloading.%s.Attempt.%s.TriggeringOutcome";

}  // namespace

using PreloadingAttemptImplRecordUMATest = ::testing::TestWithParam<
    ::testing::tuple<PreloadingPredictor, PreloadingType>>;

TEST_P(PreloadingAttemptImplRecordUMATest, TestHistogramRecordedCorrectly) {
  const auto& test_param = GetParam();
  const auto predictor = ::testing::get<0>(test_param);
  const auto preloading_type = ::testing::get<1>(test_param);
  auto attempt = std::make_unique<PreloadingAttemptImpl>(
      predictor, predictor, preloading_type,
      /*triggered_primary_page_source_id=*/0,
      /*url_match_predicate=*/
      PreloadingData::GetSameURLMatcher(GURL("http://example.com/")),
      /*planned_max_preloading_type=*/std::nullopt,
      /*sampling_seed=*/1ul);
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

class PreloadingAttemptUKMTest : public ::testing::Test {
 public:
  PreloadingAttemptUKMTest() = default;

  void SetUp() override {
    ukm_recorder_ = std::make_unique<ukm::TestAutoSetUkmRecorder>();
  }

  void TearDown() override { ukm_recorder_.reset(); }

  ukm::TestUkmRecorder* ukm_recorder() { return ukm_recorder_.get(); }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;
  std::unique_ptr<ukm::TestUkmRecorder> ukm_recorder_;
};

TEST_F(PreloadingAttemptUKMTest, NoSampling) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeatureWithParameters(features::kPreloadingConfig,
                                              {{"preloading_config", R"(
  [{
    "preloading_type": "Preconnect",
    "preloading_predictor": "UrlPointerDownOnAnchor",
    "sampling_likelihood": 1.0
  }]
  )"}});
  PreloadingConfig& config = PreloadingConfig::GetInstance();
  config.ParseConfig();

  PreloadingAttemptImpl attempt(
      preloading_predictor::kUrlPointerDownOnAnchor,
      preloading_predictor::kUrlPointerDownOnAnchor,
      PreloadingType::kPreconnect, ukm::AssignNewSourceId(),
      PreloadingData::GetSameURLMatcher(GURL("http://example.com/")),
      /*planned_max_preloading_type=*/std::nullopt,
      /*sampling_seed=*/1ul);
  attempt.RecordPreloadingAttemptMetrics(ukm::AssignNewSourceId());
  const char* entry_name =
      ukm::builders::Preloading_Attempt_PreviousPrimaryPage::kEntryName;

  // Make sure the attempt is recorded, with a sampling_likelihood of 1,000,000.
  EXPECT_EQ(ukm_recorder()->GetEntriesByName(entry_name).size(), 1ul);
  auto* entry = ukm_recorder()->GetEntriesByName(entry_name)[0].get();
  ukm_recorder()->EntryHasMetric(
      entry, ukm::builders::Preloading_Attempt_PreviousPrimaryPage::
                 kSamplingLikelihoodName);
  ukm_recorder()->ExpectEntryMetric(
      entry,
      ukm::builders::Preloading_Attempt_PreviousPrimaryPage::
          kSamplingLikelihoodName,
      1'000'000);
}

TEST_F(PreloadingAttemptUKMTest, SampledOut) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeatureWithParameters(features::kPreloadingConfig,
                                              {{"preloading_config", R"(
  [{
    "preloading_type": "Preconnect",
    "preloading_predictor": "UrlPointerDownOnAnchor",
    "sampling_likelihood": 0.0
  }]
  )"}});
  PreloadingConfig& config = PreloadingConfig::GetInstance();
  config.ParseConfig();

  PreloadingAttemptImpl attempt(
      preloading_predictor::kUrlPointerDownOnAnchor,
      preloading_predictor::kUrlPointerDownOnAnchor,
      PreloadingType::kPreconnect, ukm::AssignNewSourceId(),
      PreloadingData::GetSameURLMatcher(GURL("http://example.com/")),
      /*planned_max_preloading_type=*/std::nullopt,
      /*sampling_seed=*/1ul);
  attempt.RecordPreloadingAttemptMetrics(ukm::AssignNewSourceId());
  const char* entry_name =
      ukm::builders::Preloading_Attempt_PreviousPrimaryPage::kEntryName;

  // Make sure the attempt is not recorded.
  EXPECT_EQ(ukm_recorder()->GetEntriesByName(entry_name).size(), 0ul);
}
}  // namespace content
