// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/test/run_until.h"
#include "base/test/test_timeouts.h"
#include "build/build_config.h"
#include "content/browser/renderer_host/cross_process_frame_connector.h"
#include "content/browser/renderer_host/frame_tree.h"
#include "content/browser/renderer_host/render_frame_proxy_host.h"
#include "content/browser/site_per_process_browsertest.h"
#include "content/common/input/synthetic_smooth_scroll_gesture.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/hit_test_region_observer.h"
#include "content/public/test/synchronize_visual_properties_interceptor.h"
#include "content/public/test/test_frame_navigation_observer.h"
#include "content/test/render_document_feature.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/gesture_detection/gesture_configuration.h"
#include "ui/events/keycodes/dom/keycode_converter.h"
#include "ui/native_theme/native_theme_features.h"

namespace content {

class ScrollingIntegrationTest : public SitePerProcessBrowserTest {
 public:
  ScrollingIntegrationTest() = default;
  ~ScrollingIntegrationTest() override = default;

  void DoScroll(const gfx::Point& point,
                const gfx::Vector2d& distance,
                content::mojom::GestureSourceType source) {
    SyntheticSmoothScrollGestureParams params;
    params.gesture_source_type = source;
    params.anchor = gfx::PointF(point);
    params.distances.push_back(-distance);
    params.granularity = ui::ScrollGranularity::kScrollByPrecisePixel;

    auto gesture = std::make_unique<SyntheticSmoothScrollGesture>(params);

    // Runs until we get the SyntheticGestureCompleted callback
    base::RunLoop run_loop;
    GetRenderWidgetHostImpl()->QueueSyntheticGesture(
        std::move(gesture),
        base::BindLambdaForTesting([&](SyntheticGesture::Result result) {
          EXPECT_EQ(SyntheticGesture::GESTURE_FINISHED, result);
          run_loop.Quit();
        }));
    run_loop.Run();
  }

  double GetScrollTop() {
    FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                              ->GetPrimaryFrameTree()
                              .root();
    return EvalJs(root, "window.scrollY").ExtractDouble();
  }

  void WaitForVerticalScroll() {
    RenderFrameSubmissionObserver frame_observer(shell()->web_contents());
    gfx::PointF default_scroll_offset;
    while (frame_observer.LastRenderFrameMetadata()
               .root_scroll_offset.value_or(default_scroll_offset)
               .y() <= 0) {
      frame_observer.WaitForMetadataChange();
    }
  }

  RenderWidgetHostImpl* GetRenderWidgetHostImpl() {
    FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                              ->GetPrimaryFrameTree()
                              .root();
    return root->current_frame_host()->GetRenderWidgetHost();
  }
};

// Tests basic scrolling after navigating to a new origin works. Guards against
// bugs like https://crbug.com/899234 which are caused by invalid
// initialization due to the cross-origin provisional frame swap.
IN_PROC_BROWSER_TEST_P(ScrollingIntegrationTest,
                       ScrollAfterCrossOriginNavigation) {
  // Navigate to the a.com domain first.
  GURL url_domain_a(
      embedded_test_server()->GetURL("a.com", "/simple_page.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url_domain_a));

  // Now navigate to baz.com, this should cause a cross-origin navigation which
  // will load into a provisional frame and then swap in as a local main frame.
  // This test ensures all the correct initialization takes place in the
  // renderer so that a basic scrolling smoke test works.
  GURL url_domain_b(embedded_test_server()->GetURL(
      "baz.com", "/scrollable_page_with_iframe.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url_domain_b));
  ASSERT_TRUE(WaitForLoadStop(shell()->web_contents()));

  {
    // TODO(bokan): We currently don't have a good way to know when the
    // compositor's scrolling layers are ready after changes on the main thread.
    // We wait a timeout but that's really a hack. Fixing is tracked in
    // https://crbug.com/897520
    base::RunLoop run_loop;
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE, run_loop.QuitClosure(), base::Milliseconds(3000));
    run_loop.Run();
  }

  content::mojom::GestureSourceType source;

// TODO(bokan): Mac doesn't support touch events and for an unknown reason,
// Android doesn't like mouse wheel here. https://crbug.com/897520.
#if BUILDFLAG(IS_ANDROID)
  source = content::mojom::GestureSourceType::kTouchInput;
#else
  source = content::mojom::GestureSourceType::kTouchpadInput;
#endif

  // Perform the scroll (below the iframe), ensure it's correctly processed.
  DoScroll(gfx::Point(100, 110), gfx::Vector2d(0, 500), source);
  WaitForVerticalScroll();
  EXPECT_GT(GetScrollTop(), 0);
}

class SitePerProcessScrollAnchorTest : public SitePerProcessBrowserTest {
 public:
  SitePerProcessScrollAnchorTest() = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    SitePerProcessBrowserTestBase::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII(switches::kEnableBlinkFeatures,
                                    "ScrollAnchorSerialization");
  }
};

IN_PROC_BROWSER_TEST_P(SitePerProcessScrollAnchorTest,
                       RemoteToLocalScrollAnchorRestore) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/page_with_samesite_iframe.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();
  FrameTreeNode* child = root->child_at(0);

  GURL frame_url(embedded_test_server()->GetURL("b.com", "/title1.html"));
  EXPECT_TRUE(NavigateToURLFromRenderer(child, frame_url));

  EXPECT_NE(child->current_frame_host()->GetSiteInstance(),
            root->current_frame_host()->GetSiteInstance());

  TestFrameNavigationObserver frame_observer2(child);
  EXPECT_TRUE(ExecJs(root, "window.history.back()"));
  frame_observer2.Wait();

  EXPECT_EQ(child->current_frame_host()->GetSiteInstance(),
            root->current_frame_host()->GetSiteInstance());
}

class SitePerProcessProgrammaticScrollTest : public SitePerProcessBrowserTest {
 public:
  SitePerProcessProgrammaticScrollTest()
      : kPositiveXYPlane(0, 0, kInfinity, kInfinity) {}

  SitePerProcessProgrammaticScrollTest(
      const SitePerProcessProgrammaticScrollTest&) = delete;
  SitePerProcessProgrammaticScrollTest& operator=(
      const SitePerProcessProgrammaticScrollTest&) = delete;

 protected:
  const size_t kInfinity = 1000000u;
  const std::string kIframeOutOfViewHTML = "/iframe_out_of_view.html";
  const std::string kIframeClippedHTML = "/iframe_clipped.html";
  const std::string kInputBoxHTML = "/input_box.html";
  const std::string kIframeSelector = "iframe";
  const std::string kInputSelector = "input";
  const gfx::Rect kPositiveXYPlane;

  // Waits until the |load| handle is called inside the frame.
  void WaitForOnLoad(FrameTreeNode* node) {
    RunCommandAndWaitForResponse(node, "notifyWhenLoaded();", "LOADED");
  }

  void WaitForElementVisible(FrameTreeNode* node, const std::string& sel) {
    RunCommandAndWaitForResponse(
        node,
        base::StringPrintf("notifyWhenVisible(document.querySelector('%s'));",
                           sel.c_str()),
        "VISIBLE");
  }

  void WaitForViewportToStabilize(FrameTreeNode* node) {
    RunCommandAndWaitForResponse(node, "notifyWhenViewportStable(0);",
                                 "VIEWPORT_STABLE");
  }

  void AddFocusedInputField(FrameTreeNode* node) {
    ASSERT_TRUE(ExecJs(node, "addFocusedInputField();"));
  }

  void SetWindowScroll(FrameTreeNode* node, int x, int y) {
    ASSERT_TRUE(
        ExecJs(node, base::StringPrintf("window.scrollTo(%d, %d);", x, y)));
  }

  // Helper function to retrieve the bounding client rect of the element
  // identified by |sel| inside |rfh|.
  gfx::Rect GetBoundingClientRect(RenderFrameHostImpl* rfh,
                                  const std::string& sel) {
    return GetRectFromString(
        EvalJs(rfh, JsReplace("rectAsString(document.querySelector($1)."
                              "getBoundingClientRect());",
                              sel))
            .ExtractString());
  }

  // Returns a rect representing the current |visualViewport| in the main frame
  // of |contents|.
  gfx::Rect GetVisualViewport(FrameTreeNode* node) {
    return GetRectFromString(
        EvalJs(node, "rectAsString(visualViewportAsRect());").ExtractString());
  }

  float GetVisualViewportScale(FrameTreeNode* node) {
    return EvalJs(node, "visualViewport.scale;").ExtractDouble();
  }

 private:
  void RunCommandAndWaitForResponse(FrameTreeNode* node,
                                    const std::string& command,
                                    const std::string& response) {
    ASSERT_EQ(response, EvalJs(node, command));
  }

  gfx::Rect GetRectFromString(const std::string& str) {
    std::vector<std::string> tokens = base::SplitString(
        str, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
    EXPECT_EQ(4U, tokens.size());
    double x = 0.0, y = 0.0, width = 0.0, height = 0.0;
    EXPECT_TRUE(base::StringToDouble(tokens[0], &x));
    EXPECT_TRUE(base::StringToDouble(tokens[1], &y));
    EXPECT_TRUE(base::StringToDouble(tokens[2], &width));
    EXPECT_TRUE(base::StringToDouble(tokens[3], &height));
    return {static_cast<int>(x), static_cast<int>(y), static_cast<int>(width),
            static_cast<int>(height)};
  }
};

IN_PROC_BROWSER_TEST_P(SitePerProcessProgrammaticScrollTest,
                       ScrolledOutOfView) {
  GURL main_frame(
      embedded_test_server()->GetURL("a.com", kIframeOutOfViewHTML));
  GURL child_url_b(
      embedded_test_server()->GetURL("b.com", kIframeOutOfViewHTML));

  // This will set up the page frame tree as A(B()).
  ASSERT_TRUE(NavigateToURL(shell(), main_frame));
  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();
  WaitForOnLoad(root);
  EXPECT_TRUE(NavigateToURLFromRenderer(root->child_at(0), child_url_b));
  WaitForOnLoad(root->child_at(0));

  FrameTreeNode* nested_iframe_node = root->child_at(0);
  RenderFrameProxyHost* proxy_to_parent =
      nested_iframe_node->render_manager()->GetProxyToParent();
  CrossProcessFrameConnector* connector =
      proxy_to_parent->cross_process_frame_connector();

  EXPECT_TRUE(base::test::RunUntil([&]() {
    return blink::mojom::FrameVisibility::kRenderedOutOfViewport ==
           connector->visibility();
  }));
}

// This test verifies that smooth scrolling works correctly inside nested OOPIFs
// which are same origin with the parent. Note that since the frame tree has
// a A(B(A1())) structure, if and A1 and A2 shared the same
// SmoothScrollSequencer, then this test would time out or at best be flaky with
// random time outs. See https://crbug.com/865446 for more context.
IN_PROC_BROWSER_TEST_P(SitePerProcessProgrammaticScrollTest,
                       SmoothScrollInNestedSameProcessOOPIF) {
  GURL main_frame(
      embedded_test_server()->GetURL("a.com", kIframeOutOfViewHTML));
  GURL child_url_b(
      embedded_test_server()->GetURL("b.com", kIframeOutOfViewHTML));
  GURL same_origin(
      embedded_test_server()->GetURL("a.com", kIframeOutOfViewHTML));

  // This will set up the page frame tree as A(B(A1(A2()))) where A1 is later
  // asked to scroll the <iframe> element of A2 into view. The important bit
  // here is that the inner frame A1 is recursively scrolling (smoothly) an
  // element inside its document into view (A2's origin is irrelevant here).
  ASSERT_TRUE(NavigateToURL(shell(), main_frame));
  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();
  WaitForOnLoad(root);
  EXPECT_TRUE(NavigateToURLFromRenderer(root->child_at(0), child_url_b));
  WaitForOnLoad(root->child_at(0));
  auto* nested_ftn = root->child_at(0)->child_at(0);
  EXPECT_TRUE(NavigateToURLFromRenderer(nested_ftn, same_origin));
  WaitForOnLoad(nested_ftn);

  // *Smoothly* scroll the inner most frame into view.
  ASSERT_TRUE(ExecJs(
      nested_ftn,
      "document.querySelector('iframe').scrollIntoView({behavior: 'smooth'})"));
  WaitForElementVisible(root, kIframeSelector);
  WaitForElementVisible(root->child_at(0), kIframeSelector);
  WaitForElementVisible(nested_ftn, kIframeSelector);
}

class ScrollObserver : public RenderWidgetHost::InputEventObserver {
 public:
  ScrollObserver(double delta_x, double delta_y) { Reset(delta_x, delta_y); }
  ~ScrollObserver() override = default;

  ScrollObserver(const ScrollObserver&) = delete;
  ScrollObserver& operator=(const ScrollObserver&) = delete;

  void OnInputEvent(const blink::WebInputEvent& event) override {
    if (event.GetType() == blink::WebInputEvent::Type::kGestureScrollUpdate) {
      blink::WebGestureEvent received_update =
          *static_cast<const blink::WebGestureEvent*>(&event);
      remaining_delta_x_ -= received_update.data.scroll_update.delta_x;
      remaining_delta_y_ -= received_update.data.scroll_update.delta_y;
    } else if (event.GetType() ==
               blink::WebInputEvent::Type::kGestureScrollEnd) {
      if (run_loop_->running())
        run_loop_->Quit();
      DCHECK_EQ(0, remaining_delta_x_);
      DCHECK_EQ(0, remaining_delta_y_);
      scroll_end_received_ = true;
    }
  }

  void Wait() {
    if (!scroll_end_received_)
      run_loop_->Run();
  }

  void Reset(double delta_x, double delta_y) {
    run_loop_ = std::make_unique<base::RunLoop>();
    remaining_delta_x_ = delta_x;
    remaining_delta_y_ = delta_y;
    scroll_end_received_ = false;
  }

 private:
  std::unique_ptr<base::RunLoop> run_loop_;
  double remaining_delta_x_;
  double remaining_delta_y_;
  bool scroll_end_received_;
};

// Disabled for high flakiness on multiple platforms. See crbug.com/1063045
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest,
                       DISABLED_ScrollBubblingFromNestedOOPIFTest) {
  ui::GestureConfiguration::GetInstance()->set_scroll_debounce_interval_in_ms(
      0);
  GURL main_url(embedded_test_server()->GetURL(
      "/frame_tree/page_with_positioned_nested_frames.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));
  RenderFrameSubmissionObserver frame_observer(shell()->web_contents());

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();
  ASSERT_EQ(1U, root->child_count());

  FrameTreeNode* parent_iframe_node = root->child_at(0);
  GURL site_url(embedded_test_server()->GetURL(
      "a.com", "/frame_tree/page_with_positioned_frame.html"));
  EXPECT_EQ(site_url, parent_iframe_node->current_url());

  FrameTreeNode* nested_iframe_node = parent_iframe_node->child_at(0);
  GURL nested_site_url(
      embedded_test_server()->GetURL("baz.com", "/title1.html"));
  EXPECT_EQ(nested_site_url, nested_iframe_node->current_url());

  RenderWidgetHostViewBase* root_view = static_cast<RenderWidgetHostViewBase*>(
      root->current_frame_host()->GetRenderWidgetHost()->GetView());

  RenderWidgetHostViewBase* rwhv_nested =
      static_cast<RenderWidgetHostViewBase*>(
          nested_iframe_node->current_frame_host()
              ->GetRenderWidgetHost()
              ->GetView());

  WaitForHitTestData(nested_iframe_node->current_frame_host());

  InputEventAckWaiter ack_observer(
      root->current_frame_host()->GetRenderWidgetHost(),
      blink::WebInputEvent::Type::kGestureScrollBegin);

  std::unique_ptr<ScrollObserver> scroll_observer;

  // All GSU events will be wrapped between a single GSB-GSE pair. The expected
  // delta value is equal to summation of all scroll update deltas.
  scroll_observer = std::make_unique<ScrollObserver>(0, 15);

  root->current_frame_host()->GetRenderWidgetHost()->AddInputEventObserver(
      scroll_observer.get());

  // Now scroll the nested frame upward, this must bubble all the way up to the
  // root.
  blink::WebMouseWheelEvent scroll_event(
      blink::WebInputEvent::Type::kMouseWheel,
      blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  gfx::Rect bounds = rwhv_nested->GetViewBounds();
  float scale_factor =
      frame_observer.LastRenderFrameMetadata().page_scale_factor;
  scroll_event.SetPositionInWidget(
      std::ceil((bounds.x() - root_view->GetViewBounds().x() + 10) *
                scale_factor),
      std::ceil((bounds.y() - root_view->GetViewBounds().y() + 10) *
                scale_factor));
  scroll_event.delta_units = ui::ScrollGranularity::kScrollByPrecisePixel;
  scroll_event.delta_x = 0.0f;
  scroll_event.delta_y = 5.0f;
  scroll_event.phase = blink::WebMouseWheelEvent::kPhaseBegan;
  rwhv_nested->ProcessMouseWheelEvent(scroll_event, ui::LatencyInfo());
  ack_observer.Wait();

  // Send 10 wheel events with delta_y = 1 to the nested oopif.
  scroll_event.delta_y = 1.0f;
  scroll_event.phase = blink::WebMouseWheelEvent::kPhaseChanged;
  for (int i = 0; i < 10; i++)
    rwhv_nested->ProcessMouseWheelEvent(scroll_event, ui::LatencyInfo());

  // Send a wheel end event to complete the scrolling sequence.
  scroll_event.delta_y = 0.0f;
  scroll_event.phase = blink::WebMouseWheelEvent::kPhaseEnded;
  rwhv_nested->ProcessMouseWheelEvent(scroll_event, ui::LatencyInfo());
  scroll_observer->Wait();

  // Remove scroller_observer because it is only available in this
  // scope.
  root->current_frame_host()->GetRenderWidgetHost()->RemoveInputEventObserver(
      scroll_observer.get());
}

// Tests that scrolling bubbles from an oopif if its source body has
// "overflow:hidden" style.
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest,
                       ScrollBubblingFromOOPIFWithBodyOverflowHidden) {
  GURL url_domain_a(embedded_test_server()->GetURL(
      "a.com", "/scrollable_page_with_iframe.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url_domain_a));
  RenderFrameSubmissionObserver frame_observer(shell()->web_contents());
  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();

  FrameTreeNode* iframe_node = root->child_at(0);
  GURL url_domain_b(
      embedded_test_server()->GetURL("b.com", "/body_overflow_hidden.html"));
  EXPECT_TRUE(NavigateToURLFromRenderer(iframe_node, url_domain_b));
  WaitForHitTestData(iframe_node->current_frame_host());

  RenderWidgetHostViewBase* root_view = static_cast<RenderWidgetHostViewBase*>(
      root->current_frame_host()->GetRenderWidgetHost()->GetView());

  RenderWidgetHostViewBase* child_view = static_cast<RenderWidgetHostViewBase*>(
      iframe_node->current_frame_host()->GetRenderWidgetHost()->GetView());

  ScrollObserver scroll_observer(0, -5);
  base::ScopedObservation<RenderWidgetHostImpl,
                          RenderWidgetHost::InputEventObserver>
      scroll_observation_(&scroll_observer);
  scroll_observation_.Observe(
      root->current_frame_host()->GetRenderWidgetHost());

  // Now scroll the nested frame downward, this must bubble to the root since
  // the iframe source body is not scrollable.
  blink::WebMouseWheelEvent scroll_event(
      blink::WebInputEvent::Type::kMouseWheel,
      blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  gfx::Rect bounds = child_view->GetViewBounds();
  float scale_factor =
      frame_observer.LastRenderFrameMetadata().page_scale_factor;
  scroll_event.SetPositionInWidget(
      std::ceil((bounds.x() - root_view->GetViewBounds().x() + 10) *
                scale_factor),
      std::ceil((bounds.y() - root_view->GetViewBounds().y() + 10) *
                scale_factor));
  scroll_event.delta_units = ui::ScrollGranularity::kScrollByPrecisePixel;
  scroll_event.delta_x = 0.0f;
  scroll_event.delta_y = -5.0f;
  scroll_event.phase = blink::WebMouseWheelEvent::kPhaseBegan;
  child_view->ProcessMouseWheelEvent(scroll_event, ui::LatencyInfo());

  // Send a wheel end event to complete the scrolling sequence.
  scroll_event.delta_y = 0.0f;
  scroll_event.phase = blink::WebMouseWheelEvent::kPhaseEnded;
  child_view->ProcessMouseWheelEvent(scroll_event, ui::LatencyInfo());

  scroll_observer.Wait();
}

// This class intercepts RenderFrameProxyHost creations, and creates an
// SynchronizeVisualPropertiesInterceptor to intercept the message of
// SynchronizeVisualProperties. We may not use them all but we need to create
// the interceptors as soon as the RenderFrameProxyHost is created so we don't
// miss any messages.
class ScrollBubblingProxyObserver : RenderFrameProxyHost::TestObserver {
 public:
  ScrollBubblingProxyObserver() {
    RenderFrameProxyHost::SetObserverForTesting(this);
  }

  // We don't need to set an empty callback to
  // RenderFrameProxyHost::Set[Created|Deleted]CallbackForTesting because we
  // already bound callbacks using a weak ptr.
  ~ScrollBubblingProxyObserver() override {
    RenderFrameProxyHost::SetObserverForTesting(nullptr);
  }

  SynchronizeVisualPropertiesInterceptor* interceptor(
      RenderFrameProxyHost* proxy) {
    return interceptors_.find(proxy)->second.get();
  }

 private:
  void OnCreated(RenderFrameProxyHost* proxy_host) override {
    interceptors_.emplace(
        proxy_host,
        std::make_unique<SynchronizeVisualPropertiesInterceptor>(proxy_host));
  }

  void OnDeleted(RenderFrameProxyHost* proxy_host) override {
    // RenderFrameProxyHost can be deleted before the test is finished. In such
    // case, |interceptors_| should remove the mapped interceptor to avoid a
    // dangling pointer issue when it's destroyed.
    interceptors_.erase(proxy_host);
  }

  std::map<RenderFrameProxyHost*,
           std::unique_ptr<SynchronizeVisualPropertiesInterceptor>>
      interceptors_;
};

// Test that scrolling a nested out-of-process iframe bubbles unused scroll
// delta to a parent frame.
// Flaky on all platforms: https://crbug.com/1148741
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest,
                       DISABLED_ScrollBubblingFromOOPIFTest) {
  ScrollBubblingProxyObserver scroll_bubbling_proxy_observer;

  ui::GestureConfiguration::GetInstance()->set_scroll_debounce_interval_in_ms(
      0);
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b)"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();
  ASSERT_EQ(1U, root->child_count());

  FrameTreeNode* parent_iframe_node = root->child_at(0);

  GURL site_url(embedded_test_server()->GetURL(
      "b.com", "/frame_tree/page_with_positioned_frame.html"));
  EXPECT_TRUE(NavigateToURLFromRenderer(parent_iframe_node, site_url));

  InputEventAckWaiter ack_observer(
      parent_iframe_node->current_frame_host()->GetRenderWidgetHost(),
      blink::WebInputEvent::Type::kGestureScrollEnd);

  // Navigate the nested frame to a page large enough to have scrollbars.
  FrameTreeNode* nested_iframe_node = parent_iframe_node->child_at(0);
  GURL nested_site_url(
      embedded_test_server()->GetURL("baz.com", "/tall_page.html"));
  EXPECT_TRUE(NavigateToURLFromRenderer(nested_iframe_node, nested_site_url));

  // This test uses the position of the nested iframe within the parent iframe
  // to infer the scroll position of the parent.
  // SynchronizeVisualPropertiesInterceptor catches updates to the position in
  // order to avoid busy waiting. It gets created early to catch the initial
  // rects from the navigation.
  RenderFrameProxyHost* parent_iframe_proxy =
      nested_iframe_node->render_manager()->GetProxyToParent();

  NavigateFrameToURL(nested_iframe_node, nested_site_url);

  EXPECT_EQ(
      " Site A ------------ proxies for B C\n"
      "   +--Site B ------- proxies for A C\n"
      "        +--Site C -- proxies for A B\n"
      "Where A = http://a.com/\n"
      "      B = http://b.com/\n"
      "      C = http://baz.com/",
      DepictFrameTree(root));

  RenderWidgetHostViewBase* rwhv_parent =
      static_cast<RenderWidgetHostViewBase*>(
          parent_iframe_node->current_frame_host()
              ->GetRenderWidgetHost()
              ->GetView());

  RenderWidgetHostViewBase* rwhv_nested =
      static_cast<RenderWidgetHostViewBase*>(
          nested_iframe_node->current_frame_host()
              ->GetRenderWidgetHost()
              ->GetView());

  WaitForHitTestData(parent_iframe_node->current_frame_host());

  auto* interceptor =
      scroll_bubbling_proxy_observer.interceptor(parent_iframe_proxy);

  // Save the original offset as a point of reference.
  interceptor->WaitForRect();
  gfx::Rect update_rect = interceptor->last_rect();
  int initial_y = update_rect.y();
  interceptor->ResetRectRunLoop();

  // Scroll the parent frame downward.
  blink::WebMouseWheelEvent scroll_event(
      blink::WebInputEvent::Type::kMouseWheel,
      blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  scroll_event.SetPositionInWidget(1, 1);
  // Use precise pixels to keep these events off the animated scroll pathways,
  // which currently break this test.
  // https://bugs.chromium.org/p/chromium/issues/detail?id=710513
  scroll_event.delta_units = ui::ScrollGranularity::kScrollByPrecisePixel;
  scroll_event.delta_x = 0.0f;
  scroll_event.delta_y = -5.0f;
  scroll_event.phase = blink::WebMouseWheelEvent::kPhaseBegan;
  rwhv_parent->ProcessMouseWheelEvent(scroll_event, ui::LatencyInfo());

  // The event router sends wheel events of a single scroll sequence to the
  // target under the first wheel event. Send a wheel end event to the current
  // target view before sending a wheel event to a different one.
  scroll_event.delta_y = 0.0f;
  scroll_event.phase = blink::WebMouseWheelEvent::kPhaseEnded;
  scroll_event.dispatch_type =
      blink::WebInputEvent::DispatchType::kEventNonBlocking;
  rwhv_parent->ProcessMouseWheelEvent(scroll_event, ui::LatencyInfo());

  // Ensure that the view position is propagated to the child properly.
  interceptor->WaitForRect();
  update_rect = interceptor->last_rect();
  EXPECT_LT(update_rect.y(), initial_y);
  interceptor->ResetRectRunLoop();
  ack_observer.Reset();

  // Now scroll the nested frame upward, which should bubble to the parent.
  // The upscroll exceeds the amount that the frame was initially scrolled
  // down to account for rounding.
  scroll_event.delta_y = 6.0f;
  scroll_event.dispatch_type = blink::WebInputEvent::DispatchType::kBlocking;
  scroll_event.phase = blink::WebMouseWheelEvent::kPhaseBegan;
  rwhv_nested->ProcessMouseWheelEvent(scroll_event, ui::LatencyInfo());

  interceptor->WaitForRect();
  // This loop isn't great, but it accounts for the possibility of multiple
  // incremental updates happening as a result of the scroll animation.
  // A failure condition of this test is that the loop might not terminate
  // due to bubbling not working properly. If the overscroll bubbles to the
  // parent iframe then the nested frame's y coord will return to its
  // initial position.
  update_rect = interceptor->last_rect();
  while (update_rect.y() > initial_y) {
    base::RunLoop run_loop;
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE, run_loop.QuitClosure(), TestTimeouts::tiny_timeout());
    run_loop.Run();
    update_rect = interceptor->last_rect();
  }

  // The event router sends wheel events of a single scroll sequence to the
  // target under the first wheel event. Send a wheel end event to the current
  // target view before sending a wheel event to a different one.
  scroll_event.delta_y = 0.0f;
  scroll_event.phase = blink::WebMouseWheelEvent::kPhaseEnded;
  scroll_event.dispatch_type =
      blink::WebInputEvent::DispatchType::kEventNonBlocking;
  rwhv_nested->ProcessMouseWheelEvent(scroll_event, ui::LatencyInfo());

  interceptor->ResetRectRunLoop();
  // Once we've sent a wheel to the nested iframe that we expect to turn into
  // a bubbling scroll, we need to delay to make sure the GestureScrollBegin
  // from this new scroll doesn't hit the RenderWidgetHostImpl before the
  // GestureScrollEnd bubbled from the child.
  // This timing only seems to be needed for CrOS, but we'll enable it on
  // all platforms just to lessen the possibility of tests being flakey
  // on non-CrOS platforms.
  ack_observer.Wait();

  // Scroll the parent down again in order to test scroll bubbling from
  // gestures.
  scroll_event.delta_y = -5.0f;
  scroll_event.dispatch_type = blink::WebInputEvent::DispatchType::kBlocking;
  scroll_event.phase = blink::WebMouseWheelEvent::kPhaseBegan;
  rwhv_parent->ProcessMouseWheelEvent(scroll_event, ui::LatencyInfo());

  // The event router sends wheel events of a single scroll sequence to the
  // target under the first wheel event. Send a wheel end event to the current
  // target view before sending a wheel event to a different one.
  scroll_event.delta_y = 0.0f;
  scroll_event.phase = blink::WebMouseWheelEvent::kPhaseEnded;
  scroll_event.dispatch_type =
      blink::WebInputEvent::DispatchType::kEventNonBlocking;
  rwhv_parent->ProcessMouseWheelEvent(scroll_event, ui::LatencyInfo());

  // Ensure ensuing offset change is received, and then reset the interceptor.
  interceptor->WaitForRect();
  interceptor->ResetRectRunLoop();

  // Scroll down the nested iframe via gesture. This requires 3 separate input
  // events.
  blink::WebGestureEvent gesture_event(
      blink::WebGestureEvent::Type::kGestureScrollBegin,
      blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests(),
      blink::WebGestureDevice::kTouchpad);
  gesture_event.SetPositionInWidget(gfx::PointF(1, 1));
  gesture_event.data.scroll_begin.delta_x_hint = 0.0f;
  gesture_event.data.scroll_begin.delta_y_hint = 6.0f;
  rwhv_nested->GetRenderWidgetHost()->ForwardGestureEvent(gesture_event);

  gesture_event =
      blink::WebGestureEvent(blink::WebGestureEvent::Type::kGestureScrollUpdate,
                             blink::WebInputEvent::kNoModifiers,
                             blink::WebInputEvent::GetStaticTimeStampForTests(),
                             blink::WebGestureDevice::kTouchpad);
  gesture_event.SetPositionInWidget(gfx::PointF(1, 1));
  gesture_event.data.scroll_update.delta_x = 0.0f;
  gesture_event.data.scroll_update.delta_y = 6.0f;
  rwhv_nested->GetRenderWidgetHost()->ForwardGestureEvent(gesture_event);

  gesture_event =
      blink::WebGestureEvent(blink::WebGestureEvent::Type::kGestureScrollEnd,
                             blink::WebInputEvent::kNoModifiers,
                             blink::WebInputEvent::GetStaticTimeStampForTests(),
                             blink::WebGestureDevice::kTouchpad);
  gesture_event.SetPositionInWidget(gfx::PointF(1, 1));
  rwhv_nested->GetRenderWidgetHost()->ForwardGestureEvent(gesture_event);

  interceptor->WaitForRect();
  update_rect = interceptor->last_rect();
  // As above, if this loop does not terminate then it indicates an issue
  // with scroll bubbling.
  while (update_rect.y() > initial_y) {
    base::RunLoop run_loop;
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE, run_loop.QuitClosure(), TestTimeouts::tiny_timeout());
    run_loop.Run();
    update_rect = interceptor->last_rect();
  }

  // Test that when the child frame absorbs all of the scroll delta, it does
  // not propagate to the parent (see https://crbug.com/621624).
  interceptor->ResetRectRunLoop();
  scroll_event.delta_y = -5.0f;
  scroll_event.dispatch_type = blink::WebInputEvent::DispatchType::kBlocking;
  scroll_event.phase = blink::WebMouseWheelEvent::kPhaseBegan;
  rwhv_nested->ProcessMouseWheelEvent(scroll_event, ui::LatencyInfo());
  // It isn't possible to busy loop waiting on the renderer here because we
  // are explicitly testing that something does *not* happen. This creates a
  // small chance of false positives but shouldn't result in false negatives,
  // so flakiness implies this test is failing.
  {
    base::RunLoop run_loop;
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE, run_loop.QuitClosure(), TestTimeouts::action_timeout());
    run_loop.Run();
  }
  DCHECK_EQ(interceptor->last_rect().x(), 0);
  DCHECK_EQ(interceptor->last_rect().y(), 0);
}

// Tests that scrolling with the keyboard will bubble unused scroll to the
// OOPIF's parent.
// Disabled on Android due to flakes; see b/338341090.
#if BUILDFLAG(IS_ANDROID)
#define MAYBE_KeyboardScrollBubblingFromOOPIF \
  DISABLED_KeyboardScrollBubblingFromOOPIF
#else
#define MAYBE_KeyboardScrollBubblingFromOOPIF KeyboardScrollBubblingFromOOPIF
#endif
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest,
                       MAYBE_KeyboardScrollBubblingFromOOPIF) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/frame_tree/page_with_iframe_in_scrollable_div.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();
  ASSERT_EQ(1U, root->child_count());

  FrameTreeNode* iframe_node = root->child_at(0);

  EXPECT_EQ(
      " Site A ------------ proxies for B\n"
      "   +--Site B ------- proxies for A\n"
      "Where A = http://a.com/\n"
      "      B = http://b.com/",
      DepictFrameTree(root));

  RenderWidgetHostViewBase* rwhv_child = static_cast<RenderWidgetHostViewBase*>(
      iframe_node->current_frame_host()->GetRenderWidgetHost()->GetView());

  // This test does not involve hit testing, but input events could be dropped
  // by the renderer before the first compositor commit, so we wait here anyway
  // to avoid that.
  WaitForHitTestData(iframe_node->current_frame_host());

  EXPECT_DOUBLE_EQ(
      0.0,
      EvalJs(root,
             "var wrapperDiv = document.getElementById('wrapper-div');"
             "var initial_y = wrapperDiv.scrollTop;"
             "var waitForScrollDownPromise = new Promise(function(resolve) {"
             "  wrapperDiv.addEventListener('scroll', () => {"
             "    if (wrapperDiv.scrollTop > initial_y)"
             "      resolve(wrapperDiv.scrollTop);"
             "  });"
             "});"
             "initial_y;")
          .ExtractDouble());

  input::NativeWebKeyboardEvent key_event(
      blink::WebKeyboardEvent::Type::kRawKeyDown,
      blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  key_event.windows_key_code = ui::VKEY_DOWN;
  key_event.native_key_code =
      ui::KeycodeConverter::DomCodeToNativeKeycode(ui::DomCode::ARROW_DOWN);
  key_event.dom_code = static_cast<int>(ui::DomCode::ARROW_DOWN);
  key_event.dom_key = ui::DomKey::ARROW_DOWN;

  rwhv_child->GetRenderWidgetHost()->ForwardKeyboardEvent(key_event);

  key_event.SetType(blink::WebKeyboardEvent::Type::kKeyUp);
  rwhv_child->GetRenderWidgetHost()->ForwardKeyboardEvent(key_event);

  double scrolled_y = EvalJs(root, "waitForScrollDownPromise").ExtractDouble();
  EXPECT_GT(scrolled_y, 0.0);
}

// Ensure that the scrollability of a local subframe in an OOPIF is considered
// when acknowledging GestureScrollBegin events sent to OOPIFs.
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest, ScrollLocalSubframeInOOPIF) {
  ui::GestureConfiguration::GetInstance()->set_scroll_debounce_interval_in_ms(
      0);

  // This must be tall enough such that the outer iframe is not scrollable.
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/frame_tree/page_with_tall_positioned_frame.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();
  ASSERT_EQ(1U, root->child_count());

  FrameTreeNode* parent_iframe_node = root->child_at(0);
  GURL outer_frame_url(embedded_test_server()->GetURL(
      "baz.com", "/frame_tree/page_with_positioned_frame.html"));
  EXPECT_TRUE(NavigateToURLFromRenderer(parent_iframe_node, outer_frame_url));

  // This must be tall enough such that the inner iframe is scrollable.
  FrameTreeNode* nested_iframe_node = parent_iframe_node->child_at(0);
  GURL inner_frame_url(
      embedded_test_server()->GetURL("baz.com", "/tall_page.html"));
  EXPECT_TRUE(NavigateToURLFromRenderer(nested_iframe_node, inner_frame_url));

  ASSERT_EQ(
      " Site A ------------ proxies for B\n"
      "   +--Site B ------- proxies for A\n"
      "        +--Site B -- proxies for A\n"
      "Where A = http://a.com/\n"
      "      B = http://baz.com/",
      DepictFrameTree(root));

  RenderWidgetHostViewBase* rwhv_child = static_cast<RenderWidgetHostViewBase*>(
      nested_iframe_node->current_frame_host()
          ->GetRenderWidgetHost()
          ->GetView());

  WaitForHitTestData(parent_iframe_node->current_frame_host());

  // When we scroll the inner frame, we should have the GSB be consumed.
  // The outer iframe not being scrollable should not cause the GSB to go
  // unconsumed.
  InputEventAckWaiter ack_observer(
      parent_iframe_node->current_frame_host()->GetRenderWidgetHost(),
      base::BindRepeating([](blink::mojom::InputEventResultSource,
                             blink::mojom::InputEventResultState state,
                             const blink::WebInputEvent& event) {
        return event.GetType() ==
                   blink::WebGestureEvent::Type::kGestureScrollBegin &&
               state == blink::mojom::InputEventResultState::kConsumed;
      }));

  // Wait until renderer's compositor thread is synced. Otherwise the non fast
  // scrollable regions won't be set when the event arrives.
  MainThreadFrameObserver observer(rwhv_child->GetRenderWidgetHost());
  observer.Wait();

  // Now scroll the inner frame downward.
  blink::WebMouseWheelEvent scroll_event(
      blink::WebInputEvent::Type::kMouseWheel,
      blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  scroll_event.SetPositionInWidget(90, 110);
  scroll_event.delta_units = ui::ScrollGranularity::kScrollByPrecisePixel;
  scroll_event.delta_x = 0.0f;
  scroll_event.delta_y = -50.0f;
  scroll_event.phase = blink::WebMouseWheelEvent::kPhaseBegan;
  rwhv_child->ProcessMouseWheelEvent(scroll_event, ui::LatencyInfo());
  ack_observer.Wait();
}

INSTANTIATE_TEST_SUITE_P(All,
                         ScrollingIntegrationTest,
                         testing::ValuesIn(RenderDocumentFeatureLevelValues()));
INSTANTIATE_TEST_SUITE_P(All,
                         SitePerProcessScrollAnchorTest,
                         testing::ValuesIn(RenderDocumentFeatureLevelValues()));
INSTANTIATE_TEST_SUITE_P(All,
                         SitePerProcessProgrammaticScrollTest,
                         testing::ValuesIn(RenderDocumentFeatureLevelValues()));

}  // namespace content
