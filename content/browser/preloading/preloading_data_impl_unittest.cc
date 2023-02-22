// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/preloading/preloading_data_impl.h"

#include "base/strings/strcat.h"
#include "base/test/metrics/histogram_tester.h"
#include "content/browser/preloading/preloading.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/test/mock_navigation_handle.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_renderer_host.h"
#include "content/test/test_web_contents.h"

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

 private:
  std::unique_ptr<TestBrowserContext> browser_context_;
  std::unique_ptr<TestWebContents> web_contents_;
};

TEST_F(PreloadingDataImplTest, PredictorPrecision) {
  base::HistogramTester histogram_tester;
  auto* preloading_data =
      PreloadingDataImpl::GetOrCreateForWebContents(GetWebContents());

  // Add preloading predictions.
  GURL url_1{"https:www.example.com/page1.html"};
  GURL url_2{"https:www.example.com/page2.html"};
  GURL url_3{"https:www.example.com/page3.html"};
  const auto target = url_1;
  PreloadingPredictor predictor_1{
      preloading_predictor::kUrlPointerDownOnAnchor};
  PreloadingPredictor predictor_2{
      preloading_predictor::kUrlPointerHoverOnAnchor};

  preloading_data->AddPreloadingPrediction(
      predictor_1,
      /*confidence=*/100, PreloadingData::GetSameURLMatcher(url_1));
  preloading_data->AddPreloadingPrediction(
      predictor_1,
      /*confidence=*/100, PreloadingData::GetSameURLMatcher(url_1));
  preloading_data->AddPreloadingPrediction(
      predictor_1,
      /*confidence=*/100, PreloadingData::GetSameURLMatcher(url_2));

  preloading_data->AddPreloadingPrediction(
      predictor_2,
      /*confidence=*/100, PreloadingData::GetSameURLMatcher(url_2));
  preloading_data->AddPreloadingPrediction(
      predictor_2,
      /*confidence=*/100, PreloadingData::GetSameURLMatcher(url_3));

  // Mock navigating to the target URL.
  MockNavigationHandle navigation_handle{GetWebContents()};
  navigation_handle.set_url(target);
  navigation_handle.set_is_in_primary_main_frame(true);
  navigation_handle.set_is_same_document(false);
  navigation_handle.set_has_committed(true);
  preloading_data->DidStartNavigation(&navigation_handle);

  // Check precision UKM records.
  auto uma_predictor_precision = [](const PreloadingPredictor& predictor) {
    return base::StrCat(
        {"Preloading.Predictor.", predictor.name(), ".Precision"});
  };
  // Since, we added the predictor twice, it should count the true positives
  // twice as well.
  histogram_tester.ExpectBucketCount(uma_predictor_precision(predictor_1),
                                     PredictorConfusionMatrix::kTruePositive,
                                     2);
  histogram_tester.ExpectBucketCount(uma_predictor_precision(predictor_1),
                                     PredictorConfusionMatrix::kFalsePositive,
                                     1);

  histogram_tester.ExpectBucketCount(uma_predictor_precision(predictor_2),
                                     PredictorConfusionMatrix::kTruePositive,
                                     0);
  histogram_tester.ExpectBucketCount(uma_predictor_precision(predictor_2),
                                     PredictorConfusionMatrix::kFalsePositive,
                                     2);
}
TEST_F(PreloadingDataImplTest, PreloadingAttemptPrecision) {
  base::HistogramTester histogram_tester;
  auto* preloading_data =
      PreloadingDataImpl::GetOrCreateForWebContents(GetWebContents());

  // Add preloading predictions.
  GURL url_1{"https:www.example.com/page1.html"};
  GURL url_2{"https:www.example.com/page2.html"};
  GURL url_3{"https:www.example.com/page3.html"};
  const auto target = url_1;
  PreloadingPredictor predictor_1{
      preloading_predictor::kUrlPointerDownOnAnchor};
  PreloadingPredictor predictor_2{
      preloading_predictor::kUrlPointerHoverOnAnchor};
  std::vector<std::tuple<PreloadingPredictor, PreloadingType, GURL>> attempts{
      {predictor_1, PreloadingType::kPrerender, url_1},
      {predictor_2, PreloadingType::kPrefetch, url_2},
      {predictor_2, PreloadingType::kPrerender, url_1},
      {predictor_2, PreloadingType::kPrerender, url_2},
      {predictor_2, PreloadingType::kPrerender, url_3},
  };

  for (const auto& [predictor, preloading_type, url] : attempts) {
    preloading_data->AddPreloadingAttempt(
        predictor, preloading_type, PreloadingData::GetSameURLMatcher(url));
  }

  // Mock navigating to the target URL.
  MockNavigationHandle navigation_handle{GetWebContents()};
  navigation_handle.set_url(target);
  navigation_handle.set_is_in_primary_main_frame(true);
  navigation_handle.set_is_same_document(false);
  navigation_handle.set_has_committed(true);
  preloading_data->DidStartNavigation(&navigation_handle);

  // Check precision UKM records.
  auto uma_attempt_precision = [](const PreloadingPredictor& predictor,
                                  PreloadingType preloading_type) {
    return base::StrCat({"Preloading.", PreloadingTypeToString(preloading_type),
                         ".Attempt.", predictor.name(), ".Precision"});
  };
  // There should be no UMA records for predictor_1, prefetch attempt.
  histogram_tester.ExpectBucketCount(
      uma_attempt_precision(predictor_1, PreloadingType::kPrefetch),
      PredictorConfusionMatrix::kTruePositive, 0);
  histogram_tester.ExpectBucketCount(
      uma_attempt_precision(predictor_1, PreloadingType::kPrefetch),
      PredictorConfusionMatrix::kFalsePositive, 0);
  // There should 1 TP and 0 FP for predictor_1, prerender attempt.
  histogram_tester.ExpectBucketCount(
      uma_attempt_precision(predictor_1, PreloadingType::kPrerender),
      PredictorConfusionMatrix::kTruePositive, 1);
  histogram_tester.ExpectBucketCount(
      uma_attempt_precision(predictor_1, PreloadingType::kPrerender),
      PredictorConfusionMatrix::kFalsePositive, 0);
  // There should 0 TP and 1 FP for predictor_2, prefetch attempt.
  histogram_tester.ExpectBucketCount(
      uma_attempt_precision(predictor_2, PreloadingType::kPrefetch),
      PredictorConfusionMatrix::kTruePositive, 0);
  histogram_tester.ExpectBucketCount(
      uma_attempt_precision(predictor_2, PreloadingType::kPrefetch),
      PredictorConfusionMatrix::kFalsePositive, 1);
  // There should 1 TP and 2 FP for predictor_2, prerender attempt.
  histogram_tester.ExpectBucketCount(
      uma_attempt_precision(predictor_2, PreloadingType::kPrerender),
      PredictorConfusionMatrix::kTruePositive, 1);
  histogram_tester.ExpectBucketCount(
      uma_attempt_precision(predictor_2, PreloadingType::kPrerender),
      PredictorConfusionMatrix::kFalsePositive, 2);
}

}  // namespace content
