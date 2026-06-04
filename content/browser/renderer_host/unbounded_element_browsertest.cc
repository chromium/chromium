// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/hit_test_region_observer.h"
#include "content/shell/browser/shell.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "url/gurl.h"

namespace content {

class UnboundedElementBrowserTest : public ContentBrowserTest {
 public:
  UnboundedElementBrowserTest() = default;
  ~UnboundedElementBrowserTest() override = default;
  void SetUp() override {
#if BUILDFLAG(IS_ANDROID)
    // TODO(crbug.com/508672616): Not yet implemented on Android.
    GTEST_SKIP();
#else
    feature_list_.InitWithFeatures(
        {blink::features::kUnboundedElement,
         blink::features::kUnboundedElementOnTheOpenWeb},
        {});
    ContentBrowserTest::SetUp();
#endif
  }

 protected:
  void SetUpOnMainThread() override {
    ContentBrowserTest::SetUpOnMainThread();
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  WebContentsImpl* web_contents() const {
    return static_cast<WebContentsImpl*>(shell()->web_contents());
  }

  RenderFrameHostImpl* primary_main_frame_host() {
    return web_contents()->GetPrimaryFrameTree().root()->current_frame_host();
  }

  void WaitForFrameReady() {
    WaitForHitTestData(primary_main_frame_host());
    MainThreadFrameObserver frame_observer(
        primary_main_frame_host()->GetRenderWidgetHost());
    frame_observer.Wait();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(UnboundedElementBrowserTest, ActivationPreconditions) {
  GURL url(embedded_test_server()->GetURL("/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url));

  // Create an unbounded element via HTML snippet:
  std::string script = R"(
    document.body.innerHTML = '<div id="target" unbounded></div>';
    document.getElementById('target').showUnboundedElement().catch(e => e.name);
  )";
  // showUnboundedElement throws DOMException NotAllowedError without transient
  // user gesture.
  EXPECT_EQ("NotAllowedError", EvalJs(primary_main_frame_host(), script,
                                      EXECUTE_SCRIPT_NO_USER_GESTURE));
}

IN_PROC_BROWSER_TEST_F(UnboundedElementBrowserTest, AncestorClipping) {
  GURL url(embedded_test_server()->GetURL("/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url));

  std::string setup_script = R"(
    document.body.innerHTML = `
      <div id="container" style="width:50px; height:50px; overflow:hidden;
           position:relative;">
        <div id="child" style="width:100px; height:100px; position:absolute;
             top:0; left:0;" unbounded></div>
      </div>
    `;
    const child = document.getElementById('child');
    child.addEventListener('mousedown', () => { window.__clicked = true; });
    child.showUnboundedElement();
  )";
  EXPECT_TRUE(ExecJs(primary_main_frame_host(), setup_script));
  WaitForFrameReady();

  SimulateMouseClickAt(web_contents(), 0, blink::WebMouseEvent::Button::kLeft,
                       gfx::Point(75, 75));
  RunUntilInputProcessed(primary_main_frame_host()->GetRenderWidgetHost());

  EXPECT_TRUE(
      EvalJs(primary_main_frame_host(), "window.__clicked").ExtractBool());
}

IN_PROC_BROWSER_TEST_F(UnboundedElementBrowserTest, InputEventRouting) {
  GURL url(embedded_test_server()->GetURL("/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url));

  std::string script = R"(
    document.body.innerHTML =
        '<div id="child" style="width:100px; height:100px;" unbounded></div>';
    const div = document.getElementById('child');
    div.addEventListener('mousemove', (e) => {
      window.__mouse_x = e.clientX;
      window.__mouse_y = e.clientY;
    });
    div.showUnboundedElement();
  )";
  EXPECT_TRUE(ExecJs(primary_main_frame_host(), script));
  WaitForFrameReady();

  SimulateMouseEvent(web_contents(), blink::WebInputEvent::Type::kMouseMove,
                     gfx::Point(50, 50));
  RunUntilInputProcessed(primary_main_frame_host()->GetRenderWidgetHost());

  EXPECT_EQ(50, EvalJs(primary_main_frame_host(), "window.__mouse_x"));
  EXPECT_EQ(50, EvalJs(primary_main_frame_host(), "window.__mouse_y"));
}

// TODO(crbug.com/508672616): This test is currently broken because visibility
// styles are not yet reset to "hidden" when
// unbounded_surface_client_->OnDismissed() is called.
IN_PROC_BROWSER_TEST_F(UnboundedElementBrowserTest,
                       DISABLED_LightDismissEscKey) {
  GURL url(embedded_test_server()->GetURL("/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url));

  std::string script = R"(
    document.body.innerHTML =
      '<div id="target" style="width:50px; height:50px;" unbounded></div>';
    document.getElementById('target').showUnboundedElement();
  )";
  ASSERT_TRUE(ExecJs(primary_main_frame_host(), script));
  WaitForFrameReady();

  SimulateKeyPress(web_contents(), ui::DomKey::ESCAPE, ui::DomCode::ESCAPE,
                   ui::VKEY_ESCAPE, false, false, false, false);
  RunUntilInputProcessed(primary_main_frame_host()->GetRenderWidgetHost());

  std::string get_style =
      "getComputedStyle(document.getElementById('target')).visibility";
  EXPECT_EQ("hidden", EvalJs(primary_main_frame_host(), get_style));
}

// TODO(crbug.com/508672616): This test is currently broken because visibility
// styles are not yet reset to "hidden" when
// unbounded_surface_client_->OnDismissed() is called.
IN_PROC_BROWSER_TEST_F(UnboundedElementBrowserTest,
                       DISABLED_LightDismissClickOutside) {
  GURL url(embedded_test_server()->GetURL("/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url));

  std::string script = R"(
    document.body.innerHTML =
      '<div id="target" style="width:50px; height:50px;" unbounded></div>';
    document.getElementById('target').showUnboundedElement();
  )";
  ASSERT_TRUE(ExecJs(primary_main_frame_host(), script));
  WaitForFrameReady();

  SimulateMouseClickAt(web_contents(), 0, blink::WebMouseEvent::Button::kLeft,
                       gfx::Point(300, 300));
  RunUntilInputProcessed(primary_main_frame_host()->GetRenderWidgetHost());
  std::string get_style =
      "getComputedStyle(document.getElementById('target')).visibility";
  EXPECT_EQ("hidden", EvalJs(primary_main_frame_host(), get_style));
}

IN_PROC_BROWSER_TEST_F(UnboundedElementBrowserTest, PopoverInsideUnbounded) {
  GURL url(embedded_test_server()->GetURL("/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url));

  std::string script = R"(
    document.body.innerHTML = `
      <div id="child" style="width:100px; height:100px;" unbounded>
        <div id="popover" popover>Nested Popover</div>
      </div>
    `;
    document.getElementById('child').showUnboundedElement().then(() => {
      document.getElementById('popover').showPopover();
    });
  )";
  ASSERT_TRUE(ExecJs(primary_main_frame_host(), script));
  WaitForFrameReady();

  EXPECT_TRUE(EvalJs(primary_main_frame_host(),
                     "document.getElementById('popover')"
                     ".matches(':popover-open')")
                  .ExtractBool());
}

IN_PROC_BROWSER_TEST_F(UnboundedElementBrowserTest, CompositorPopupAllocation) {
  GURL url(embedded_test_server()->GetURL("/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url));

  std::string script = R"(
    document.body.innerHTML =
      '<div id="target" style="width:100px; height:100px;" unbounded></div>';
    document.getElementById('target').showUnboundedElement();
  )";
  EXPECT_TRUE(ExecJs(primary_main_frame_host(), script));
  WaitForFrameReady();

  ASSERT_EQ(1u, web_contents()->GetPopupWidgets().size());
  RenderWidgetHostView* popup_view = web_contents()->GetPopupWidgets()[0];
  gfx::Rect bounds = popup_view->GetViewBounds();
  EXPECT_EQ(100, bounds.width());
  EXPECT_EQ(100, bounds.height());
}

IN_PROC_BROWSER_TEST_F(UnboundedElementBrowserTest,
                       RequestWithEmptyBoundsThrowsException) {
  GURL url(embedded_test_server()->GetURL("/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url));

  // Execute script that calls showUnboundedElement on an unattached element
  // (empty bounds) and catches the exception name.
  std::string script = R"(
    const div = document.createElement('div');
    div.setAttribute('unbounded', '');
    div.showUnboundedElement().then(() => "Success", e => e.name);
  )";
  EXPECT_EQ("NotSupportedError", EvalJs(primary_main_frame_host(), script));
}

IN_PROC_BROWSER_TEST_F(UnboundedElementBrowserTest,
                       RequestWithoutAttributeThrowsException) {
  GURL url(embedded_test_server()->GetURL("/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url));

  // Execute script that calls showUnboundedElement on an element
  // without the 'unbounded' attribute and catches the exception name.
  std::string script = R"(
    const div = document.createElement('div');
    div.showUnboundedElement().then(() => "Success", e => e.name);
  )";
  EXPECT_EQ("InvalidStateError", EvalJs(primary_main_frame_host(), script));
}

class UnboundedElementHighDPIBrowserTest : public UnboundedElementBrowserTest {
 public:
  UnboundedElementHighDPIBrowserTest() = default;
  ~UnboundedElementHighDPIBrowserTest() override = default;

  void SetUp() override {
    EnablePixelOutput(2.0f);
    UnboundedElementBrowserTest::SetUp();
  }
};

IN_PROC_BROWSER_TEST_F(UnboundedElementHighDPIBrowserTest,
                       CompositorPopupAllocationHighDPI) {
  GURL url(embedded_test_server()->GetURL("/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url));

  std::string script = R"(
    document.body.innerHTML =
      '<div id="target" style="width:100px; height:100px;" unbounded></div>';
    document.getElementById('target').showUnboundedElement();
  )";
  EXPECT_TRUE(ExecJs(primary_main_frame_host(), script));
  WaitForFrameReady();

  ASSERT_EQ(1u, web_contents()->GetPopupWidgets().size());
  RenderWidgetHostView* popup_view = web_contents()->GetPopupWidgets()[0];

  float dsf = popup_view->GetDeviceScaleFactor();
  EXPECT_EQ(2.0f, dsf);

  gfx::Rect bounds = popup_view->GetViewBounds();
  EXPECT_EQ(100, bounds.width());
  EXPECT_EQ(100, bounds.height());
}

IN_PROC_BROWSER_TEST_F(UnboundedElementBrowserTest,
                       VisualOverflowBoundsAndMasking) {
  GURL url(embedded_test_server()->GetURL("/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url));

  std::string script = R"(
    document.body.innerHTML = `
      <style>
        #child {
          width: 200px;
          height: 90px;
          border-radius: 6px;
          box-shadow: 0 4px 12px rgba(0,0,0,0.15);
        }
      </style>
      <div id="child" unbounded>
        <div class="item">Content</div>
      </div>
    `;
    document.getElementById('child').showUnboundedElement();
  )";

  EXPECT_TRUE(ExecJs(primary_main_frame_host(), script));
  WaitForFrameReady();

  EXPECT_EQ("visible", EvalJs(primary_main_frame_host(), R"(
    window.getComputedStyle(document.querySelector('.item')).visibility;
  )"));

  ASSERT_EQ(1u, web_contents()->GetPopupWidgets().size());
  RenderWidgetHostView* popup_view = web_contents()->GetPopupWidgets()[0];
  RenderWidgetHostImpl* rwhi =
      static_cast<RenderWidgetHostImpl*>(popup_view->GetRenderWidgetHost());
  RenderFrameHostImpl* rfh =
      static_cast<RenderFrameHostImpl*>(primary_main_frame_host());
  EXPECT_EQ(rwhi, rfh->active_unbounded_widget());
  gfx::Rect popup_bounds = popup_view->GetViewBounds();
  EXPECT_GE(popup_bounds.width(), 200);
  EXPECT_GE(popup_bounds.height(), 90);
}

}  // namespace content
