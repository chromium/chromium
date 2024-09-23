// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "content/test/content_browser_test_utils_internal.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/controllable_http_response.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/loader/background_resource_fetch_histograms.h"

namespace content {
namespace {

using testing::ElementsAre;

// TODO(crbug.com/40244488): When we will add VirtualTestSuites of web tests for
// BackgroundResourceFetch feature, duplicated basic tests should be deleted
// from BackgroundResourceFetchBrowserTest.
class BackgroundResourceFetchBrowserTest : public ContentBrowserTest {
 public:
  BackgroundResourceFetchBrowserTest() {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/
        {blink::features::kBackgroundResourceFetch,
         // Needed to trigger cache-aware loading
         blink::features::kWebFontsCacheAwareTimeoutAdaption},
        /*disabled_features=*/{});
  }
  BackgroundResourceFetchBrowserTest(
      const BackgroundResourceFetchBrowserTest&) = delete;
  BackgroundResourceFetchBrowserTest& operator=(
      const BackgroundResourceFetchBrowserTest&) = delete;

  ~BackgroundResourceFetchBrowserTest() override = default;

 protected:
  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
  }

  WebContentsImpl* web_contents() const {
    return static_cast<WebContentsImpl*>(shell()->web_contents());
  }
  RenderFrameHostImpl* current_frame_host() {
    return web_contents()->GetPrimaryFrameTree().root()->current_frame_host();
  }

  void StartServerAndNavigateToTestPage() {
    ASSERT_TRUE(embedded_test_server()->Start());
    EXPECT_TRUE(
        NavigateToURL(shell(), embedded_test_server()->GetURL("/hello.html")));
  }

  void TestScriptLoad(const std::string& path, bool expect_success) {
    const std::string script = JsReplace(R"(
      new Promise(resolve => {
          window.resolveTest = resolve;
          const script = document.createElement('script');
          script.src = $1;
          script.addEventListener('error', () => {resolve('load error')});
          document.body.appendChild(script);
        });
    )",
                                         path);
    EXPECT_EQ(expect_success ? "script executed" : "load error",
              EvalJs(shell(), script));
  }

  void TestFontLoad(const std::string& path, bool expect_success) {
    const std::string script = JsReplace(R"(
      new Promise(resolve => {
          const fontface =
              new FontFace('CustomFont', $1);
          document.fonts.add(fontface);
          fontface.load()
            .then(() => {resolve('font loaded');})
            .catch(() => {resolve('load error');});
        });
    )",
                                         path);
    EXPECT_EQ(expect_success ? "font loaded" : "load error",
              EvalJs(shell(), script));
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(BackgroundResourceFetchBrowserTest, ScriptLoad) {
  StartServerAndNavigateToTestPage();
  base::HistogramTester histograms;
  // Test normal script load behavior.
  TestScriptLoad("background_resource_fetch/test.js", /*expect_success=*/true);
  FetchHistogramsFromChildProcesses();
  EXPECT_THAT(histograms.GetAllSamples(
                  blink::kBackgroundResourceFetchSupportStatusHistogramName),
              ElementsAre(base::Bucket(
                  blink::BackgroundResourceFetchSupportStatus::kSupported, 1)));
}

IN_PROC_BROWSER_TEST_F(BackgroundResourceFetchBrowserTest, ScriptLoadFailure) {
  StartServerAndNavigateToTestPage();
  // Test script load failure.
  TestScriptLoad("background_resource_fetch/404.js", /*expect_success=*/false);
}

IN_PROC_BROWSER_TEST_F(BackgroundResourceFetchBrowserTest,
                       ScriptLoadFailureAfterResponse) {
  StartServerAndNavigateToTestPage();
  // Test script load failure behavior after receiving a response header.
  TestScriptLoad("background_resource_fetch/invalid-chunked-encoding.js",
                 /*expect_success=*/false);
}

IN_PROC_BROWSER_TEST_F(BackgroundResourceFetchBrowserTest, ScriptLoadTwice) {
  StartServerAndNavigateToTestPage();
  base::HistogramTester histograms;

  // Test normal script load behavior.
  TestScriptLoad("background_resource_fetch/test.js?1",
                 /*expect_success=*/true);
  // Test normal script load behavior again.
  TestScriptLoad("background_resource_fetch/test.js?2",
                 /*expect_success=*/true);

  FetchHistogramsFromChildProcesses();
  EXPECT_THAT(histograms.GetAllSamples(
                  blink::kBackgroundResourceFetchSupportStatusHistogramName),
              ElementsAre(base::Bucket(
                  blink::BackgroundResourceFetchSupportStatus::kSupported, 2)));
}

IN_PROC_BROWSER_TEST_F(BackgroundResourceFetchBrowserTest, ScriptLoadStop) {
  StartServerAndNavigateToTestPage();
  // window.stop() should cause script load error.
  EXPECT_EQ("load error", EvalJs(shell(), R"(
        new Promise(resolve => {
            window.resolveTest = resolve;
            const script = document.createElement('script');
            script.src = 'background_resource_fetch/test.js';
            script.addEventListener('error', () => {resolve('load error')});
            document.body.appendChild(script);
            window.stop();
          });
      )"));
}

IN_PROC_BROWSER_TEST_F(BackgroundResourceFetchBrowserTest,
                       NavigationWhileLoadingScript) {
  StartServerAndNavigateToTestPage();
  EXPECT_TRUE(ExecJs(shell(), R"(
        (() => {
            const script = document.createElement('script');
            script.src = 'slow?10';
            document.body.appendChild(script);
          })();
      )"));

  GURL new_url(embedded_test_server()->GetURL("/hello.html?2"));
  EXPECT_TRUE(ExecJs(shell(), JsReplace("location.replace($1)", new_url)));
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
}

IN_PROC_BROWSER_TEST_F(BackgroundResourceFetchBrowserTest,
                       UnupportedSyncRequest) {
  StartServerAndNavigateToTestPage();
  base::HistogramTester histograms;

  // Fetch empty.html using sync XMLHttpRequest.
  EXPECT_EQ(200, EvalJs(shell(), R"(
        (() => {
          const request = new XMLHttpRequest();
          request.open('GET', '/hello.html', false);
          request.send(null);
          return request.status;
        })();
      )"));

  FetchHistogramsFromChildProcesses();

  // Sync XMLHttpRequest is not supported. So the kUnsupportedSyncRequest UMA
  // must have been recorded.
  EXPECT_THAT(
      histograms.GetAllSamples(
          blink::kBackgroundResourceFetchSupportStatusHistogramName),
      ElementsAre(base::Bucket(
          blink::BackgroundResourceFetchSupportStatus::kUnsupportedSyncRequest,
          1)));
}

IN_PROC_BROWSER_TEST_F(BackgroundResourceFetchBrowserTest,
                       UnupportedNonGetRequest) {
  StartServerAndNavigateToTestPage();
  base::HistogramTester histograms;

  // Fetch empty.html using POST method.
  EXPECT_EQ("", EvalJs(shell(), R"(
        new Promise(async (resolve) => {
            const res = await fetch('empty.html',
                                    {method: 'POST', body: 'body'});
            resolve(await res.text());
          });
      )"));

  FetchHistogramsFromChildProcesses();

  // POST method is not supported. So the kUnsupportedNonGetRequest UMA must
  // have been recorded.
  EXPECT_THAT(
      histograms.GetAllSamples(
          blink::kBackgroundResourceFetchSupportStatusHistogramName),
      ElementsAre(base::Bucket(blink::BackgroundResourceFetchSupportStatus::
                                   kUnsupportedNonGetRequest,
                               1)));
}

IN_PROC_BROWSER_TEST_F(BackgroundResourceFetchBrowserTest,
                       UnupportedNonHttpUrlRequest) {
  StartServerAndNavigateToTestPage();
  base::HistogramTester histograms;

  // Fetch chrome-untrusted://example.com/.
  EXPECT_EQ("failed", EvalJs(shell(), R"(
        new Promise(async (resolve) => {
            try {
              await fetch('chrome-untrusted://example.com/');
            } catch (e) {
              resolve('failed');
            }
          });
      )"));

  FetchHistogramsFromChildProcesses();

  // Non HTTP url request not supported. So the UnupportedNonHttpUrlRequest UMA
  // must have been recorded.
  EXPECT_THAT(
      histograms.GetAllSamples(
          blink::kBackgroundResourceFetchSupportStatusHistogramName),
      ElementsAre(base::Bucket(blink::BackgroundResourceFetchSupportStatus::
                                   kUnsupportedNonHttpUrlRequest,
                               1)));
}

IN_PROC_BROWSER_TEST_F(BackgroundResourceFetchBrowserTest,
                       UnupportedKeepAliveRequest) {
  StartServerAndNavigateToTestPage();
  base::HistogramTester histograms;

  // Fetch empty.html with {keepalive: true}.
  EXPECT_EQ("", EvalJs(shell(), R"(
        new Promise(async (resolve) => {
            const res = await fetch('empty.html', {keepalive: true});
            resolve(await res.text());
          });
      )"));

  FetchHistogramsFromChildProcesses();

  // POST method is not supported. So the UnupportedKeepAliveRequest UMA must
  // have been recorded.
  EXPECT_THAT(
      histograms.GetAllSamples(
          blink::kBackgroundResourceFetchSupportStatusHistogramName),
      ElementsAre(base::Bucket(blink::BackgroundResourceFetchSupportStatus::
                                   kUnsupportedKeepAliveRequest,
                               1)));
}

IN_PROC_BROWSER_TEST_F(BackgroundResourceFetchBrowserTest, FontLoad) {
  StartServerAndNavigateToTestPage();
  // Test font loading behavior that triggers cache-aware loading when
  // WebFontsCacheAwareTimeoutAdaption feature is enabled.
  TestFontLoad("url(background_resource_fetch/SpaceOnly.otf)",
               /*expect_success=*/true);
}

IN_PROC_BROWSER_TEST_F(BackgroundResourceFetchBrowserTest, FontLoadFailure) {
  StartServerAndNavigateToTestPage();
  // Test font loading failure behavior that triggers cache-aware loading when
  // WebFontsCacheAwareTimeoutAdaption feature is enabled.
  TestFontLoad("url(background_resource_fetch/not_found.otf)",
               /*expect_success=*/false);
}

IN_PROC_BROWSER_TEST_F(BackgroundResourceFetchBrowserTest,
                       ScriptLoadAfterCrossOriginSameSiteNavigation) {
  StartServerAndNavigateToTestPage();
  GURL url(embedded_test_server()->GetURL("example.com", "/hello.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url));

  // Test normal script load behavior.
  TestScriptLoad("background_resource_fetch/test.js", /*expect_success=*/true);

  // Navigate to a cross origin same site page. This is intended to check that
  // `background_resource_fetch_context_` which is attached to the
  // RenderFrameImpl is replaced. If `background_resource_fetch_context_` is not
  // replaced, the network request after the navigation will cause the initiator
  // lock compatibility check failure in the network process.
  GURL new_url(
      embedded_test_server()->GetURL("sub.example.com", "/hello.html"));
  EXPECT_TRUE(ExecJs(shell(), JsReplace("location.replace($1)", new_url)));
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));

  // Test normal script load behavior after navigation.
  TestScriptLoad("background_resource_fetch/test.js", /*expect_success=*/true);
}

IN_PROC_BROWSER_TEST_F(BackgroundResourceFetchBrowserTest, Redirect) {
  StartServerAndNavigateToTestPage();
  EXPECT_EQ("Redirected", EvalJs(shell(), R"(
        new Promise(async (resolve) => {
            const res = await fetch('/background_resource_fetch/redirect');
            resolve(await res.text());
          });
      )"));
}

IN_PROC_BROWSER_TEST_F(BackgroundResourceFetchBrowserTest, RedirectRejected) {
  StartServerAndNavigateToTestPage();
  EXPECT_EQ("fetch failed", EvalJs(shell(), R"(
        new Promise(async (resolve) => {
            try {
              await fetch('/background_resource_fetch/redirect',
                          {redirect: 'error'});
            } catch (e) {
              resolve('fetch failed')
            }
          });
      )"));
}

}  // namespace
}  // namespace content
