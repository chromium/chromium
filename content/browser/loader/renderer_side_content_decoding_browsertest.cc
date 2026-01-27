// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Browser tests for the RendererSideContentDecoding feature, specifically
// testing scenarios involving successful decoding and simulated failures during
// data pipe creation.

#include <string_view>

#include "base/test/scoped_feature_list.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "services/network/public/cpp/features.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

// Test fixture for RendererSideContentDecoding browser tests.
// Parameterized to run tests in two modes:
// - ExpectSuccess (true): Normal operation, Mojo data pipe creation succeeds.
// - ExpectFail (false): Simulates Mojo data pipe creation failure using the
//   `RendererSideContentDecodingForceMojoFailureForTesting` feature parameter.
class RendererSideContentDecodingBrowserTest
    : public ContentBrowserTest,
      public ::testing::WithParamInterface<bool> {
 public:
  RendererSideContentDecodingBrowserTest() {
    // Enable the main feature and optionally the failure simulation parameter
    // based on the test parameter (`GetParam()`).
    features_.InitWithFeaturesAndParameters(
        {{network::features::kRendererSideContentDecoding,
          {{"RendererSideContentDecodingForceMojoFailureForTesting",
            ShouldSucceed() ? "false" : "true"}}}},
        {});
  }
  ~RendererSideContentDecodingBrowserTest() override = default;

  // Provides human-readable names for the test parameter values.
  static std::string DescribeParams(
      const testing::TestParamInfo<ParamType>& info) {
    return info.param ? "ExpectSuccess" : "ExpectFail";
  }

 protected:
  // Starts the embedded test server and navigates the shell to a simple page.
  void StartServerAndNavigateToTestPage() {
    ASSERT_TRUE(embedded_test_server()->Start());
    EXPECT_TRUE(
        NavigateToURL(shell(), embedded_test_server()->GetURL("/hello.html")));
  }

  // Returns true if the current test parameter expects success (no simulated
  // failure), false otherwise.
  bool ShouldSucceed() const { return GetParam(); }

 private:
  base::test::ScopedFeatureList features_;
};

INSTANTIATE_TEST_SUITE_P(
    /* No prefix */,
    RendererSideContentDecodingBrowserTest,
    // Run with both true (ExpectSuccess) and false (ExpectFail).
    ::testing::Bool(),
    &RendererSideContentDecodingBrowserTest::DescribeParams);

// Tests loading a compressed Javascript file (.js.gz).
// In ExpectSuccess mode, the script should execute.
// In ExpectFail mode, the script load should fail, triggering the 'error'
// event, and a specific error message should be logged to the console.
IN_PROC_BROWSER_TEST_P(RendererSideContentDecodingBrowserTest,
                       CompressedScriptLoad) {
  StartServerAndNavigateToTestPage();
  // Watch for console errors related to resource loading.
  DevToolsInspectorLogWatcher log_watcher(shell()->web_contents());
  const std::string_view script = R"(
    new Promise(resolve => {
        window.resolveTest = resolve;
        const script = document.createElement('script');
        script.src = './loader/compressed.js.gz';
        script.addEventListener('error', () => {resolve('load error')});
        document.body.appendChild(script);
      });
  )";
  // compressed.js.gz contains: window.resolveTest('script executed');
  EXPECT_EQ(ShouldSucceed() ? "script executed" : "load error",
            EvalJs(shell(), script));

  log_watcher.FlushAndStopWatching();
  // Verify the console message in case of failure.
  EXPECT_EQ(log_watcher.last_message(),
            ShouldSucceed()
                ? ""
                : "Failed to load resource: net::ERR_INSUFFICIENT_RESOURCES");
}

// Tests navigating to a compressed HTML file (.html.gz).
// In ExpectSuccess mode, the navigation should complete, and the page title
// should be set correctly.
// In ExpectFail mode, the navigation should fail, and a specific error message
// ("Failed to decode content") should be logged to the console by
// RenderFrameImpl.
IN_PROC_BROWSER_TEST_P(RendererSideContentDecodingBrowserTest,
                       CompressedPageNavigation) {
  StartServerAndNavigateToTestPage();
  // compressed.html.gz contains: <title>page loaded</title>
  std::u16string expected_title(u"page loaded");
  TitleWatcher title_watcher(shell()->web_contents(), expected_title);

  // Watch for the specific console error message logged on navigation failure.
  WebContentsConsoleObserver console_observer(shell()->web_contents());
  console_observer.SetPattern("Failed to decode content");

  // Initiate navigation to the compressed HTML file.
  EXPECT_TRUE(ExecJs(shell(), JsReplace("window.location.href = $1",
                                        "./loader/compressed.html.gz")));

  if (ShouldSucceed()) {
    // Wait for the title change to confirm successful navigation and decoding.
    EXPECT_EQ(expected_title, title_watcher.WaitAndGetTitle());
  } else {
    // Wait for the expected console error message.
    ASSERT_TRUE(console_observer.Wait());
  }
}

}  // namespace content
