// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <tuple>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/memory/ptr_util.h"
#include "base/optional.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind_test_util.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_timeouts.h"
#include "build/build_config.h"
#include "components/download/public/common/download_item.h"
#include "components/viz/host/host_frame_sink_manager.h"
#include "content/browser/compositor/surface_utils.h"
#include "content/browser/portal/portal.h"
#include "content/browser/renderer_host/input/synthetic_smooth_scroll_gesture.h"
#include "content/browser/renderer_host/input/synthetic_tap_gesture.h"
#include "content/browser/renderer_host/input/synthetic_touchpad_pinch_gesture.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/renderer_host/render_frame_host_manager.h"
#include "content/browser/renderer_host/render_frame_proxy_host.h"
#include "content/browser/renderer_host/render_widget_host_input_event_router.h"
#include "content/browser/renderer_host/render_widget_host_view_base.h"
#include "content/browser/renderer_host/render_widget_host_view_child_frame.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/common/frame.mojom-test-utils.h"
#include "content/common/input/synthetic_pinch_gesture_params.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/download_manager.h"
#include "content/public/browser/site_isolation_policy.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/page_type.h"
#include "content/public/test/accessibility_notification_waiter.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/hit_test_region_observer.h"
#include "content/public/test/navigation_handle_observer.h"
#include "content/public/test/render_frame_host_test_support.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/url_loader_interceptor.h"
#include "content/shell/browser/shell.h"
#include "content/shell/browser/shell_browser_context.h"
#include "content/shell/browser/shell_content_browser_client.h"
#include "content/test/content_browser_test_utils_internal.h"
#include "content/test/portal/portal_activated_observer.h"
#include "content/test/portal/portal_created_observer.h"
#include "content/test/portal/portal_interceptor_for_testing.h"
#include "content/test/test_render_frame_host_factory.h"
#include "net/base/escape.h"
#include "net/base/net_errors.h"
#include "net/base/url_util.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/input/focus_type.mojom.h"
#include "third_party/blink/public/mojom/portal/portal.mojom.h"
#include "url/gurl.h"
#include "url/url_constants.h"

using testing::_;

namespace content {

class PortalBrowserTest : public ContentBrowserTest {
 protected:
  PortalBrowserTest() {}

  void SetUp() override {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{blink::features::kPortals,
                              blink::features::kPortalsCrossOrigin},
        /*disabled_features=*/{});
    ContentBrowserTest::SetUp();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(switches::kValidateInputEventStream);
    command_line->AppendSwitchASCII(switches::kEnableBlinkFeatures,
                                    "OverscrollCustomization");
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    ContentBrowserTest::SetUpOnMainThread();
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  Portal* CreatePortalToUrl(WebContentsImpl* host_contents,
                            GURL portal_url,
                            int number_of_navigations = 1,
                            bool expected_to_succeed = true) {
    EXPECT_GE(number_of_navigations, 1);
    RenderFrameHostImpl* main_frame = host_contents->GetMainFrame();

    // Create portal and wait for navigation.
    PortalCreatedObserver portal_created_observer(main_frame);
    TestNavigationObserver navigation_observer(nullptr, number_of_navigations);
    navigation_observer.set_wait_event(
        TestNavigationObserver::WaitEvent::kNavigationFinished);
    navigation_observer.StartWatchingNewWebContents();
    EXPECT_TRUE(
        ExecJs(main_frame,
               JsReplace("{"
                         "  let portal = document.createElement('portal');"
                         "  portal.src = $1;"
                         "  document.body.appendChild(portal);"
                         "}",
                         portal_url),
               EXECUTE_SCRIPT_NO_USER_GESTURE));
    Portal* portal = portal_created_observer.WaitUntilPortalCreated();
    navigation_observer.StopWatchingNewWebContents();

    WebContentsImpl* portal_contents = portal->GetPortalContents();
    EXPECT_TRUE(portal_contents);

    navigation_observer.WaitForNavigationFinished();
    EXPECT_EQ(expected_to_succeed, WaitForLoadStop(portal_contents));

    return portal;
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Tests that the renderer can create a Portal.
IN_PROC_BROWSER_TEST_F(PortalBrowserTest, CreatePortal) {
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("portal.test", "/title1.html")));
  WebContentsImpl* web_contents_impl =
      static_cast<WebContentsImpl*>(shell()->web_contents());
  RenderFrameHostImpl* main_frame = web_contents_impl->GetMainFrame();

  PortalCreatedObserver portal_created_observer(main_frame);
  EXPECT_TRUE(
      ExecJs(main_frame,
             "document.body.appendChild(document.createElement('portal'));"));
  Portal* portal = portal_created_observer.WaitUntilPortalCreated();
  EXPECT_NE(nullptr, portal);
}

// This fixture enables CompositeCrossOriginIframes for the following regression
// test. Since we're testing stability and not behaviour specific to this flag,
// this fixture and its test may be removed upon removal of the
// CompositeCrossOriginIframes flag.
class PortalCompositeCrossOriginIframesBrowserTest : public PortalBrowserTest {
 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    PortalBrowserTest::SetUpCommandLine(command_line);
    scoped_feature_list_.InitAndEnableFeature(
        blink::features::kCompositeCrossOriginIframes);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Regression test for https://crbug.com/1028269
// Tests that the renderer can create a portal without crashing when
// CompositeCrossOriginIframes is enabled.
IN_PROC_BROWSER_TEST_F(PortalCompositeCrossOriginIframesBrowserTest,
                       CreatePortal) {
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("portal.test", "/title1.html")));
  WebContentsImpl* web_contents_impl =
      static_cast<WebContentsImpl*>(shell()->web_contents());
  RenderFrameHostImpl* main_frame = web_contents_impl->GetMainFrame();

  GURL a_url(embedded_test_server()->GetURL("a.com", "/title1.html"));
  Portal* portal = CreatePortalToUrl(web_contents_impl, a_url);
  EXPECT_NE(nullptr, portal);

  // Ensure that the renderer is still alive.
  EXPECT_EQ(true, EvalJs(main_frame, "true"));
}

// Tests the the renderer can navigate a Portal.
IN_PROC_BROWSER_TEST_F(PortalBrowserTest, NavigatePortal) {
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("portal.test", "/title1.html")));
  WebContentsImpl* web_contents_impl =
      static_cast<WebContentsImpl*>(shell()->web_contents());
  RenderFrameHostImpl* main_frame = web_contents_impl->GetMainFrame();

  // Tests that a portal can navigate by setting its src before appending it to
  // the DOM.
  GURL a_url(embedded_test_server()->GetURL("a.com", "/title1.html"));
  Portal* portal = CreatePortalToUrl(web_contents_impl, a_url);
  WebContents* portal_contents = portal->GetPortalContents();
  EXPECT_EQ(portal_contents->GetLastCommittedURL(), a_url);

  // Tests that a portal can navigate by setting its src.
  {
    TestNavigationObserver navigation_observer(portal_contents);

    GURL b_url(embedded_test_server()->GetURL("b.com", "/title1.html"));
    EXPECT_TRUE(
        ExecJs(main_frame,
               JsReplace("document.querySelector('portal').src = $1;", b_url)));
    navigation_observer.Wait();
    EXPECT_EQ(navigation_observer.last_navigation_url(), b_url);
    EXPECT_EQ(portal_contents->GetLastCommittedURL(), b_url);
  }

  // Tests that a portal can navigate by setting the attribute src.
  {
    TestNavigationObserver navigation_observer(portal_contents);

    GURL c_url(embedded_test_server()->GetURL("c.com", "/title1.html"));
    EXPECT_TRUE(ExecJs(
        main_frame,
        JsReplace("document.querySelector('portal').setAttribute('src', $1);",
                  c_url)));
    navigation_observer.Wait();
    EXPECT_EQ(navigation_observer.last_navigation_url(), c_url);
    EXPECT_EQ(portal_contents->GetLastCommittedURL(), c_url);
  }
}

// Tests that a portal can be activated.
IN_PROC_BROWSER_TEST_F(PortalBrowserTest, ActivatePortal) {
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("portal.test", "/title1.html")));
  WebContentsImpl* web_contents_impl =
      static_cast<WebContentsImpl*>(shell()->web_contents());
  RenderFrameHostImpl* main_frame = web_contents_impl->GetMainFrame();

  GURL a_url(embedded_test_server()->GetURL("a.com", "/title1.html"));
  Portal* portal = CreatePortalToUrl(web_contents_impl, a_url);

  // Ensure that the portal WebContents exists and is different from the tab's
  // WebContents.
  WebContents* portal_contents = portal->GetPortalContents();
  EXPECT_NE(nullptr, portal_contents);
  EXPECT_NE(portal_contents, shell()->web_contents());

  PortalActivatedObserver activated_observer(portal);
  ExecuteScriptAsync(main_frame,
                     "document.querySelector('portal').activate();");
  activated_observer.WaitForActivate();

  // After activation, the shell's WebContents should be the previous portal's
  // WebContents.
  EXPECT_EQ(portal_contents, shell()->web_contents());

  EXPECT_EQ(blink::mojom::PortalActivateResult::kPredecessorWillUnload,
            activated_observer.WaitForActivateResult());
}

// Tests if a portal can be activated and the predecessor can be adopted.
IN_PROC_BROWSER_TEST_F(PortalBrowserTest, AdoptPredecessor) {
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("portal.test", "/title1.html")));
  WebContentsImpl* web_contents_impl =
      static_cast<WebContentsImpl*>(shell()->web_contents());
  RenderFrameHostImpl* main_frame = web_contents_impl->GetMainFrame();

  GURL a_url(embedded_test_server()->GetURL("a.com", "/title1.html"));
  Portal* portal = CreatePortalToUrl(web_contents_impl, a_url);

  // Ensure that the portal WebContents exists and is different from the tab's
  // WebContents.
  WebContentsImpl* portal_contents = portal->GetPortalContents();
  EXPECT_NE(nullptr, portal_contents);
  EXPECT_NE(portal_contents, shell()->web_contents());

  RenderFrameHostImpl* portal_frame = portal_contents->GetMainFrame();
  EXPECT_TRUE(ExecJs(portal_frame,
                     "window.addEventListener('portalactivate', e => { "
                     "  var portal = e.adoptPredecessor(); "
                     "  document.body.appendChild(portal); "
                     "});"));

  {
    PortalActivatedObserver activated_observer(portal);
    PortalCreatedObserver adoption_observer(portal_frame);
    EXPECT_TRUE(ExecJs(main_frame,
                       "let portal = document.querySelector('portal');"
                       "portal.activate().then(() => { "
                       "  document.body.removeChild(portal); "
                       "});"));
    EXPECT_EQ(blink::mojom::PortalActivateResult::kPredecessorWasAdopted,
              activated_observer.WaitForActivateResult());
    adoption_observer.WaitUntilPortalCreated();
  }
  // After activation, the shell's WebContents should be the previous portal's
  // WebContents.
  EXPECT_EQ(portal_contents, shell()->web_contents());
  // The original predecessor WebContents should be adopted as a portal.
  EXPECT_TRUE(web_contents_impl->IsPortal());
  EXPECT_EQ(web_contents_impl->GetOuterWebContents(), portal_contents);
}

// Tests that the RenderFrameProxyHost is created and initialized when the
// portal is initialized.
IN_PROC_BROWSER_TEST_F(PortalBrowserTest, RenderFrameProxyHostCreated) {
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("portal.test", "/title1.html")));
  WebContentsImpl* web_contents_impl =
      static_cast<WebContentsImpl*>(shell()->web_contents());

  GURL a_url(embedded_test_server()->GetURL("a.com", "/title1.html"));
  Portal* portal = CreatePortalToUrl(web_contents_impl, a_url);
  WebContentsImpl* portal_contents = portal->GetPortalContents();
  RenderFrameProxyHost* proxy_host = portal_contents->GetFrameTree()
                                         ->root()
                                         ->render_manager()
                                         ->GetProxyToOuterDelegate();
  EXPECT_TRUE(proxy_host->is_render_frame_proxy_live());
}

// Tests that the portal's outer delegate frame tree node and any iframes
// inside the portal are deleted when the portal element is removed from the
// document.
IN_PROC_BROWSER_TEST_F(PortalBrowserTest, DetachPortal) {
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("portal.test", "/title1.html")));
  WebContentsImpl* web_contents =
      static_cast<WebContentsImpl*>(shell()->web_contents());
  RenderFrameHostImpl* main_frame = web_contents->GetMainFrame();

  GURL a_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(a)"));
  // Wait for a second navigation for the inner iframe.
  Portal* portal = CreatePortalToUrl(web_contents, a_url, 2);

  WebContentsImpl* portal_contents = portal->GetPortalContents();
  FrameTreeNode* portal_main_frame_node =
      portal_contents->GetFrameTree()->root();

  // Remove portal from document and wait for frames to be deleted.
  FrameDeletedObserver fdo1(portal_main_frame_node->render_manager()
                                ->GetOuterDelegateNode()
                                ->current_frame_host());
  FrameDeletedObserver fdo2(
      portal_main_frame_node->child_at(0)->current_frame_host());
  EXPECT_TRUE(
      ExecJs(main_frame,
             "document.body.removeChild(document.querySelector('portal'));"));
  fdo1.Wait();
  fdo2.Wait();
}

// This is for testing how portals interact with input hit testing. It is
// parameterized on the kind of viz hit testing used.
class PortalHitTestBrowserTest : public PortalBrowserTest {
 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    PortalBrowserTest::SetUpCommandLine(command_line);
    IsolateAllSitesForTesting(command_line);
  }
};

namespace {

// Fails the test if an input event is sent to the given RenderWidgetHost.
class FailOnInputEvent : public RenderWidgetHost::InputEventObserver {
 public:
  explicit FailOnInputEvent(RenderWidgetHostImpl* rwh)
      : rwh_(rwh->GetWeakPtr()) {
    rwh->AddInputEventObserver(this);
  }

  ~FailOnInputEvent() override {
    if (rwh_)
      rwh_->RemoveInputEventObserver(this);
  }

  void OnInputEvent(const blink::WebInputEvent& event) override {
    FAIL() << "Unexpected " << blink::WebInputEvent::GetName(event.GetType());
  }

 private:
  base::WeakPtr<RenderWidgetHostImpl> rwh_;
};

}  // namespace

// Tests that input events targeting the portal are only received by the parent
// renderer.
IN_PROC_BROWSER_TEST_F(PortalHitTestBrowserTest, DispatchInputEvent) {
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("portal.test", "/title1.html")));
  WebContentsImpl* web_contents_impl =
      static_cast<WebContentsImpl*>(shell()->web_contents());
  RenderFrameHostImpl* main_frame = web_contents_impl->GetMainFrame();

  GURL a_url(embedded_test_server()->GetURL("a.com", "/title1.html"));
  Portal* portal = CreatePortalToUrl(web_contents_impl, a_url);
  WebContentsImpl* portal_contents = portal->GetPortalContents();
  RenderFrameHostImpl* portal_frame = portal_contents->GetMainFrame();
  EXPECT_TRUE(static_cast<RenderWidgetHostViewBase*>(portal_frame->GetView())
                  ->IsRenderWidgetHostViewChildFrame());
  RenderWidgetHostViewChildFrame* portal_view =
      static_cast<RenderWidgetHostViewChildFrame*>(portal_frame->GetView());
  WaitForHitTestData(portal_frame);

  FailOnInputEvent no_input_to_portal_frame(
      portal_frame->GetRenderWidgetHost());
  EXPECT_TRUE(ExecJs(
      main_frame,
      "var clicked = false;"
      "document.querySelector('portal').onmousedown = _ => clicked = true;"));
  EXPECT_TRUE(ExecJs(portal_frame,
                     "var clicked = false;"
                     "document.body.onmousedown = _ => clicked = true;"));
  EXPECT_EQ(false, EvalJs(main_frame, "clicked"));
  EXPECT_EQ(false, EvalJs(portal_frame, "clicked"));

  // Route the mouse event.
  gfx::Point root_location =
      portal_view->TransformPointToRootCoordSpace(gfx::Point(5, 5));
  InputEventAckWaiter waiter(main_frame->GetRenderWidgetHost(),
                             blink::WebInputEvent::Type::kMouseDown);
  SimulateMouseEvent(web_contents_impl, blink::WebInputEvent::Type::kMouseDown,
                     blink::WebPointerProperties::Button::kLeft, root_location);
  waiter.Wait();

  // Check that the click event was only received by the main frame.
  EXPECT_EQ(true, EvalJs(main_frame, "clicked"));
  EXPECT_EQ(false, EvalJs(portal_frame, "clicked"));
}

// Tests that input events performed over on OOPIF inside a portal are targeted
// to the portal's parent.
IN_PROC_BROWSER_TEST_F(PortalHitTestBrowserTest, NoInputToOOPIFInPortal) {
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("portal.test", "/title1.html")));
  WebContentsImpl* web_contents_impl =
      static_cast<WebContentsImpl*>(shell()->web_contents());
  RenderFrameHostImpl* main_frame = web_contents_impl->GetMainFrame();

  // Create portal and wait for navigation.
  // In the case of crbug.com/1002228 , this does not appear to reproduce if the
  // portal element is too small, so we give it an explicit size.
  Portal* portal = nullptr;
  {
    PortalCreatedObserver portal_created_observer(main_frame);
    GURL a_url(embedded_test_server()->GetURL("a.com", "/title1.html"));
    TestNavigationObserver navigation_observer(a_url);
    navigation_observer.StartWatchingNewWebContents();
    EXPECT_TRUE(ExecJs(
        main_frame, JsReplace("var portal = document.createElement('portal');"
                              "portal.src = $1;"
                              "portal.style.width = '500px';"
                              "portal.style.height = '500px';"
                              "portal.style.border = 'solid';"
                              "document.body.appendChild(portal);",
                              a_url)));
    portal = portal_created_observer.WaitUntilPortalCreated();
    navigation_observer.Wait();
  }
  WebContentsImpl* portal_contents = portal->GetPortalContents();
  RenderFrameHostImpl* portal_frame = portal_contents->GetMainFrame();
  WaitForHitTestData(portal_frame);

  // Add an out-of-process iframe to the portal.
  GURL b_url(embedded_test_server()->GetURL("b.com", "/title1.html"));
  TestNavigationObserver iframe_navigation_observer(portal_contents);
  EXPECT_TRUE(ExecJs(portal_frame,
                     JsReplace("var iframe = document.createElement('iframe');"
                               "iframe.src = $1;"
                               "document.body.appendChild(iframe);",
                               b_url)));
  iframe_navigation_observer.Wait();
  EXPECT_EQ(b_url, iframe_navigation_observer.last_navigation_url());
  RenderFrameHostImpl* portal_iframe =
      portal_frame->child_at(0)->current_frame_host();
  EXPECT_TRUE(static_cast<RenderWidgetHostViewBase*>(portal_iframe->GetView())
                  ->IsRenderWidgetHostViewChildFrame());
  RenderWidgetHostViewChildFrame* oopif_view =
      static_cast<RenderWidgetHostViewChildFrame*>(portal_iframe->GetView());
  EXPECT_NE(portal_frame->GetSiteInstance(), portal_iframe->GetSiteInstance());
  WaitForHitTestData(portal_iframe);

  FailOnInputEvent no_input_to_portal_frame(
      portal_frame->GetRenderWidgetHost());
  FailOnInputEvent no_input_to_oopif(portal_iframe->GetRenderWidgetHost());
  EXPECT_TRUE(ExecJs(main_frame,
                     "var clicked = false;"
                     "portal.onmousedown = _ => clicked = true;"));
  EXPECT_TRUE(ExecJs(portal_frame,
                     "var clicked = false;"
                     "document.body.onmousedown = _ => clicked = true;"));
  EXPECT_TRUE(ExecJs(portal_iframe,
                     "var clicked = false;"
                     "document.body.onmousedown = _ => clicked = true;"));

  // Route the mouse event.
  gfx::Point root_location =
      oopif_view->TransformPointToRootCoordSpace(gfx::Point(5, 5));
  InputEventAckWaiter waiter(main_frame->GetRenderWidgetHost(),
                             blink::WebInputEvent::Type::kMouseDown);
  SimulateMouseEvent(web_contents_impl, blink::WebInputEvent::Type::kMouseDown,
                     blink::WebPointerProperties::Button::kLeft, root_location);
  waiter.Wait();

  // Check that the click event was only received by the main frame.
  EXPECT_EQ(true, EvalJs(main_frame, "clicked"));
  EXPECT_EQ(false, EvalJs(portal_frame, "clicked"));
  EXPECT_EQ(false, EvalJs(portal_iframe, "clicked"));
}

// Tests that an OOPIF inside a portal receives input events after the portal is
// activated.
// Flaky on macOS: https://crbug.com/1042703
#if defined(OS_MAC)
#define MAYBE_InputToOOPIFAfterActivation DISABLED_InputToOOPIFAfterActivation
#else
#define MAYBE_InputToOOPIFAfterActivation InputToOOPIFAfterActivation
#endif
IN_PROC_BROWSER_TEST_F(PortalHitTestBrowserTest,
                       MAYBE_InputToOOPIFAfterActivation) {
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("portal.test", "/title1.html")));
  WebContentsImpl* web_contents_impl =
      static_cast<WebContentsImpl*>(shell()->web_contents());
  RenderFrameHostImpl* main_frame = web_contents_impl->GetMainFrame();

  // Create portal.
  // TODO(crbug.com/1029330): We currently need to give portal a large enough
  // size to prevent overlap with iframe as this results in the test becoming
  // flaky.
  Portal* portal = nullptr;
  {
    PortalCreatedObserver portal_created_observer(main_frame);
    GURL a_url(embedded_test_server()->GetURL("a.com", "/title1.html"));
    TestNavigationObserver navigation_observer(a_url);
    navigation_observer.StartWatchingNewWebContents();
    EXPECT_TRUE(ExecJs(
        main_frame, JsReplace("var portal = document.createElement('portal');"
                              "portal.src = $1;"
                              "portal.style.width = '500px';"
                              "portal.style.height = '500px';"
                              "document.body.appendChild(portal);",
                              a_url)));
    portal = portal_created_observer.WaitUntilPortalCreated();
    navigation_observer.Wait();
  }
  WebContentsImpl* portal_contents = portal->GetPortalContents();
  RenderFrameHostImpl* portal_frame = portal_contents->GetMainFrame();
  WaitForHitTestData(portal_frame);

  // Add an out-of-process iframe to the portal.
  GURL b_url(embedded_test_server()->GetURL("b.com", "/title1.html"));
  TestNavigationObserver iframe_navigation_observer(portal_contents);
  EXPECT_TRUE(ExecJs(portal_frame,
                     JsReplace("var iframe = document.createElement('iframe');"
                               "iframe.src = $1;"
                               "document.body.appendChild(iframe);",
                               b_url)));
  iframe_navigation_observer.Wait();
  EXPECT_EQ(b_url, iframe_navigation_observer.last_navigation_url());

  RenderFrameHostImpl* oopif = portal_frame->child_at(0)->current_frame_host();
  RenderWidgetHostViewBase* oopif_view =
      static_cast<RenderWidgetHostViewBase*>(oopif->GetView());
  EXPECT_TRUE(oopif_view->IsRenderWidgetHostViewChildFrame());
  EXPECT_NE(portal_frame->GetSiteInstance(), oopif->GetSiteInstance());
  WaitForHitTestData(oopif);
  EXPECT_TRUE(ExecJs(oopif,
                     "var clicked = false;"
                     "document.body.onmousedown = _ => clicked = true;"));

  // Activate the portal.
  {
    PortalActivatedObserver activated_observer(portal);
    EXPECT_TRUE(ExecJs(main_frame,
                       "let portal = document.querySelector('portal');"
                       "portal.activate().then(() => { "
                       "  document.body.removeChild(portal); "
                       "});"));
    activated_observer.WaitForActivate();

    RenderWidgetHostViewBase* view =
        portal_frame->GetRenderWidgetHost()->GetView();
    viz::FrameSinkId root_frame_sink_id = view->GetRootFrameSinkId();
    HitTestRegionObserver hit_test_observer(root_frame_sink_id);

    // The hit test region for the portal frame should be at index 1 after
    // activation, so we wait for the hit test data to update until it's in
    // this state.
    auto hit_test_index = [&]() -> base::Optional<size_t> {
      const auto& display_hit_test_query_map =
          GetHostFrameSinkManager()->display_hit_test_query();
      auto it = display_hit_test_query_map.find(root_frame_sink_id);
      // On Mac, we create a new root layer after activation, so the hit test
      // data may not have anything for the new layer yet.
      if (it == display_hit_test_query_map.end())
        return base::nullopt;
      CHECK_EQ(portal_frame->GetRenderWidgetHost()->GetView(), view);
      size_t index;
      if (!it->second->FindIndexOfFrameSink(view->GetFrameSinkId(), &index))
        return base::nullopt;
      return index;
    };
    hit_test_observer.WaitForHitTestData();
    while (hit_test_index() != 1u)
      hit_test_observer.WaitForHitTestDataChange();
  }
  EXPECT_EQ(shell()->web_contents(), portal_contents);

  // Send a mouse event to the OOPIF.
  gfx::Point root_location =
      oopif_view->TransformPointToRootCoordSpace(gfx::Point(10, 10));
  InputEventAckWaiter waiter(oopif->GetRenderWidgetHost(),
                             blink::WebInputEvent::Type::kMouseDown);
  SimulateMouseEvent(shell()->web_contents(),
                     blink::WebInputEvent::Type::kMouseDown,
                     blink::WebPointerProperties::Button::kLeft, root_location);
  waiter.Wait();

  // Check that the click event was received by the iframe.
  EXPECT_EQ(true, EvalJs(oopif, "clicked"));
}

// Tests that async hit testing does not target portals.
IN_PROC_BROWSER_TEST_F(PortalBrowserTest, AsyncEventTargetingIgnoresPortals) {
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("portal.test", "/title1.html")));
  WebContentsImpl* web_contents_impl =
      static_cast<WebContentsImpl*>(shell()->web_contents());
  RenderFrameHostImpl* main_frame = web_contents_impl->GetMainFrame();

  GURL a_url(embedded_test_server()->GetURL("a.com", "/title1.html"));
  Portal* portal = CreatePortalToUrl(web_contents_impl, a_url);
  WebContentsImpl* portal_contents = portal->GetPortalContents();
  RenderFrameHostImpl* portal_frame = portal_contents->GetMainFrame();
  ASSERT_TRUE(static_cast<RenderWidgetHostViewBase*>(portal_frame->GetView())
                  ->IsRenderWidgetHostViewChildFrame());
  RenderWidgetHostViewChildFrame* portal_view =
      static_cast<RenderWidgetHostViewChildFrame*>(portal_frame->GetView());
  WaitForHitTestData(portal_frame);

  viz::mojom::InputTargetClient* target_client =
      main_frame->GetRenderWidgetHost()->input_target_client().get();
  ASSERT_TRUE(target_client);

  gfx::PointF root_location =
      portal_view->TransformPointToRootCoordSpaceF(gfx::PointF(5, 5));

  // Query the renderer for the target widget. The root should claim the point
  // for itself, not the portal.
  base::RunLoop run_loop;
  base::OnceClosure quit_closure = run_loop.QuitClosure();
  viz::FrameSinkId received_frame_sink_id;
  target_client->FrameSinkIdAt(
      root_location, 0,
      base::BindLambdaForTesting(
          [&](const viz::FrameSinkId& id, const gfx::PointF& point) {
            received_frame_sink_id = id;
            std::move(quit_closure).Run();
          }));
  run_loop.Run();

  viz::FrameSinkId root_frame_sink_id =
      static_cast<RenderWidgetHostViewBase*>(main_frame->GetView())
          ->GetFrameSinkId();
  EXPECT_EQ(root_frame_sink_id, received_frame_sink_id)
      << "Note: The portal's FrameSinkId is " << portal_view->GetFrameSinkId();
}

// Tests that trying to navigate to a chrome:// URL kills the renderer.
IN_PROC_BROWSER_TEST_F(PortalBrowserTest, NavigateToChrome) {
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("portal.test", "/title1.html")));
  WebContentsImpl* web_contents_impl =
      static_cast<WebContentsImpl*>(shell()->web_contents());
  RenderFrameHostImpl* main_frame = web_contents_impl->GetMainFrame();

  // Create portal.
  PortalCreatedObserver portal_created_observer(main_frame);
  EXPECT_TRUE(ExecJs(main_frame,
                     "var portal = document.createElement('portal');"
                     "document.body.appendChild(portal);"));
  Portal* portal = portal_created_observer.WaitUntilPortalCreated();
  PortalInterceptorForTesting* portal_interceptor =
      PortalInterceptorForTesting::From(portal);

  // Try to navigate to chrome://settings and wait for the process to die.
  portal_interceptor->SetNavigateCallback(base::BindRepeating(
      [](Portal* portal, const GURL& url, blink::mojom::ReferrerPtr referrer,
         blink::mojom::Portal::NavigateCallback callback) {
        GURL chrome_url("chrome://settings");
        portal->Navigate(chrome_url, std::move(referrer), std::move(callback));
      },
      portal));
  RenderProcessHostBadIpcMessageWaiter kill_waiter(main_frame->GetProcess());
  GURL a_url(embedded_test_server()->GetURL("a.com", "/title1.html"));
  ignore_result(ExecJs(main_frame, JsReplace("portal.src = $1;", a_url)));

  EXPECT_EQ(bad_message::RPH_MOJO_PROCESS_ERROR, kill_waiter.Wait());
}

// Regression test for crbug.com/969714. Tests that receiving a touch ack
// from the predecessor after portal activation doesn't cause a crash.
IN_PROC_BROWSER_TEST_F(PortalBrowserTest, TouchAckAfterActivate) {
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("portal.test", "/title1.html")));
  WebContentsImpl* web_contents_impl =
      static_cast<WebContentsImpl*>(shell()->web_contents());
  RenderFrameHostImpl* main_frame = web_contents_impl->GetMainFrame();

  GURL a_url(embedded_test_server()->GetURL("a.com", "/title1.html"));
  Portal* portal = CreatePortalToUrl(web_contents_impl, a_url);
  EXPECT_TRUE(
      ExecJs(main_frame,
             JsReplace("document.body.addEventListener('touchstart', e => {"
                       "  document.querySelector('portal').activate();"
                       "}, {passive: false});")));
  WebContentsImpl* portal_contents = portal->GetPortalContents();

  RenderWidgetHostImpl* render_widget_host = main_frame->GetRenderWidgetHost();
  RenderFrameHostImpl* portal_frame = portal_contents->GetMainFrame();
  RenderWidgetHostViewChildFrame* portal_view =
      static_cast<RenderWidgetHostViewChildFrame*>(portal_frame->GetView());
  InputEventAckWaiter input_event_ack_waiter(
      render_widget_host, blink::WebInputEvent::Type::kTouchStart);
  WaitForHitTestData(portal_frame);

  SyntheticTapGestureParams params;
  params.gesture_source_type = SyntheticGestureParams::TOUCH_INPUT;
  params.position =
      portal_view->TransformPointToRootCoordSpaceF(gfx::PointF(5, 5));

  PortalActivatedObserver activated_observer(portal);
  std::unique_ptr<SyntheticTapGesture> gesture =
      std::make_unique<SyntheticTapGesture>(params);
  render_widget_host->QueueSyntheticGesture(std::move(gesture),
                                            base::DoNothing());
  activated_observer.WaitForActivate();
  EXPECT_EQ(portal_contents, shell()->web_contents());

  // Wait for a touch ack to be sent from the predecessor.
  input_event_ack_waiter.Wait();
}

// Regression test for crbug.com/973647. Tests that receiving a touch ack
// after activation and predecessor adoption doesn't cause a crash.
IN_PROC_BROWSER_TEST_F(PortalBrowserTest, TouchAckAfterActivateAndAdopt) {
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("portal.test", "/title1.html")));
  WebContentsImpl* web_contents_impl =
      static_cast<WebContentsImpl*>(shell()->web_contents());
  RenderFrameHostImpl* main_frame = web_contents_impl->GetMainFrame();

  GURL a_url(embedded_test_server()->GetURL("a.com", "/title1.html"));
  Portal* portal = CreatePortalToUrl(web_contents_impl, a_url);
  const int TOUCH_ACK_DELAY_IN_MILLISECONDS = 500;
  EXPECT_TRUE(
      ExecJs(main_frame,
             JsReplace("document.body.addEventListener('touchstart', e => {"
                       "  document.querySelector('portal').activate();"
                       "  var stop = performance.now() + $1;"
                       "  while (performance.now() < stop) {}"
                       "}, {passive: false});",
                       TOUCH_ACK_DELAY_IN_MILLISECONDS)));
  WebContentsImpl* portal_contents = portal->GetPortalContents();

  RenderFrameHostImpl* portal_frame = portal_contents->GetMainFrame();
  EXPECT_TRUE(ExecJs(portal_frame,
                     "window.addEventListener('portalactivate', e => {"
                     "  var portal = e.adoptPredecessor();"
                     "  document.body.appendChild(portal);"
                     "});"));
  WaitForHitTestData(portal_frame);

  RenderWidgetHostImpl* render_widget_host = main_frame->GetRenderWidgetHost();
  InputEventAckWaiter input_event_ack_waiter(
      render_widget_host, blink::WebInputEvent::Type::kTouchStart);

  SyntheticTapGestureParams params;
  RenderWidgetHostViewChildFrame* portal_view =
      static_cast<RenderWidgetHostViewChildFrame*>(portal_frame->GetView());
  params.gesture_source_type = SyntheticGestureParams::TOUCH_INPUT;
  params.position =
      portal_view->TransformPointToRootCoordSpaceF(gfx::PointF(5, 5));

  std::unique_ptr<SyntheticTapGesture> gesture =
      std::make_unique<SyntheticTapGesture>(params);
  {
    PortalActivatedObserver activated_observer(portal);
    PortalCreatedObserver adoption_observer(portal_frame);
    render_widget_host->QueueSyntheticGesture(std::move(gesture),
                                              base::DoNothing());
    activated_observer.WaitForActivate();
    EXPECT_EQ(portal_contents, shell()->web_contents());
    // Wait for predecessor to be adopted.
    adoption_observer.WaitUntilPortalCreated();
  }

  // Wait for a touch ack to be sent from the predecessor.
  input_event_ack_waiter.Wait();
}

// Regression test for crbug.com/973647. Tests that receiving a touch ack
// after activation and reactivating a predecessor doesn't cause a crash.
IN_PROC_BROWSER_TEST_F(PortalBrowserTest, TouchAckAfterActivateAndReactivate) {
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("portal.test", "/title1.html")));
  WebContentsImpl* web_contents_impl =
      static_cast<WebContentsImpl*>(shell()->web_contents());
  RenderFrameHostImpl* main_frame = web_contents_impl->GetMainFrame();

  GURL a_url(embedded_test_server()->GetURL("a.com", "/title1.html"));
  Portal* portal = CreatePortalToUrl(web_contents_impl, a_url);
  const int TOUCH_ACK_DELAY_IN_MILLISECONDS = 500;
  EXPECT_TRUE(
      ExecJs(main_frame,
             JsReplace("document.body.addEventListener('touchstart', e => {"
                       "  document.querySelector('portal').activate();"
                       "  var stop = performance.now() + $1;"
                       "  while (performance.now() < stop) {}"
                       "}, {passive: false});",
                       TOUCH_ACK_DELAY_IN_MILLISECONDS)));
  WebContentsImpl* portal_contents = portal->GetPortalContents();

  RenderFrameHostImpl* portal_frame = portal_contents->GetMainFrame();
  EXPECT_TRUE(ExecJs(portal_frame,
                     "window.addEventListener('portalactivate', e => {"
                     "  var portal = e.adoptPredecessor();"
                     "  document.body.appendChild(portal);"
                     "  portal.activate();"
                     "});"));
  WaitForHitTestData(portal_frame);

  RenderWidgetHostImpl* render_widget_host = main_frame->GetRenderWidgetHost();
  InputEventAckWaiter input_event_ack_waiter(
      render_widget_host, blink::WebInputEvent::Type::kTouchStart);

  SyntheticTapGestureParams params;
  params.gesture_source_type = SyntheticGestureParams::TOUCH_INPUT;
  params.position = gfx::PointF(20, 20);

  std::unique_ptr<SyntheticTapGesture> gesture =
      std::make_unique<SyntheticTapGesture>(params);

  base::Optional<PortalActivatedObserver> predecessor_activated;
  {
    PortalCreatedObserver adoption_observer(portal_frame);
    adoption_observer.set_created_callback(base::BindLambdaForTesting(
        [&](Portal* portal) { predecessor_activated.emplace(portal); }));

    PortalActivatedObserver activated_observer(portal);
    render_widget_host->QueueSyntheticGesture(std::move(gesture),
                                              base::DoNothing());
    activated_observer.WaitForActivate();
    EXPECT_EQ(portal_contents, shell()->web_contents());

    adoption_observer.WaitUntilPortalCreated();
  }

  predecessor_activated->WaitForActivate();
  // Sanity check to see if the predecessor was reactivated.
  EXPECT_EQ(web_contents_impl, shell()->web_contents());

  // Wait for a touch ack to be sent from the predecessor.
  input_event_ack_waiter.Wait();
}

// TODO(crbug.com/985078): Fix on Mac.
#if !defined(OS_MAC)
IN_PROC_BROWSER_TEST_F(PortalBrowserTest, TouchStateClearedBeforeActivation) {
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("portal.test", "/title1.html")));
  WebContentsImpl* web_contents_impl =
      static_cast<WebContentsImpl*>(shell()->web_contents());
  RenderFrameHostImpl* main_frame = web_contents_impl->GetMainFrame();

  GURL a_url(embedded_test_server()->GetURL("a.com", "/title1.html"));
  Portal* portal = CreatePortalToUrl(web_contents_impl, a_url);
  EXPECT_TRUE(
      ExecJs(main_frame,
             JsReplace("document.body.addEventListener('touchstart', e => {"
                       "  document.querySelector('portal').activate();"
                       "}, {passive: false});")));
  WebContentsImpl* portal_contents = portal->GetPortalContents();

  RenderFrameHostImpl* portal_frame = portal_contents->GetMainFrame();
  EXPECT_TRUE(ExecJs(portal_frame,
                     "window.addEventListener('portalactivate', e => {"
                     "  var portal = e.adoptPredecessor();"
                     "  document.body.appendChild(portal);"
                     "  portal.activate();"
                     "});"));
  WaitForHitTestData(portal_frame);

  RenderWidgetHostImpl* render_widget_host = main_frame->GetRenderWidgetHost();

  SyntheticTapGestureParams params;
  params.gesture_source_type = SyntheticGestureParams::TOUCH_INPUT;
  params.position = gfx::PointF(20, 20);

  // Activate the portal, and then wait for the predecessor to be reactivated.
  base::Optional<PortalActivatedObserver> adopted_activated;
  {
    PortalCreatedObserver adoption_observer(portal_frame);
    adoption_observer.set_created_callback(base::BindLambdaForTesting(
        [&](Portal* portal) { adopted_activated.emplace(portal); }));

    PortalActivatedObserver activated_observer(portal);
    std::unique_ptr<SyntheticTapGesture> gesture =
        std::make_unique<SyntheticTapGesture>(params);
    InputEventAckWaiter input_event_ack_waiter(
        render_widget_host, blink::WebInputEvent::Type::kTouchCancel);
    render_widget_host->QueueSyntheticGestureCompleteImmediately(
        std::move(gesture));
    // Wait for synthetic cancel event to be sent.
    input_event_ack_waiter.Wait();
    activated_observer.WaitForActivate();
    EXPECT_EQ(portal_contents, shell()->web_contents());

    adoption_observer.WaitUntilPortalCreated();
  }
  adopted_activated->WaitForActivate();
  // Sanity check to see if the predecessor was reactivated.
  EXPECT_EQ(web_contents_impl, shell()->web_contents());

  InputEventAckWaiter input_event_ack_waiter(
      render_widget_host, blink::WebInputEvent::Type::kTouchStart);
  std::unique_ptr<SyntheticTapGesture> gesture =
      std::make_unique<SyntheticTapGesture>(params);
  render_widget_host->QueueSyntheticGesture(std::move(gesture),
                                            base::DoNothing());
  // Waits for touch to be acked. If touch state wasn't cleared before initial
  // activation, a DCHECK will be hit before the ack is sent.
  input_event_ack_waiter.Wait();
}
#endif

// TODO(crbug.com/985078): Fix on Mac.
#if !defined(OS_MAC)
IN_PROC_BROWSER_TEST_F(PortalBrowserTest, GestureCleanedUpBeforeActivation) {
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("portal.test", "/title1.html")));
  WebContentsImpl* web_contents_impl =
      static_cast<WebContentsImpl*>(shell()->web_contents());
  RenderFrameHostImpl* main_frame = web_contents_impl->GetMainFrame();

  GURL a_url(embedded_test_server()->GetURL("a.com", "/title1.html"));
  Portal* portal = CreatePortalToUrl(web_contents_impl, a_url);
  EXPECT_TRUE(
      ExecJs(main_frame,
             JsReplace("document.body.addEventListener('touchstart', e => {"
                       "  document.querySelector('portal').activate();"
                       "}, {once: true});")));
  WebContentsImpl* portal_contents = portal->GetPortalContents();
  RenderFrameHostImpl* portal_frame = portal_contents->GetMainFrame();
  EXPECT_TRUE(ExecJs(portal_frame,
                     "window.addEventListener('portalactivate', e => {"
                     "  var portal = e.adoptPredecessor();"
                     "  document.body.appendChild(portal);"
                     "  portal.activate(); "
                     "});"));
  WaitForHitTestData(portal_frame);

  RenderWidgetHostImpl* render_widget_host = main_frame->GetRenderWidgetHost();

  SyntheticTapGestureParams params;
  params.gesture_source_type = SyntheticGestureParams::TOUCH_INPUT;
  params.position = gfx::PointF(20, 20);
  params.duration_ms = 1;

  // Simulate a tap and activate the portal.
  base::Optional<PortalActivatedObserver> adopted_activated;
  {
    PortalCreatedObserver adoption_observer(portal_frame);
    adoption_observer.set_created_callback(base::BindLambdaForTesting(
        [&](Portal* portal) { adopted_activated.emplace(portal); }));

    PortalActivatedObserver activated_observer(portal);
    std::unique_ptr<SyntheticTapGesture> gesture =
        std::make_unique<SyntheticTapGesture>(params);
    render_widget_host->QueueSyntheticGestureCompleteImmediately(
        std::move(gesture));
    activated_observer.WaitForActivate();
    EXPECT_EQ(portal_contents, shell()->web_contents());

    adoption_observer.WaitUntilPortalCreated();
  }

  // Wait for predecessor to be reactivated.
  adopted_activated->WaitForActivate();
  EXPECT_EQ(web_contents_impl, shell()->web_contents());

  // Simulate another tap.
  InputEventAckWaiter input_event_ack_waiter(
      render_widget_host, blink::WebInputEvent::Type::kGestureTap);
  auto gesture = std::make_unique<SyntheticTapGesture>(params);
  render_widget_host->QueueSyntheticGesture(std::move(gesture),
                                            base::DoNothing());
  // Wait for the tap gesture ack. If the initial gesture wasn't cleaned up, the
  // new gesture created will cause an error in the gesture validator.
  input_event_ack_waiter.Wait();
}
#endif

// Touch input transfer is only implemented in the content layer for Aura.
#if defined(USE_AURA)
IN_PROC_BROWSER_TEST_F(PortalBrowserTest, TouchInputTransferAcrossActivation) {
  EXPECT_TRUE(NavigateToURL(
      shell(),
      embedded_test_server()->GetURL("portal.test", "/portals/scroll.html")));
  WebContentsImpl* web_contents_impl =
      static_cast<WebContentsImpl*>(shell()->web_contents());
  RenderFrameHostImpl* main_frame = web_contents_impl->GetMainFrame();
  RenderWidgetHostImpl* render_widget_host = main_frame->GetRenderWidgetHost();

  GURL portal_url(embedded_test_server()->GetURL(
      "portal.test", "/portals/scroll-portal.html"));
  Portal* portal = CreatePortalToUrl(web_contents_impl, portal_url);
  WebContentsImpl* portal_contents = portal->GetPortalContents();
  RenderFrameHostImpl* portal_frame = portal_contents->GetMainFrame();
  WaitForHitTestData(portal_frame);

  // Create and dispatch a synthetic scroll to trigger activation.
  SyntheticSmoothScrollGestureParams params;
  params.gesture_source_type = SyntheticGestureParams::TOUCH_INPUT;
  params.anchor =
      gfx::PointF(50, main_frame->GetView()->GetViewBounds().height() - 100);
  params.distances.push_back(-gfx::Vector2d(0, 100));

  std::unique_ptr<SyntheticSmoothScrollGesture> gesture =
      std::make_unique<SyntheticSmoothScrollGesture>(params);
  base::RunLoop run_loop;
  render_widget_host->QueueSyntheticGesture(
      std::move(gesture),
      base::BindLambdaForTesting([&](SyntheticGesture::Result result) {
        EXPECT_EQ(SyntheticGesture::Result::GESTURE_FINISHED, result);
        run_loop.Quit();
      }));
  run_loop.Run();

  // Check if the activated page scrolled.
  EXPECT_NE(0, EvalJs(portal_frame, "window.scrollY"));
}
#endif

// TODO(crbug.com/1010675): Test fails flakily.
// Touch input transfer is only implemented in the content layer for Aura.
#if defined(USE_AURA)
IN_PROC_BROWSER_TEST_F(PortalBrowserTest,
                       TouchInputTransferAcrossReactivation) {
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL(
                   "portal.test",
                   "/portals/touch-input-transfer-across-reactivation.html")));
  WebContentsImpl* web_contents_impl =
      static_cast<WebContentsImpl*>(shell()->web_contents());
  RenderFrameHostImpl* main_frame = web_contents_impl->GetMainFrame();
  RenderWidgetHostImpl* render_widget_host = main_frame->GetRenderWidgetHost();

  GURL portal_url(embedded_test_server()->GetURL(
      "portal.test", "/portals/reactivate-predecessor.html"));
  Portal* portal = CreatePortalToUrl(web_contents_impl, portal_url);
  WaitForHitTestData(main_frame);

  PortalActivatedObserver activated_observer(portal);

  // Create and dispatch a synthetic scroll to trigger activation.
  SyntheticSmoothScrollGestureParams params;
  params.gesture_source_type = SyntheticGestureParams::TOUCH_INPUT;
  params.anchor =
      gfx::PointF(50, main_frame->GetView()->GetViewBounds().height() - 100);
  params.distances.push_back(-gfx::Vector2d(0, 250));
  params.speed_in_pixels_s = 200;

  std::unique_ptr<SyntheticSmoothScrollGesture> gesture =
      std::make_unique<SyntheticSmoothScrollGesture>(params);
  base::RunLoop run_loop;
  render_widget_host->QueueSyntheticGesture(
      std::move(gesture),
      base::BindLambdaForTesting([&](SyntheticGesture::Result result) {
        EXPECT_EQ(SyntheticGesture::Result::GESTURE_FINISHED, result);
        run_loop.Quit();
      }));
  // Portal should activate when the gesture begins.
  activated_observer.WaitForActivate();
  // Wait till the scroll gesture finishes.
  run_loop.Run();
  // The predecessor should have been reactivated (we should be back to the
  // starting page).
  EXPECT_EQ(web_contents_impl, shell()->web_contents());
  // The starting page should have scrolled.
  // NOTE: This assumes that the scroll gesture is long enough that touch events
  // are still sent after the predecessor is reactivated.
  int scroll_y_after_portal_activate =
      EvalJs(main_frame, "scrollYAfterPortalActivate").ExtractInt();
  EXPECT_LT(scroll_y_after_portal_activate,
            EvalJs(main_frame, "window.scrollY"));
}
#endif

// Tests that the outer FrameTreeNode is deleted after activation.
IN_PROC_BROWSER_TEST_F(PortalBrowserTest, FrameDeletedAfterActivation) {
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("portal.test", "/title1.html")));
  WebContentsImpl* web_contents_impl =
      static_cast<WebContentsImpl*>(shell()->web_contents());
  RenderFrameHostImpl* main_frame = web_contents_impl->GetMainFrame();

  GURL a_url(embedded_test_server()->GetURL("a.com", "/title1.html"));
  Portal* portal = CreatePortalToUrl(web_contents_impl, a_url);
  WebContentsImpl* portal_contents = portal->GetPortalContents();

  FrameTreeNode* outer_frame_tree_node = FrameTreeNode::GloballyFindByID(
      portal_contents->GetOuterDelegateFrameTreeNodeId());
  EXPECT_TRUE(outer_frame_tree_node);

  EXPECT_TRUE(ExecJs(portal_contents->GetMainFrame(),
                     "window.onportalactivate = e => "
                     "document.body.appendChild(e.adoptPredecessor());"));

  {
    FrameDeletedObserver observer(outer_frame_tree_node->current_frame_host());
    PortalCreatedObserver portal_created_observer(
        portal_contents->GetMainFrame());
    ExecuteScriptAsync(main_frame,
                       "document.querySelector('portal').activate();");
    observer.Wait();

    // Observes the creation of a new portal due to the adoption of the
    // predecessor during the activate event.
    // TODO(lfg): We only wait for the adoption callback to avoid a race
    // receiving a sync IPC in a nested message loop while the browser is
    // sending out another sync IPC to the GPU process.
    // https://crbug.com/976367.
    portal_created_observer.WaitUntilPortalCreated();
  }
}

// Tests that activating a portal at the same time as it is being removed
// doesn't crash the browser.
IN_PROC_BROWSER_TEST_F(PortalBrowserTest, RemovePortalWhenUnloading) {
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("portal.test", "/title1.html")));
  WebContentsImpl* web_contents_impl =
      static_cast<WebContentsImpl*>(shell()->web_contents());
  RenderFrameHostImpl* main_frame = web_contents_impl->GetMainFrame();

  // Create a container for the portal.
  EXPECT_TRUE(ExecJs(main_frame,
                     "var div = document.createElement('div');"
                     "document.body.appendChild(div);"));

  // Create portal.
  PortalCreatedObserver portal_created_observer(main_frame);
  EXPECT_TRUE(ExecJs(main_frame,
                     "var portal = document.createElement('portal');"
                     "div.appendChild(portal);"));

  // Add a same-origin iframe in the same div as the portal that activates the
  // portal on its unload handler.
  EXPECT_TRUE(
      ExecJs(main_frame,
             "var iframe = document.createElement('iframe');"
             "iframe.src = 'about:blank';"
             "div.appendChild(iframe);"
             "iframe.contentWindow.onunload = () => portal.activate();"));

  // Remove the div from the document. This destroys the portal's WebContents
  // and should destroy the Portal object as well, so that the activate message
  // is not processed.
  EXPECT_TRUE(ExecJs(main_frame, "div.remove();"));
}

class PortalOrphanedNavigationBrowserTest
    : public PortalBrowserTest,
      public testing::WithParamInterface<std::tuple<bool, bool>> {
 public:
  PortalOrphanedNavigationBrowserTest()
      : cross_site_(std::get<0>(GetParam())),
        commit_after_adoption_(std::get<1>(GetParam())) {}

  // Provides meaningful param names instead of /0, /1, ...
  static std::string DescribeParams(
      const testing::TestParamInfo<ParamType>& info) {
    bool cross_site;
    bool commit_after_adoption;
    std::tie(cross_site, commit_after_adoption) = info.param;
    return base::StringPrintf("%sSite_Commit%sAdoption",
                              cross_site ? "Cross" : "Same",
                              commit_after_adoption ? "After" : "Before");
  }

 protected:
  bool cross_site() const { return cross_site_; }
  bool commit_after_adoption() const { return commit_after_adoption_; }

 private:
  // Whether the predecessor navigates cross site while orphaned.
  const bool cross_site_;
  // Whether the predecessor's navigation commits before or after adoption.
  const bool commit_after_adoption_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         PortalOrphanedNavigationBrowserTest,
                         testing::Combine(testing::Bool(), testing::Bool()),
                         PortalOrphanedNavigationBrowserTest::DescribeParams);

// Tests that a portal can navigate while orphaned.
IN_PROC_BROWSER_TEST_P(PortalOrphanedNavigationBrowserTest,
                       OrphanedNavigation) {
  GURL main_url(embedded_test_server()->GetURL("portal.test", "/title1.html"));
  ASSERT_TRUE(NavigateToURL(shell(), main_url));
  WebContentsImpl* web_contents_impl =
      static_cast<WebContentsImpl*>(shell()->web_contents());

  GURL portal_url(embedded_test_server()->GetURL("a.com", "/title1.html"));
  Portal* portal = CreatePortalToUrl(web_contents_impl, portal_url);

  GURL predecessor_nav_url(embedded_test_server()->GetURL(
      cross_site() ? "b.com" : "portal.test", "/title2.html"));

  if (commit_after_adoption()) {
    // Block the activate callback so that there is ample time to start the
    // navigation while orphaned.
    // TODO(mcnee): Ideally, we would have a test interceptor to precisely
    // control when to proceed with adoption.
    const int adoption_delay = TestTimeouts::tiny_timeout().InMilliseconds();
    EXPECT_TRUE(
        ExecJs(portal->GetPortalContents()->GetMainFrame(),
               JsReplace("window.onportalactivate = e => {"
                         "  let end = performance.now() + $1;"
                         "  while (performance.now() < end);"
                         "  document.body.appendChild(e.adoptPredecessor());"
                         "};",
                         adoption_delay)));
  } else {
    // Block the activate callback so that the predecessor portal stays
    // orphaned.
    EXPECT_TRUE(ExecJs(portal->GetPortalContents()->GetMainFrame(),
                       "window.onportalactivate = e => { while(true) {} };"));
  }

  // Activate the portal and navigate the predecessor.
  PortalActivatedObserver activated_observer(portal);
  TestNavigationManager navigation_manager(web_contents_impl,
                                           predecessor_nav_url);
  ExecuteScriptAsync(web_contents_impl->GetMainFrame(),
                     JsReplace("document.querySelector('portal').activate();"
                               "window.location.href = $1;",
                               predecessor_nav_url));
  activated_observer.WaitForActivate();
  if (commit_after_adoption()) {
    ASSERT_TRUE(navigation_manager.WaitForRequestStart());
    EXPECT_EQ(blink::mojom::PortalActivateResult::kPredecessorWasAdopted,
              activated_observer.WaitForActivateResult());
  }
  navigation_manager.WaitForNavigationFinished();
  EXPECT_TRUE(navigation_manager.was_successful());
}

// Tests that the browser doesn't crash if the renderer tries to create the
// PortalHost after the parent renderer dropped the portal.
IN_PROC_BROWSER_TEST_F(PortalBrowserTest,
                       AccessPortalHostAfterPortalDestruction) {
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("portal.test", "/title1.html")));
  WebContentsImpl* web_contents_impl =
      static_cast<WebContentsImpl*>(shell()->web_contents());

  GURL a_url(embedded_test_server()->GetURL("a.com", "/title1.html"));
  Portal* portal = CreatePortalToUrl(web_contents_impl, a_url);
  WebContentsImpl* portal_contents = portal->GetPortalContents();
  RenderFrameHostImpl* portal_frame = portal_contents->GetMainFrame();

  // Simulate the portal being dropped, but not the destruction of the
  // WebContents.
  web_contents_impl->GetMainFrame()->DestroyPortal(portal);

  // Get the portal renderer to access the WebContents.
  RenderProcessHostBadIpcMessageWaiter kill_waiter(portal_frame->GetProcess());
  ExecuteScriptAsync(portal_frame,
                     "window.portalHost.postMessage('message', '*');");
  EXPECT_EQ(bad_message::RPH_MOJO_PROCESS_ERROR, kill_waiter.Wait());
}

// Tests that activation early in navigation fails. Even though the navigation
// hasn't yet committed, allowing activation could allow a portal to prevent
// the user from navigating away.
IN_PROC_BROWSER_TEST_F(PortalBrowserTest, ActivateEarlyInNavigation) {
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("portal.test", "/title1.html")));
  WebContentsImpl* web_contents_impl =
      static_cast<WebContentsImpl*>(shell()->web_contents());
  RenderFrameHostImpl* main_frame = web_contents_impl->GetMainFrame();

  GURL url = embedded_test_server()->GetURL("a.com", "/title2.html");
  CreatePortalToUrl(web_contents_impl, url);

  // Have the outer page try to navigate away but stop it early in the request,
  // where it is still possible to stop.
  GURL destination =
      embedded_test_server()->GetURL("portal.test", "/title3.html");
  TestNavigationObserver navigation_observer(web_contents_impl);
  NavigationHandleObserver handle_observer(web_contents_impl, destination);
  TestNavigationManager navigation_manager(web_contents_impl, destination);
  NavigationController::LoadURLParams params(destination);
  params.transition_type = ui::PageTransitionFromInt(
      ui::PAGE_TRANSITION_TYPED | ui::PAGE_TRANSITION_FROM_ADDRESS_BAR);
  web_contents_impl->GetController().LoadURLWithParams(params);
  ASSERT_TRUE(navigation_manager.WaitForRequestStart());

  // Then activate the portal, because navigation has begun and beforeunload
  // has been dispatched, the activation should fail.
  EvalJsResult result = EvalJs(main_frame,
                               "document.querySelector('portal').activate()"
                               ".then(() => 'success', e => e.message)");
  EXPECT_THAT(result.ExtractString(),
              ::testing::HasSubstr("Cannot activate portal while document is in"
                                   " beforeunload or has started unloading"));

  // The navigation should commit properly thereafter.
  navigation_manager.WaitForNavigationFinished();
  navigation_observer.Wait();
  EXPECT_EQ(web_contents_impl, shell()->web_contents());
  EXPECT_TRUE(navigation_observer.last_navigation_succeeded());
  EXPECT_EQ(destination, navigation_observer.last_navigation_url());
}

// Tests that activation late in navigation is rejected (since it's too late to
// stop the navigation).
IN_PROC_BROWSER_TEST_F(PortalBrowserTest, ActivateLateInNavigation) {
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("portal.test", "/title1.html")));
  WebContentsImpl* web_contents_impl =
      static_cast<WebContentsImpl*>(shell()->web_contents());
  RenderFrameHostImpl* main_frame = web_contents_impl->GetMainFrame();

  GURL url = embedded_test_server()->GetURL("a.com", "/title2.html");
  CreatePortalToUrl(web_contents_impl, url);

  // Have the outer page try to navigate away and reach the point where it's
  // about to process the response (after which it will commit). It is too late
  // to abort the navigation.
  GURL destination =
      embedded_test_server()->GetURL("portal.test", "/title3.html");
  TestNavigationObserver navigation_observer(web_contents_impl);
  TestNavigationManager navigation_manager(web_contents_impl, destination);
  NavigationController::LoadURLParams params(destination);
  params.transition_type = ui::PageTransitionFromInt(
      ui::PAGE_TRANSITION_TYPED | ui::PAGE_TRANSITION_FROM_ADDRESS_BAR);
  web_contents_impl->GetController().LoadURLWithParams(params);
  ASSERT_TRUE(navigation_manager.WaitForResponse());

  // Then activate the portal. Since this is late in navigation, we expect the
  // activation to fail. Since commit hasn't actually happened yet, though,
  // there is time for the renderer to process the promise rejection.
  EvalJsResult result = EvalJs(main_frame,
                               "document.querySelector('portal').activate()"
                               ".then(() => 'success', e => e.message)");
  EXPECT_THAT(result.ExtractString(),
              ::testing::HasSubstr("Cannot activate portal while document is in"
                                   " beforeunload or has started unloading"));

  // The navigation should commit properly thereafter.
  navigation_manager.ResumeNavigation();
  navigation_observer.Wait();
  EXPECT_EQ(web_contents_impl, shell()->web_contents());
  EXPECT_TRUE(navigation_observer.last_navigation_succeeded());
  EXPECT_EQ(destination, navigation_observer.last_navigation_url());
}

namespace {

class LocalMainFrameInterceptorBadPortalActivateResult
    : public blink::mojom::LocalMainFrameInterceptorForTesting {
 public:
  explicit LocalMainFrameInterceptorBadPortalActivateResult(
      RenderFrameHostImpl* frame_host)
      : frame_host_(frame_host) {}

  blink::mojom::LocalMainFrame* GetForwardingInterface() override {
    if (!local_main_frame_)
      frame_host_->GetRemoteAssociatedInterfaces()->GetInterface(
          &local_main_frame_);
    return local_main_frame_.get();
  }

  void OnPortalActivated(
      const blink::PortalToken& portal_token,
      mojo::PendingAssociatedRemote<blink::mojom::Portal> portal,
      mojo::PendingAssociatedReceiver<blink::mojom::PortalClient> portal_client,
      blink::TransferableMessage data,
      OnPortalActivatedCallback callback) override {
    GetForwardingInterface()->OnPortalActivated(
        portal_token, std::move(portal), std::move(portal_client),
        std::move(data),
        base::BindOnce(
            [](OnPortalActivatedCallback callback,
               blink::mojom::PortalActivateResult) {
              // Replace the true result with one that the renderer is not
              // allowed to send.
              std::move(callback).Run(blink::mojom::PortalActivateResult::
                                          kRejectedDueToPredecessorNavigation);
            },
            std::move(callback)));
  }

 private:
  RenderFrameHostImpl* frame_host_;
  mojo::AssociatedRemote<blink::mojom::LocalMainFrame> local_main_frame_;
};

class RenderFrameHostImplForLocalMainFrameInterceptor
    : public RenderFrameHostImpl {
 private:
  using RenderFrameHostImpl::RenderFrameHostImpl;

  blink::mojom::LocalMainFrame* GetAssociatedLocalMainFrame() final {
    return &interceptor_;
  }

  LocalMainFrameInterceptorBadPortalActivateResult interceptor_{this};

  friend class RenderFrameHostFactoryForLocalMainFrameInterceptor;
};

class RenderFrameHostFactoryForLocalMainFrameInterceptor
    : public TestRenderFrameHostFactory {
 protected:
  std::unique_ptr<RenderFrameHostImpl> CreateRenderFrameHost(
      SiteInstance* site_instance,
      scoped_refptr<RenderViewHostImpl> render_view_host,
      RenderFrameHostDelegate* delegate,
      FrameTree* frame_tree,
      FrameTreeNode* frame_tree_node,
      int32_t routing_id,
      const base::UnguessableToken& frame_token,
      bool renderer_initiated_creation) override {
    return base::WrapUnique(new RenderFrameHostImplForLocalMainFrameInterceptor(
        site_instance, std::move(render_view_host), delegate, frame_tree,
        frame_tree_node, routing_id, frame_token, renderer_initiated_creation,
        RenderFrameHostImpl::LifecycleState::kActive));
  }
};

}  // namespace

// Tests that the browser filters the renderer's replies to the portal
// activation event and terminates misbehaving renderers.
IN_PROC_BROWSER_TEST_F(PortalBrowserTest, MisbehavingRendererActivated) {
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("portal.test", "/title1.html")));
  WebContentsImpl* web_contents_impl =
      static_cast<WebContentsImpl*>(shell()->web_contents());
  RenderFrameHostImpl* main_frame = web_contents_impl->GetMainFrame();

  // Arrange for a special kind of RenderFrameHost to be created which permits
  // its NavigationControl messages to be intercepted.
  RenderFrameHostFactoryForLocalMainFrameInterceptor scoped_rfh_factory;
  GURL url = embedded_test_server()->GetURL("a.com", "/title2.html");
  Portal* portal = CreatePortalToUrl(web_contents_impl, url);

  // Then activate the portal. Due to the apparent misbehavior from the
  // renderer, the caller should be informed that it was aborted due to a bug,
  // and the portal's renderer (having been framed for the crime) should be
  // killed.
  PortalActivatedObserver activated_observer(portal);
  RenderProcessHostBadIpcMessageWaiter kill_waiter(
      portal->GetPortalContents()->GetMainFrame()->GetProcess());
  ExecuteScriptAsync(main_frame,
                     "document.querySelector('portal').activate();");
  EXPECT_EQ(blink::mojom::PortalActivateResult::kAbortedDueToBug,
            activated_observer.WaitForActivateResult());
  EXPECT_EQ(bad_message::RPH_MOJO_PROCESS_ERROR, kill_waiter.Wait());
}

// Test that user facing session history is retained after a portal activation.
// Before activation, we have the host WebContents's navigation entries, which
// is the session history presented to the user, plus the portal WebContents's
// navigation entry, which is not presented as part of that session history.
// Upon activation, the host WebContents's navigation entries are merged into
// the activated portal's WebContents. The resulting session history in the
// activated WebContents is presented to the user.
IN_PROC_BROWSER_TEST_F(PortalBrowserTest, PortalHistoryWithActivation) {
  // We have an additional navigation entry in the portal host's contents, so
  // that we test that we're retaining more than just the last committed entry
  // of the portal host.
  GURL previous_url(
      embedded_test_server()->GetURL("portal.test", "/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), previous_url));

  GURL main_url(embedded_test_server()->GetURL("portal.test", "/title2.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  WebContentsImpl* web_contents_impl =
      static_cast<WebContentsImpl*>(shell()->web_contents());
  RenderFrameHostImpl* main_frame = web_contents_impl->GetMainFrame();

  GURL portal_url(embedded_test_server()->GetURL("a.com", "/title1.html"));
  Portal* portal = CreatePortalToUrl(web_contents_impl, portal_url);
  WebContentsImpl* portal_contents = portal->GetPortalContents();

  NavigationControllerImpl& main_controller =
      web_contents_impl->GetController();
  NavigationControllerImpl& portal_controller =
      portal_contents->GetController();

  EXPECT_EQ(2, main_controller.GetEntryCount());
  ASSERT_TRUE(main_controller.GetLastCommittedEntry());
  EXPECT_EQ(main_url, main_controller.GetLastCommittedEntry()->GetURL());
  EXPECT_EQ(1, portal_controller.GetEntryCount());
  ASSERT_TRUE(portal_controller.GetLastCommittedEntry());
  EXPECT_EQ(portal_url, portal_controller.GetLastCommittedEntry()->GetURL());

  PortalActivatedObserver activated_observer(portal);
  ASSERT_TRUE(
      ExecJs(main_frame, "document.querySelector('portal').activate();"));
  activated_observer.WaitForActivate();

  NavigationControllerImpl& activated_controller = portal_controller;

  ASSERT_EQ(3, activated_controller.GetEntryCount());
  ASSERT_EQ(2, activated_controller.GetLastCommittedEntryIndex());
  EXPECT_EQ(previous_url, activated_controller.GetEntryAtIndex(0)->GetURL());
  EXPECT_EQ(main_url, activated_controller.GetEntryAtIndex(1)->GetURL());
  EXPECT_EQ(portal_url, activated_controller.GetEntryAtIndex(2)->GetURL());
}

// Test that we may go back/forward across a portal activation as though it
// were a regular navigation.
IN_PROC_BROWSER_TEST_F(PortalBrowserTest,
                       PortalHistoryActivateAndGoBackForward) {
  GURL main_url(embedded_test_server()->GetURL("portal.test", "/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));
  WebContentsImpl* web_contents_impl =
      static_cast<WebContentsImpl*>(shell()->web_contents());
  RenderFrameHostImpl* main_frame = web_contents_impl->GetMainFrame();

  GURL portal_url(embedded_test_server()->GetURL("a.com", "/title1.html"));
  Portal* portal = CreatePortalToUrl(web_contents_impl, portal_url);

  PortalActivatedObserver activated_observer(portal);
  ASSERT_TRUE(
      ExecJs(main_frame, "document.querySelector('portal').activate();"));
  activated_observer.WaitForActivate();

  web_contents_impl = static_cast<WebContentsImpl*>(shell()->web_contents());
  NavigationControllerImpl& controller = web_contents_impl->GetController();

  ASSERT_TRUE(controller.CanGoBack());
  TestNavigationObserver go_back_observer(web_contents_impl);
  controller.GoBack();
  go_back_observer.Wait();
  // These back/forward navigations do not involve a contents swap, since the
  // original contents is gone as it was not adopted.
  ASSERT_EQ(web_contents_impl, shell()->web_contents());

  ASSERT_EQ(2, controller.GetEntryCount());
  ASSERT_EQ(0, controller.GetLastCommittedEntryIndex());
  EXPECT_EQ(main_url, controller.GetEntryAtIndex(0)->GetURL());
  EXPECT_EQ(portal_url, controller.GetEntryAtIndex(1)->GetURL());

  ASSERT_TRUE(controller.CanGoForward());
  TestNavigationObserver go_forward_observer(web_contents_impl);
  controller.GoForward();
  go_forward_observer.Wait();
  ASSERT_EQ(web_contents_impl, shell()->web_contents());

  ASSERT_EQ(2, controller.GetEntryCount());
  ASSERT_EQ(1, controller.GetLastCommittedEntryIndex());
  EXPECT_EQ(main_url, controller.GetEntryAtIndex(0)->GetURL());
  EXPECT_EQ(portal_url, controller.GetEntryAtIndex(1)->GetURL());
}

// Activation does not cancel new pending navigations in portals.
IN_PROC_BROWSER_TEST_F(PortalBrowserTest,
                       ActivationWithPortalPendingNavigation) {
  GURL main_url(embedded_test_server()->GetURL("portal.test", "/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));
  WebContentsImpl* web_contents_impl =
      static_cast<WebContentsImpl*>(shell()->web_contents());
  RenderFrameHostImpl* main_frame = web_contents_impl->GetMainFrame();

  GURL portal_url(embedded_test_server()->GetURL("a.com", "/title1.html"));
  Portal* portal = CreatePortalToUrl(web_contents_impl, portal_url);
  WebContentsImpl* portal_contents = portal->GetPortalContents();

  NavigationControllerImpl& portal_controller =
      portal_contents->GetController();

  // Have the portal navigate so that we have a pending navigation.
  GURL pending_url(embedded_test_server()->GetURL("a.com", "/title2.html"));
  TestNavigationManager pending_navigation(portal_contents, pending_url);
  EXPECT_TRUE(ExecJs(
      main_frame,
      JsReplace("document.querySelector('portal').src = $1;", pending_url)));
  EXPECT_TRUE(pending_navigation.WaitForRequestStart());

  // Navigating via frame proxy does not create a pending NavigationEntry. We'll
  // check for an ongoing NavigationRequest instead.
  FrameTreeNode* portal_node =
      portal_contents->GetMainFrame()->frame_tree_node();
  NavigationRequest* navigation_request = portal_node->navigation_request();
  ASSERT_TRUE(navigation_request);

  PortalActivatedObserver activated_observer(portal);
  ASSERT_TRUE(
      ExecJs(main_frame, "document.querySelector('portal').activate();"));
  activated_observer.WaitForActivate();

  NavigationControllerImpl& activated_controller = portal_controller;

  pending_navigation.WaitForNavigationFinished();
  ASSERT_EQ(2, activated_controller.GetEntryCount());
  ASSERT_EQ(1, activated_controller.GetLastCommittedEntryIndex());
  EXPECT_EQ(main_url, activated_controller.GetEntryAtIndex(0)->GetURL());
  EXPECT_EQ(pending_url, activated_controller.GetEntryAtIndex(1)->GetURL());
}

IN_PROC_BROWSER_TEST_F(PortalBrowserTest, DidFocusIPCFromFrameInsidePortal) {
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("portal.test", "/title1.html")));
  WebContentsImpl* web_contents_impl =
      static_cast<WebContentsImpl*>(shell()->web_contents());
  RenderFrameHostImpl* main_frame = web_contents_impl->GetMainFrame();

  GURL url = embedded_test_server()->GetURL("a.com", "/title1.html");
  Portal* portal = CreatePortalToUrl(web_contents_impl, url);
  WebContentsImpl* portal_contents = portal->GetPortalContents();
  RenderFrameHostImpl* portal_main_frame = portal_contents->GetMainFrame();

  GURL b_url(embedded_test_server()->GetURL("b.com", "/title1.html"));
  TestNavigationObserver iframe_navigation_observer(portal_contents);
  EXPECT_TRUE(ExecJs(portal_main_frame,
                     JsReplace("var iframe = document.createElement('iframe');"
                               "iframe.src = $1;"
                               "document.body.appendChild(iframe);",
                               b_url)));
  iframe_navigation_observer.Wait();
  EXPECT_EQ(b_url, iframe_navigation_observer.last_navigation_url());

  web_contents_impl->SetAsFocusedWebContentsIfNecessary();
  EXPECT_EQ(web_contents_impl->GetFocusedWebContents(), web_contents_impl);
  EXPECT_EQ(web_contents_impl->GetFocusedFrame(), main_frame);

  // Simulate renderer sending LocalFrameHost::DidFocusFrame IPC.
  RenderFrameHostImpl* iframe_rfhi =
      portal_main_frame->child_at(0)->current_frame_host();
  iframe_rfhi->DidFocusFrame();

  // Focus should not have changed.
  EXPECT_EQ(web_contents_impl->GetFocusedWebContents(), web_contents_impl);
  EXPECT_EQ(web_contents_impl->GetFocusedFrame(), main_frame);

  if (!SiteIsolationPolicy::UseDedicatedProcessesForAllSites())
    return;

  // Simulate renderer sending RemoteFrameHost::DidFocusFrame IPC.
  RenderFrameProxyHost* iframe_rfph =
      portal_main_frame->child_at(0)->render_manager()->GetRenderFrameProxyHost(
          portal_main_frame->GetSiteInstance());
  iframe_rfph->DidFocusFrame();

  // Focus should not have changed.
  EXPECT_EQ(web_contents_impl->GetFocusedWebContents(), web_contents_impl);
  EXPECT_EQ(web_contents_impl->GetFocusedFrame(), main_frame);
}

// Test that a renderer process is killed if it sends an AdvanceFocus IPC to
// advance focus into a portal.
IN_PROC_BROWSER_TEST_F(PortalBrowserTest, AdvanceFocusIntoPortal) {
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("portal.test", "/title1.html")));
  WebContentsImpl* web_contents_impl =
      static_cast<WebContentsImpl*>(shell()->web_contents());
  RenderFrameHostImpl* main_frame = web_contents_impl->GetMainFrame();

  GURL url = embedded_test_server()->GetURL("a.com", "/title1.html");
  Portal* portal = CreatePortalToUrl(web_contents_impl, url);
  WebContentsImpl* portal_contents = portal->GetPortalContents();
  RenderFrameHostImpl* portal_main_frame = portal_contents->GetMainFrame();

  RenderFrameProxyHost* outer_delegate_proxy =
      portal_main_frame->frame_tree_node()
          ->render_manager()
          ->GetProxyToOuterDelegate();
  RenderProcessHostBadIpcMessageWaiter rph_kill_waiter(
      main_frame->GetProcess());
  outer_delegate_proxy->AdvanceFocus(blink::mojom::FocusType::kNone,
                                     main_frame->GetFrameToken());
  base::Optional<bad_message::BadMessageReason> result = rph_kill_waiter.Wait();
  EXPECT_TRUE(result.has_value());
  EXPECT_EQ(result.value(), bad_message::RFPH_ADVANCE_FOCUS_INTO_PORTAL);
}

namespace {
void WaitForAccessibilityTree(WebContents* web_contents) {
  AccessibilityNotificationWaiter waiter(web_contents, ui::kAXModeComplete,
                                         ax::mojom::Event::kNone);
  waiter.WaitForNotification();
}
}  // namespace

IN_PROC_BROWSER_TEST_F(PortalBrowserTest,
                       RootAccessibilityManagerShouldUpdateAfterActivation) {
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("portal.test", "/title1.html")));
  WebContentsImpl* web_contents_impl =
      static_cast<WebContentsImpl*>(shell()->web_contents());
  RenderFrameHostImpl* main_frame = web_contents_impl->GetMainFrame();
  WaitForAccessibilityTree(web_contents_impl);

  // Create portal.
  GURL url = embedded_test_server()->GetURL("a.com", "/title1.html");
  Portal* portal = CreatePortalToUrl(web_contents_impl, url);
  WebContentsImpl* portal_contents = portal->GetPortalContents();
  RenderFrameHostImpl* portal_frame = portal_contents->GetMainFrame();
  WaitForAccessibilityTree(portal_contents);
  if (!main_frame->browser_accessibility_manager() ||
      !portal_frame->browser_accessibility_manager()->GetRootManager())
    WaitForAccessibilityTree(web_contents_impl);

  EXPECT_NE(nullptr, portal_frame->browser_accessibility_manager());
  EXPECT_EQ(main_frame->browser_accessibility_manager(),
            portal_frame->browser_accessibility_manager()->GetRootManager());
  // Activate portal and adopt predecessor.
  EXPECT_TRUE(ExecJs(portal_frame,
                     "window.addEventListener('portalactivate', e => { "
                     "  var portal = e.adoptPredecessor(); "
                     "  document.body.appendChild(portal); "
                     "});"));
  {
    PortalActivatedObserver activated_observer(portal);
    PortalCreatedObserver adoption_observer(portal_frame);
    EXPECT_TRUE(ExecJs(main_frame,
                       "let portal = document.querySelector('portal');"
                       "portal.activate().then(() => { "
                       "  document.body.removeChild(portal); "
                       "});"));
    EXPECT_EQ(blink::mojom::PortalActivateResult::kPredecessorWasAdopted,
              activated_observer.WaitForActivateResult());
    adoption_observer.WaitUntilPortalCreated();
  }

  EXPECT_EQ(portal_frame->browser_accessibility_manager()->GetRootManager(),
            portal_frame->browser_accessibility_manager());
}

IN_PROC_BROWSER_TEST_F(PortalBrowserTest,
                       CrossSitePortalNavCommitsAfterActivation) {
  ASSERT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("portal.test", "/title1.html")));
  WebContentsImpl* web_contents_impl =
      static_cast<WebContentsImpl*>(shell()->web_contents());
  RenderFrameHostImpl* main_frame = web_contents_impl->GetMainFrame();

  GURL a_url(embedded_test_server()->GetURL("a.com", "/title1.html"));
  Portal* portal = CreatePortalToUrl(web_contents_impl, a_url);

  TestNavigationObserver nav_observer(portal->GetPortalContents());
  GURL b_url(embedded_test_server()->GetURL("b.com", "/title1.html"));
  // Start a cross site navigation in the portal contents and immediately
  // activate the portal. The navigation starts in a child frame but commits
  // after the frame becomes the root frame.
  ExecuteScriptAsync(main_frame,
                     JsReplace("let portal = document.querySelector('portal');"
                               "portal.src = $1;"
                               "portal.activate();",
                               b_url));
  nav_observer.Wait();
}

// This is similar to CrossSitePortalNavCommitsAfterActivation, but we navigate
// an adopted predecessor that hasn't been attached and the navigation commits
// after the predecessor is reactivated.
IN_PROC_BROWSER_TEST_F(
    PortalBrowserTest,
    CrossSitePortalNavInUnattachedPredecessorCommitsAfterActivation) {
  ASSERT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("portal.test", "/title1.html")));
  WebContentsImpl* web_contents_impl =
      static_cast<WebContentsImpl*>(shell()->web_contents());
  RenderFrameHostImpl* main_frame = web_contents_impl->GetMainFrame();

  GURL a_url(embedded_test_server()->GetURL("a.com", "/title1.html"));
  Portal* portal = CreatePortalToUrl(web_contents_impl, a_url);
  WebContentsImpl* portal_contents = portal->GetPortalContents();
  RenderFrameHostImpl* portal_frame = portal_contents->GetMainFrame();

  GURL b_url(embedded_test_server()->GetURL("b.com", "/title1.html"));
  EXPECT_TRUE(
      ExecJs(portal_frame,
             JsReplace("window.addEventListener('portalactivate', (e) => {"
                       "  let predecessor = e.adoptPredecessor();"
                       "  predecessor.src = $1;"
                       "  predecessor.activate();"
                       "});",
                       b_url)));

  TestNavigationObserver nav_observer(web_contents_impl);
  ExecuteScriptAsync(main_frame,
                     "document.querySelector('portal').activate();");
  nav_observer.Wait();
}

// Ensure portal activations respect navigation precedence. If there is an
// ongoing browser initiated navigation, then a portal activation without user
// activation cannot proceed.
IN_PROC_BROWSER_TEST_F(PortalBrowserTest, NavigationPrecedence) {
  GURL main_url1(embedded_test_server()->GetURL("portal.test", "/title1.html"));
  GURL main_url2(embedded_test_server()->GetURL("portal.test", "/title2.html"));
  ASSERT_TRUE(NavigateToURL(shell(), main_url1));
  ASSERT_TRUE(NavigateToURL(shell(), main_url2));
  WebContentsImpl* web_contents_impl =
      static_cast<WebContentsImpl*>(shell()->web_contents());
  RenderFrameHostImpl* main_frame = web_contents_impl->GetMainFrame();

  GURL portal_url(embedded_test_server()->GetURL("a.com", "/title1.html"));
  CreatePortalToUrl(web_contents_impl, portal_url);

  TestNavigationManager pending_back_navigation(web_contents_impl, main_url1);
  ASSERT_TRUE(web_contents_impl->GetController().CanGoBack());
  web_contents_impl->GetController().GoBack();
  EXPECT_TRUE(pending_back_navigation.WaitForRequestStart());

  EXPECT_EQ("reject", EvalJs(main_frame,
                             "document.querySelector('portal').activate().then("
                             "    () => 'resolve', () => 'reject');",
                             EXECUTE_SCRIPT_NO_USER_GESTURE));

  pending_back_navigation.WaitForNavigationFinished();
  EXPECT_TRUE(pending_back_navigation.was_successful());
}

IN_PROC_BROWSER_TEST_F(PortalBrowserTest, RejectActivationOfErrorPages) {
  net::EmbeddedTestServer bad_https_server(net::EmbeddedTestServer::TYPE_HTTPS);
  bad_https_server.AddDefaultHandlers(GetTestDataFilePath());
  bad_https_server.SetSSLConfig(net::EmbeddedTestServer::CERT_EXPIRED);
  ASSERT_TRUE(bad_https_server.Start());

  GURL main_url(embedded_test_server()->GetURL("portal.test", "/title1.html"));
  ASSERT_TRUE(NavigateToURL(shell(), main_url));
  WebContentsImpl* web_contents_impl =
      static_cast<WebContentsImpl*>(shell()->web_contents());
  RenderFrameHostImpl* main_frame = web_contents_impl->GetMainFrame();

  GURL bad_portal_url(bad_https_server.GetURL("a.com", "/title1.html"));
  Portal* portal = CreatePortalToUrl(web_contents_impl, bad_portal_url,
                                     /* number_of_navigations */ 1,
                                     /* expected_to_succeed */ false);

  PortalActivatedObserver activated_observer(portal);
  EXPECT_EQ("reject", EvalJs(main_frame,
                             "document.querySelector('portal').activate().then("
                             "    () => 'resolve', () => 'reject');"));
  EXPECT_EQ(blink::mojom::PortalActivateResult::kRejectedDueToErrorInPortal,
            activated_observer.result());
}

IN_PROC_BROWSER_TEST_F(PortalBrowserTest,
                       RejectActivationOfPostCommitErrorPages) {
  GURL main_url(embedded_test_server()->GetURL("portal.test", "/title1.html"));
  ASSERT_TRUE(NavigateToURL(shell(), main_url));
  WebContentsImpl* web_contents_impl =
      static_cast<WebContentsImpl*>(shell()->web_contents());
  RenderFrameHostImpl* main_frame = web_contents_impl->GetMainFrame();

  GURL portal_url(embedded_test_server()->GetURL("a.com", "/title1.html"));
  Portal* portal = CreatePortalToUrl(web_contents_impl, portal_url);
  WebContentsImpl* portal_contents = portal->GetPortalContents();

  std::string error_html = "Error page";
  TestNavigationObserver error_observer(portal_contents);
  portal_contents->GetController().LoadPostCommitErrorPage(
      portal_contents->GetMainFrame(), portal_url, error_html,
      net::ERR_BLOCKED_BY_CLIENT);
  error_observer.Wait();
  EXPECT_FALSE(error_observer.last_navigation_succeeded());
  EXPECT_EQ(net::ERR_BLOCKED_BY_CLIENT, error_observer.last_net_error_code());

  PortalActivatedObserver activated_observer(portal);
  EXPECT_EQ("reject", EvalJs(main_frame,
                             "document.querySelector('portal').activate().then("
                             "    () => 'resolve', () => 'reject');"));
  EXPECT_EQ(blink::mojom::PortalActivateResult::kRejectedDueToErrorInPortal,
            activated_observer.result());
}

IN_PROC_BROWSER_TEST_F(PortalBrowserTest, RejectActivationOfCrashedPages) {
  GURL main_url(embedded_test_server()->GetURL("portal.test", "/title1.html"));
  ASSERT_TRUE(NavigateToURL(shell(), main_url));
  WebContentsImpl* web_contents_impl =
      static_cast<WebContentsImpl*>(shell()->web_contents());
  RenderFrameHostImpl* main_frame = web_contents_impl->GetMainFrame();

  GURL portal_url(embedded_test_server()->GetURL("a.com", "/title1.html"));
  Portal* portal = CreatePortalToUrl(web_contents_impl, portal_url);
  WebContentsImpl* portal_contents = portal->GetPortalContents();
  CrashTab(portal_contents);

  PortalActivatedObserver activated_observer(portal);
  EXPECT_TRUE(
      ExecJs(main_frame, "document.querySelector('portal').activate();"));
  EXPECT_EQ(blink::mojom::PortalActivateResult::kRejectedDueToErrorInPortal,
            activated_observer.WaitForActivateResult());
}

IN_PROC_BROWSER_TEST_F(PortalBrowserTest, ActivatePreviouslyCrashedPortal) {
  GURL main_url(embedded_test_server()->GetURL("portal.test", "/title1.html"));
  ASSERT_TRUE(NavigateToURL(shell(), main_url));
  WebContentsImpl* web_contents_impl =
      static_cast<WebContentsImpl*>(shell()->web_contents());
  RenderFrameHostImpl* main_frame = web_contents_impl->GetMainFrame();

  GURL portal_url(embedded_test_server()->GetURL("a.com", "/title1.html"));
  Portal* portal = CreatePortalToUrl(web_contents_impl, portal_url);
  WebContentsImpl* portal_contents = portal->GetPortalContents();
  CrashTab(portal_contents);

  TestNavigationObserver navigation_observer(portal_contents);
  EXPECT_TRUE(ExecJs(
      main_frame,
      JsReplace("document.querySelector('portal').src = $1;", portal_url)));
  navigation_observer.Wait();

  PortalActivatedObserver activated_observer(portal);
  EXPECT_TRUE(
      ExecJs(main_frame, "document.querySelector('portal').activate();"));
  EXPECT_EQ(blink::mojom::PortalActivateResult::kPredecessorWillUnload,
            activated_observer.WaitForActivateResult());
}

IN_PROC_BROWSER_TEST_F(PortalBrowserTest, CallCreateProxyAndAttachPortalTwice) {
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("portal.test", "/title1.html")));
  WebContentsImpl* web_contents_impl =
      static_cast<WebContentsImpl*>(shell()->web_contents());
  RenderFrameHostImpl* main_frame = web_contents_impl->GetMainFrame();

  GURL url = embedded_test_server()->GetURL("a.com", "/title1.html");
  Portal* portal = CreatePortalToUrl(web_contents_impl, url);
  PortalInterceptorForTesting* portal_interceptor =
      PortalInterceptorForTesting::From(portal);
  // Hijacks navigate request and calls CreateProxyAndAttachPortal instead.
  portal_interceptor->SetNavigateCallback(base::BindRepeating(
      [](Portal* portal, const GURL& url, blink::mojom::ReferrerPtr referrer,
         blink::mojom::Portal::NavigateCallback callback) {
        portal->CreateProxyAndAttachPortal();
        std::move(callback).Run();
      },
      portal));

  RenderProcessHostBadIpcMessageWaiter rph_kill_waiter(
      main_frame->GetProcess());
  GURL dummy_url = embedded_test_server()->GetURL("b.com", "/title1.html");
  ExecuteScriptAsync(
      main_frame,
      JsReplace("document.querySelector('portal').src = $1", dummy_url));
  EXPECT_EQ(bad_message::RPH_MOJO_PROCESS_ERROR, rph_kill_waiter.Wait());
}

IN_PROC_BROWSER_TEST_F(PortalBrowserTest, CrossSiteActivationReusingRVH) {
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("portal.test", "/title1.html")));
  WebContentsImpl* web_contents_impl =
      static_cast<WebContentsImpl*>(shell()->web_contents());

  GURL a_url(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL b_url(embedded_test_server()->GetURL("b.com", "/title1.html"));

  Portal* portal = CreatePortalToUrl(web_contents_impl, a_url);
  WebContentsImpl* portal_contents = portal->GetPortalContents();
  LeaveInPendingDeletionState(portal_contents->GetMainFrame());

  // Navigate portal to b.com.
  TestNavigationObserver b_nav_observer(portal_contents);
  EXPECT_TRUE(ExecJs(portal_contents->GetMainFrame(),
                     JsReplace("location.href = $1;", b_url)));
  b_nav_observer.Wait();

  // Navigate portal to a.com once activated.
  EXPECT_TRUE(
      ExecJs(portal_contents->GetMainFrame(),
             JsReplace("window.addEventListener('portalactivate', (e) => {"
                       "  location.href = $1;"
                       "});",
                       a_url)));

  TestNavigationObserver nav_observer(portal_contents);
  ExecuteScriptAsync(web_contents_impl->GetMainFrame(),
                     "document.querySelector('portal').activate();");
  nav_observer.Wait();
  EXPECT_TRUE(nav_observer.last_navigation_succeeded());
}

class PortalOOPIFBrowserTest : public PortalBrowserTest {
 protected:
  PortalOOPIFBrowserTest() {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    IsolateAllSitesForTesting(command_line);
  }
};

// Tests that creating and destroying OOPIFs inside the portal works as
// intended.
IN_PROC_BROWSER_TEST_F(PortalOOPIFBrowserTest, OOPIFInsidePortal) {
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("portal.test", "/title1.html")));
  WebContentsImpl* web_contents_impl =
      static_cast<WebContentsImpl*>(shell()->web_contents());

  GURL a_url(embedded_test_server()->GetURL("a.com", "/title1.html"));
  Portal* portal = CreatePortalToUrl(web_contents_impl, a_url);
  WebContentsImpl* portal_contents = portal->GetPortalContents();
  RenderFrameHostImpl* portal_main_frame = portal_contents->GetMainFrame();

  // Add an out-of-process iframe to the portal.
  GURL b_url(embedded_test_server()->GetURL("b.com", "/title1.html"));
  TestNavigationObserver iframe_navigation_observer(portal_contents);
  EXPECT_TRUE(ExecJs(portal_main_frame,
                     JsReplace("var iframe = document.createElement('iframe');"
                               "iframe.src = $1;"
                               "document.body.appendChild(iframe);",
                               b_url)));
  iframe_navigation_observer.Wait();
  EXPECT_EQ(b_url, iframe_navigation_observer.last_navigation_url());
  RenderFrameHostImpl* portal_iframe =
      portal_main_frame->child_at(0)->current_frame_host();
  EXPECT_NE(portal_main_frame->GetSiteInstance(),
            portal_iframe->GetSiteInstance());

  // Remove the OOPIF from the portal.
  RenderFrameDeletedObserver deleted_observer(portal_iframe);
  EXPECT_TRUE(
      ExecJs(portal_main_frame, "document.querySelector('iframe').remove();"));
  deleted_observer.WaitUntilDeleted();
}

namespace {

class DownloadObserver : public DownloadManager::Observer {
 public:
  DownloadObserver()
      : manager_(BrowserContext::GetDownloadManager(
            ShellContentBrowserClient::Get()->browser_context())) {
    manager_->AddObserver(this);
  }

  ~DownloadObserver() override {
    if (manager_)
      manager_->RemoveObserver(this);
  }

  ::testing::AssertionResult DownloadObserved() {
    if (download_url_.is_empty())
      return ::testing::AssertionFailure() << "no download observed";
    return ::testing::AssertionSuccess()
           << "download observed: " << download_url_;
  }

  ::testing::AssertionResult AwaitDownload() {
    if (download_url_.is_empty() && !dropped_download_) {
      base::RunLoop run_loop;
      quit_closure_ = run_loop.QuitClosure();
      run_loop.Run();
      quit_closure_.Reset();
    }
    return DownloadObserved();
  }

  // DownloadManager::Observer

  void ManagerGoingDown(DownloadManager* manager) override {
    DCHECK_EQ(manager_, manager);
    manager_->RemoveObserver(this);
    manager_ = nullptr;
  }

  void OnDownloadCreated(DownloadManager* manager,
                         download::DownloadItem* item) override {
    DCHECK_EQ(manager_, manager);
    if (download_url_.is_empty()) {
      download_url_ = item->GetURL();
      if (!quit_closure_.is_null())
        std::move(quit_closure_).Run();
    }
  }

  void OnDownloadDropped(DownloadManager* manager) override {
    DCHECK_EQ(manager_, manager);
    dropped_download_ = true;
    if (!quit_closure_.is_null())
      std::move(quit_closure_).Run();
  }

 private:
  DownloadManager* manager_;
  bool dropped_download_ = false;
  GURL download_url_;
  base::OnceClosure quit_closure_;
};

}  // namespace

IN_PROC_BROWSER_TEST_F(PortalBrowserTest, DownloadsBlockedInMainFrame) {
  WebContentsImpl* web_contents_impl =
      static_cast<WebContentsImpl*>(shell()->web_contents());
  ASSERT_TRUE(NavigateToURL(
      web_contents_impl,
      embedded_test_server()->GetURL("portal.test", "/title1.html")));
  CreatePortalToUrl(web_contents_impl, embedded_test_server()->GetURL(
                                           "portal.test", "/title2.html"));

  GURL download_url = embedded_test_server()->GetURL(
      "portal.test", "/set-header?Content-Disposition: attachment");

  DownloadObserver download_observer;
  EXPECT_TRUE(ExecJs(
      web_contents_impl,
      JsReplace("document.querySelector('portal').src = $1", download_url)));
  EXPECT_FALSE(download_observer.AwaitDownload());
}

IN_PROC_BROWSER_TEST_F(PortalBrowserTest, DownloadsBlockedInSubframe) {
  WebContentsImpl* web_contents_impl =
      static_cast<WebContentsImpl*>(shell()->web_contents());
  ASSERT_TRUE(NavigateToURL(
      web_contents_impl,
      embedded_test_server()->GetURL("portal.test", "/title1.html")));
  CreatePortalToUrl(web_contents_impl, embedded_test_server()->GetURL(
                                           "portal.test", "/title2.html"));

  GURL download_url = embedded_test_server()->GetURL(
      "portal.test", "/set-header?Content-Disposition: attachment");
  GURL iframe_url = embedded_test_server()->GetURL(
      "portal.test", "/iframe?" + net::EscapeQueryParamValue(
                                      download_url.spec(), /*use_plus=*/false));

  DownloadObserver download_observer;
  EXPECT_TRUE(ExecJs(
      web_contents_impl,
      JsReplace("document.querySelector('portal').src = $1", iframe_url)));
  EXPECT_FALSE(download_observer.AwaitDownload());
}

IN_PROC_BROWSER_TEST_F(PortalBrowserTest, DownloadsBlockedViaDownloadLink) {
  WebContentsImpl* web_contents_impl =
      static_cast<WebContentsImpl*>(shell()->web_contents());
  ASSERT_TRUE(NavigateToURL(
      web_contents_impl,
      embedded_test_server()->GetURL("portal.test", "/title1.html")));
  Portal* portal = CreatePortalToUrl(
      web_contents_impl,
      embedded_test_server()->GetURL("portal.test", "/title2.html"));

  DownloadObserver download_observer;
  EXPECT_TRUE(ExecJs(portal->GetPortalContents(),
                     "let a = document.createElement('a');\n"
                     "a.download = 'download.html';\n"
                     "a.href = '/title3.html';\n"
                     "a.click();\n"));
  EXPECT_FALSE(download_observer.AwaitDownload());
}

// The following tests check code paths that won't be hit on Android as we
// do not create DevTools windows on Android.
#if !defined(OS_ANDROID)
IN_PROC_BROWSER_TEST_F(PortalBrowserTest, CallActivateOnTwoPortals) {
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("portal.test", "/title1.html")));
  WebContentsImpl* web_contents_impl =
      static_cast<WebContentsImpl*>(shell()->web_contents());
  RenderFrameHostImpl* main_frame = web_contents_impl->GetMainFrame();
  shell()->ShowDevTools();

  GURL url_a = embedded_test_server()->GetURL("a.com", "/title1.html");
  Portal* portal_a = CreatePortalToUrl(web_contents_impl, url_a);
  GURL url_b = embedded_test_server()->GetURL("b.com", "/title1.html");
  Portal* portal_b = CreatePortalToUrl(web_contents_impl, url_b);

  PortalInterceptorForTesting* portal_interceptor =
      PortalInterceptorForTesting::From(portal_a);
  // Hijacks navigate request and calls Activate on both portals.
  portal_interceptor->SetNavigateCallback(base::BindRepeating(
      [](Portal* portal_a, Portal* portal_b, const GURL&,
         blink::mojom::ReferrerPtr,
         blink::mojom::Portal::NavigateCallback callback) {
        portal_a->Activate(blink::TransferableMessage(), base::TimeTicks::Now(),
                           base::DoNothing());
        portal_b->Activate(blink::TransferableMessage(), base::TimeTicks::Now(),
                           base::DoNothing());
        std::move(callback).Run();
      },
      portal_a, portal_b));

  RenderProcessHostBadIpcMessageWaiter rph_kill_waiter(
      main_frame->GetProcess());
  GURL dummy_url = embedded_test_server()->GetURL("c.com", "/title1.html");
  ExecuteScriptAsync(
      main_frame,
      JsReplace("document.querySelector('portal').src = $1", dummy_url));
  EXPECT_EQ(bad_message::RPH_MOJO_PROCESS_ERROR, rph_kill_waiter.Wait());
}

IN_PROC_BROWSER_TEST_F(PortalBrowserTest, CallActivateTwice) {
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("portal.test", "/title1.html")));
  WebContentsImpl* web_contents_impl =
      static_cast<WebContentsImpl*>(shell()->web_contents());
  RenderFrameHostImpl* main_frame = web_contents_impl->GetMainFrame();
  shell()->ShowDevTools();

  GURL url = embedded_test_server()->GetURL("a.com", "/title1.html");
  Portal* portal = CreatePortalToUrl(web_contents_impl, url);
  PortalInterceptorForTesting* portal_interceptor =
      PortalInterceptorForTesting::From(portal);
  // Hijacks navigate request and calls Activate twice instead.
  portal_interceptor->SetNavigateCallback(base::BindRepeating(
      [](Portal* portal, const GURL&, blink::mojom::ReferrerPtr,
         blink::mojom::Portal::NavigateCallback callback) {
        portal->Activate(blink::TransferableMessage(), base::TimeTicks::Now(),
                         base::DoNothing());
        portal->Activate(blink::TransferableMessage(), base::TimeTicks::Now(),
                         base::DoNothing());
        std::move(callback).Run();
      },
      portal));

  RenderProcessHostBadIpcMessageWaiter rph_kill_waiter(
      main_frame->GetProcess());
  GURL dummy_url = embedded_test_server()->GetURL("b.com", "/title1.html");
  ExecuteScriptAsync(
      main_frame,
      JsReplace("document.querySelector('portal').src = $1", dummy_url));
  EXPECT_EQ(bad_message::RPH_MOJO_PROCESS_ERROR, rph_kill_waiter.Wait());
}
#endif

namespace {

static constexpr struct {
  base::StringPiece token_id;
  base::StringPiece token;
} kOriginTrialTokens[] = {
    // Generated by:
    //  tools/origin_trials/generate_token.py --version 3 --expire-days 3650 \
    //      https://portal.test Portals
    // Token details:
    //  Version: 3
    //  Origin: https://portal.test:443
    //  Is Subdomain: None
    //  Is Third Party: None
    //  Usage Restriction: None
    //  Feature: Portals
    //  Expiry: 1907172789 (2030-06-08 18:13:09 UTC)
    //  Signature (Base64):
    //  3nrCPtI01xhkOinmRegbwhnA5VrNBJUnxLv2yPxSKdtUMyoo9iUZszqtkaTFyV8Al/VJigcAOzLLsKOZ2N6DBQ==
    {"portals",
     "A956wj7SNNcYZDop5kXoG8IZwOVazQSVJ8S79sj8UinbVDMqKPYlGbM6rZGkxclfAJf1SYoHA"
     "Dsyy7CjmdjegwUAAABReyJvcmlnaW4iOiAiaHR0cHM6Ly9wb3J0YWwudGVzdDo0NDMiLCAiZm"
     "VhdHVyZSI6ICJQb3J0YWxzIiwgImV4cGlyeSI6IDE5MDcxNzI3ODl9"},
};

}  // namespace

// Tests that origin trials correctly toggle the feature, and that default
// states are as intended for the same-origin origin trial
// (https://crbug.com/1040212).
//
// That these controls provide suitable safeguards and functionality is tested
// elsewhere.
class PortalOriginTrialBrowserTest : public ContentBrowserTest {
 protected:
  PortalOriginTrialBrowserTest() = default;

  bool PlatformSupportsPortalsOriginTrial() {
#if defined(OS_ANDROID)
    return true;
#else
    return false;
#endif
  }

  void SetUp() override {
    ContentBrowserTest::SetUp();
    EXPECT_EQ(base::FeatureList::IsEnabled(blink::features::kPortals),
              PlatformSupportsPortalsOriginTrial());
    EXPECT_FALSE(
        base::FeatureList::IsEnabled(blink::features::kPortalsCrossOrigin));
  }

  void SetUpOnMainThread() override {
    ContentBrowserTest::SetUpOnMainThread();
    url_loader_interceptor_.emplace(
        base::BindRepeating(&PortalOriginTrialBrowserTest::InterceptRequest));
  }

  void TearDownOnMainThread() override { url_loader_interceptor_.reset(); }

  // URLLoaderInterceptor callback
  static bool InterceptRequest(URLLoaderInterceptor::RequestParams* params) {
    // Find the appropriate origin trial token.
    base::StringPiece origin_trial_token;
    std::string origin_trial_query_param;
    if (net::GetValueForKeyInQuery(params->url_request.url, "origintrial",
                                   &origin_trial_query_param)) {
      for (const auto& pair : kOriginTrialTokens)
        if (pair.token_id == origin_trial_query_param)
          origin_trial_token = pair.token;
    }

    // Construct and send the response.
    std::string headers =
        "HTTP/1.1 200 OK\nContent-Type: text/html; charset=utf-8\n";
    if (!origin_trial_token.empty())
      base::StrAppend(&headers, {"Origin-Trial: ", origin_trial_token, "\n"});
    headers += '\n';
    std::string body = "<!DOCTYPE html><body>Hello world!</body>";
    URLLoaderInterceptor::WriteResponse(headers, body, params->client.get());
    return true;
  }

 private:
  base::Optional<URLLoaderInterceptor> url_loader_interceptor_;
};

IN_PROC_BROWSER_TEST_F(PortalOriginTrialBrowserTest, WithoutTrialToken) {
  WebContentsImpl* web_contents_impl =
      static_cast<WebContentsImpl*>(shell()->web_contents());
  ASSERT_TRUE(NavigateToURL(web_contents_impl, GURL("https://portal.test/")));
  EXPECT_EQ(false, EvalJs(web_contents_impl, "'HTMLPortalElement' in self"));
}

IN_PROC_BROWSER_TEST_F(PortalOriginTrialBrowserTest, WithTrialToken) {
  WebContentsImpl* web_contents_impl =
      static_cast<WebContentsImpl*>(shell()->web_contents());
  ASSERT_TRUE(NavigateToURL(web_contents_impl,
                            GURL("https://portal.test/?origintrial=portals")));
  EXPECT_EQ(PlatformSupportsPortalsOriginTrial(),
            EvalJs(web_contents_impl, "'HTMLPortalElement' in self"));
}

class PortalPixelBrowserTest : public PortalBrowserTest {
 public:
  void SetUp() override {
    EnablePixelOutput();
    PortalBrowserTest::SetUp();
  }
};

// Ensures content is correctly rastered with respect to the page scale factor.
// Note: a portaled page is unique in that it has both kinds of page scale
// factor simultaneously; an external page scale factor that comes from the
// page scale on the embedder page as well as the natural page scale factor on
// the portal page. Though the portal cannot be pinch-zoomed until activated,
// it may use a viewport <meta> tag to set an initial scale factor. This test
// loads a portal that has a zoomed out page, then pinch zooms in on the
// embedder page. Both page scales should be accounted for so the pattern in
// the portal should appear the correct size (4x4 checkerboard tiles) as well
// as be re-rastered for the embedder's zoom so it should appear crisp.
//
// Flaky on Android: https://crbug.com/1120213
#if defined(OS_ANDROID)
#define MAYBE_PageScaleRaster DISABLED_PageScaleRaster
#else
#define MAYBE_PageScaleRaster PageScaleRaster
#endif
IN_PROC_BROWSER_TEST_F(PortalPixelBrowserTest, MAYBE_PageScaleRaster) {
  ShellContentBrowserClient::Get()->set_override_web_preferences_callback(
      base::BindRepeating([](blink::web_pref::WebPreferences* prefs) {
        // Enable processing of the viewport <meta> tag in the same way the
        // Android browser would.
        prefs->viewport_enabled = true;
        prefs->viewport_meta_enabled = true;
        prefs->shrinks_viewport_contents_to_fit = true;
        prefs->viewport_style = blink::web_pref::ViewportStyle::kMobile;

        // Hide scrollbars to make pixel testing more robust.
        prefs->hide_scrollbars = true;
      }));

  EXPECT_TRUE(NavigateToURL(
      shell(),
      embedded_test_server()->GetURL("portal.test", "/portals/raster.html")));

  auto* main_contents = static_cast<WebContentsImpl*>(shell()->web_contents());

  std::vector<WebContents*> inner_web_contents =
      main_contents->GetInnerWebContents();
  ASSERT_EQ(1u, inner_web_contents.size());
  auto* portal_contents = static_cast<WebContentsImpl*>(inner_web_contents[0]);

  RenderFrameSubmissionObserver portal_frame_observer(
      portal_contents->GetFrameTree()->root());

  // Perform a pinch-zoom action into the top-left of the page.
  {
    content::RenderWidgetHostImpl* widget_host =
        content::RenderWidgetHostImpl::From(
            main_contents->GetRenderViewHost()->GetWidget());

    content::SyntheticPinchGestureParams params;
    params.gesture_source_type =
        content::SyntheticGestureParams::TOUCHPAD_INPUT;
    params.scale_factor = 4.f;
    params.anchor = gfx::PointF();
    params.relative_pointer_speed_in_pixels_s = 40000;
    auto pinch_gesture =
        std::make_unique<content::SyntheticTouchpadPinchGesture>(params);

    base::RunLoop run_loop;
    widget_host->QueueSyntheticGesture(
        std::move(pinch_gesture),
        base::BindOnce(
            [](base::OnceClosure quit_closure,
               content::SyntheticGesture::Result result) {
              EXPECT_EQ(content::SyntheticGesture::GESTURE_FINISHED, result);
              std::move(quit_closure).Run();
            },
            run_loop.QuitClosure()));
    run_loop.Run();
  }

  const float kScaleTolerance = 0.1f;

  // The portal should have its external page scale factor set from the main
  // frame's pinch zoom. However, it should have a page scale factor set as
  // well, coming from the initial-scale value of its viewport <meta> tag.
  {
    portal_frame_observer.WaitForExternalPageScaleFactor(4.f, kScaleTolerance);
    EXPECT_EQ(
        0.5, portal_frame_observer.LastRenderFrameMetadata().page_scale_factor);
  }

  // This test passes if the result matches the previously rendered
  // expectation. A small amount of jitter is allowed due to differences in
  // graphics drivers or raster code, but the resulting image should appear
  // crisp.
  {
    // Compare only the top-left 200x200 rect - the checkerboard DIV is 100x100
    // with initial-scale of 0.5. The embedder page zooms in to 4x so the
    // content rect should only be 200x200 output pixels.
    const gfx::Size kCompareSize(200, 200);
    base::FilePath reference =
        content::GetTestFilePath("portals", "raster-expected.png");
    EXPECT_TRUE(CompareWebContentsOutputToReference(main_contents, reference,
                                                    kCompareSize));
  }

  // Now activate the portal. Since this replaces the embedder as the main
  // WebContents, the external page scale factor coming from the embedder
  // should be cleared.
  {
    RenderFrameHostImpl* main_frame = main_contents->GetMainFrame();
    ExecuteScriptAsync(main_frame,
                       "document.querySelector('portal').activate();");
    portal_frame_observer.WaitForExternalPageScaleFactor(1.f, kScaleTolerance);
  }
}

}  // namespace content
