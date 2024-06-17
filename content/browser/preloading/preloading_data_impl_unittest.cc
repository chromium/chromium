// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/preloading/preloading_data_impl.h"

#include <algorithm>

#include "base/strings/strcat.h"
#include "base/test/metrics/histogram_tester.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/browser/preloading/preloading.h"
#include "content/browser/preloading/preloading_confidence.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/test/mock_navigation_handle.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_renderer_host.h"
#include "content/test/test_web_contents.h"
#include "services/metrics/public/cpp/metrics_utils.h"
#include "services/metrics/public/cpp/ukm_builders.h"

namespace content {

class PreloadingDataImplTest : public RenderViewHostTestHarness {
 public:
  PreloadingDataImplTest() = default;

  PreloadingDataImplTest(const PreloadingDataImplTest&) = delete;
  PreloadingDataImplTest& operator=(const PreloadingDataImplTest&) = delete;

  void SetUp() override {
    RenderViewHostTestHarness::SetUp();

    browser_context_ = std::make_unique<TestBrowserContext>();
    web_contents_ = TestWebContents::Create(
        browser_context_.get(),
        SiteInstanceImpl::Create(browser_context_.get()));
  }

  void TearDown() override {
    web_contents_.reset();
    browser_context_.reset();
    RenderViewHostTestHarness::TearDown();
  }

  WebContents* GetWebContents() { return web_contents_.get(); }

  std::string UmaPredictorPrecision(const PreloadingPredictor& predictor) {
    return base::StrCat(
        {"Preloading.Predictor.", predictor.name(), ".Precision"});
  }

  std::string UmaPredictorRecall(const PreloadingPredictor& predictor) {
    return base::StrCat({"Preloading.Predictor.", predictor.name(), ".Recall"});
  }

  std::string UmaAttemptPrecision(const PreloadingPredictor& predictor,
                                  PreloadingType preloading_type) {
    return base::StrCat({"Preloading.", PreloadingTypeToString(preloading_type),
                         ".Attempt.", predictor.name(), ".Precision"});
  }

  std::string UmaAttemptRecall(const PreloadingPredictor& predictor,
                               PreloadingType preloading_type) {
    return base::StrCat({"Preloading.", PreloadingTypeToString(preloading_type),
                         ".Attempt.", predictor.name(), ".Recall"});
  }

 private:
  std::unique_ptr<TestBrowserContext> browser_context_;
  std::unique_ptr<TestWebContents> web_contents_;
};

TEST_F(PreloadingDataImplTest, PredictorPrecisionAndRecall) {
  base::HistogramTester histogram_tester;
  auto* preloading_data =
      PreloadingDataImpl::GetOrCreateForWebContents(GetWebContents());

  preloading_data->SetIsNavigationInDomainCallback(
      preloading_predictor::kUrlPointerDownOnAnchor,
      base::BindRepeating(
          [](NavigationHandle* /*navigation_handle*/) { return true; }));
  preloading_data->SetIsNavigationInDomainCallback(
      preloading_predictor::kUrlPointerHoverOnAnchor,
      base::BindRepeating(
          [](NavigationHandle* /*navigation_handle*/) { return true; }));
  preloading_data->SetIsNavigationInDomainCallback(
      preloading_predictor::kLinkRel,
      base::BindRepeating(
          [](NavigationHandle* /*navigation_handle*/) { return true; }));

  // Add preloading predictions.
  GURL url_1{"https://www.example.com/page1.html"};
  GURL url_2{"https://www.example.com/page2.html"};
  GURL url_3{"https://www.example.com/page3.html"};
  const auto target = url_1;
  PreloadingPredictor predictor_1{
      preloading_predictor::kUrlPointerDownOnAnchor};
  PreloadingPredictor predictor_2{
      preloading_predictor::kUrlPointerHoverOnAnchor};
  PreloadingPredictor predictor_3{preloading_predictor::kLinkRel};
  ukm::SourceId triggered_primary_page_source_id =
      GetWebContents()->GetPrimaryMainFrame()->GetPageUkmSourceId();
  preloading_data->AddPreloadingPrediction(
      predictor_1, PreloadingConfidence{100},
      PreloadingData::GetSameURLMatcher(url_1),
      triggered_primary_page_source_id);
  preloading_data->AddPreloadingPrediction(
      predictor_1, PreloadingConfidence{100},
      PreloadingData::GetSameURLMatcher(url_1),
      triggered_primary_page_source_id);
  preloading_data->AddPreloadingPrediction(
      predictor_1, PreloadingConfidence{100},
      PreloadingData::GetSameURLMatcher(url_2),
      triggered_primary_page_source_id);

  preloading_data->AddPreloadingPrediction(
      predictor_2, PreloadingConfidence{100},
      PreloadingData::GetSameURLMatcher(url_2),
      triggered_primary_page_source_id);
  preloading_data->AddPreloadingPrediction(
      predictor_2, PreloadingConfidence{100},
      PreloadingData::GetSameURLMatcher(url_3),
      triggered_primary_page_source_id);

  NavigationSimulator::NavigateAndCommitFromBrowser(GetWebContents(), target);

  // Check precision UKM records.
  // Since, we added the predictor twice, it should count the true positives
  // twice as well.
  histogram_tester.ExpectBucketCount(UmaPredictorPrecision(predictor_1),
                                     PredictorConfusionMatrix::kTruePositive,
                                     2);
  histogram_tester.ExpectBucketCount(UmaPredictorPrecision(predictor_1),
                                     PredictorConfusionMatrix::kFalsePositive,
                                     1);

  histogram_tester.ExpectBucketCount(UmaPredictorPrecision(predictor_2),
                                     PredictorConfusionMatrix::kTruePositive,
                                     0);
  histogram_tester.ExpectBucketCount(UmaPredictorPrecision(predictor_2),
                                     PredictorConfusionMatrix::kFalsePositive,
                                     2);

  histogram_tester.ExpectTotalCount(UmaPredictorPrecision(predictor_3), 0);

  // Check recall UKM records.
  // It should only record 1 TP and not 2, and also no FN.
  histogram_tester.ExpectBucketCount(UmaPredictorRecall(predictor_1),
                                     PredictorConfusionMatrix::kTruePositive,
                                     1);
  histogram_tester.ExpectBucketCount(UmaPredictorRecall(predictor_1),
                                     PredictorConfusionMatrix::kFalseNegative,
                                     0);
  // It should only record 1 FN.
  histogram_tester.ExpectBucketCount(UmaPredictorRecall(predictor_2),
                                     PredictorConfusionMatrix::kTruePositive,
                                     0);
  histogram_tester.ExpectBucketCount(UmaPredictorRecall(predictor_2),
                                     PredictorConfusionMatrix::kFalseNegative,
                                     1);
  // For the missing predictor we should record 1 FN.
  histogram_tester.ExpectBucketCount(UmaPredictorRecall(predictor_3),
                                     PredictorConfusionMatrix::kTruePositive,
                                     0);
  histogram_tester.ExpectBucketCount(UmaPredictorRecall(predictor_3),
                                     PredictorConfusionMatrix::kFalseNegative,
                                     1);
}

TEST_F(PreloadingDataImplTest, PageLoadWithoutAttemptIsFalseNegative) {
  base::HistogramTester histogram_tester;
  auto* preloading_data =
      PreloadingDataImpl::GetOrCreateForWebContents(GetWebContents());

  const PreloadingPredictor predictor{
      preloading_predictor::kUrlPointerDownOnAnchor};
  preloading_data->SetIsNavigationInDomainCallback(
      predictor,
      base::BindRepeating(
          [](NavigationHandle* /*navigation_handle*/) { return true; }));

  const GURL target{"https://www.example.com/page1.html"};

  NavigationSimulator::NavigateAndCommitFromBrowser(GetWebContents(), target);

  // The lack of an attempt represents a false negative.
  histogram_tester.ExpectUniqueSample(
      UmaAttemptRecall(predictor, PreloadingType::kPrefetch),
      PredictorConfusionMatrix::kFalseNegative, 1);
  histogram_tester.ExpectUniqueSample(
      UmaAttemptRecall(predictor, PreloadingType::kPrerender),
      PredictorConfusionMatrix::kFalseNegative, 1);
}

TEST_F(PreloadingDataImplTest, PreloadingAttemptPrecisionAndRecall) {
  base::HistogramTester histogram_tester;
  auto* preloading_data =
      PreloadingDataImpl::GetOrCreateForWebContents(GetWebContents());

  preloading_data->SetIsNavigationInDomainCallback(
      preloading_predictor::kUrlPointerDownOnAnchor,
      base::BindRepeating(
          [](NavigationHandle* /*navigation_handle*/) { return true; }));
  preloading_data->SetIsNavigationInDomainCallback(
      preloading_predictor::kUrlPointerHoverOnAnchor,
      base::BindRepeating(
          [](NavigationHandle* /*navigation_handle*/) { return true; }));
  preloading_data->SetIsNavigationInDomainCallback(
      preloading_predictor::kLinkRel,
      base::BindRepeating(
          [](NavigationHandle* /*navigation_handle*/) { return false; }));

  // Add preloading predictions.
  GURL url_1{"https://www.example.com/page1.html"};
  GURL url_2{"https://www.example.com/page2.html"};
  GURL url_3{"https://www.example.com/page3.html"};
  const auto target = url_1;
  PreloadingPredictor predictor_1{
      preloading_predictor::kUrlPointerDownOnAnchor};
  PreloadingPredictor predictor_2{
      preloading_predictor::kUrlPointerHoverOnAnchor};
  PreloadingPredictor predictor_3{preloading_predictor::kLinkRel};

  std::vector<std::tuple<PreloadingPredictor, PreloadingType, GURL>> attempts{
      {predictor_1, PreloadingType::kPrerender, url_1},
      {predictor_2, PreloadingType::kPrefetch, url_2},
      {predictor_2, PreloadingType::kPrerender, url_1},
      {predictor_2, PreloadingType::kPrerender, url_2},
      {predictor_2, PreloadingType::kPrerender, url_3},
  };

  for (const auto& [predictor, preloading_type, url] : attempts) {
    preloading_data->AddPreloadingAttempt(
        predictor, preloading_type, PreloadingData::GetSameURLMatcher(url),
        /*planned_max_preloading_type=*/std::nullopt,
        GetWebContents()->GetPrimaryMainFrame()->GetPageUkmSourceId());
  }

  NavigationSimulator::NavigateAndCommitFromBrowser(GetWebContents(), target);

  // Check precision UKM records.
  // There should be no UMA records for predictor_1, prefetch attempt.
  histogram_tester.ExpectBucketCount(
      UmaAttemptPrecision(predictor_1, PreloadingType::kPrefetch),
      PredictorConfusionMatrix::kTruePositive, 0);
  histogram_tester.ExpectBucketCount(
      UmaAttemptPrecision(predictor_1, PreloadingType::kPrefetch),
      PredictorConfusionMatrix::kFalsePositive, 0);
  // There should 1 TP and 0 FP for predictor_1, prerender attempt.
  histogram_tester.ExpectBucketCount(
      UmaAttemptPrecision(predictor_1, PreloadingType::kPrerender),
      PredictorConfusionMatrix::kTruePositive, 1);
  histogram_tester.ExpectBucketCount(
      UmaAttemptPrecision(predictor_1, PreloadingType::kPrerender),
      PredictorConfusionMatrix::kFalsePositive, 0);
  // There should 0 TP and 1 FP for predictor_2, prefetch attempt.
  histogram_tester.ExpectBucketCount(
      UmaAttemptPrecision(predictor_2, PreloadingType::kPrefetch),
      PredictorConfusionMatrix::kTruePositive, 0);
  histogram_tester.ExpectBucketCount(
      UmaAttemptPrecision(predictor_2, PreloadingType::kPrefetch),
      PredictorConfusionMatrix::kFalsePositive, 1);
  // There should 1 TP and 2 FP for predictor_2, prerender attempt.
  histogram_tester.ExpectBucketCount(
      UmaAttemptPrecision(predictor_2, PreloadingType::kPrerender),
      PredictorConfusionMatrix::kTruePositive, 1);
  histogram_tester.ExpectBucketCount(
      UmaAttemptPrecision(predictor_2, PreloadingType::kPrerender),
      PredictorConfusionMatrix::kFalsePositive, 2);

  // Check recall UKM records.
  // predictor_1, prerender should be TP
  histogram_tester.ExpectBucketCount(
      UmaAttemptRecall(predictor_1, PreloadingType::kPrerender),
      PredictorConfusionMatrix::kTruePositive, 1);
  histogram_tester.ExpectBucketCount(
      UmaAttemptRecall(predictor_1, PreloadingType::kPrerender),
      PredictorConfusionMatrix::kFalseNegative, 0);
  // predictor_1, prefetch should be FN
  histogram_tester.ExpectBucketCount(
      UmaAttemptRecall(predictor_1, PreloadingType::kPrefetch),
      PredictorConfusionMatrix::kTruePositive, 0);
  histogram_tester.ExpectBucketCount(
      UmaAttemptRecall(predictor_1, PreloadingType::kPrefetch),
      PredictorConfusionMatrix::kFalseNegative, 1);
  // predictor_2, prerender should be TP
  histogram_tester.ExpectBucketCount(
      UmaAttemptRecall(predictor_2, PreloadingType::kPrerender),
      PredictorConfusionMatrix::kTruePositive, 1);
  histogram_tester.ExpectBucketCount(
      UmaAttemptRecall(predictor_2, PreloadingType::kPrerender),
      PredictorConfusionMatrix::kFalseNegative, 0);
  // predictor_2, prefetch should be FN
  histogram_tester.ExpectBucketCount(
      UmaAttemptRecall(predictor_2, PreloadingType::kPrefetch),
      PredictorConfusionMatrix::kTruePositive, 0);
  histogram_tester.ExpectBucketCount(
      UmaAttemptRecall(predictor_2, PreloadingType::kPrefetch),
      PredictorConfusionMatrix::kFalseNegative, 1);
  // 'page_in_domain_check' returns false for predictor_3: TP=0, FN=0.
  histogram_tester.ExpectBucketCount(
      UmaAttemptRecall(predictor_3, PreloadingType::kPrerender),
      PredictorConfusionMatrix::kTruePositive, 0);
  histogram_tester.ExpectBucketCount(
      UmaAttemptRecall(predictor_3, PreloadingType::kPrerender),
      PredictorConfusionMatrix::kFalseNegative, 0);
  histogram_tester.ExpectBucketCount(
      UmaAttemptRecall(predictor_3, PreloadingType::kPrefetch),
      PredictorConfusionMatrix::kTruePositive, 0);
  histogram_tester.ExpectBucketCount(
      UmaAttemptRecall(predictor_3, PreloadingType::kPrefetch),
      PredictorConfusionMatrix::kFalseNegative, 0);
}

namespace {
void RunSamplingTest(WebContents* web_contents,
                     int num_predictions,
                     int expected_sampling_amount_bucket) {
  using Preloading_Prediction = ukm::builders::Preloading_Prediction;

  ukm::TestAutoSetUkmRecorder test_ukm_recorder;
  ukm::SourceId triggered_primary_page_source_id =
      web_contents->GetPrimaryMainFrame()->GetPageUkmSourceId();

  auto* preloading_data =
      PreloadingDataImpl::GetOrCreateForWebContents(web_contents);
  preloading_data->SetIsNavigationInDomainCallback(
      preloading_predictor::kUrlPointerHoverOnAnchor,
      base::BindRepeating(
          [](NavigationHandle* /*navigation_handle*/) { return true; }));

  // Add a number of predictions. If they're beyond the limit, we should only
  // keep a random sample to stay within the limit.
  preloading_data->SetMaxPredictionsToTenForTesting();
  const size_t expected_predictions_size = std::min(10, num_predictions);
  for (int i = 0; i < num_predictions; ++i) {
    preloading_data->AddPreloadingPrediction(
        preloading_predictor::kUrlPointerHoverOnAnchor,
        PreloadingConfidence{100},
        PreloadingData::GetSameURLMatcher(
            GURL(base::StrCat({"https://www.example.com/page",
                               base::NumberToString(i), ".html"}))),
        triggered_primary_page_source_id);
  }
  EXPECT_EQ(expected_predictions_size,
            preloading_data->GetPredictionsSizeForTesting());

  NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents, GURL("https://www.example.com/somewhere_else.html"));

  std::vector<ukm::TestUkmRecorder::HumanReadableUkmEntry> actual_ukm_entries =
      test_ukm_recorder.GetEntries(
          Preloading_Prediction::kEntryName,
          {Preloading_Prediction::kSamplingAmountName});
  EXPECT_EQ(expected_predictions_size, actual_ukm_entries.size());
  for (const auto& entry : actual_ukm_entries) {
    EXPECT_EQ(expected_sampling_amount_bucket,
              entry.metrics.at(Preloading_Prediction::kSamplingAmountName));
  }
}
}  // namespace

TEST_F(PreloadingDataImplTest, MaxPredictions) {
  constexpr double kBucketSpacing = 1.3;
  RunSamplingTest(GetWebContents(), /*num_predictions=*/20,
                  /*expected_sampling_amount_bucket=*/
                  ukm::GetExponentialBucketMin(500'000, kBucketSpacing));
}

TEST_F(PreloadingDataImplTest, NoSamplingUnderMaxPredictions) {
  RunSamplingTest(GetWebContents(), /*num_predictions=*/5,
                  /*expected_sampling_amount_bucket=*/0);
}

}  // namespace content
