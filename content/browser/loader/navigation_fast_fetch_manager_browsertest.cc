// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/loader/navigation_fast_fetch_manager.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "content/common/features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/content_mock_cert_verifier.h"
#include "content/public/test/prerender_test_util.h"
#include "content/shell/browser/shell.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "url/gurl.h"

namespace content {

class NavigationFastFetchManagerBrowserTest : public ContentBrowserTest {
 public:
  NavigationFastFetchManagerBrowserTest()
      : prerender_helper_(base::BindRepeating(
            &NavigationFastFetchManagerBrowserTest::web_contents,
            base::Unretained(this))) {
    feature_list_.InitAndEnableFeature(features::kNavigationFastFetchDryRun);
  }
  net::EmbeddedTestServer* https_server() { return &https_server_; }
  WebContents* web_contents() { return shell()->web_contents(); }
  test::PrerenderTestHelper& prerender_helper() { return prerender_helper_; }

 protected:
  void SetUpOnMainThread() override {
    ContentBrowserTest::SetUpOnMainThread();
    mock_cert_verifier_.mock_cert_verifier()->set_default_result(net::OK);
    https_server()->AddDefaultHandlers(GetTestDataFilePath());
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    ContentBrowserTest::SetUpCommandLine(command_line);
    mock_cert_verifier_.SetUpCommandLine(command_line);
  }

  void SetUpInProcessBrowserTestFixture() override {
    ContentBrowserTest::SetUpInProcessBrowserTestFixture();
    mock_cert_verifier_.SetUpInProcessBrowserTestFixture();
  }

  void TearDownInProcessBrowserTestFixture() override {
    ContentBrowserTest::TearDownInProcessBrowserTestFixture();
    mock_cert_verifier_.TearDownInProcessBrowserTestFixture();
  }

 private:
  test::PrerenderTestHelper prerender_helper_;
  base::test::ScopedFeatureList feature_list_;
  content::ContentMockCertVerifier mock_cert_verifier_;
  net::EmbeddedTestServer https_server_{net::EmbeddedTestServer::TYPE_HTTPS};
};

IN_PROC_BROWSER_TEST_F(NavigationFastFetchManagerBrowserTest,
                       IneligibleServiceWorker) {
  ASSERT_TRUE(https_server()->Start());

  // Navigate to an initial page under the service worker scope.
  GURL empty_url = https_server()->GetURL("/service_worker/empty.html");
  ASSERT_TRUE(NavigateToURL(shell(), empty_url));

  // Register a service worker.
  const std::string register_script =
      "navigator.serviceWorker.register('/service_worker/empty.js').then("
      "  () => { return 'success'; },"
      "  (err) => { return 'fail: ' + err; }"
      ");";
  EXPECT_EQ("success", EvalJs(shell(), register_script));

  // Wait for the service worker to become active.
  const std::string wait_script =
      "navigator.serviceWorker.ready.then(() => 'ready');";
  EXPECT_EQ("ready", EvalJs(shell(), wait_script));

  // Navigate away to about:blank to clean up navigation state.
  ASSERT_TRUE(NavigateToURL(shell(), GURL("about:blank")));

  // Start recording histograms.
  base::HistogramTester histogram_tester;

  // Navigate back to the page under the service worker scope.
  ASSERT_TRUE(NavigateToURL(shell(), empty_url));

  // Verify that EligibilityReason::kHasServiceWorker was recorded.
  histogram_tester.ExpectBucketCount(
      "Navigation.Experimental.FastFetch.EligibilityReason",
      NavigationFastFetchManager::EligibilityReason::kHasServiceWorker, 1);
}

IN_PROC_BROWSER_TEST_F(NavigationFastFetchManagerBrowserTest,
                       PrerenderIneligible) {
  ASSERT_TRUE(https_server()->Start());

  // Navigate to an initial page.
  GURL initial_url = https_server()->GetURL("/empty.html");
  ASSERT_TRUE(NavigateToURL(shell(), initial_url));

  // Start recording histograms.
  base::HistogramTester histogram_tester;

  // Trigger a prerender navigation.
  GURL prerender_url = https_server()->GetURL("/title1.html");
  prerender_helper().AddPrerender(prerender_url);

  // Verify that EligibilityReason::kIsPrerender was recorded.
  histogram_tester.ExpectBucketCount(
      "Navigation.Experimental.FastFetch.EligibilityReason",
      NavigationFastFetchManager::EligibilityReason::kIsPrerender, 1);

  // Activate the prerender.
  prerender_helper().NavigatePrimaryPage(prerender_url);

  // Verify that EligibilityReason::kIsPrerenderActivation was recorded.
  histogram_tester.ExpectBucketCount(
      "Navigation.Experimental.FastFetch.EligibilityReason",
      NavigationFastFetchManager::EligibilityReason::kIsPrerenderActivation, 1);
}

}  // namespace content
