// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/strcat.h"
#include "base/test/bind.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/url_loader_interceptor.h"
#include "content/shell/browser/shell.h"
#include "content/shell/common/shell_switches.h"

using content::URLLoaderInterceptor;

namespace {
constexpr char kBaseDataDir[] = "content/test/data/origin_trials/";

void NavigateViaRenderer(content::WebContents* web_contents, const GURL& url) {
  EXPECT_TRUE(
      content::ExecJs(web_contents->GetPrimaryMainFrame(),
                      base::StrCat({"location.href='", url.spec(), "';"})));
  // Enqueue a no-op script execution, which will block until the navigation
  // initiated above completes.
  EXPECT_TRUE(content::ExecJs(web_contents->GetPrimaryMainFrame(), "true"));
  EXPECT_TRUE(content::WaitForLoadStop(web_contents));
  EXPECT_EQ(web_contents->GetLastCommittedURL(), url);
}

}  // namespace

namespace content {

class OriginTrialsBrowserTest : public content::ContentBrowserTest {
 public:
  OriginTrialsBrowserTest() : ContentBrowserTest() {}

  OriginTrialsBrowserTest(const OriginTrialsBrowserTest&) = delete;
  OriginTrialsBrowserTest& operator=(const OriginTrialsBrowserTest&) = delete;

  ~OriginTrialsBrowserTest() override {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
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

  RenderFrameHost* GetFrameByName(const std::string frame_name) {
    return FrameMatchingPredicate(
        shell()->web_contents()->GetPrimaryPage(),
        base::BindRepeating(FrameMatchesName, frame_name));
  }

  RenderFrameHost* GetPrimaryMainFrame() {
    return shell()->web_contents()->GetPrimaryMainFrame();
  }

  testing::AssertionResult HasTrialEnabled(RenderFrameHost* frame) {
    // Test if we can invoke normalMethod(), which is only available when the
    // Frobulate OT is enabled.
    return content::ExecJs(frame,
                           "internals.originTrialsTest().normalMethod();");
  }

  testing::AssertionResult HasNavigationTrialEnabled(RenderFrameHost* frame) {
    // Test if we can invoke navigationMethod(), which is only available when
    // the FrobulateNavigation OT is enabled.
    return content::ExecJs(frame,
                           "internals.originTrialsTest().navigationMethod();");
  }

 private:
  std::unique_ptr<URLLoaderInterceptor> url_loader_interceptor_;
};

IN_PROC_BROWSER_TEST_F(OriginTrialsBrowserTest, Basic) {
  EXPECT_TRUE(NavigateToURL(shell(), GURL("https://example.test/basic.html")));
  EXPECT_TRUE(HasTrialEnabled(GetPrimaryMainFrame()));
}

IN_PROC_BROWSER_TEST_F(OriginTrialsBrowserTest,
                       NonNavigationTrialNotActivatedAcrossNavigations) {
  EXPECT_TRUE(NavigateToURL(shell(), GURL("https://example.test/basic.html")));
  EXPECT_TRUE(HasTrialEnabled(GetPrimaryMainFrame()));
  NavigateViaRenderer(shell()->web_contents(),
                      GURL("https://other.test/notrial.html"));
  EXPECT_FALSE(HasTrialEnabled(GetPrimaryMainFrame()));
}

IN_PROC_BROWSER_TEST_F(OriginTrialsBrowserTest, Navigation) {
  EXPECT_TRUE(
      NavigateToURL(shell(), GURL("https://example.test/navigation.html")));
  EXPECT_TRUE(HasNavigationTrialEnabled(GetPrimaryMainFrame()));
}

IN_PROC_BROWSER_TEST_F(OriginTrialsBrowserTest,
                       NavigationTrialActivatedAcrossNavigations) {
  EXPECT_TRUE(
      NavigateToURL(shell(), GURL("https://example.test/navigation.html")));
  EXPECT_TRUE(HasNavigationTrialEnabled(GetPrimaryMainFrame()));

  NavigateViaRenderer(shell()->web_contents(),
                      GURL("https://other.test/notrial.html"));
  // Navigation trial should be enabled after navigating from navigation.html,
  // because it is a cross-navigation OT.
  EXPECT_TRUE(HasNavigationTrialEnabled(GetPrimaryMainFrame()));

  NavigateViaRenderer(shell()->web_contents(),
                      GURL("https://other.test/basic.html"));
  // Navigation trial should not be enabled after a second navigation, because
  // cross-navigation OTs should only be forwarded to immediate navigations from
  // where the trial was activated.
  EXPECT_FALSE(HasNavigationTrialEnabled(GetPrimaryMainFrame()));
}

const char kCallWorkerScript[] =
    "(() => {"
    "  const worker = new Worker('/worker.js');"
    "  const waitResult = new Promise((resolve, reject) => {"
    "    worker.onmessage = function(e) {"
    "      (e.data?resolve:reject)(`return ${e.data} from worker`);"
    "    };"
    "  });"
    "  worker.postMessage('ping');"
    "  return waitResult;"
    "})()";

class ForceEnabledOriginTrialsBrowserTest
    : public OriginTrialsBrowserTest,
      public testing::WithParamInterface<bool>,
      public WebContentsObserver {
 public:
  ForceEnabledOriginTrialsBrowserTest() = default;
  ~ForceEnabledOriginTrialsBrowserTest() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    OriginTrialsBrowserTest::SetUpCommandLine(command_line);
    if (disable_site_isolation_)
      command_line->AppendSwitch(switches::kDisableSiteIsolation);
  }

  void ReadyToCommitNavigation(NavigationHandle* navigation_handle) override {
    if (navigation_handle->GetURL() == url_to_enable_trial_) {
      navigation_handle->ForceEnableOriginTrials(
          std::vector<std::string>({"Frobulate"}));
    }
  }

  void set_url_to_enable_trial(const GURL& url) { url_to_enable_trial_ = url; }

 protected:
  bool disable_site_isolation_ = GetParam();
  const GURL main_url_ =
      GURL("https://example.test/force_enabled_main_frame.html");

 private:
  GURL url_to_enable_trial_;
};

IN_PROC_BROWSER_TEST_P(ForceEnabledOriginTrialsBrowserTest,
                       ForceEnabledOriginTrials_MainPage) {
  set_url_to_enable_trial(main_url_);
  Observe(shell()->web_contents());
  EXPECT_TRUE(NavigateToURL(shell(), main_url_));

  // Trial should be enabled for main frame.
  EXPECT_TRUE(HasTrialEnabled(GetPrimaryMainFrame()));

  // OT are enabled per-frame. Subframes should not have OT.
  EXPECT_FALSE(HasTrialEnabled(GetFrameByName("same-origin")));
  EXPECT_FALSE(HasTrialEnabled(GetFrameByName("cross-origin")));
  EXPECT_FALSE(ExecJs(GetFrameByName("same-origin"), kCallWorkerScript));

  // With site isolation, the cross-site iframe on |main_url_| will get its own
  // process.  Otherwise, we'll only get one main frame process.
  ASSERT_EQ(AreAllSitesIsolatedForTesting() ? 2 : 1,
            RenderProcessHost::GetCurrentRenderProcessCountForTesting());

  // OT does not persist when we navigated away.
  NavigateViaRenderer(shell()->web_contents(),
                      GURL("https://other.test/notrial.html"));
  EXPECT_FALSE(HasTrialEnabled(GetPrimaryMainFrame()));
}

IN_PROC_BROWSER_TEST_P(ForceEnabledOriginTrialsBrowserTest,
                       ForceEnabledOriginTrials_IframeInPage) {
  set_url_to_enable_trial(main_url_.Resolve("/notrial.html"));
  Observe(shell()->web_contents());
  EXPECT_TRUE(NavigateToURL(shell(), main_url_));

  // Main frame does not have trial.
  EXPECT_FALSE(HasTrialEnabled(GetPrimaryMainFrame()));

  // Same-origin frame (which loads notrial.html) has trial.
  EXPECT_TRUE(HasTrialEnabled(GetFrameByName("same-origin")));

  // Cross-origin frame has no trial.
  EXPECT_FALSE(HasTrialEnabled(GetFrameByName("cross-origin")));

  // Worker in same-origin frame (which loads notrial.html>worker.js) has trial.
  EXPECT_TRUE(ExecJs(GetFrameByName("same-origin"), kCallWorkerScript));

  // When Iframe navigates away, it loses origin trial.
  const GURL url("https://other.test/notrial.html");
  TestNavigationObserver navigation_observer(url);
  navigation_observer.WatchExistingWebContents();
  ASSERT_TRUE(ExecJs(GetFrameByName("same-origin"),
                     content::JsReplace("location.href=$1", url.spec())));
  navigation_observer.WaitForNavigationFinished();
  EXPECT_FALSE(HasTrialEnabled(GetFrameByName("same-origin")));
}

IN_PROC_BROWSER_TEST_P(ForceEnabledOriginTrialsBrowserTest,
                       ForceEnabledOriginTrials_InjectedIframe) {
  const GURL frame_url("https://newly-loaded.test/notrial.html");
  set_url_to_enable_trial(frame_url);

  Observe(shell()->web_contents());
  EXPECT_TRUE(NavigateToURL(shell(), main_url_));

  // Main frame and all iframes don't have trial.
  EXPECT_FALSE(HasTrialEnabled(GetPrimaryMainFrame()));
  EXPECT_FALSE(HasTrialEnabled(GetFrameByName("same-origin")));
  EXPECT_FALSE(HasTrialEnabled(GetFrameByName("cross-origin")));
  EXPECT_FALSE(ExecJs(GetFrameByName("same-origin"), kCallWorkerScript));

  // Create an iframe with origin trial and wait for it to load
  TestNavigationObserver navigation_observer(frame_url);
  navigation_observer.WatchExistingWebContents();
  ASSERT_TRUE(ExecJs(
      GetFrameByName("same-origin"),
      content::JsReplace("{"
                         "  const ifrm = document.createElement('iframe');"
                         "  ifrm.name='new-frame';"
                         "  ifrm.src=$1;"
                         "  document.body.appendChild(ifrm);"
                         "}",
                         frame_url.spec())));
  navigation_observer.WaitForNavigationFinished();

  // The newly created iframe should have origin trial.
  EXPECT_TRUE(HasTrialEnabled(GetFrameByName("new-frame")));
}

INSTANTIATE_TEST_SUITE_P(SiteIsolation,
                         ForceEnabledOriginTrialsBrowserTest,
                         /*disable_site_isolation=*/::testing::Bool());

}  // namespace content
