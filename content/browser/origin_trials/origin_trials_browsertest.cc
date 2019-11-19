// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/strcat.h"
#include "base/test/bind_test_util.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/url_loader_interceptor.h"
#include "content/shell/browser/shell.h"
#include "content/shell/common/shell_switches.h"

using content::URLLoaderInterceptor;

namespace {
constexpr char kBaseDataDir[] = "content/test/data/origin_trials/";

void NavigateViaRenderer(content::WebContents* web_contents, const GURL& url) {
  EXPECT_TRUE(
      content::ExecJs(web_contents->GetMainFrame(),
                      base::StrCat({"location.href='", url.spec(), "';"})));
  // Enqueue a no-op script execution, which will block until the navigation
  // initiated above completes.
  EXPECT_TRUE(content::ExecJs(web_contents->GetMainFrame(), "true"));
  EXPECT_TRUE(content::WaitForLoadStop(web_contents));
  EXPECT_EQ(web_contents->GetLastCommittedURL(), url);
}

}  // namespace

namespace content {

class OriginTrialsBrowserTest : public content::ContentBrowserTest {
 public:
  OriginTrialsBrowserTest() : ContentBrowserTest() {}
  ~OriginTrialsBrowserTest() override {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    ContentBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kExposeInternalsForTesting);
  }

  void SetUpOnMainThread() override {
    ContentBrowserTest::SetUpOnMainThread();

    // We use a URLLoaderInterceptor, rather than the EmbeddedTestServer, since
    // the origin trial token in the response is associated with a fixed
    // origin, whereas EmbeddedTestServer serves content on a random port.
    url_loader_interceptor_ =
        std::make_unique<URLLoaderInterceptor>(base::BindLambdaForTesting(
            [&](URLLoaderInterceptor::RequestParams* params) -> bool {
              URLLoaderInterceptor::WriteResponse(
                  base::StrCat(
                      {kBaseDataDir, params->url_request.url.path_piece()}),
                  params->client.get());
              return true;
            }));
  }

  void TearDownOnMainThread() override {
    url_loader_interceptor_.reset();
    ContentBrowserTest::TearDownOnMainThread();
  }

 private:
  std::unique_ptr<URLLoaderInterceptor> url_loader_interceptor_;

  DISALLOW_COPY_AND_ASSIGN(OriginTrialsBrowserTest);
};

IN_PROC_BROWSER_TEST_F(OriginTrialsBrowserTest, Basic) {
  EXPECT_TRUE(NavigateToURL(shell(), GURL("https://example.test/basic.html")));
  // Ensure we can invoke normalMethod(), which is only available when the
  // Frobulate OT is enabled.
  EXPECT_TRUE(content::ExecJs(shell()->web_contents()->GetMainFrame(),
                              "internals.originTrialsTest().normalMethod();"));
}

IN_PROC_BROWSER_TEST_F(OriginTrialsBrowserTest,
                       NonNavigationTrialNotActivatedAcrossNavigations) {
  EXPECT_TRUE(NavigateToURL(shell(), GURL("https://example.test/basic.html")));
  EXPECT_TRUE(content::ExecJs(shell()->web_contents()->GetMainFrame(),
                              "internals.originTrialsTest().normalMethod();"));
  NavigateViaRenderer(shell()->web_contents(),
                      GURL("https://other.test/notrial.html"));
  EXPECT_TRUE(content::ExecJs(shell()->web_contents()->GetMainFrame(),
                              "internals.originTrialsTest();"));
  EXPECT_FALSE(content::ExecJs(shell()->web_contents()->GetMainFrame(),
                               "internals.originTrialsTest().normalMethod();"));
}

IN_PROC_BROWSER_TEST_F(OriginTrialsBrowserTest, Navigation) {
  EXPECT_TRUE(
      NavigateToURL(shell(), GURL("https://example.test/navigation.html")));
  // Ensure we can invoke navigationMethod(), which is only available when the
  // FrobulateNavigation OT is enabled.
  EXPECT_TRUE(
      content::ExecJs(shell()->web_contents()->GetMainFrame(),
                      "internals.originTrialsTest().navigationMethod();"));
}

IN_PROC_BROWSER_TEST_F(OriginTrialsBrowserTest,
                       NavigationTrialActivatedAcrossNavigations) {
  EXPECT_TRUE(
      NavigateToURL(shell(), GURL("https://example.test/navigation.html")));
  EXPECT_TRUE(
      content::ExecJs(shell()->web_contents()->GetMainFrame(),
                      "internals.originTrialsTest().navigationMethod();"));

  NavigateViaRenderer(shell()->web_contents(),
                      GURL("https://other.test/notrial.html"));
  // Ensure we can invoke navigationMethod() after having navigated from
  // navigation.html, since navigationMethod() is exposed via a cross-navigation
  // OT.
  EXPECT_TRUE(
      content::ExecJs(shell()->web_contents()->GetMainFrame(),
                      "internals.originTrialsTest().navigationMethod();"));

  NavigateViaRenderer(shell()->web_contents(),
                      GURL("https://other.test/basic.html"));
  // Ensure we can't invoke navigationMethod() after a second navigation, as
  // cross-navigation OTs should only be forwarded to immediate navigations from
  // where the trial was activated.
  EXPECT_FALSE(
      content::ExecJs(shell()->web_contents()->GetMainFrame(),
                      "internals.originTrialsTest().navigationMethod();"));
}

}  // namespace content
