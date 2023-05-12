// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/preloading/preloading_decider.h"

#include "base/test/scoped_feature_list.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
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
    ResetUKM();
  }

  WebContents* web_contents() { return shell()->web_contents(); }
  void ResetUKM() {
    ukm_recorder_ = std::make_unique<ukm::TestAutoSetUkmRecorder>();
  }

  const GURL GetTestURL(const char* file) const {
    return https_server_->GetURL(file);
  }
  void NavigateTo(const GURL& url) {
    ASSERT_TRUE(NavigateToURL(shell(), url));
    base::RunLoop().RunUntilIdle();
  }

  void NavigateAway() {
    ASSERT_TRUE(NavigateToURL(shell(), GURL(url::kAboutBlankURL)));
    base::RunLoop().RunUntilIdle();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<net::EmbeddedTestServer> https_server_;
  std::unique_ptr<ukm::TestAutoSetUkmRecorder> ukm_recorder_;
};

IN_PROC_BROWSER_TEST_F(PreloadingDeciderBrowserTest,
                       SetIsNavigationInDomainCallback) {
  base::HistogramTester histogram_tester;
  NavigateTo(GetTestURL("/preloading/preloading_decider.html"));
  EXPECT_EQ(true, EvalJs(web_contents(),
                         "HTMLScriptElement.supports('speculationrules')"));
  base::RunLoop().RunUntilIdle();

  // Now navigate to another page
  EXPECT_TRUE(ExecJs(web_contents(),
                     R"(
    let bar = document.getElementById("bar");
    bar.click();
    )"));
  base::RunLoop().RunUntilIdle();
  histogram_tester.ExpectBucketCount(
      "Preloading.Predictor.SpeculationRules.Recall",
      /*content::PredictorConfusionMatrix::kFalseNegative*/ 3, 1);
  histogram_tester.ExpectBucketCount(
      "Preloading.Predictor.UrlPointerDownOnAnchor.Recall",
      /*content::PredictorConfusionMatrix::kFalseNegative*/ 3, 1);
  histogram_tester.ExpectBucketCount(
      "Preloading.Predictor.UrlPointerHoverOnAnchor.Recall",
      /*content::PredictorConfusionMatrix::kFalseNegative*/ 3, 1);
}

}  // namespace content
