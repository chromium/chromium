// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/preloading/preloading_decider.h"

#include "base/test/scoped_feature_list.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/browser/preloading/preloading_data_impl.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/preloading_test_util.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/shell/browser/shell.h"
#include "net/dns/mock_host_resolver.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"

namespace content {

class PreloadingDeciderBrowserTest : public ContentBrowserTest {
 public:
  PreloadingDeciderBrowserTest() = default;
  ~PreloadingDeciderBrowserTest() override = default;

  void SetUp() override {
    feature_list_.InitWithFeaturesAndParameters(
        {
            {blink::features::kSpeculationRulesPointerDownHeuristics, {}},
            {blink::features::kSpeculationRulesPointerHoverHeuristics, {}},
        },
        {});

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
    EXPECT_EQ(true, EvalJsAfterLifecycleUpdate(web_contents(), "", "true"));
    auto* preloading_decider = PreloadingDecider::GetOrCreateForCurrentDocument(
        web_contents()->GetPrimaryMainFrame());
    ASSERT_TRUE(preloading_decider);
    EXPECT_TRUE(preloading_decider->HasCandidatesForTesting());
  }

 private:
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
}

}  // namespace content
