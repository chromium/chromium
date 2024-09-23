// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/json/json_writer.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/platform_thread.h"
#include "base/values.h"
#include "build/build_config.h"
#include "content/browser/attribution_reporting/attribution_manager_impl.h"
#include "content/browser/attribution_reporting/attribution_test_utils.h"
#include "content/browser/attribution_reporting/test/source_observer.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/common/features.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/context_menu_interceptor.h"
#include "content/public/test/hit_test_region_observer.h"
#include "content/shell/browser/shell.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/controllable_http_response.h"
#include "net/test/embedded_test_server/default_handlers.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/blink/public/common/input/web_mouse_event.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/point_conversions.h"
#include "url/gurl.h"

namespace content {

namespace {

using ::testing::ElementsAre;
using ::testing::Field;
using ::testing::IsEmpty;
using ::testing::Pair;
using ::testing::Pointee;
using ::testing::UnorderedElementsAre;

class AttributionSourceBrowserTest : public ContentBrowserTest {
 public:
  AttributionSourceBrowserTest() = default;

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    embedded_test_server()->ServeFilesFromSourceDirectory(
        "content/test/data/attribution_reporting");
    embedded_test_server()->ServeFilesFromSourceDirectory("content/test/data");
    ASSERT_TRUE(embedded_test_server()->Start());

    https_server_ = std::make_unique<net::EmbeddedTestServer>(
        net::EmbeddedTestServer::TYPE_HTTPS);
    https_server_->SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
    net::test_server::RegisterDefaultHandlers(https_server_.get());
    https_server_->ServeFilesFromSourceDirectory(
        "content/test/data/attribution_reporting");
    https_server_->ServeFilesFromSourceDirectory("content/test/data");
    ASSERT_TRUE(https_server_->Start());
  }

  WebContents* web_contents() { return shell()->web_contents(); }

  net::EmbeddedTestServer* https_server() { return https_server_.get(); }

 private:
  AttributionManagerImpl::ScopedUseInMemoryStorageForTesting
      attribution_manager_in_memory_setting_;
  std::unique_ptr<net::EmbeddedTestServer> https_server_;
};

class AttributionSourceDisabledBrowserTest : public AttributionSourceBrowserTest {
 public:
  AttributionSourceDisabledBrowserTest() {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/
        {},
        /*disabled_features=*/{features::kPrivacySandboxAdsAPIsM1Override,
                               features::kPrivacySandboxAdsAPIsOverride});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Verifies that impressions are not logged when the Runtime feature isn't
// enabled.
IN_PROC_BROWSER_TEST_F(AttributionSourceDisabledBrowserTest,
                       ImpressionWithoutFeatureEnabled_NotReceived) {
  EXPECT_TRUE(NavigateToURL(
      web_contents(),
      https_server()->GetURL("b.test", "/page_with_impression_creator.html")));

  // Create an anchor tag with impression attributes and click the link.
  GURL register_source_url = https_server()->GetURL(
      "b.test", "/attribution_reporting/register_source_headers.html");
  EXPECT_TRUE(ExecJs(web_contents(), JsReplace(R"(
    createAttributionSrcAnchor({id: 'link',
                        url: 'page_with_conversion_redirect.html',
                        attributionsrc: $1,
                        target: '_top'});)",
                                               register_source_url)));

  SourceObserver source_observer(web_contents());
  EXPECT_TRUE(ExecJs(shell(), "simulateClick('link');"));

  // No impression should be observed.
  EXPECT_TRUE(source_observer.WaitForNavigationWithNoImpression());
}

class AttributionSourceDeclarationBrowserTest
    : public AttributionSourceBrowserTest {
 public:
  AttributionSourceDeclarationBrowserTest() = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    // Sets up the blink runtime feature for ConversionMeasurement.
    command_line->AppendSwitch(
        switches::kEnableExperimentalWebPlatformFeatures);
  }
};

IN_PROC_BROWSER_TEST_F(AttributionSourceDeclarationBrowserTest,
                       ImpressionTagNavigatesRemoteFrame_ImpressionReceived) {
  EXPECT_TRUE(NavigateToURL(
      web_contents(),
      https_server()->GetURL("b.test", "/page_with_impression_creator.html")));

  // Create an impression tag with a target frame that does not exist, which
  // will open a new window to navigate.
  GURL register_source_url = https_server()->GetURL(
      "b.test", "/attribution_reporting/register_source_headers.html");
  EXPECT_TRUE(ExecJs(web_contents(), JsReplace(R"(
    createAttributionSrcAnchor({id: 'link',
                        url: 'page_with_conversion_redirect.html',
                        attributionsrc: $1,
                        target: 'target'});)",
                                               register_source_url)));

  SourceObserver source_observer(nullptr);
  source_observer.StartWatchingNewWebContents();
  EXPECT_TRUE(ExecJs(shell(), "simulateClick('link');"));

  source_observer.Wait();
}

// Flaky: crbug.com/1077216
IN_PROC_BROWSER_TEST_F(
    AttributionSourceDeclarationBrowserTest,
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

  // Create an impression tag with a target frame that does not exist, which
  // will open a new window to navigate.
  GURL register_source_url = https_server()->GetURL(
      "b.test", "/attribution_reporting/register_source_headers.html");
  EXPECT_TRUE(ExecJs(initial_web_contents, JsReplace(R"(
    createAttributionSrcAnchor({id: 'link',
                        url: 'page_with_conversion_redirect.html',
                        attributionsrc: $1,
                        target: 'target'});)",
                                                     register_source_url)));

  SourceObserver source_observer(remote_web_contents);
  EXPECT_TRUE(ExecJs(shell(), "simulateClick('link');"));

  source_observer.Wait();
}

IN_PROC_BROWSER_TEST_F(
    AttributionSourceDeclarationBrowserTest,
    ImpressionTagNavigatesFromMiddleClick_ImpressionReceived) {
  GURL page_url =
      https_server()->GetURL("b.test", "/page_with_impression_creator.html");
  EXPECT_TRUE(NavigateToURL(web_contents(), page_url));

  // Create an impression tag that is opened via middle click. This navigates in
  // a new WebContents.
  GURL register_source_url = https_server()->GetURL(
      "b.test", "/attribution_reporting/register_source_headers.html");
  EXPECT_TRUE(ExecJs(web_contents(), JsReplace(R"(
    createAttributionSrcAnchor({id: 'link',
                        url: 'page_with_conversion_redirect.html',
                        attributionsrc: $1,
                        target: 'target'});)",
                                               register_source_url)));

  SourceObserver source_observer(nullptr);
  source_observer.StartWatchingNewWebContents();
  EXPECT_TRUE(ExecJs(shell(), "simulateMiddleClick(\'link\');"));

  source_observer.Wait();
}

// See https://crbug.com/1186077.
IN_PROC_BROWSER_TEST_F(
    AttributionSourceDeclarationBrowserTest,
    TagNavigatesFromMiddleClickInSubframe_ImpressionReceived) {
  GURL page_url = https_server()->GetURL("b.test", "/page_with_iframe.html");
  EXPECT_TRUE(NavigateToURL(web_contents(), page_url));
  EXPECT_TRUE(ExecJs(shell(), R"(
     let frame = document.getElementById('test_iframe');
     frame.setAttribute('allow', 'attribution-reporting');)"));

  GURL subframe_url =
      https_server()->GetURL("c.test", "/page_with_impression_creator.html");
  NavigateIframeToURL(web_contents(), "test_iframe", subframe_url);

  // Create an impression tag that is opened via middle click in the subframe.
  RenderFrameHost* subframe =
      ChildFrameAt(web_contents()->GetPrimaryMainFrame(), 0);

  GURL register_source_url = https_server()->GetURL(
      "b.test", "/attribution_reporting/register_source_headers.html");
  EXPECT_TRUE(ExecJs(subframe, JsReplace(R"(
    createAttributionSrcAnchor({id: 'link',
                        url: 'page_with_conversion_redirect.html',
                        attributionsrc: $1,
                        target: 'target'});)",
                                         register_source_url)));

  SourceObserver source_observer(nullptr);
  source_observer.StartWatchingNewWebContents();
  EXPECT_TRUE(ExecJs(subframe, "simulateMiddleClick(\'link\');"));

  source_observer.Wait();
}

// https://crbug.com/1219907 started flaking after Field Trial Testing Config
// was enabled for content_browsertests.
IN_PROC_BROWSER_TEST_F(
    AttributionSourceDeclarationBrowserTest,
    DISABLED_ImpressionTagNavigatesFromEnterPress_ImpressionReceived) {
  GURL page_url =
      https_server()->GetURL("b.test", "/page_with_impression_creator.html");
  EXPECT_TRUE(NavigateToURL(web_contents(), page_url));

  // Create an impression tag with a target frame that does not exist, which
  // will open a new window to navigate.
  GURL register_source_url = https_server()->GetURL(
      "b.test", "/attribution_reporting/register_source_headers.html");
  EXPECT_TRUE(ExecJs(web_contents(), JsReplace(R"(
    createAttributionSrcAnchor({id: 'link',
                        url: 'page_with_conversion_redirect.html',
                        attributionsrc: $1,
                        target: 'target'});)",
                                               register_source_url)));

  // Focus the element, wait for it to receive focus, and simulate an enter
  // press.
  std::u16string expected_title = u"focused";
  content::TitleWatcher title_watcher(web_contents(), expected_title);
  EXPECT_TRUE(ExecJs(shell(), R"(
    let link = document.getElementById('link');
    link.addEventListener('focus', function() { document.title = 'focused'; });
    link.focus();)"));
  EXPECT_EQ(expected_title, title_watcher.WaitAndGetTitle());

  SourceObserver source_observer(web_contents());
  content::SimulateKeyPress(web_contents(), ui::DomKey::ENTER,
                            ui::DomCode::ENTER, ui::VKEY_RETURN, false, false,
                            false, false);

  source_observer.Wait();
}

IN_PROC_BROWSER_TEST_F(AttributionSourceDeclarationBrowserTest,
                       ImpressionOnInsecureSite_NotRegistered) {
  // Navigate to a page with the non-https server.
  EXPECT_TRUE(NavigateToURL(
      web_contents(), embedded_test_server()->GetURL(
                          "b.test", "/page_with_impression_creator.html")));

  GURL register_source_url = https_server()->GetURL(
      "b.test", "/attribution_reporting/register_source_headers.html");
  EXPECT_TRUE(ExecJs(web_contents(), JsReplace(R"(
    createAttributionSrcAnchor({id: 'link',
                        url: 'page_with_conversion_redirect.html',
                        attributionsrc: $1});)",
                                               register_source_url)));

  SourceObserver source_observer(web_contents());
  EXPECT_TRUE(ExecJs(shell(), "simulateClick('link');"));

  // We should see a null impression on the navigation
  EXPECT_TRUE(source_observer.WaitForNavigationWithNoImpression());
}

IN_PROC_BROWSER_TEST_F(AttributionSourceDeclarationBrowserTest,
                       ImpressionWithInsecureReportingOrigin_ReceivesToken) {
  // Navigate to a page with the non-https server.
  EXPECT_TRUE(NavigateToURL(
      web_contents(),
      https_server()->GetURL("b.test", "/page_with_impression_creator.html")));

  GURL register_source_url = embedded_test_server()->GetURL(
      "b.test", "/attribution_reporting/register_source_headers.html");
  EXPECT_TRUE(ExecJs(web_contents(), JsReplace(R"(
    createAttributionSrcAnchor({id: 'link',
                        url: 'page_with_conversion_redirect.html',
                        attributionsrc: $1});)",
                                               register_source_url)));

  SourceObserver source_observer(web_contents());
  EXPECT_TRUE(ExecJs(shell(), "simulateClick('link');"));

  // We should see an impression, as there may be registrations that happen on
  // the navigation redirect.
  source_observer.Wait();
}

IN_PROC_BROWSER_TEST_F(AttributionSourceDeclarationBrowserTest,
                       ImpressionWithPermissionsPolicyDisabled_NotRegistered) {
  EXPECT_TRUE(NavigateToURL(
      web_contents(),
      https_server()->GetURL(
          "b.test", "/page_with_conversion_measurement_disabled.html")));

  GURL register_source_url = https_server()->GetURL(
      "b.test", "/attribution_reporting/register_source_headers.html");
  EXPECT_TRUE(ExecJs(web_contents(), JsReplace(R"(
    createAttributionSrcAnchor({id: 'link',
                        url: 'page_with_conversion_redirect.html',
                        attributionsrc: $1});)",
                                               register_source_url)));

  SourceObserver source_observer(web_contents());
  EXPECT_TRUE(ExecJs(shell(), "simulateClick('link');"));

  // We should see a null impression on the navigation
  EXPECT_TRUE(source_observer.WaitForNavigationWithNoImpression());
}

IN_PROC_BROWSER_TEST_F(
    AttributionSourceDeclarationBrowserTest,
    ImpressionInSubframeWithoutPermissionsPolicy_Registered) {
  GURL page_url = https_server()->GetURL("b.test", "/page_with_iframe.html");
  EXPECT_TRUE(NavigateToURL(web_contents(), page_url));

  GURL subframe_url =
      https_server()->GetURL("c.test", "/page_with_impression_creator.html");
  NavigateIframeToURL(web_contents(), "test_iframe", subframe_url);

  RenderFrameHost* subframe =
      ChildFrameAt(web_contents()->GetPrimaryMainFrame(), 0);

  GURL register_source_url = https_server()->GetURL(
      "b.test", "/attribution_reporting/register_source_headers.html");
  EXPECT_TRUE(ExecJs(subframe, JsReplace(R"(
    createAttributionSrcAnchor({id: 'link',
                        url: 'page_with_conversion_redirect.html',
                        attributionsrc: $1});)",
                                         register_source_url)));

  // For now, we expect an impression because the attribution-reporting
  // permission policy has a default of *.
  SourceObserver source_observer(web_contents());
  EXPECT_TRUE(ExecJs(subframe, "simulateClick('link');"));
  source_observer.Wait();
}

IN_PROC_BROWSER_TEST_F(
    AttributionSourceDeclarationBrowserTest,
    ImpressionInSubframeWithPermissionsPolicyDisabled_NotRegistered) {
  GURL page_url = https_server()->GetURL(
      "b.test", "/attribution_reporting/page_with_disallowed_iframe.html");
  EXPECT_TRUE(NavigateToURL(web_contents(), page_url));

  GURL subframe_url =
      https_server()->GetURL("c.test", "/page_with_impression_creator.html");
  NavigateIframeToURL(web_contents(), "test_iframe", subframe_url);

  RenderFrameHost* subframe =
      ChildFrameAt(web_contents()->GetPrimaryMainFrame(), 0);

  GURL register_source_url = https_server()->GetURL(
      "b.test", "/attribution_reporting/register_source_headers.html");
  EXPECT_TRUE(ExecJs(subframe, JsReplace(R"(
    createAttributionSrcAnchor({id: 'link',
                        url: 'page_with_conversion_redirect.html',
                        attributionsrc: $1});)",
                                         register_source_url)));

  // We should see a null impression on the navigation
  SourceObserver source_observer(web_contents());
  EXPECT_TRUE(ExecJs(subframe, "simulateClick('link');"));
  EXPECT_TRUE(source_observer.WaitForNavigationWithNoImpression());
}

IN_PROC_BROWSER_TEST_F(AttributionSourceDeclarationBrowserTest,
                       ImpressionInSubframeWithPermissionsPolicy_Registered) {
  GURL page_url = https_server()->GetURL("b.test", "/page_with_iframe.html");
  EXPECT_TRUE(NavigateToURL(web_contents(), page_url));
  EXPECT_TRUE(ExecJs(shell(), R"(
     let frame = document.getElementById('test_iframe');
     frame.setAttribute('allow', 'attribution-reporting');)"));

  GURL subframe_url =
      https_server()->GetURL("c.test", "/page_with_impression_creator.html");
  NavigateIframeToURL(web_contents(), "test_iframe", subframe_url);

  RenderFrameHost* subframe =
      ChildFrameAt(web_contents()->GetPrimaryMainFrame(), 0);

  GURL register_source_url = https_server()->GetURL(
      "b.test", "/attribution_reporting/register_source_headers.html");
  EXPECT_TRUE(ExecJs(subframe, JsReplace(R"(
    createAttributionSrcAnchor({id: 'link',
                        url: 'page_with_conversion_redirect.html',
                        attributionsrc: $1});)",
                                         register_source_url)));

  SourceObserver source_observer(web_contents());
  EXPECT_TRUE(ExecJs(subframe, "simulateClick('link');"));

  source_observer.Wait();
}

// Tests that when a context menu is shown, there is an impression attached to
// the ContextMenu data forwarded to the browser process.

// TODO(johnidel): SimulateMouseClickAt() does not work on Android, find a
// different way to invoke the context menu that works on Android.
// https://crbug.com/1219907 started flaking after Field Trial Testing Config
// was enabled for content_browsertests.
#if !BUILDFLAG(IS_ANDROID)
IN_PROC_BROWSER_TEST_F(AttributionSourceDeclarationBrowserTest,
                       DISABLED_ContextMenuShownForImpression_ImpressionSet) {
  EXPECT_TRUE(NavigateToURL(
      web_contents(),
      https_server()->GetURL("b.test", "/page_with_impression_creator.html")));

  GURL register_url =
      https_server()->GetURL("c.test", "/register_source_headers.html");
  EXPECT_TRUE(ExecJs(web_contents(), JsReplace(R"(
  createAttributionSrcAnchor({url: 'page_with_conversion_redirect.html',
                              attributionsrc: $1,
                              id: 'link2'});)",
                                               register_url)));

  // Allow the anchor to be rendered before trying to click on it.
  base::PlatformThread::Sleep(base::Milliseconds(50));

  auto context_menu_interceptor =
      std::make_unique<content::ContextMenuInterceptor>(
          web_contents()->GetPrimaryMainFrame(),
          ContextMenuInterceptor::ShowBehavior::kPreventShow);

  content::SimulateMouseClickAt(
      web_contents(), 0, blink::WebMouseEvent::Button::kRight,
      gfx::ToFlooredPoint(
          GetCenterCoordinatesOfElementWithId(web_contents(), "link2")));

  context_menu_interceptor->Wait();
  blink::UntrustworthyContextMenuParams params =
      context_menu_interceptor->get_params();
  EXPECT_TRUE(params.impression);
}
#endif  // !BUILDFLAG(IS_ANDROID)

IN_PROC_BROWSER_TEST_F(AttributionSourceDeclarationBrowserTest,
                       ImpressionNavigationReloads_NoImpression) {
  EXPECT_TRUE(NavigateToURL(
      web_contents(),
      https_server()->GetURL("b.test", "/page_with_impression_creator.html")));

  GURL register_url =
      https_server()->GetURL("c.test", "/register_source_headers.html");
  EXPECT_TRUE(ExecJs(web_contents(), JsReplace(R"(
  createAttributionSrcAnchor({url: 'page_with_conversion_redirect.html',
                              attributionsrc: $1,
                              id: 'link'});)",
                                               register_url)));

  SourceObserver source_observer(web_contents());
  EXPECT_TRUE(ExecJs(web_contents(), "simulateClick('link');"));
  source_observer.Wait();

  SourceObserver reload_observer(web_contents());
  shell()->Reload();

  // The reload navigation should not have an impression set.
  EXPECT_TRUE(reload_observer.WaitForNavigationWithNoImpression());
}

// Same as the above test but via a renderer initiated reload.
IN_PROC_BROWSER_TEST_F(AttributionSourceDeclarationBrowserTest,
                       RendererReloadImpressionNavigation_NoImpression) {
  EXPECT_TRUE(NavigateToURL(
      web_contents(),
      https_server()->GetURL("b.test", "/page_with_impression_creator.html")));

  GURL register_url =
      https_server()->GetURL("c.test", "/register_source_headers.html");
  EXPECT_TRUE(ExecJs(web_contents(), JsReplace(R"(
  createAttributionSrcAnchor({url: 'page_with_conversion_redirect.html',
                              attributionsrc: $1,
                              id: 'link'});)",
                                               register_url)));

  SourceObserver source_observer(web_contents());
  EXPECT_TRUE(ExecJs(web_contents(), "simulateClick('link');"));
  source_observer.Wait();

  SourceObserver reload_observer(web_contents());
  EXPECT_TRUE(ExecJs(web_contents(), "window.location.reload()"));

  // The reload navigation should not have an impression set.
  EXPECT_TRUE(reload_observer.WaitForNavigationWithNoImpression());
}

IN_PROC_BROWSER_TEST_F(AttributionSourceDeclarationBrowserTest,
                       BackNavigateToImpressionNavigation_NoImpression) {
  EXPECT_TRUE(NavigateToURL(
      web_contents(),
      https_server()->GetURL("b.test", "/page_with_impression_creator.html")));

  GURL register_url =
      https_server()->GetURL("c.test", "/register_source_headers.html");
  EXPECT_TRUE(ExecJs(web_contents(), JsReplace(R"(
  createAttributionSrcAnchor({url: 'page_with_conversion_redirect.html',
                              attributionsrc: $1,
                              id: 'link'});)",
                                               register_url)));

  SourceObserver source_observer(web_contents());
  EXPECT_TRUE(ExecJs(web_contents(), "simulateClick('link');"));
  source_observer.Wait();

  // Navigate away so we can back navigate to the impression's navigated page.
  EXPECT_TRUE(NavigateToURL(web_contents(), GURL("about:blank")));

  // The back navigation should not have an impression set.
  SourceObserver back_nav_observer(web_contents());
  shell()->GoBackOrForward(-1);
  EXPECT_TRUE(back_nav_observer.WaitForNavigationWithNoImpression());

  // Navigate back to the original page and ensure subsequent clicks also log
  // impressions.
  SourceObserver second_back_nav_observer(web_contents());
  shell()->GoBackOrForward(-1);
  EXPECT_TRUE(second_back_nav_observer.WaitForNavigationWithNoImpression());

  // Wait for the page to load and render the impression tag.
  EXPECT_TRUE(WaitForLoadStop(web_contents()));

  EXPECT_TRUE(ExecJs(web_contents(), JsReplace(R"(
  createAttributionSrcAnchor({url: 'page_with_conversion_redirect.html',
                              attributionsrc: $1,
                              id: 'link'});)",
                                               register_url)));

  SourceObserver second_impression_observer(web_contents());
  EXPECT_TRUE(ExecJs(web_contents(), "simulateClick('link');"));

  second_impression_observer.Wait();
}

IN_PROC_BROWSER_TEST_F(AttributionSourceDeclarationBrowserTest,
                       ImpressionInSubframeInInsecureContext_NotRegistered) {
  // Start with localhost(secure) iframing a.test (insecure) iframing
  // localhost(secure). This context is insecure since the middle iframe in the
  // ancestor chain is insecure.

  GURL main_frame_url =
      embedded_test_server()->GetURL("/page_with_iframe.html");
  EXPECT_TRUE(NavigateToURL(web_contents(), main_frame_url));

  EXPECT_TRUE(ExecJs(shell(), R"(
        let frame = document.getElementById('test_iframe');
        frame.setAttribute('allow', 'attribution-reporting');)"));

  GURL middle_iframe_url = embedded_test_server()->GetURL(
      "insecure.example", "/page_with_iframe.html");
  NavigateIframeToURL(web_contents(), "test_iframe", middle_iframe_url);

  RenderFrameHost* middle_iframe =
      ChildFrameAt(web_contents()->GetPrimaryMainFrame(), 0);

  GURL innermost_iframe_url(
      embedded_test_server()->GetURL("/page_with_impression_creator.html"));
  EXPECT_TRUE(ExecJs(middle_iframe, JsReplace(R"(
      let frame = document.getElementById('test_iframe');
      frame.setAttribute('allow', 'attribution-reporting');
      frame.src = $1;)",
                                              innermost_iframe_url)));
  EXPECT_TRUE(WaitForLoadStop(web_contents()));

  RenderFrameHost* innermost_iframe = ChildFrameAt(middle_iframe, 0);

  GURL register_url =
      https_server()->GetURL("c.test", "/register_source_headers.html");
  EXPECT_TRUE(ExecJs(innermost_iframe, JsReplace(R"(
  createAttributionSrcAnchor({url: 'page_with_conversion_redirect.html',
                              attributionsrc: $1,
                              id: 'link'});)",
                                                 register_url)));

  SourceObserver source_observer(web_contents());
  EXPECT_TRUE(ExecJs(innermost_iframe, "simulateClick('link');"));

  // We should see a null impression on the navigation.
  EXPECT_TRUE(source_observer.WaitForNavigationWithNoImpression());
}

}  // namespace
}  // namespace content
