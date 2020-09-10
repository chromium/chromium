// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>
#include <memory>

#include "base/bind.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "content/browser/conversions/conversion_manager_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/hit_test_region_observer.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/shell/browser/shell.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/default_handlers.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "third_party/blink/public/common/input/web_mouse_event.h"
#include "ui/gfx/geometry/point.h"
#include "url/gurl.h"

namespace content {

// WebContentsObserver that waits until an impression is available on a
// navigation handle for a finished navigation.
class ImpressionObserver : public TestNavigationObserver {
 public:
  explicit ImpressionObserver(WebContents* contents,
                              size_t num_impressions = 1u)
      : TestNavigationObserver(contents),
        expected_num_impressions_(num_impressions) {}

  // WebContentsObserver
  void OnDidFinishNavigation(NavigationHandle* navigation_handle) override {
    if (!navigation_handle->GetImpression()) {
      if (waiting_for_null_impression_)
        impression_loop_.Quit();
      return;
    }

    last_impression_ = *(navigation_handle->GetImpression());
    num_impressions_++;

    if (!waiting_for_null_impression_ &&
        num_impressions_ >= expected_num_impressions_) {
      impression_loop_.Quit();
    }
  }

  const Impression& last_impression() { return *last_impression_; }

  // Waits for |expected_num_impressions_| navigations with impressions, and
  // returns the last impression.
  const Impression& Wait() {
    if (num_impressions_ >= expected_num_impressions_)
      return *last_impression_;
    impression_loop_.Run();
    return last_impression();
  }

  bool WaitForNavigationWithNoImpression() {
    waiting_for_null_impression_ = true;
    impression_loop_.Run();
    waiting_for_null_impression_ = false;
    return true;
  }

 private:
  size_t num_impressions_ = 0u;
  const size_t expected_num_impressions_ = 0u;
  base::Optional<Impression> last_impression_;
  bool waiting_for_null_impression_ = false;
  base::RunLoop impression_loop_;
};

class ImpressionDisabledBrowserTest : public ContentBrowserTest {
 public:
  ImpressionDisabledBrowserTest() {
    feature_list_.InitAndEnableFeature(features::kConversionMeasurement);
    ConversionManagerImpl::RunInMemoryForTesting();
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    embedded_test_server()->ServeFilesFromSourceDirectory(
        "content/test/data/conversions");
    embedded_test_server()->ServeFilesFromSourceDirectory("content/test/data");
    content::SetupCrossSiteRedirector(embedded_test_server());
    ASSERT_TRUE(embedded_test_server()->Start());

    https_server_ = std::make_unique<net::EmbeddedTestServer>(
        net::EmbeddedTestServer::TYPE_HTTPS);
    https_server_->SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
    net::test_server::RegisterDefaultHandlers(https_server_.get());
    https_server_->ServeFilesFromSourceDirectory(
        "content/test/data/conversions");
    https_server_->ServeFilesFromSourceDirectory("content/test/data");
    SetupCrossSiteRedirector(https_server_.get());
    ASSERT_TRUE(https_server_->Start());
  }

  WebContents* web_contents() { return shell()->web_contents(); }

  net::EmbeddedTestServer* https_server() { return https_server_.get(); }

 private:
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<net::EmbeddedTestServer> https_server_;
};

// Verifies that impressions are not logged when the Runtime feature isn't
// enabled.
IN_PROC_BROWSER_TEST_F(ImpressionDisabledBrowserTest,
                       ImpressionWithoutFeatureEnabled_NotReceived) {
  ImpressionObserver impression_observer(web_contents());
  EXPECT_TRUE(NavigateToURL(
      web_contents(),
      https_server()->GetURL("b.test", "/page_with_impression_creator.html")));

  EXPECT_TRUE(ExecJs(web_contents(), R"(
    createImpressionTag("link",
                        "page_with_conversion_redirect.html",
                        "1" /* impression data */,
                        "https://a.com" /* conversion_destination */);)"));
  EXPECT_TRUE(ExecJs(shell(), "simulateClick(\'link\');"));

  // No impression should be observed.
  EXPECT_TRUE(impression_observer.WaitForNavigationWithNoImpression());
}

class ImpressionDeclarationBrowserTest : public ImpressionDisabledBrowserTest {
 public:
  ImpressionDeclarationBrowserTest() = default;

  // Sets up the blink runtime feature for ConversionMeasurement.
  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(
        switches::kEnableExperimentalWebPlatformFeatures);
  }
};

IN_PROC_BROWSER_TEST_F(ImpressionDeclarationBrowserTest,
                       ImpressionTagClicked_ImpressionReceived) {
  ImpressionObserver impression_observer(web_contents());
  GURL page_url =
      https_server()->GetURL("b.test", "/page_with_impression_creator.html");
  EXPECT_TRUE(NavigateToURL(web_contents(), page_url));

  // Create an anchor tag with impression attributes and click the link. By
  // default the target is set to "_top".
  EXPECT_TRUE(ExecJs(web_contents(), R"(
    createImpressionTagWithReportingAndExpiry("link" /* id */,
                        "page_with_conversion_redirect.html" /* url */,
                        "1" /* impression data */,
                        "https://a.com" /* conversion_destination */,
                        "https://report.com" /* report_origin */,
                        1000 /* expiry */);)"));
  EXPECT_TRUE(ExecJs(shell(), "simulateClick(\'link\');"));

  // Wait for the impression to be seen by the observer.
  Impression last_impression = impression_observer.Wait();

  // Verify the attributes of the impression are set as expected.
  EXPECT_EQ(1UL, last_impression.impression_data);
  EXPECT_EQ(url::Origin::Create(GURL("https://a.com")),
            last_impression.conversion_destination);
  EXPECT_EQ(url::Origin::Create(GURL("https://report.com")),
            last_impression.reporting_origin);
  EXPECT_EQ(base::TimeDelta::FromMilliseconds(1000), *last_impression.expiry);
}

IN_PROC_BROWSER_TEST_F(ImpressionDeclarationBrowserTest,
                       ImpressionTagNavigatesRemoteFrame_ImpressionReceived) {
  EXPECT_TRUE(NavigateToURL(
      web_contents(),
      https_server()->GetURL("b.test", "/page_with_impression_creator.html")));

  // Create an impression tag with a target frame that does not exist, which
  // will open a new window to navigate.
  EXPECT_TRUE(ExecJs(web_contents(), R"(
    createImpressionTagWithTarget("link",
                        "page_with_conversion_redirect.html",
                        "1" /* impression data */,
                        "https://a.com" /* conversion_destination */,
                        "target" /* target */);)"));

  ImpressionObserver impression_observer(nullptr);
  impression_observer.StartWatchingNewWebContents();
  EXPECT_TRUE(ExecJs(shell(), "simulateClick(\'link\');"));

  // Wait for the impression to be seen by the observer.
  Impression last_impression = impression_observer.Wait();
  EXPECT_EQ(1UL, impression_observer.last_impression().impression_data);
}

// Flaky: crbug.com/1077216
IN_PROC_BROWSER_TEST_F(
    ImpressionDeclarationBrowserTest,
    DISABLED_ImpressionTagNavigatesExistingRemoteFrame_ImpressionReceived) {
  EXPECT_TRUE(NavigateToURL(
      web_contents(),
      https_server()->GetURL("b.test", "/page_with_impression_creator.html")));

  WebContents* initial_web_contents = web_contents();

  ShellAddedObserver new_shell_observer;
  GURL remote_url = https_server()->GetURL("c.test", "/title1.html");
  EXPECT_TRUE(ExecJs(web_contents(),
                     JsReplace("window.open($1, 'target');", remote_url)));

  // Get the new web contents associated with the remote frame.
  WebContents* remote_web_contents =
      new_shell_observer.GetShell()->web_contents();

  // Click on the impression and target the existing remote frame.
  EXPECT_TRUE(ExecJs(initial_web_contents, R"(
    createImpressionTagWithTarget("link",
                        "page_with_conversion_redirect.html",
                        "1" /* impression data */,
                        "https://a.com" /* conversion_destination */,
                        "target" /* target */);)"));

  ImpressionObserver impression_observer(remote_web_contents);
  EXPECT_TRUE(ExecJs(shell(), "simulateClick(\'link\');"));

  // Wait for the impression to be seen by the observer.
  Impression last_impression = impression_observer.Wait();
  EXPECT_EQ(1UL, impression_observer.last_impression().impression_data);
}

IN_PROC_BROWSER_TEST_F(ImpressionDeclarationBrowserTest,
                       ImpressionTagWithOutOfBoundData_DefaultedTo0) {
  ImpressionObserver impression_observer(web_contents());
  EXPECT_TRUE(NavigateToURL(
      web_contents(),
      https_server()->GetURL("b.test", "/page_with_impression_creator.html")));

  // The provided data overflows an unsigned 64 bit int, and should be handled
  // properly.
  EXPECT_TRUE(ExecJs(web_contents(), R"(
    createImpressionTag("link",
                        "page_with_conversion_redirect.html",
                        "FFFFFFFFFFFFFFFFFFFFFF" /* impression data */,
                        "https://a.com" /* conversion_destination */);)"));
  EXPECT_TRUE(ExecJs(shell(), "simulateClick(\'link\');"));

  // Wait for the impression to be seen by the observer.
  Impression last_impression = impression_observer.Wait();
  EXPECT_EQ(0UL, impression_observer.last_impression().impression_data);
}

IN_PROC_BROWSER_TEST_F(
    ImpressionDeclarationBrowserTest,
    ImpressionTagNavigatesFromMiddleClick_ImpressionReceived) {
  GURL page_url =
      https_server()->GetURL("b.test", "/page_with_impression_creator.html");
  EXPECT_TRUE(NavigateToURL(web_contents(), page_url));

  // Create an impression tag that is opened via middle click. This navigates in
  // a new WebContents.
  EXPECT_TRUE(ExecJs(web_contents(), R"(
    createImpressionTag("link",
                        "page_with_conversion_redirect.html",
                        "1" /* impression data */,
                        "https://a.com" /* conversion_destination */);)"));

  ImpressionObserver impression_observer(nullptr);
  impression_observer.StartWatchingNewWebContents();
  EXPECT_TRUE(ExecJs(shell(), "simulateMiddleClick(\'link\');"));

  Impression last_impression = impression_observer.Wait();

  // Verify the attributes of the impression are set as expected.
  EXPECT_EQ(1UL, last_impression.impression_data);
}

IN_PROC_BROWSER_TEST_F(
    ImpressionDeclarationBrowserTest,
    ImpressionTagNavigatesFromEnterPress_ImpressionReceived) {
  GURL page_url =
      https_server()->GetURL("b.test", "/page_with_impression_creator.html");
  EXPECT_TRUE(NavigateToURL(web_contents(), page_url));

  EXPECT_TRUE(ExecJs(web_contents(), R"(
    createImpressionTag("link",
                        "page_with_conversion_redirect.html",
                        "1" /* impression data */,
                        "https://a.com" /* conversion_destination */);)"));

  // Focus the element, wait for it to receive focus, and simulate an enter
  // press.
  base::string16 expected_title = base::ASCIIToUTF16("focused");
  content::TitleWatcher title_watcher(web_contents(), expected_title);
  EXPECT_TRUE(ExecJs(shell(), R"(
    let link = document.getElementById('link');
    link.addEventListener('focus', function() { document.title = 'focused'; });
    link.focus();)"));
  EXPECT_EQ(expected_title, title_watcher.WaitAndGetTitle());

  ImpressionObserver impression_observer(web_contents());
  content::SimulateKeyPress(web_contents(), ui::DomKey::ENTER,
                            ui::DomCode::ENTER, ui::VKEY_RETURN, false, false,
                            false, false);

  Impression last_impression = impression_observer.Wait();

  // Verify the attributes of the impression are set as expected.
  EXPECT_EQ(1UL, last_impression.impression_data);
}

IN_PROC_BROWSER_TEST_F(ImpressionDeclarationBrowserTest,
                       ImpressionOnInsecureSite_NotRegistered) {
  // Navigate to a page with the non-https server.
  EXPECT_TRUE(NavigateToURL(
      web_contents(), embedded_test_server()->GetURL(
                          "b.test", "/page_with_impression_creator.html")));

  ImpressionObserver impression_observer(web_contents());
  EXPECT_TRUE(ExecJs(web_contents(), R"(
    createImpressionTag("link",
                        "page_with_conversion_redirect.html",
                        "1" /* impression data */,
                        "https://a.com" /* conversion_destination */);)"));
  EXPECT_TRUE(ExecJs(shell(), "simulateClick(\'link\');"));

  // We should see a null impression on the navigation
  EXPECT_TRUE(impression_observer.WaitForNavigationWithNoImpression());
}

IN_PROC_BROWSER_TEST_F(ImpressionDeclarationBrowserTest,
                       ImpressionWithInsecureDestination_NotRegistered) {
  // Navigate to a page with the non-https server.
  EXPECT_TRUE(NavigateToURL(
      web_contents(),
      https_server()->GetURL("b.test", "/page_with_impression_creator.html")));

  ImpressionObserver impression_observer(web_contents());
  EXPECT_TRUE(ExecJs(web_contents(), R"(
    createImpressionTag("link",
                        "page_with_conversion_redirect.html",
                        "1" /* impression data */,
                        "http://a.com" /* conversion_destination */);)"));
  EXPECT_TRUE(ExecJs(shell(), "simulateClick(\'link\');"));

  // We should see a null impression on the navigation
  EXPECT_TRUE(impression_observer.WaitForNavigationWithNoImpression());
}

IN_PROC_BROWSER_TEST_F(ImpressionDeclarationBrowserTest,
                       ImpressionWithInsecureReportingOrigin_NotRegistered) {
  // Navigate to a page with the non-https server.
  EXPECT_TRUE(NavigateToURL(
      web_contents(),
      https_server()->GetURL("b.test", "/page_with_impression_creator.html")));

  ImpressionObserver impression_observer(web_contents());
  EXPECT_TRUE(ExecJs(web_contents(), R"(
    createImpressionTagWithReportingAndExpiry("link",
                        "page_with_conversion_redirect.html",
                        "1" /* impression data */,
                        "https://a.com" /* conversion_destination */,
                        "http://reporting.com" /* report_origin */,
                        1000 /* expiry */);)"));
  EXPECT_TRUE(ExecJs(shell(), "simulateClick(\'link\');"));

  // We should see a null impression on the navigation
  EXPECT_TRUE(impression_observer.WaitForNavigationWithNoImpression());
}

IN_PROC_BROWSER_TEST_F(ImpressionDeclarationBrowserTest,
                       ImpressionWithFeaturePolicyDisabled_NotRegistered) {
  EXPECT_TRUE(NavigateToURL(
      web_contents(),
      https_server()->GetURL(
          "b.test", "/page_with_conversion_measurement_disabled.html")));

  ImpressionObserver impression_observer(web_contents());
  EXPECT_TRUE(ExecJs(web_contents(), R"(
    createImpressionTag("link",
                        "page_with_conversion_redirect.html",
                        "1" /* impression data */,
                        "https://a.com" /* conversion_destination */);)"));
  EXPECT_TRUE(ExecJs(shell(), "simulateClick('link');"));

  // We should see a null impression on the navigation
  EXPECT_TRUE(impression_observer.WaitForNavigationWithNoImpression());
}

IN_PROC_BROWSER_TEST_F(ImpressionDeclarationBrowserTest,
                       ImpressionInSubframeWithoutFeaturePolicy_NotRegistered) {
  GURL page_url = https_server()->GetURL("b.test", "/page_with_iframe.html");
  EXPECT_TRUE(NavigateToURL(web_contents(), page_url));

  GURL subframe_url =
      https_server()->GetURL("c.test", "/page_with_impression_creator.html");
  NavigateIframeToURL(web_contents(), "test_iframe", subframe_url);

  ImpressionObserver impression_observer(web_contents());
  RenderFrameHost* subframe = ChildFrameAt(web_contents()->GetMainFrame(), 0);
  EXPECT_TRUE(ExecJs(subframe, R"(
    createImpressionTag("link",
                        "page_with_conversion_redirect.html",
                        "1" /* impression data */,
                        "https://a.com" /* conversion_destination */);)"));
  EXPECT_TRUE(ExecJs(subframe, "simulateClick('link');"));

  // We should see a null impression on the navigation
  EXPECT_TRUE(impression_observer.WaitForNavigationWithNoImpression());
}

IN_PROC_BROWSER_TEST_F(ImpressionDeclarationBrowserTest,
                       ImpressionInSubframeWithFeaturePolicy_Registered) {
  GURL page_url = https_server()->GetURL("b.test", "/page_with_iframe.html");
  EXPECT_TRUE(NavigateToURL(web_contents(), page_url));
  EXPECT_TRUE(ExecJs(shell(), R"(
     let frame = document.getElementById('test_iframe');
     frame.setAttribute('allow', 'conversion-measurement');)"));

  GURL subframe_url =
      https_server()->GetURL("c.test", "/page_with_impression_creator.html");
  NavigateIframeToURL(web_contents(), "test_iframe", subframe_url);

  ImpressionObserver impression_observer(web_contents());
  RenderFrameHost* subframe = ChildFrameAt(web_contents()->GetMainFrame(), 0);
  EXPECT_TRUE(ExecJs(subframe, R"(
    createImpressionTag("link",
                        "page_with_conversion_redirect.html",
                        "1" /* impression data */,
                        "https://a.com" /* conversion_destination */);)"));
  EXPECT_TRUE(ExecJs(subframe, "simulateClick('link');"));

  // We should see a null impression on the navigation
  EXPECT_EQ(1u, impression_observer.Wait().impression_data);
}

// Tests that when a context menu is shown, there is an impression attached to
// the ContextMenu data forwarded to the browser process.

// TODO(johnidel): SimulateMouseClickAt() does not work on Android, find a
// different way to invoke the context menu that works on Android.
#if !defined(OS_ANDROID)
IN_PROC_BROWSER_TEST_F(ImpressionDeclarationBrowserTest,
                       ContextMenuShownForImpression_ImpressionSet) {
  // Navigate to a page with the non-https server.
  EXPECT_TRUE(NavigateToURL(
      web_contents(),
      https_server()->GetURL("b.test", "/page_with_impression_creator.html")));

  EXPECT_TRUE(ExecJs(web_contents(), R"(
    createImpressionTagAtLocation("link",
                        "page_with_conversion_redirect.html",
                        "10" /* impression data */,
                        "https://dest.com" /* conversion_destination */,
                        100 /* left */, 100 /* top */);)"));

  content::RenderProcessHost* render_process_host =
      web_contents()->GetMainFrame()->GetProcess();
  auto context_menu_filter = base::MakeRefCounted<content::ContextMenuFilter>();
  render_process_host->AddFilter(context_menu_filter.get());

  content::SimulateMouseClickAt(web_contents(), 0,
                                blink::WebMouseEvent::Button::kRight,
                                gfx::Point(100, 100));

  context_menu_filter->Wait();
  content::UntrustworthyContextMenuParams params =
      context_menu_filter->get_params();
  EXPECT_TRUE(params.impression);
  EXPECT_EQ(16UL, params.impression->impression_data);
  EXPECT_EQ(url::Origin::Create(GURL("https://dest.com")),
            params.impression->conversion_destination);
}
#endif  // !defined(OS_ANDROID)

IN_PROC_BROWSER_TEST_F(ImpressionDeclarationBrowserTest,
                       ImpressionNavigationReloads_NoImpression) {
  EXPECT_TRUE(NavigateToURL(
      web_contents(),
      https_server()->GetURL("b.test", "/page_with_impression_creator.html")));

  ImpressionObserver impression_observer(web_contents());
  EXPECT_TRUE(ExecJs(web_contents(), R"(
    createImpressionTag("link",
                        "page_with_conversion_redirect.html",
                        "1" /* impression data */,
                        "https://a.com" /* conversion_destination */);)"));
  EXPECT_TRUE(ExecJs(shell(), "simulateClick(\'link\');"));
  EXPECT_EQ(1UL, impression_observer.Wait().impression_data);

  ImpressionObserver reload_observer(web_contents());
  shell()->Reload();

  // The reload navigation should not have an impression set.
  EXPECT_TRUE(reload_observer.WaitForNavigationWithNoImpression());
}

// Same as the above test but via a renderer initiated reload.
IN_PROC_BROWSER_TEST_F(ImpressionDeclarationBrowserTest,
                       RendererReloadImpressionNavigation_NoImpression) {
  EXPECT_TRUE(NavigateToURL(
      web_contents(),
      https_server()->GetURL("b.test", "/page_with_impression_creator.html")));

  ImpressionObserver impression_observer(web_contents());
  EXPECT_TRUE(ExecJs(web_contents(), R"(
    createImpressionTag("link",
                        "page_with_conversion_redirect.html",
                        "1" /* impression data */,
                        "https://a.com" /* conversion_destination */);)"));
  EXPECT_TRUE(ExecJs(shell(), "simulateClick(\'link\');"));
  EXPECT_EQ(1UL, impression_observer.Wait().impression_data);

  ImpressionObserver reload_observer(web_contents());
  EXPECT_TRUE(ExecJs(web_contents(), "window.location.reload()"));

  // The reload navigation should not have an impression set.
  EXPECT_TRUE(reload_observer.WaitForNavigationWithNoImpression());
}

IN_PROC_BROWSER_TEST_F(ImpressionDeclarationBrowserTest,
                       BackNavigateToImpressionNavigation_NoImpression) {
  EXPECT_TRUE(NavigateToURL(
      web_contents(),
      https_server()->GetURL("b.test", "/page_with_impression_creator.html")));

  ImpressionObserver impression_observer(web_contents());

  // Click the default impression on the page.
  EXPECT_TRUE(ExecJs(shell(), "simulateClick(\'impression_tag\');"));
  EXPECT_EQ(1UL, impression_observer.Wait().impression_data);

  // Navigate away so we can back navigate to the impression's navigated page.
  EXPECT_TRUE(NavigateToURL(web_contents(), GURL("about:blank")));

  // The back navigation should not have an impression set.
  ImpressionObserver back_nav_observer(web_contents());
  shell()->GoBackOrForward(-1);
  EXPECT_TRUE(back_nav_observer.WaitForNavigationWithNoImpression());

  // Navigate back to the original page and ensure subsequent clicks also log
  // impressions.
  ImpressionObserver second_back_nav_observer(web_contents());
  shell()->GoBackOrForward(-1);
  EXPECT_TRUE(second_back_nav_observer.WaitForNavigationWithNoImpression());

  // Wait for the page to load and render the impression tag.
  EXPECT_TRUE(WaitForLoadStop(web_contents()));
  ImpressionObserver second_impression_observer(web_contents());
  EXPECT_TRUE(ExecJs(shell(), "simulateClick(\'impression_tag\');"));
  EXPECT_EQ(1UL, second_impression_observer.Wait().impression_data);
}

}  // namespace content
