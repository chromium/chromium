// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actor/tools/model/page_stability_java_script_feature.h"

#import "base/files/file_path.h"
#import "base/path_service.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "base/test/metrics/histogram_tester.h"
#import "base/test/scoped_feature_list.h"
#import "base/time/time_delta_from_string.h"
#import "ios/chrome/browser/intelligence/actor/tools/model/actor_tool_java_script_feature_test_base.h"
#import "ios/chrome/browser/intelligence/features/features.h"
#import "ios/web/public/test/js_test_util.h"
#import "ios/web/public/test/web_view_interaction_test_util.h"
#import "ios/web/public/web_state.h"
#import "net/test/embedded_test_server/embedded_test_server.h"
#import "testing/gtest/include/gtest/gtest.h"

namespace actor {

class PageStabilityJavaScriptFeatureTest
    : public ActorToolJavaScriptFeatureTestBase {
 protected:
  std::string kPageStabilityIntervalDurationParam = "300ms";
  base::TimeDelta kPageStabilityIntervalDuration =
      base::TimeDeltaFromString(kPageStabilityIntervalDurationParam).value();
  std::string kPageStabilityThrottleThresholdParam = "100ms";
  base::TimeDelta kPageStabilityThrottleThreshold =
      base::TimeDeltaFromString(kPageStabilityThrottleThresholdParam).value();

  PageStabilityJavaScriptFeatureTest()
      : test_server_(net::EmbeddedTestServer::TYPE_HTTP) {
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        kPageStabilityMetrics,
        {{"PageStabilityIntervalDuration", kPageStabilityIntervalDurationParam},
         {"PageStabilityThrottleThreshold",
          kPageStabilityThrottleThresholdParam}});
  }

  void SetUp() override {
    ActorToolJavaScriptFeatureTestBase::SetUp();
    test_server_.ServeFilesFromSourceDirectory(
        base::FilePath("ios/testing/data/http_server_files/"));
    ASSERT_TRUE(test_server_.Start());
  }

  void LoadTestPage() {
    ASSERT_TRUE(LoadUrl(test_server_.GetURL("/actor/page_stability.html")));
    ASSERT_NE(WaitForMainFrame(feature()), nullptr);
  }

  PageStabilityJavaScriptFeature* feature() {
    return PageStabilityJavaScriptFeature::GetInstance();
  }

  net::EmbeddedTestServer test_server_;
  base::test::ScopedFeatureList scoped_feature_list_;
  base::HistogramTester histogram_tester_;
};

TEST_F(PageStabilityJavaScriptFeatureTest, RecordsMutationMetrics) {
  LoadTestPage();
  ASSERT_NE(WaitForMainFrame(feature()), nullptr);

  web::test::TapWebViewElementWithId(web_state(), "mutate");

  base::test::ios::SpinRunLoopWithMinDelay(kPageStabilityIntervalDuration +
                                           base::Milliseconds(100));
  histogram_tester_.ExpectBucketCount(
      "IOS.Actor.PageStability.InteractionMetrics.MutationCount", 1, 1);
  histogram_tester_.ExpectTotalCount(
      "IOS.Actor.PageStability.InteractionMetrics.TimeToFirstMutation", 1);
  histogram_tester_.ExpectTotalCount(
      "IOS.Actor.PageStability.InteractionMetrics.TimeToLastMutation", 1);
}

TEST_F(PageStabilityJavaScriptFeatureTest, RecordsZeroCountForNoMutations) {
  LoadTestPage();

  web::test::TapWebViewElementWithId(web_state(), "benign");

  base::test::ios::SpinRunLoopWithMinDelay(kPageStabilityIntervalDuration +
                                           base::Milliseconds(100));
  histogram_tester_.ExpectBucketCount(
      "IOS.Actor.PageStability.InteractionMetrics.MutationCount", 0, 1);
  histogram_tester_.ExpectTotalCount(
      "IOS.Actor.PageStability.InteractionMetrics.TimeToFirstMutation", 0);
  histogram_tester_.ExpectTotalCount(
      "IOS.Actor.PageStability.InteractionMetrics.TimeToLastMutation", 0);
}

TEST_F(PageStabilityJavaScriptFeatureTest,
       RecordsCumulativeCountForSustainedMutations) {
  LoadTestPage();

  // Make any interaction with the page trigger 5 mutations over 200ms.
  web::test::ExecuteJavaScript(
      web_state(), "window.sustainedMutations = {duration: 200, count: 5};");
  web::test::TapWebViewElementWithId(web_state(), "mutate");

  base::test::ios::SpinRunLoopWithMinDelay(kPageStabilityIntervalDuration +
                                           base::Milliseconds(100));
  histogram_tester_.ExpectBucketCount(
      "IOS.Actor.PageStability.InteractionMetrics.MutationCount", 5, 1);
  histogram_tester_.ExpectTotalCount(
      "IOS.Actor.PageStability.InteractionMetrics.TimeToFirstMutation", 1);
  histogram_tester_.ExpectTotalCount(
      "IOS.Actor.PageStability.InteractionMetrics.TimeToLastMutation", 1);
}

TEST_F(PageStabilityJavaScriptFeatureTest,
       RapidInteractionsAreThrottledToOneWindow) {
  LoadTestPage();

  // Trigger 5 interactions in rapid succession. They should be throttled but
  // the total mutation count is still recorded.
  web::test::ExecuteJavaScriptForFeature(
      web_state(),
      @"for (let i = 0; i < 5; i++) {"
       "  setTimeout(() => {"
       "    document.getElementById('mutate').click();"
       "  }, i * 20);"
       "}",
      feature());

  base::test::ios::SpinRunLoopWithMinDelay(kPageStabilityIntervalDuration +
                                           base::Milliseconds(100));
  histogram_tester_.ExpectBucketCount(
      "IOS.Actor.PageStability.InteractionMetrics.MutationCount", 5, 1);
  histogram_tester_.ExpectTotalCount(
      "IOS.Actor.PageStability.InteractionMetrics.TimeToFirstMutation", 1);
  histogram_tester_.ExpectTotalCount(
      "IOS.Actor.PageStability.InteractionMetrics.TimeToLastMutation", 1);
}

}  // namespace actor
