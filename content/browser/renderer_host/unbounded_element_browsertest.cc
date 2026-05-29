// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
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
    feature_list_.InitWithFeatures(
        {blink::features::kUnboundedElement,
         blink::features::kUnboundedElementOnTheOpenWeb},
        {});
    ContentBrowserTest::SetUp();
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

  void wait_for_frame() {
    std::ignore = EvalJs(primary_main_frame_host(),
                         "new Promise(r => requestAnimationFrame(() => "
                         "requestAnimationFrame(r)))");
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(UnboundedElementBrowserTest,
                       DISABLED_ActivationPreconditions) {
  GURL url(embedded_test_server()->GetURL("/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url));

  // Create an unbounded element via HTML snippet:
  std::string script = R"(
    document.body.innerHTML = '<div id="target" unbounded></div>';
    document.getElementById('target').showUnboundedElement().catch(e => e.name);
  )";
  // showUnboundedElement throws DOMException NotAllowedError without transient
  // user gesture.
  EXPECT_EQ("NotAllowedError", EvalJs(primary_main_frame_host(), script));
}

// TODO(crbug.com/508672616): Not yet implemented on Android.
#if BUILDFLAG(IS_ANDROID)
#define MAYBE_CompositorPopupAllocation DISABLED_CompositorPopupAllocation
#define MAYBE_CompositorPopupAllocationHighDPI \
  DISABLED_CompositorPopupAllocationHighDPI
#else
#define MAYBE_CompositorPopupAllocation CompositorPopupAllocation
#define MAYBE_CompositorPopupAllocationHighDPI CompositorPopupAllocationHighDPI
#endif

IN_PROC_BROWSER_TEST_F(UnboundedElementBrowserTest, LightDismissEscKey) {
  GURL url(embedded_test_server()->GetURL("/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url));

  std::string script = R"(
    document.body.innerHTML =
      '<div id="target" style="width:50px; height:50px;" unbounded></div>';
    document.getElementById('target').showUnboundedElement();
  )";
  ASSERT_TRUE(ExecJs(primary_main_frame_host(), script));

  SimulateKeyPress(web_contents(), ui::DomKey::ESCAPE, ui::DomCode::ESCAPE,
                   ui::VKEY_ESCAPE, false, false, false, false);

  std::string get_style =
      "getComputedStyle(document.getElementById('target')).visibility";
  EXPECT_EQ("hidden", EvalJs(primary_main_frame_host(), get_style));
}

IN_PROC_BROWSER_TEST_F(UnboundedElementBrowserTest, LightDismissClickOutside) {
  GURL url(embedded_test_server()->GetURL("/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url));

  std::string script = R"(
    document.body.innerHTML =
      '<div id="target" style="width:50px; height:50px;" unbounded></div>';
    document.getElementById('target').showUnboundedElement();
  )";
  ASSERT_TRUE(ExecJs(primary_main_frame_host(), script));

  SimulateMouseClickAt(web_contents(), 0, blink::WebMouseEvent::Button::kLeft,
                       gfx::Point(300, 300));
  std::string get_style =
      "getComputedStyle(document.getElementById('target')).visibility";
  EXPECT_EQ("hidden", EvalJs(primary_main_frame_host(), get_style));
}

IN_PROC_BROWSER_TEST_F(UnboundedElementBrowserTest,
                       MAYBE_CompositorPopupAllocation) {
  GURL url(embedded_test_server()->GetURL("/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url));

  std::string script = R"(
    (async () => {
      document.body.innerHTML =
        '<div id="target" style="width:100px; height:100px;" unbounded></div>';
      await document.getElementById('target').showUnboundedElement();
      return true;
    })();
  )";
  EXPECT_EQ(true, EvalJs(primary_main_frame_host(), script));
  wait_for_frame();

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
    (async () => {
      const div = document.createElement('div');
      div.setAttribute('unbounded', '');
      try {
        await div.showUnboundedElement();
        return "Success";
      } catch (e) {
        return e.name;
      }
    })();
  )";
  EXPECT_EQ("NotSupportedError", EvalJs(primary_main_frame_host(), script));
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
                       MAYBE_CompositorPopupAllocationHighDPI) {
  GURL url(embedded_test_server()->GetURL("/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url));

  std::string script = R"(
    (async () => {
      document.body.innerHTML =
        '<div id="target" style="width:100px; height:100px;" unbounded></div>';
      await document.getElementById('target').showUnboundedElement();
      return true;
    })();
  )";
  EXPECT_EQ(true, EvalJs(primary_main_frame_host(), script));
  wait_for_frame();

  ASSERT_EQ(1u, web_contents()->GetPopupWidgets().size());
  RenderWidgetHostView* popup_view = web_contents()->GetPopupWidgets()[0];

  float dsf = popup_view->GetDeviceScaleFactor();
  EXPECT_EQ(2.0f, dsf);

  gfx::Rect bounds = popup_view->GetViewBounds();
  EXPECT_EQ(100, bounds.width());
  EXPECT_EQ(100, bounds.height());
}

}  // namespace content
