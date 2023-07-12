// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>
#include <string>

#include "base/test/scoped_feature_list.h"
#include "build/buildflag.h"
#include "content/public/browser/visibility.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "services/network/public/cpp/features.h"
#include "url/gurl.h"

namespace content {

namespace {

class ResourceSchedulerBrowserTest : public ContentBrowserTest {
 public:
  ResourceSchedulerBrowserTest(const ResourceSchedulerBrowserTest&) = delete;
  ResourceSchedulerBrowserTest& operator=(const ResourceSchedulerBrowserTest&) =
      delete;

 protected:
  ResourceSchedulerBrowserTest() {}
  ~ResourceSchedulerBrowserTest() override {}

  void SetUpInProcessBrowserTestFixture() override {
    ASSERT_TRUE(embedded_test_server()->Start());
  }
};

IN_PROC_BROWSER_TEST_F(ResourceSchedulerBrowserTest,
                       DISABLED_ResourceLoadingExperimentIncognito) {
  GURL url(embedded_test_server()->GetURL(
      "/resource_loading/resource_loading_non_mobile.html"));

  Shell* otr_browser = CreateOffTheRecordBrowser();
  EXPECT_TRUE(NavigateToURL(otr_browser, url));
  EXPECT_EQ(9, EvalJs(otr_browser, "getResourceNumber()"));
}

IN_PROC_BROWSER_TEST_F(ResourceSchedulerBrowserTest,
                       DISABLED_ResourceLoadingExperimentNormal) {
  GURL url(embedded_test_server()->GetURL(
      "/resource_loading/resource_loading_non_mobile.html"));
  Shell* browser = shell();
  EXPECT_TRUE(NavigateToURL(browser, url));
  EXPECT_EQ(9, EvalJs(browser, "getResourceNumber()"));
}

// The following test doesn't work on Android or iOS.
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
class VisibilityAwareResourceSchedulerBrowserTest : public ContentBrowserTest {
 public:
  // TODO(https://crbug.com/1457817): Avoid relying on this histogram to test
  // whether a request is deprioritized or not. Tests would not work if the
  // histogram expired.
  static constexpr char kDeprioritizedHistogramName[] =
      "Network.VisibilityAwareResourceScheduler.Deprioritized";

  VisibilityAwareResourceSchedulerBrowserTest() {
    feature_list_.InitAndEnableFeature(
        network::features::kVisibilityAwareResourceScheduler);
  }
  ~VisibilityAwareResourceSchedulerBrowserTest() override = default;

  void SetUp() override {
    ASSERT_TRUE(embedded_test_server()->Start());
    ContentBrowserTest::SetUp();
  }

  bool FetchScript(GURL url) {
    EvalJsResult result = EvalJs(shell(), JsReplace(R"(
      new Promise(resolve => {
        const script = document.createElement("script");
        script.src = $1;
        script.onerror = () => resolve("blocked");
        script.onload = () => resolve("fetched");
        document.body.appendChild(script);
      });
    )",
                                                    url));
    return result.ExtractString() == "fetched";
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(VisibilityAwareResourceSchedulerBrowserTest, Simple) {
  base::HistogramTester histograms;

  GURL url(embedded_test_server()->GetURL("/title1.html"));
  ASSERT_TRUE(NavigateToURL(shell(), url));

  shell()->web_contents()->UpdateWebContentsVisibility(Visibility::HIDDEN);
  ASSERT_TRUE(FetchScript(embedded_test_server()->GetURL("/empty-script.js")));
  FetchHistogramsFromChildProcesses();
  // Navigation is not deprioritized, a script fetch is deprioritized.
  EXPECT_THAT(histograms.GetAllSamples(kDeprioritizedHistogramName),
              testing::ElementsAre(base::Bucket(0, 1), base::Bucket(1, 1)));

  shell()->web_contents()->UpdateWebContentsVisibility(Visibility::VISIBLE);
  ASSERT_TRUE(FetchScript(embedded_test_server()->GetURL("/empty-script.js")));
  FetchHistogramsFromChildProcesses();
  // Navigation is not deprioritized, the first script fetch is deprioritized,
  // and the second script fetch is not deprioritized.
  EXPECT_THAT(histograms.GetAllSamples(kDeprioritizedHistogramName),
              testing::ElementsAre(base::Bucket(0, 2), base::Bucket(1, 1)));
}
#endif  // !BUILDFLAG(IS_ANDROID)

}  // anonymous namespace

}  // namespace content
