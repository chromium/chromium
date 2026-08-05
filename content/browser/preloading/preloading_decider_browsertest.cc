// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/preloading/preloading_decider.h"

#include "base/strings/stringprintf.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "base/timer/elapsed_timer.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/browser/preloading/preloading.h"
#include "content/browser/preloading/preloading_data_impl.h"
#include "content/browser/preloading/prerender/prerender_features.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/preloading_test_util.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/shell/browser/shell.h"
#include "net/dns/mock_host_resolver.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/preloading/anchor_element_interaction_host.mojom.h"
#include "ui/gfx/geometry/point_conversions.h"

namespace content {

class PreloadingDeciderBrowserTest : public ContentBrowserTest {
 public:
  PreloadingDeciderBrowserTest() = default;
  ~PreloadingDeciderBrowserTest() override = default;

  void SetUp() override {
    feature_list_.InitWithFeaturesAndParameters(
        {
            {blink::features::kPreloadingHeuristicsMLModel,
             {{"enact_candidates", "true"}}},
        },
        {
            // Disable the memory requirement of Prerender2 so the test can run
            // on any bot.
            {blink::features::kPrerender2MemoryControls},
            // These tests cover the moderate hover heuristic. The eager one
            // fires first on a real hover, which would be a second, unrelated
            // heuristic firing for the same anchor.
            {blink::features::kPreloadingEagerHoverHeuristics},
        });

    ContentBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    ContentBrowserTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
    https_server_ = std::make_unique<net::EmbeddedTestServer>(
        net::EmbeddedTestServer::TYPE_HTTPS);
    https_server_->AddDefaultHandlers(GetTestDataFilePath());

    ASSERT_TRUE(https_server_->Start());
  }

  WebContents* web_contents() { return shell()->web_contents(); }

  const GURL GetTestURL(const char* file) const {
    return https_server_->GetURL(file);
  }

  void ExpectCandidatesReceived() {
    // The renderer queues updates and waits for style to be clean.
    EXPECT_EQ(true, EvalJsAfterLifecycleUpdate(web_contents(), "", "true"));
    EXPECT_EQ(true, EvalJsAfterLifecycleUpdate(web_contents(), "", "true"));

    auto* preloading_decider = PreloadingDecider::GetOrCreateForCurrentDocument(
        web_contents()->GetPrimaryMainFrame());
    ASSERT_TRUE(preloading_decider);
    EXPECT_TRUE(preloading_decider->HasCandidatesForTesting());
  }

  // Sends a real pointerdown over the element, so that the renderer's
  // AnchorElementInteractionTracker sees it. Deliberately only a mouse down:
  // a full click would navigate.
  void SimulateMouseDownOnElement(std::string_view id) {
    SimulateMouseEvent(web_contents(), blink::WebInputEvent::Type::kMouseDown,
                       blink::WebMouseEvent::Button::kLeft,
                       gfx::ToFlooredPoint(GetCenterCoordinatesOfElementWithId(
                           web_contents(), id)));
  }

  // Moves the mouse onto the element, which makes the renderer start its hover
  // dwell timer. Moves straight there rather than via another element: pausing
  // over a second anchor for longer than a dwell threshold would start its
  // hover heuristic as well, and record an extra preloading prediction.
  void SimulateMouseHoverOnElement(std::string_view id) {
    SimulateMouseEvent(web_contents(), blink::WebInputEvent::Type::kMouseMove,
                       gfx::ToFlooredPoint(GetCenterCoordinatesOfElementWithId(
                           web_contents(), id)));
  }

  // Interaction heuristics are handled by the renderer, so enactment only
  // happens after a round trip. It is done once the candidate is no longer on
  // standby.
  [[nodiscard]] bool WaitUntilEnacted(const GURL& url,
                                      blink::mojom::SpeculationAction action) {
    auto* preloading_decider = PreloadingDecider::GetOrCreateForCurrentDocument(
        web_contents()->GetPrimaryMainFrame());
    return base::test::RunUntil([&]() {
      return !preloading_decider->IsOnStandByForTesting(url, action);
    });
  }

 private:
  content::test::PreloadingConfigOverride preloading_config_override_;
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<net::EmbeddedTestServer> https_server_;
};

IN_PROC_BROWSER_TEST_F(PreloadingDeciderBrowserTest,
                       SetIsNavigationInDomainCallback) {
  base::HistogramTester histogram_tester;
  ASSERT_TRUE(NavigateToURL(shell(),
                            GetTestURL("/preloading/preloading_decider.html")));
  ExpectCandidatesReceived();

  // Now navigate to another page
  TestNavigationObserver nav_observer(web_contents());
  EXPECT_TRUE(ExecJs(web_contents(), "document.getElementById('bar').click()"));
  nav_observer.Wait();

  histogram_tester.ExpectBucketCount(
      "Preloading.Predictor.SpeculationRules.Recall",
      PredictorConfusionMatrix::kFalseNegative, 1);
  histogram_tester.ExpectBucketCount(
      "Preloading.Predictor.UrlPointerDownOnAnchor.Recall",
      PredictorConfusionMatrix::kFalseNegative, 1);
  histogram_tester.ExpectBucketCount(
      "Preloading.Predictor.UrlPointerHoverOnAnchor.Recall",
      PredictorConfusionMatrix::kFalseNegative, 1);
  histogram_tester.ExpectBucketCount(
      "Preloading.Predictor.PreloadingHeuristicsMLModel.Recall",
      PredictorConfusionMatrix::kFalseNegative, 1);
}

class PreloadingDeciderNonImmediateBrowserTest
    : public PreloadingDeciderBrowserTest,
      public ::testing::WithParamInterface<
          ::testing::tuple<PreloadingPredictor, PreloadingType>> {
 public:
  static std::string DescribeParams(
      const testing::TestParamInfo<ParamType>& info) {
    const auto& [predictor, type] = info.param;
    return base::StringPrintf(
        "%s_%s", std::string(predictor.name()).c_str(),
        std::string(PreloadingTypeToString(type)).c_str());
  }

  static constexpr PreloadingPredictor kNonImmediatePredictors[] = {
      preloading_predictor::kUrlPointerDownOnAnchor,
      preloading_predictor::kUrlPointerHoverOnAnchor,
      preloading_predictor::kPreloadingHeuristicsMLModel,
  };

  static constexpr PreloadingType kPreloadingTypes[] = {
      PreloadingType::kPrefetch,
      PreloadingType::kPrerender,
  };

  PreloadingPredictor predictor() const {
    return ::testing::get<0>(GetParam());
  }

  PreloadingType type() const { return ::testing::get<1>(GetParam()); }
};

INSTANTIATE_TEST_SUITE_P(
    All,
    PreloadingDeciderNonImmediateBrowserTest,
    ::testing::Combine(
        ::testing::ValuesIn(
            PreloadingDeciderNonImmediateBrowserTest::kNonImmediatePredictors),
        ::testing::ValuesIn(
            PreloadingDeciderNonImmediateBrowserTest::kPreloadingTypes)),
    PreloadingDeciderNonImmediateBrowserTest::DescribeParams);

IN_PROC_BROWSER_TEST_P(PreloadingDeciderNonImmediateBrowserTest,
                       EnactModerateCandidate) {
  base::ScopedMockElapsedTimersForTest mock_elapsed_timer;
  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder ukm_recorder;

  ASSERT_TRUE(NavigateToURL(
      shell(), GetTestURL("/preloading/preloading_decider_moderate.html")));
  ExpectCandidatesReceived();

  std::string next_page_id;
  GURL next_page_url;
  blink::mojom::SpeculationAction action;
  switch (type()) {
    case PreloadingType::kPrefetch:
      next_page_id = "b";
      next_page_url = GetTestURL("/title1.html?b");
      action = blink::mojom::SpeculationAction::kPrefetch;
      break;
    case PreloadingType::kPrerender:
      next_page_id = "c";
      next_page_url = GetTestURL("/title1.html?c");
      action = blink::mojom::SpeculationAction::kPrerender;
      break;
    default:
      FAIL();
  }

  // Trigger the non-immediate predictor. The pointer heuristics run in the
  // renderer, so drive them with real input rather than by calling
  // PreloadingDecider directly.
  auto* preloading_decider = PreloadingDecider::GetOrCreateForCurrentDocument(
      web_contents()->GetPrimaryMainFrame());
  ASSERT_TRUE(preloading_decider);
  if (predictor() == preloading_predictor::kUrlPointerDownOnAnchor) {
    SimulateMouseDownOnElement(next_page_id);
  } else if (predictor() == preloading_predictor::kUrlPointerHoverOnAnchor) {
    SimulateMouseHoverOnElement(next_page_id);
  } else if (predictor() ==
             preloading_predictor::kPreloadingHeuristicsMLModel) {
    preloading_decider->OnPreloadingHeuristicsModelDone(next_page_url,
                                                        /*score=*/1.0);
  } else {
    FAIL();
  }
  // Enactment is asynchronous when it is driven from the renderer.
  ASSERT_TRUE(WaitUntilEnacted(next_page_url, action));

  TestNavigationObserver nav_observer(web_contents());
  EXPECT_TRUE(
      ExecJs(web_contents(),
             JsReplace("document.getElementById($1).click()", next_page_id)));
  nav_observer.Wait();

  const ukm::SourceId source_id = nav_observer.next_page_ukm_source_id();

  const std::string type_str{PreloadingTypeToString(type())};
  const char* type_cstr = type_str.c_str();

  // For non-immediate predictors, there are two PreloadingPredictors that
  // contribute to a preloading attempt. One creates a candidate, but does not
  // start the preloading attempt. The other starts the attempt. We assert below
  // that both predictors have appropriate attribution in the recorded metrics.
  {
    const std::string rule_predictor_str{
        content_preloading_predictor::kSpeculationRules.name()};
    const char* rule_predictor_cstr = rule_predictor_str.c_str();

    // We intentionally don't record a prediction for non-immediate speculation
    // rules. They aren't predictions per se, but a declaration to the browser
    // that preloading would be safe.
    histogram_tester.ExpectTotalCount(
        base::StringPrintf("Preloading.Predictor.%s.Precision",
                           rule_predictor_cstr),
        0);
    histogram_tester.ExpectUniqueSample(
        base::StringPrintf("Preloading.Predictor.%s.Recall",
                           rule_predictor_cstr),
        PredictorConfusionMatrix::kFalseNegative, 1);

    histogram_tester.ExpectUniqueSample(
        base::StringPrintf("Preloading.%s.Attempt.%s.Precision", type_cstr,
                           rule_predictor_cstr),
        PredictorConfusionMatrix::kTruePositive, 1);
    histogram_tester.ExpectUniqueSample(
        base::StringPrintf("Preloading.%s.Attempt.%s.Recall", type_cstr,
                           rule_predictor_cstr),
        PredictorConfusionMatrix::kTruePositive, 1);
    histogram_tester.ExpectUniqueSample(
        base::StringPrintf("Preloading.%s.Attempt.%s.TriggeringOutcome",
                           type_cstr, rule_predictor_cstr),
        PreloadingTriggeringOutcome::kSuccess, 1);

    test::PreloadingAttemptUkmEntryBuilder attempt_entry_builder(
        content_preloading_predictor::kSpeculationRules);
    ukm::TestUkmRecorder::HumanReadableUkmEntry expected_attempt_entry =
        attempt_entry_builder.BuildEntry(
            source_id, type(), PreloadingEligibility::kEligible,
            PreloadingHoldbackStatus::kAllowed,
            PreloadingTriggeringOutcome::kSuccess,
            PreloadingFailureReason::kUnspecified, /*accurate=*/true,
            base::ScopedMockElapsedTimersForTest::kMockElapsedTime,
            blink::mojom::SpeculationEagerness::kModerate);
    std::vector<ukm::TestUkmRecorder::HumanReadableUkmEntry> attempts =
        ukm_recorder.GetEntries(ukm::builders::Preloading_Attempt::kEntryName,
                                test::kPreloadingAttemptUkmMetrics);
    std::erase_if(attempts, [&](const auto& entry) {
      return entry.metrics.at(
                 ukm::builders::Preloading_Attempt::kPreloadingPredictorName) !=
             content_preloading_predictor::kSpeculationRules.ukm_value();
    });
    if (base::FeatureList::IsEnabled(
            features::kPrerender2FallbackPrefetchSpecRules) &&
        type() == PreloadingType::kPrerender) {
      // If type is prerender, `PrerendererImpl` triggers prefetch ahead
      // prerender. Ignore the corresponding UKM as it is checkid in
      // prerenderer_impl_browsertest.cc.
      ASSERT_EQ(attempts.size(), 2u);
      EXPECT_EQ(attempts[1], expected_attempt_entry)
          << test::ActualVsExpectedUkmEntryToString(attempts[1],
                                                    expected_attempt_entry);
    } else {
      ASSERT_EQ(attempts.size(), 1u);
      EXPECT_EQ(attempts[0], expected_attempt_entry)
          << test::ActualVsExpectedUkmEntryToString(attempts[0],
                                                    expected_attempt_entry);
    }

    std::vector<ukm::TestUkmRecorder::HumanReadableUkmEntry> predictions =
        ukm_recorder.GetEntries(
            ukm::builders::Preloading_Prediction::kEntryName,
            test::kPreloadingPredictionUkmMetrics);
    std::erase_if(predictions, [&](const auto& entry) {
      return entry.metrics.at(ukm::builders::Preloading_Prediction::
                                  kPreloadingPredictorName) !=
             content_preloading_predictor::kSpeculationRules.ukm_value();
    });
    EXPECT_TRUE(predictions.empty());
  }

  {
    const std::string predictor_str{predictor().name()};
    const char* predictor_cstr = predictor_str.c_str();

    histogram_tester.ExpectUniqueSample(
        base::StringPrintf("Preloading.Predictor.%s.Precision", predictor_cstr),
        PredictorConfusionMatrix::kTruePositive, 1);
    histogram_tester.ExpectUniqueSample(
        base::StringPrintf("Preloading.Predictor.%s.Recall", predictor_cstr),
        PredictorConfusionMatrix::kTruePositive, 1);
    histogram_tester.ExpectUniqueSample(
        base::StringPrintf("Preloading.%s.Attempt.%s.Precision", type_cstr,
                           predictor_cstr),
        PredictorConfusionMatrix::kTruePositive, 1);
    histogram_tester.ExpectUniqueSample(
        base::StringPrintf("Preloading.%s.Attempt.%s.Recall", type_cstr,
                           predictor_cstr),
        PredictorConfusionMatrix::kTruePositive, 1);
    histogram_tester.ExpectUniqueSample(
        base::StringPrintf("Preloading.%s.Attempt.%s.TriggeringOutcome",
                           type_cstr, predictor_cstr),
        PreloadingTriggeringOutcome::kSuccess, 1);

    constexpr bool accurate = true;
    constexpr int confidence = 100;

    test::PreloadingAttemptUkmEntryBuilder attempt_entry_builder(predictor());
    ukm::TestUkmRecorder::HumanReadableUkmEntry expected_attempt_entry =
        attempt_entry_builder.BuildEntry(
            source_id, type(), PreloadingEligibility::kEligible,
            PreloadingHoldbackStatus::kAllowed,
            PreloadingTriggeringOutcome::kSuccess,
            PreloadingFailureReason::kUnspecified, accurate,
            base::ScopedMockElapsedTimersForTest::kMockElapsedTime,
            blink::mojom::SpeculationEagerness::kModerate);
    std::vector<ukm::TestUkmRecorder::HumanReadableUkmEntry> attempts =
        ukm_recorder.GetEntries(ukm::builders::Preloading_Attempt::kEntryName,
                                test::kPreloadingAttemptUkmMetrics);
    std::erase_if(attempts, [&](const auto& entry) {
      return entry.metrics.at(
                 ukm::builders::Preloading_Attempt::kPreloadingPredictorName) !=
             predictor().ukm_value();
    });
    if (base::FeatureList::IsEnabled(
            features::kPrerender2FallbackPrefetchSpecRules) &&
        type() == PreloadingType::kPrerender) {
      // If type is prerender, `PrerendererImpl` triggers prefetch ahead
      // prerender. Ignore the corresponding UKM as it is checkid in
      // prerenderer_impl_browsertest.cc.
      ASSERT_EQ(attempts.size(), 2u);
      EXPECT_EQ(attempts[1], expected_attempt_entry)
          << test::ActualVsExpectedUkmEntryToString(attempts[1],
                                                    expected_attempt_entry);
    } else {
      ASSERT_EQ(attempts.size(), 1u);
      EXPECT_EQ(attempts[0], expected_attempt_entry)
          << test::ActualVsExpectedUkmEntryToString(attempts[0],
                                                    expected_attempt_entry);
    }

    test::PreloadingPredictionUkmEntryBuilder prediction_entry_builder(
        predictor());
    ukm::TestUkmRecorder::HumanReadableUkmEntry expected_prediction_entry =
        prediction_entry_builder.BuildEntry(source_id, confidence, accurate);
    std::vector<ukm::TestUkmRecorder::HumanReadableUkmEntry> predictions =
        ukm_recorder.GetEntries(
            ukm::builders::Preloading_Prediction::kEntryName,
            test::kPreloadingPredictionUkmMetrics);
    std::erase_if(predictions, [&](const auto& entry) {
      return entry.metrics.at(ukm::builders::Preloading_Prediction::
                                  kPreloadingPredictorName) !=
             predictor().ukm_value();
    });
    ASSERT_EQ(predictions.size(), 1u);
    EXPECT_EQ(predictions[0], expected_prediction_entry)
        << test::ActualVsExpectedUkmEntryToString(predictions[0],
                                                  expected_prediction_entry);
  }
}

IN_PROC_BROWSER_TEST_F(PreloadingDeciderBrowserTest,
                       EnactModerateNewTabPrerender) {
  base::HistogramTester histogram_tester;
  ASSERT_TRUE(NavigateToURL(
      shell(), GetTestURL("/preloading/preloading_decider_moderate.html")));
  ExpectCandidatesReceived();

  const GURL new_tab_url = GetTestURL("/title1.html?d");

  auto* preloading_decider = PreloadingDecider::GetOrCreateForCurrentDocument(
      web_contents()->GetPrimaryMainFrame());
  ASSERT_TRUE(preloading_decider);

  TestNavigationObserver nav_observer(new_tab_url);
  nav_observer.StartWatchingNewWebContents();

  // The pointerdown heuristic runs in the renderer, so drive it with real
  // input and wait for the resulting enactment.
  SimulateMouseDownOnElement("d");
  ASSERT_TRUE(WaitUntilEnacted(new_tab_url,
                               blink::mojom::SpeculationAction::kPrerender));

  EXPECT_TRUE(ExecJs(web_contents(), "document.getElementById('d').click()"));
  nav_observer.Wait();

  // Also navigate the current tab away, so any of its metrics are flushed.
  ASSERT_TRUE(NavigateToURL(shell(), GetTestURL("/title2.html")));

  {
    const std::string rule_predictor_str{
        content_preloading_predictor::kSpeculationRules.name()};
    const char* rule_predictor_cstr = rule_predictor_str.c_str();

    // We intentionally don't record a prediction for non-immediate speculation
    // rules. They aren't predictions per se, but a declaration to the browser
    // that preloading would be safe.
    histogram_tester.ExpectTotalCount(
        base::StringPrintf("Preloading.Predictor.%s.Precision",
                           rule_predictor_cstr),
        0);
    histogram_tester.ExpectUniqueSample(
        base::StringPrintf("Preloading.Predictor.%s.Recall",
                           rule_predictor_cstr),
        PredictorConfusionMatrix::kFalseNegative, 1);

    histogram_tester.ExpectUniqueSample(
        base::StringPrintf("Preloading.Prerender.Attempt.%s.Precision",
                           rule_predictor_cstr),
        PredictorConfusionMatrix::kTruePositive, 1);
    histogram_tester.ExpectUniqueSample(
        base::StringPrintf("Preloading.Prerender.Attempt.%s.Recall",
                           rule_predictor_cstr),
        PredictorConfusionMatrix::kTruePositive, 1);
  }

  {
    const std::string predictor_str{
        preloading_predictor::kUrlPointerDownOnAnchor.name()};
    const char* predictor_cstr = predictor_str.c_str();

    histogram_tester.ExpectUniqueSample(
        base::StringPrintf("Preloading.Predictor.%s.Precision", predictor_cstr),
        PredictorConfusionMatrix::kTruePositive, 1);
    histogram_tester.ExpectUniqueSample(
        base::StringPrintf("Preloading.Predictor.%s.Recall", predictor_cstr),
        PredictorConfusionMatrix::kTruePositive, 1);
    histogram_tester.ExpectUniqueSample(
        base::StringPrintf("Preloading.Prerender.Attempt.%s.Precision",
                           predictor_cstr),
        PredictorConfusionMatrix::kTruePositive, 1);
    histogram_tester.ExpectUniqueSample(
        base::StringPrintf("Preloading.Prerender.Attempt.%s.Recall",
                           predictor_cstr),
        PredictorConfusionMatrix::kTruePositive, 1);
  }
}

IN_PROC_BROWSER_TEST_F(PreloadingDeciderBrowserTest, PredictionWithoutAttempt) {
  base::HistogramTester histogram_tester;
  ASSERT_TRUE(NavigateToURL(
      shell(), GetTestURL("/preloading/preloading_decider_moderate.html")));
  ExpectCandidatesReceived();

  auto* preloading_decider = PreloadingDecider::GetOrCreateForCurrentDocument(
      web_contents()->GetPrimaryMainFrame());
  ASSERT_TRUE(preloading_decider);

  TestNavigationObserver nav_observer(web_contents());
  // No speculation rule covers this anchor, so the renderer has nothing to
  // enact and the browser records the prediction itself.
  preloading_decider->OnPointerDown(GetTestURL("/title1.html?a"),
                                    /*renderer_enacted=*/false);
  EXPECT_TRUE(ExecJs(web_contents(), "document.getElementById('a').click()"));
  nav_observer.Wait();

  {
    const std::string predictor_str{
        preloading_predictor::kUrlPointerDownOnAnchor.name()};
    const char* predictor_cstr = predictor_str.c_str();

    histogram_tester.ExpectUniqueSample(
        base::StringPrintf("Preloading.Predictor.%s.Precision", predictor_cstr),
        PredictorConfusionMatrix::kTruePositive, 1);
    histogram_tester.ExpectUniqueSample(
        base::StringPrintf("Preloading.Predictor.%s.Recall", predictor_cstr),
        PredictorConfusionMatrix::kTruePositive, 1);
    histogram_tester.ExpectTotalCount(
        base::StringPrintf("Preloading.Prerender.Attempt.%s.Precision",
                           predictor_cstr),
        0);
    histogram_tester.ExpectUniqueSample(
        base::StringPrintf("Preloading.Prerender.Attempt.%s.Recall",
                           predictor_cstr),
        PredictorConfusionMatrix::kFalseNegative, 1);
  }
}

}  // namespace content
