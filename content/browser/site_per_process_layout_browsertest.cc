// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <optional>

#include "base/json/json_reader.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/gmock_expected_support.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "cc/base/math_util.h"
#include "content/browser/renderer_host/cross_process_frame_connector.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_view_child_frame.h"
#include "content/browser/site_per_process_browsertest.h"
#include "content/common/input/actions_parser.h"
#include "content/common/input/synthetic_pointer_action.h"
#include "content/common/input/synthetic_touchscreen_pinch_gesture.h"
#include "content/public/browser/render_process_host_priority_client.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/hit_test_region_observer.h"
#include "content/public/test/synchronize_visual_properties_interceptor.h"
#include "content/public/test/test_frame_navigation_observer.h"
#include "content/test/render_document_feature.h"
#include "content/test/render_widget_host_visibility_observer.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "third_party/blink/public/mojom/frame/frame.mojom-test-utils.h"

#if defined(USE_AURA)
#include "ui/aura/window_tree_host.h"
#endif

#if BUILDFLAG(IS_MAC)
#include "content/common/input/synthetic_touchpad_pinch_gesture.h"
#include "ui/base/test/scoped_preferred_scroller_style_mac.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ui/aura/test/test_screen.h"
#endif

namespace content {

namespace {

double GetFrameDeviceScaleFactor(const ToRenderFrameHost& adapter) {
  return EvalJs(adapter, "window.devicePixelRatio;").ExtractDouble();
}

// Layout child frames in cross_site_iframe_factory.html so that they are the
// same width as the viewport, and 75% of the height of the window. This is for
// testing viewport intersection. Note this does not recurse into child frames
// and re-layout in the same way since children might be in a different origin.
void LayoutNonRecursiveForTestingViewportIntersection(
    const ToRenderFrameHost& execution_target) {
  static const char kRafScript[] = R"(
      let width = window.innerWidth;
      let height = window.innerHeight * 0.75;
      for (let i = 0; i < window.frames.length; i++) {
        let child = document.getElementById("child-" + i);
        child.width = width;
        child.height = height;
      }
  )";
  ASSERT_TRUE(EvalJsAfterLifecycleUpdate(execution_target, kRafScript, "")
                  .error.empty());
}

// Check |intersects_viewport| on widget and process.
bool CheckIntersectsViewport(bool expected, FrameTreeNode* node) {
  RenderProcessHostPriorityClient::Priority priority =
      node->current_frame_host()->GetRenderWidgetHost()->GetPriority();
  return priority.intersects_viewport == expected &&
         node->current_frame_host()->GetProcess()->GetIntersectsViewport() ==
             expected;
}

// Helper function to generate a click on the given RenderWidgetHost.  The
// mouse event is forwarded directly to the RenderWidgetHost without any
// hit-testing.
void SimulateMouseClick(RenderWidgetHost* rwh, int x, int y) {
  blink::WebMouseEvent mouse_event(
      blink::WebInputEvent::Type::kMouseDown,
      blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  mouse_event.button = blink::WebPointerProperties::Button::kLeft;
  mouse_event.SetPositionInWidget(x, y);
  rwh->ForwardMouseEvent(mouse_event);
}

}  // namespace

// Class to monitor incoming UpdateViewportIntersection messages. The caller has
// to guarantee that `rfph` lives at least as long as
// UpdateViewportIntersectionMessageFilter.
class UpdateViewportIntersectionMessageFilter
    : public blink::mojom::RemoteFrameHostInterceptorForTesting {
 public:
  explicit UpdateViewportIntersectionMessageFilter(
      content::RenderFrameProxyHost* rfph)
      : intersection_state_(blink::mojom::ViewportIntersectionState::New()),
        swapped_impl_(rfph->frame_host_receiver_for_testing(), this) {}

  ~UpdateViewportIntersectionMessageFilter() override = default;

  const blink::mojom::ViewportIntersectionStatePtr& GetIntersectionState()
      const {
    return intersection_state_;
  }

  blink::mojom::RemoteFrameHost* GetForwardingInterface() override {
    return swapped_impl_.old_impl();
  }

  void UpdateViewportIntersection(
      blink::mojom::ViewportIntersectionStatePtr intersection_state,
      const std::optional<blink::FrameVisualProperties>& visual_properties)
      override {
    intersection_state_ = std::move(intersection_state);
    msg_received_ = true;
    if (run_loop_)
      run_loop_->Quit();
  }

  bool MessageReceived() const { return msg_received_; }

  void Clear() {
    msg_received_ = false;
    intersection_state_ = blink::mojom::ViewportIntersectionState::New();
  }

  void Wait() {
    DCHECK(!run_loop_);
    if (msg_received_) {
      msg_received_ = false;
      return;
    }
    std::unique_ptr<base::RunLoop> run_loop(new base::RunLoop);
    run_loop_ = run_loop.get();
    run_loop_->Run();
    run_loop_ = nullptr;
    msg_received_ = false;
  }

  void set_run_loop(base::RunLoop* run_loop) { run_loop_ = run_loop; }

 private:
  raw_ptr<base::RunLoop> run_loop_ = nullptr;
  bool msg_received_;
  blink::mojom::ViewportIntersectionStatePtr intersection_state_;
  mojo::test::ScopedSwapImplForTesting<blink::mojom::RemoteFrameHost>
      swapped_impl_;
};

// TODO(tonikitoo): Move to fake_remote_frame.h|cc in case it is useful
// for other tests.
class FakeRemoteMainFrame : public blink::mojom::RemoteMainFrame {
 public:
  FakeRemoteMainFrame() = default;
  ~FakeRemoteMainFrame() override = default;

  void Init(
      mojo::PendingAssociatedReceiver<blink::mojom::RemoteMainFrame> receiver) {
    receiver_.Bind(std::move(receiver));
  }

  // blink::mojom::RemoteMainFrame overrides:
  void UpdateTextAutosizerPageInfo(
      blink::mojom::TextAutosizerPageInfoPtr page_info) override {}

 private:
  mojo::AssociatedReceiver<blink::mojom::RemoteMainFrame> receiver_{this};
};

// This class intercepts RenderFrameProxyHost creations, and overrides their
// respective blink::mojom::RemoteMainFrame instances, so that it can watch for
// text autosizer page info updates.
class UpdateTextAutosizerInfoProxyObserver
    : public RenderFrameProxyHost::TestObserver {
 public:
  UpdateTextAutosizerInfoProxyObserver() {
    RenderFrameProxyHost::SetObserverForTesting(this);
  }
  ~UpdateTextAutosizerInfoProxyObserver() override {
    RenderFrameProxyHost::SetObserverForTesting(nullptr);
  }

  const blink::mojom::TextAutosizerPageInfo& TextAutosizerPageInfo(
      RenderFrameProxyHost* proxy) {
    return remote_frames_[proxy]->page_info();
  }

 private:
  class Remote : public FakeRemoteMainFrame {
   public:
    explicit Remote(RenderFrameProxyHost* proxy) {
      Init(proxy->BindRemoteMainFrameReceiverForTesting());
    }
    void UpdateTextAutosizerPageInfo(
        blink::mojom::TextAutosizerPageInfoPtr page_info) override {
      page_info_ = *page_info;
    }
    const blink::mojom::TextAutosizerPageInfo& page_info() {
      return page_info_;
    }

   private:
    blink::mojom::TextAutosizerPageInfo page_info_;
  };

  void OnRemoteMainFrameBound(RenderFrameProxyHost* proxy_host) override {
    remote_frames_[proxy_host] = std::make_unique<Remote>(proxy_host);
  }

  std::map<RenderFrameProxyHost*, std::unique_ptr<Remote>> remote_frames_;
};

// Class to intercept incoming TextAutosizerPageInfoChanged messages. The caller
// has to guarantee that `render_frame_host` lives at least as long as
// TextAutosizerPageInfoInterceptor.
class TextAutosizerPageInfoInterceptor
    : public blink::mojom::LocalMainFrameHostInterceptorForTesting {
 public:
  explicit TextAutosizerPageInfoInterceptor(
      RenderFrameHostImpl* render_frame_host)
      : swapped_impl_(
            render_frame_host->local_main_frame_host_receiver_for_testing(),
            this) {}

  ~TextAutosizerPageInfoInterceptor() override = default;

  LocalMainFrameHost* GetForwardingInterface() override {
    return swapped_impl_.old_impl();
  }

  void WaitForPageInfo(std::optional<int> target_main_frame_width,
                       std::optional<float> target_device_scale_adjustment) {
    if (remote_page_info_seen_)
      return;
    target_main_frame_width_ = target_main_frame_width;
    target_device_scale_adjustment_ = target_device_scale_adjustment;
    run_loop_ = std::make_unique<base::RunLoop>();
    run_loop_->Run();
    run_loop_.reset();
  }

  const blink::mojom::TextAutosizerPageInfo& GetTextAutosizerPageInfo() {
    return *remote_page_info_;
  }

  void TextAutosizerPageInfoChanged(
      blink::mojom::TextAutosizerPageInfoPtr remote_page_info) override {
    if ((!target_main_frame_width_ ||
         remote_page_info->main_frame_width != target_main_frame_width_) &&
        (!target_device_scale_adjustment_ ||
         remote_page_info->device_scale_adjustment !=
             target_device_scale_adjustment_)) {
      return;
    }
    remote_page_info_ = remote_page_info.Clone();
    remote_page_info_seen_ = true;
    if (run_loop_)
      run_loop_->Quit();
    GetForwardingInterface()->TextAutosizerPageInfoChanged(
        std::move(remote_page_info));
  }

 private:
  bool remote_page_info_seen_ = false;
  blink::mojom::TextAutosizerPageInfoPtr remote_page_info_ =
      blink::mojom::TextAutosizerPageInfo::New(/*main_frame_width=*/0,
                                               /*main_frame_layout_width=*/0,
                                               /*device_scale_adjustment=*/1.f);
  std::unique_ptr<base::RunLoop> run_loop_;
  std::optional<int> target_main_frame_width_;
  std::optional<float> target_device_scale_adjustment_;
  mojo::test::ScopedSwapImplForTesting<blink::mojom::LocalMainFrameHost>
      swapped_impl_;
};

class SitePerProcessHighDPIBrowserTest : public SitePerProcessBrowserTest {
 public:
  const double kDeviceScaleFactor = 2.0;

  SitePerProcessHighDPIBrowserTest() = default;

 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    SitePerProcessBrowserTestBase::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII(
        switches::kForceDeviceScaleFactor,
        base::StringPrintf("%f", kDeviceScaleFactor));
  }
};

IN_PROC_BROWSER_TEST_P(SitePerProcessHighDPIBrowserTest,
                       SubframeLoadsWithCorrectDeviceScaleFactor) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b)"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // On Android forcing device scale factor does not work for tests, therefore
  // we ensure that make frame and iframe have the same DIP scale there, but
  // not necessarily kDeviceScaleFactor.
  const double expected_dip_scale =
#if BUILDFLAG(IS_ANDROID)
      GetFrameDeviceScaleFactor(web_contents());
#else
      SitePerProcessHighDPIBrowserTest::kDeviceScaleFactor;
#endif

  EXPECT_EQ(expected_dip_scale, GetFrameDeviceScaleFactor(web_contents()));

  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();
  EXPECT_EQ(expected_dip_scale, GetFrameDeviceScaleFactor(root));
  ASSERT_EQ(1U, root->child_count());

  FrameTreeNode* child = root->child_at(0);
  EXPECT_EQ(expected_dip_scale, GetFrameDeviceScaleFactor(child));
}

class SitePerProcessCompositorViewportBrowserTest
    : public SitePerProcessBrowserTestBase,
      public testing::WithParamInterface<double> {
 public:
  SitePerProcessCompositorViewportBrowserTest() = default;

 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    SitePerProcessBrowserTestBase::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII(switches::kForceDeviceScaleFactor,
                                    base::StringPrintf("%f", GetParam()));
  }
};

// DISABLED: crbug.com/1071995
IN_PROC_BROWSER_TEST_P(SitePerProcessCompositorViewportBrowserTest,
                       DISABLED_OopifCompositorViewportSizeRelativeToParent) {
  // Load page with very tall OOPIF.
  GURL main_url(
      embedded_test_server()->GetURL("a.com", "/super_tall_parent.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();
  ASSERT_EQ(1U, root->child_count());

  FrameTreeNode* child = root->child_at(0);

  GURL nested_site_url(
      embedded_test_server()->GetURL("b.com", "/super_tall_page.html"));
  EXPECT_TRUE(NavigateToURLFromRenderer(child, nested_site_url));

  EXPECT_EQ(
      " Site A ------------ proxies for B\n"
      "   +--Site B ------- proxies for A\n"
      "Where A = http://a.com/\n"
      "      B = http://b.com/",
      DepictFrameTree(root));

  // Observe frame submission from parent.
  RenderFrameSubmissionObserver parent_observer(
      root->current_frame_host()
          ->GetRenderWidgetHost()
          ->render_frame_metadata_provider());
  parent_observer.WaitForAnyFrameSubmission();
  gfx::Size parent_viewport_size =
      parent_observer.LastRenderFrameMetadata().viewport_size_in_pixels;

  // Observe frame submission from child.
  RenderFrameSubmissionObserver child_observer(
      child->current_frame_host()
          ->GetRenderWidgetHost()
          ->render_frame_metadata_provider());
  child_observer.WaitForAnyFrameSubmission();
  gfx::Size child_viewport_size =
      child_observer.LastRenderFrameMetadata().viewport_size_in_pixels;

  // Verify child's compositor viewport is no more than about 30% larger than
  // the parent's. See RemoteFrameView::GetCompositingRect() for explanation of
  // the choice of 30%. Add +1 to child viewport height to account for rounding.
  EXPECT_GE(ceilf(1.3f * parent_viewport_size.height()),
            child_viewport_size.height() - 1);

  // Verify the child's ViewBounds are much larger.
  RenderWidgetHostViewBase* child_rwhv = static_cast<RenderWidgetHostViewBase*>(
      child->current_frame_host()->GetRenderWidgetHost()->GetView());
  // 30,000 is based on div/iframe sizes in the test HTML files.
  EXPECT_LT(30000, child_rwhv->GetViewBounds().height());
}

#if BUILDFLAG(IS_ANDROID)
// Android doesn't support forcing device scale factor in tests.
INSTANTIATE_TEST_SUITE_P(SitePerProcess,
                         SitePerProcessCompositorViewportBrowserTest,
                         testing::Values(1.0));
#else
INSTANTIATE_TEST_SUITE_P(SitePerProcess,
                         SitePerProcessCompositorViewportBrowserTest,
                         testing::Values(1.0, 1.5, 2.0));
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest,
                       SubframeUpdateToCorrectDeviceScaleFactor) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b)"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  EXPECT_EQ(1.0, GetFrameDeviceScaleFactor(web_contents()));

  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();
  ASSERT_EQ(1U, root->child_count());

  FrameTreeNode* child = root->child_at(0);
  EXPECT_EQ(1.0, GetFrameDeviceScaleFactor(child));

  double expected_dip_scale = 2.0;

  // TODO(oshima): allow DeviceScaleFactor change on other platforms
  // (win, linux, mac, android and mus).
  aura::TestScreen* test_screen =
      static_cast<aura::TestScreen*>(display::Screen::GetScreen());
  test_screen->CreateHostForPrimaryDisplay();
  test_screen->SetDeviceScaleFactor(expected_dip_scale);

  // This forces |expected_dip_scale| to be applied to the aura::WindowTreeHost
  // and aura::Window.
  aura::WindowTreeHost* window_tree_host = shell()->window()->GetHost();
  window_tree_host->SetBoundsInPixels(window_tree_host->GetBoundsInPixels());

  // Wait until dppx becomes 2 if the frame's dpr hasn't beeen updated
  // to 2 yet.
  const char kScript[] = R"(
      new Promise(resolve => {
        if (window.devicePixelRatio == 2)
          resolve(window.devicePixelRatio);
        window.matchMedia('screen and (min-resolution: 2dppx)')
            .addListener(function(e) {
          if (e.matches) {
            resolve(window.devicePixelRatio);
          }
        });
      });
      )";
  // Make sure that both main frame and iframe are updated to 2x.
  EXPECT_EQ(expected_dip_scale, EvalJs(child, kScript).ExtractDouble());

  EXPECT_EQ(expected_dip_scale,
            EvalJs(web_contents(), kScript).ExtractDouble());
}

#endif

// Tests that when a large OOPIF has been scaled, the compositor raster area
// sent from the embedder is correct.
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_MAC)
// Temporarily disabled on Android because this doesn't account for browser
// control height or page scale factor.
// Flaky on Mac. https://crbug.com/840314
#define MAYBE_ScaledIframeRasterSize DISABLED_ScaledframeRasterSize
#else
#define MAYBE_ScaledIframeRasterSize ScaledIframeRasterSize
#endif
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest,
                       MAYBE_ScaledIframeRasterSize) {
  GURL http_url(embedded_test_server()->GetURL(
      "a.com", "/frame_tree/page_with_scaled_large_frame.html"));
  EXPECT_TRUE(NavigateToURL(shell(), http_url));

  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();

  FrameTreeNode* child = root->child_at(0);
  RenderFrameProxyHost* child_proxy =
      child->render_manager()->GetProxyToParent();
  auto filter =
      std::make_unique<UpdateViewportIntersectionMessageFilter>(child_proxy);

  // Force a lifecycle update and wait for it to finish; by the time this call
  // returns, the viewport intersection IPC should already have been received
  // by the browser process and handled by the filter.
  EvalJsResult eval_result = EvalJsAfterLifecycleUpdate(
      root->current_frame_host(),
      "document.getElementsByTagName('div')[0].scrollTo(0, 5000);",
      "document.getElementsByTagName('div')[0].getBoundingClientRect().top;");
  ASSERT_TRUE(eval_result.error.empty());
  int div_offset_top = eval_result.ExtractInt();
  gfx::Rect compositing_rect =
      filter->GetIntersectionState()->compositor_visible_rect;

  float device_scale_factor = 1.0f;
  device_scale_factor = GetFrameDeviceScaleFactor(shell()->web_contents());

  // The math below replicates the calculations in
  // RemoteFrameView::GetCompositingRect(). That could be subject to tweaking,
  // which would have to be reflected in these test expectations. Also, any
  // changes to Blink that would affect the size of the frame rect or the
  // visible viewport would need to be accounted for.
  // The multiplication by 5 accounts for the 0.2 scale factor in the test,
  // which increases the area that has to be drawn in the OOPIF.
  int view_height = root->current_frame_host()
                        ->GetRenderWidgetHost()
                        ->GetView()
                        ->GetViewBounds()
                        .height() *
                    5 * device_scale_factor;

  // The raster size is expanded by a factor of 1.3 to allow for some scrolling
  // without requiring re-raster. The expanded area to be rasterized should be
  // centered around the iframe's visible area within the parent document, hence
  // the expansion in each direction (top, bottom, left, right) is
  // (0.15 * viewport dimension).
  int expansion = ceilf(view_height * 0.15f);
  int expected_height = view_height + expansion * 2;

  // 5000 = div scroll offset in scaled pixels
  // 5 = scale factor from top-level document to iframe contents
  // 2 = iframe border in scaled pixels
  int expected_offset =
      ((5000 - (div_offset_top * 5) - 2) * device_scale_factor) - expansion;

  // Allow a small amount for rounding differences from applying page and
  // device scale factors at different times.
  float tolerance = ceilf(device_scale_factor);
  EXPECT_NEAR(compositing_rect.height(), expected_height, tolerance);
  EXPECT_NEAR(compositing_rect.y(), expected_offset, tolerance);
}

// Similar to ScaledIFrameRasterSize but with nested OOPIFs to ensure
// propagation works correctly.
#if BUILDFLAG(IS_ANDROID)
// Temporarily disabled on Android because this doesn't account for browser
// control height or page scale factor.
#define MAYBE_ScaledNestedIframeRasterSize DISABLED_ScaledNestedIframeRasterSize
#else
#define MAYBE_ScaledNestedIframeRasterSize ScaledNestedIframeRasterSize
#endif
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest,
                       MAYBE_ScaledNestedIframeRasterSize) {
  GURL http_url(embedded_test_server()->GetURL(
      "a.com", "/frame_tree/page_with_scaled_large_frames_nested.html"));
  EXPECT_TRUE(NavigateToURL(shell(), http_url));

  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();
  FrameTreeNode* child_b = root->child_at(0);

  EXPECT_TRUE(NavigateToURLFromRenderer(
      child_b,
      embedded_test_server()->GetURL(
          "bar.com", "/frame_tree/page_with_large_scrollable_frame.html")));

  EXPECT_EQ(
      " Site A ------------ proxies for B C\n"
      "   +--Site B ------- proxies for A C\n"
      "        +--Site C -- proxies for A B\n"
      "Where A = http://a.com/\n"
      "      B = http://bar.com/\n"
      "      C = http://baz.com/",
      DepictFrameTree(root));

  // This adds the filter to the immediate child iframe. It verifies that the
  // child sets the nested iframe's compositing rect correctly.
  FrameTreeNode* child_c = child_b->child_at(0);
  RenderFrameProxyHost* child_c_proxy =
      child_c->render_manager()->GetProxyToParent();
  auto filter =
      std::make_unique<UpdateViewportIntersectionMessageFilter>(child_c_proxy);

  // Scroll the child frame so that it is partially clipped. This will cause the
  // top 10 pixels of the child frame to be clipped. Applying the scale factor
  // means that in the coordinate system of the subframes, 50px are clipped.
  ASSERT_TRUE(EvalJsAfterLifecycleUpdate(root->current_frame_host(),
                                         "window.scrollBy(0, 10)", "")
                  .error.empty());

  // This scrolls the div containing in the 'Site B' iframe that contains the
  // 'Site C' iframe, and then we verify that the 'Site C' frame receives the
  // correct compositor frame. Force a lifecycle update after the scroll and
  // wait for it to finish; by the time this call returns, the viewport
  // intersection IPC should already have been received by the browser process
  // and handled by the filter. Extract the page offset of the leaf iframe
  // within the middle document.
  EvalJsResult child_eval_result = EvalJsAfterLifecycleUpdate(
      child_b->current_frame_host(),
      "document.getElementsByTagName('div')[0].scrollTo(0, 5000);",
      "document.getElementsByTagName('div')[0].getBoundingClientRect().top;");
  ASSERT_TRUE(child_eval_result.error.empty());
  int child_div_offset_top = child_eval_result.ExtractInt();

  gfx::Rect compositing_rect =
      filter->GetIntersectionState()->compositor_visible_rect;

  float scale_factor = 1.0f;
  scale_factor = GetFrameDeviceScaleFactor(shell()->web_contents());

  // See comment in ScaledIframeRasterSize for explanation of this. In this
  // case, the raster area of the large iframe should be restricted to
  // approximately the area of its containing frame which is unclipped by the
  // main frame. The containing frame is clipped by 50 pixels at the top, due
  // to the scroll offset of the main frame, so we subtract that from the full
  // height of the containing frame.
  int view_height = (child_b->current_frame_host()
                         ->GetRenderWidgetHost()
                         ->GetView()
                         ->GetViewBounds()
                         .height() -
                     50) *
                    scale_factor;
  // 30% padding is added to the view_height to prevent frequent re-rasters.
  // The extra padding is centered around the view height, hence expansion by
  // 0.15 in each direction.
  int expansion = ceilf(view_height * 0.15f);
  int expected_height = view_height + expansion * 2;

  // Explanation of terms:
  //   5000 = offset from top of nested iframe to top of containing div, due to
  //          scroll offset of div. This needs to be scaled by DSF or the test
  //          will fail on HighDPI devices.
  //   child_div_offset_top = offset of containing div from top of child frame
  //   50 = offset of child frame's intersection with the top document viewport
  //       from the top of the child frame (i.e, clipped amount at top of child)
  //   view_height * 0.15 = padding added to the top of the compositing rect
  //                        (half the the 30% total padding)
  int expected_offset = (5000 * scale_factor) -
                        ((child_div_offset_top - 50) * scale_factor) -
                        expansion;

  // Allow a small amount for rounding differences from applying page and
  // device scale factors at different times.
  EXPECT_NEAR(compositing_rect.height(), expected_height, ceilf(scale_factor));
  EXPECT_NEAR(compositing_rect.y(), expected_offset, ceilf(scale_factor));
}

// Tests that when an OOPIF is inside a multicolumn container, its compositing
// rect is set correctly.
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest,
                       IframeInMulticolCompositingRect) {
  GURL http_url(embedded_test_server()->GetURL(
      "a.com", "/frame_tree/page_with_iframe_in_multicol.html"));
  EXPECT_TRUE(NavigateToURL(shell(), http_url));

  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();

  FrameTreeNode* child = root->child_at(0);
  RenderFrameProxyHost* child_proxy =
      child->render_manager()->GetProxyToParent();
  auto filter =
      std::make_unique<UpdateViewportIntersectionMessageFilter>(child_proxy);

  // Force a lifecycle update and wait for it to finish. Changing the width of
  // the iframe should cause the parent renderer to propagate a new
  // ViewportIntersectionState while running the rendering pipeline. By the time
  // this call returns, the viewport intersection IPC should already have been
  // received by the browser process and handled by the filter.
  EvalJsResult eval_result = EvalJsAfterLifecycleUpdate(
      root->current_frame_host(),
      "document.querySelector('iframe').style.width = '250px'", "");
  ASSERT_TRUE(filter->MessageReceived());
  gfx::Rect compositing_rect =
      filter->GetIntersectionState()->compositor_visible_rect;

  float scale_factor = 1.0f;
  scale_factor = GetFrameDeviceScaleFactor(shell()->web_contents());

  gfx::Point visible_offset(0, 0);
  gfx::Size visible_size =
      gfx::ScaleToFlooredSize(gfx::Size(250, 150), scale_factor, scale_factor);
  gfx::Rect visible_rect(visible_offset, visible_size);
  float tolerance = ceilf(scale_factor);
  EXPECT_NEAR(compositing_rect.x(), visible_rect.x(), tolerance);
  EXPECT_NEAR(compositing_rect.y(), visible_rect.y(), tolerance);
  EXPECT_NEAR(compositing_rect.width(), visible_rect.width(), tolerance);
  EXPECT_NEAR(compositing_rect.height(), visible_rect.height(), tolerance);
  EXPECT_TRUE(compositing_rect.Contains(visible_rect));
}

// Flaky on multiple platforms (crbug.com/1094562).
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest,
                       DISABLED_FrameViewportIntersectionTestSimple) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b(c),d,e(f))"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();
  RenderFrameProxyHost* child2_proxy =
      root->child_at(2)->render_manager()->GetProxyToParent();
  auto child2_filter =
      std::make_unique<UpdateViewportIntersectionMessageFilter>(child2_proxy);

  // Force lifecycle update in root and child2 to make sure child2 has sent
  // viewport intersection into to grand child before child2 becomes throttled.
  ASSERT_TRUE(EvalJsAfterLifecycleUpdate(root->current_frame_host(), "", "")
                  .error.empty());
  ASSERT_TRUE(EvalJsAfterLifecycleUpdate(
                  root->child_at(2)->current_frame_host(), "", "")
                  .error.empty());
  child2_filter->Clear();

  LayoutNonRecursiveForTestingViewportIntersection(shell()->web_contents());

  // Root should always intersect.
  EXPECT_TRUE(CheckIntersectsViewport(true, root));
  // Child 0 should be entirely in viewport.
  EXPECT_TRUE(CheckIntersectsViewport(true, root->child_at(0)));
  // Make sure child0 has has a chance to propagate viewport intersection to
  // grand child.
  ASSERT_TRUE(EvalJsAfterLifecycleUpdate(
                  root->child_at(0)->current_frame_host(), "", "")
                  .error.empty());
  // Grand child should match parent.
  EXPECT_TRUE(CheckIntersectsViewport(true, root->child_at(0)->child_at(0)));
  // Child 1 should be partially in viewport.
  EXPECT_TRUE(CheckIntersectsViewport(true, root->child_at(1)));
  // Child 2 should be not be in viewport.
  EXPECT_TRUE(CheckIntersectsViewport(false, root->child_at(2)));
  // Can't use EvalJsAfterLifecycleUpdate on child2, because it's
  // render-throttled. But it should still have propagated state down to the
  // grandchild.
  child2_filter->Wait();
  // Grand child should match parent.
  EXPECT_TRUE(CheckIntersectsViewport(false, root->child_at(2)->child_at(0)));
}

IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest,
                       FrameViewportOffsetTestSimple) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b(c))"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // This will catch b sending viewport intersection information to c.
  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();
  RenderFrameProxyHost* iframe_c_proxy =
      root->child_at(0)->child_at(0)->render_manager()->GetProxyToParent();
  auto filter =
      std::make_unique<UpdateViewportIntersectionMessageFilter>(iframe_c_proxy);

  // Use EvalJsAfterLifecycleUpdate to force animation frames in `a` and `b` to
  // ensure that the viewport intersection for initial layout state has been
  // propagated. The layout of `a` will not change again, so we can read back
  // its layout info after the animation frame. The layout of `b` will change,
  // so we don't read back its layout yet.
  std::string script(R"(
    let iframe = document.querySelector("iframe");
    [iframe.offsetLeft, iframe.offsetTop];
  )");
  EvalJsResult iframe_b_result =
      EvalJsAfterLifecycleUpdate(root->current_frame_host(), "", script);
  base::Value iframe_b_offset = iframe_b_result.ExtractList();
  int iframe_b_offset_left = iframe_b_offset.GetList()[0].GetInt();
  int iframe_b_offset_top = iframe_b_offset.GetList()[1].GetInt();

  // Make sure a new IPC is sent after dirty-ing layout.
  filter->Clear();

  // Dirty layout in `b` to generate a new IPC to `c`. This will be the final
  // layout state for `b`, so read back layout info here.
  std::string raf_script(R"(
    let iframe = document.querySelector("iframe");
    let margin = getComputedStyle(iframe).marginTop.replace("px", "");
    iframe.style.margin = String(parseInt(margin) + 1) + "px";
  )");
  EvalJsResult iframe_c_result = EvalJsAfterLifecycleUpdate(
      root->child_at(0)->current_frame_host(), raf_script, script);
  base::Value iframe_c_offset = iframe_c_result.ExtractList();
  int iframe_c_offset_left = iframe_c_offset.GetList()[0].GetInt();
  int iframe_c_offset_top = iframe_c_offset.GetList()[1].GetInt();

  // The IPC should already have been sent
  EXPECT_TRUE(filter->MessageReceived());

  // +4 for a 2px border on each iframe.
  gfx::Vector2dF expected(iframe_b_offset_left + iframe_c_offset_left + 4,
                          iframe_b_offset_top + iframe_c_offset_top + 4);
  const float device_scale_factor =
      root->render_manager()->GetRenderWidgetHostView()->GetDeviceScaleFactor();
  // Convert from CSS to physical pixels
  expected.Scale(device_scale_factor);
  gfx::Transform actual = filter->GetIntersectionState()->main_frame_transform;
  const std::optional<gfx::PointF> viewport_offset_source_point =
      actual.InverseMapPoint(gfx::PointF());
  ASSERT_TRUE(viewport_offset_source_point.has_value());
  const gfx::Vector2dF viewport_offset =
      gfx::PointF() - viewport_offset_source_point.value();
  float tolerance = ceilf(device_scale_factor);
  EXPECT_NEAR(expected.x(), viewport_offset.x(), tolerance);
  EXPECT_NEAR(expected.y(), viewport_offset.y(), tolerance);
}

// TODO(crbug.com/40743132): Flaky test.
IN_PROC_BROWSER_TEST_P(
    SitePerProcessBrowserTest,
    DISABLED_NestedIframeTransformedIntoViewViewportIntersection) {
  GURL http_url(embedded_test_server()->GetURL(
      "a.com", "/frame_tree/page_with_frame_transformed_into_viewport.html"));
  EXPECT_TRUE(NavigateToURL(shell(), http_url));

  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();
  FrameTreeNode* child_b = root->child_at(0);

  EXPECT_TRUE(NavigateToURLFromRenderer(
      child_b,
      embedded_test_server()->GetURL(
          "bar.com", "/frame_tree/page_with_cross_origin_frame_at_half.html")));

  EXPECT_EQ(
      " Site A ------------ proxies for B C\n"
      "   +--Site B ------- proxies for A C\n"
      "        +--Site C -- proxies for A B\n"
      "Where A = http://a.com/\n"
      "      B = http://bar.com/\n"
      "      C = http://baz.com/",
      DepictFrameTree(root));

  FrameTreeNode* child_c = child_b->child_at(0);
  RenderFrameProxyHost* child_c_proxy =
      child_c->render_manager()->GetProxyToParent();
  auto filter =
      std::make_unique<UpdateViewportIntersectionMessageFilter>(child_c_proxy);

  // Scroll the div containing the 'Site B' iframe to trigger a viewport
  // intersection update.
  ASSERT_TRUE(EvalJsAfterLifecycleUpdate(
                  child_b->current_frame_host(),
                  "document.getElementsByTagName('div')[0].scrollTo(0, 5000);",
                  "")
                  .error.empty());
  ASSERT_TRUE(filter->MessageReceived());

  // Check that we currently intersect with the viewport.
  gfx::Rect viewport_intersection =
      filter->GetIntersectionState()->viewport_intersection;

  EXPECT_GT(viewport_intersection.height(), 0);
  EXPECT_GT(viewport_intersection.width(), 0);
}

// Verify that OOPIF select element popup menu coordinates account for scroll
// offset in containers embedding frame.
// TODO(crbug.com/40583339): Reenable this.
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest,
                       DISABLED_PopupMenuInTallIframeTest) {
  GURL main_url(embedded_test_server()->GetURL(
      "/frame_tree/page_with_tall_positioned_frame.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();
  FrameTreeNode* child_node = root->child_at(0);
  GURL site_url(embedded_test_server()->GetURL(
      "baz.com", "/site_isolation/page-with-select.html"));
  EXPECT_TRUE(NavigateToURLFromRenderer(child_node, site_url));

  RenderFrameProxyHost* root_proxy = root->render_manager()->GetProxyToParent();
  auto filter =
      std::make_unique<UpdateViewportIntersectionMessageFilter>(root_proxy);

  // Position the select element so that it is out of the viewport, then scroll
  // it into view.
  EXPECT_TRUE(ExecJs(child_node,
                     "document.querySelector('select').style.top='2000px';"));
  EXPECT_TRUE(ExecJs(root, "window.scrollTo(0, 1900);"));

  // Wait for a viewport intersection update to be dispatched to the child, and
  // ensure it is processed by the browser before continuing.
  filter->Wait();
  {
    // This yields the UI thread in order to ensure that the new viewport
    // intersection is sent to the to child renderer before the mouse click
    // below.
    base::RunLoop loop;
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, loop.QuitClosure());
    loop.Run();
  }

  auto show_popup_waiter = std::make_unique<ShowPopupWidgetWaiter>(
      web_contents(), child_node->current_frame_host());
  SimulateMouseClick(child_node->current_frame_host()->GetRenderWidgetHost(),
                     55, 2005);

  // Dismiss the popup.
  SimulateMouseClick(child_node->current_frame_host()->GetRenderWidgetHost(), 1,
                     1);

  // The test passes if this wait returns, indicating that the popup was
  // scrolled into view and the OOPIF renderer displayed it. Other tests verify
  // the correctness of popup menu coordinates.
  show_popup_waiter->Wait();
}

// Test to verify that viewport intersection is propagated to nested OOPIFs
// even when a parent OOPIF has been throttled.
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest,
                       NestedFrameViewportIntersectionUpdated) {
  GURL main_url(embedded_test_server()->GetURL(
      "foo.com", "/frame_tree/scrollable_page_with_positioned_frame.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();
  FrameTreeNode* child_node = root->child_at(0);
  GURL site_url(embedded_test_server()->GetURL(
      "bar.com", "/frame_tree/page_with_positioned_frame.html"));
  EXPECT_TRUE(NavigateToURLFromRenderer(child_node, site_url));

  EXPECT_EQ(
      " Site A ------------ proxies for B C\n"
      "   +--Site B ------- proxies for A C\n"
      "        +--Site C -- proxies for A B\n"
      "Where A = http://foo.com/\n"
      "      B = http://bar.com/\n"
      "      C = http://baz.com/",
      DepictFrameTree(root));

  // This will intercept messages sent from B to C, describing C's viewport
  // intersection.
  RenderFrameProxyHost* child_proxy =
      child_node->render_manager()->GetProxyToParent();
  auto filter =
      std::make_unique<UpdateViewportIntersectionMessageFilter>(child_proxy);

  // Run requestAnimationFrame in A and B to make sure initial layout has
  // completed and initial IPCs sent.
  ASSERT_TRUE(EvalJsAfterLifecycleUpdate(root->current_frame_host(), "", "")
                  .error.empty());
  ASSERT_TRUE(
      EvalJsAfterLifecycleUpdate(child_node->current_frame_host(), "", "")
          .error.empty());
  filter->Clear();

  // Scroll the child frame out of view, causing it to become throttled.
  ASSERT_TRUE(ExecJs(root->current_frame_host(), "window.scrollTo(0, 5000)"));
  filter->Wait();
  EXPECT_TRUE(filter->GetIntersectionState()->viewport_intersection.IsEmpty());

  // Scroll the frame back into view.
  ASSERT_TRUE(ExecJs(root->current_frame_host(), "window.scrollTo(0, 0)"));
  filter->Wait();
  EXPECT_FALSE(filter->GetIntersectionState()->viewport_intersection.IsEmpty());
}

// Test to verify that the main frame document intersection
// is propagated to out of process iframes by scrolling a nested iframe
// in and out of intersecting with the main frame document.
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest,
                       NestedFrameMainFrameDocumentIntersectionUpdated) {
  GURL main_url(embedded_test_server()->GetURL(
      "foo.com", "/frame_tree/scrollable_page_with_positioned_frame.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();
  FrameTreeNode* child_node_b = root->child_at(0);
  GURL site_url(embedded_test_server()->GetURL(
      "bar.com", "/frame_tree/scrollable_page_with_positioned_frame.html"));
  EXPECT_TRUE(NavigateToURLFromRenderer(child_node_b, site_url));

  EXPECT_EQ(
      " Site A ------------ proxies for B C\n"
      "   +--Site B ------- proxies for A C\n"
      "        +--Site C -- proxies for A B\n"
      "Where A = http://foo.com/\n"
      "      B = http://bar.com/\n"
      "      C = http://baz.com/",
      DepictFrameTree(root));

  FrameTreeNode* child_node_c = child_node_b->child_at(0);
  RenderFrameProxyHost* child_proxy_c =
      child_node_c->render_manager()->GetProxyToParent();
  auto filter =
      std::make_unique<UpdateViewportIntersectionMessageFilter>(child_proxy_c);

  // Run requestAnimationFrame in A and B to make sure initial layout has
  // completed and initial IPC's sent.
  ASSERT_TRUE(EvalJsAfterLifecycleUpdate(root->current_frame_host(), "", "")
                  .error.empty());
  ASSERT_TRUE(
      EvalJsAfterLifecycleUpdate(child_node_b->current_frame_host(), "", "")
          .error.empty());
  filter->Clear();

  // Scroll the child frame out of view, causing it to become throttled.
  ASSERT_TRUE(
      ExecJs(child_node_b->current_frame_host(), "window.scrollTo(0, 5000)"));
  filter->Wait();
  EXPECT_TRUE(
      filter->GetIntersectionState()->main_frame_intersection.IsEmpty());

  // Scroll the frame back into view.
  ASSERT_TRUE(
      ExecJs(child_node_b->current_frame_host(), "window.scrollTo(0, 0)"));
  filter->Wait();
  EXPECT_FALSE(
      filter->GetIntersectionState()->main_frame_intersection.IsEmpty());
}

// Tests that outermost_main_frame_scroll_position is not shared by frames in
// the same process. This is a regression test for https://crbug.com/1063760.
//
// Set up the frame tree to be A(B1(C1),B2(C2)). Send IPC's with different
// ViewportIntersection information to B1 and B2, and then check that the
// information they propagate to C1 and C2 is different.
// Disabled because of https://crbug.com/1136263
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest,
                       DISABLED_MainFrameScrollOffset) {
  GURL a_url = embedded_test_server()->GetURL(
      "a.com", "/frame_tree/scrollable_page_with_two_frames.html");
  GURL b_url = embedded_test_server()->GetURL(
      "b.com", "/frame_tree/page_with_large_iframe.html");
  GURL c_url = embedded_test_server()->GetURL("c.com", "/title1.html");

  EXPECT_TRUE(NavigateToURL(shell(), a_url));
  FrameTreeNode* a_node = web_contents()->GetPrimaryFrameTree().root();

  FrameTreeNode* b1_node = a_node->child_at(0);
  EXPECT_TRUE(NavigateToURLFromRenderer(b1_node, b_url));

  FrameTreeNode* c1_node = b1_node->child_at(0);
  EXPECT_TRUE(NavigateToURLFromRenderer(c1_node, c_url));

  FrameTreeNode* b2_node = a_node->child_at(1);
  EXPECT_TRUE(NavigateToURLFromRenderer(b2_node, b_url));

  FrameTreeNode* c2_node = b2_node->child_at(0);
  EXPECT_TRUE(NavigateToURLFromRenderer(c2_node, c_url));

  // This will intercept messages sent from B1 to C1, describing C1's viewport
  // intersection.
  RenderFrameProxyHost* c1_proxy =
      c1_node->render_manager()->GetProxyToParent();
  auto b1_to_c1_message_filter =
      std::make_unique<UpdateViewportIntersectionMessageFilter>(c1_proxy);

  // This will intercept messages sent from B2 to C2, describing C2's viewport
  // intersection.
  RenderFrameProxyHost* c2_proxy =
      c2_node->render_manager()->GetProxyToParent();
  auto b2_to_c2_message_filter =
      std::make_unique<UpdateViewportIntersectionMessageFilter>(c2_proxy);

  // Running requestAnimationFrame will ensure that any pending IPC's have been
  // sent by the renderer and received by the browser.
  auto flush_ipcs = [](FrameTreeNode* node) {
    ASSERT_TRUE(EvalJsAfterLifecycleUpdate(node->current_frame_host(), "", "")
                    .error.empty());
  };

  flush_ipcs(a_node);
  flush_ipcs(b1_node);
  flush_ipcs(b2_node);
  b1_to_c1_message_filter->Clear();
  b2_to_c2_message_filter->Clear();

  // Now that everything is in a stable, consistent state, we will send viewport
  // intersection IPC's to B1 and B2 that contain a different
  // outermost_main_frame_scroll_position, and then verify that each of them
  // propagates their own value of outermost_main_frame_scroll_position to C1
  // and C2, respectively. The IPC code mimics messages that A would send to B1
  // and B2.
  auto b1_intersection_state = b1_node->render_manager()
                                   ->GetProxyToParent()
                                   ->cross_process_frame_connector()
                                   ->intersection_state();

  b1_intersection_state.outermost_main_frame_scroll_position.Offset(10, 0);
  // A change in outermost_main_frame_scroll_position by itself will not cause
  // B1 to be marked dirty, so we also modify viewport_intersection.
  b1_intersection_state.viewport_intersection.set_y(
      b1_intersection_state.viewport_intersection.y() + 7);
  b1_intersection_state.viewport_intersection.set_height(
      b1_intersection_state.viewport_intersection.height() - 7);

  ForceUpdateViewportIntersection(b1_node, b1_intersection_state);

  auto b2_intersection_state = b2_node->render_manager()
                                   ->GetProxyToParent()
                                   ->cross_process_frame_connector()
                                   ->intersection_state();

  b2_intersection_state.outermost_main_frame_scroll_position.Offset(20, 0);
  b2_intersection_state.viewport_intersection.set_y(
      b2_intersection_state.viewport_intersection.y() + 7);
  b2_intersection_state.viewport_intersection.set_height(
      b2_intersection_state.viewport_intersection.height() - 7);

  ForceUpdateViewportIntersection(b2_node, b2_intersection_state);

  // Once IPC's have been flushed to the C frames, we should see conflicting
  // values for outermost_main_frame_scroll_position.
  flush_ipcs(b1_node);
  flush_ipcs(b2_node);
  ASSERT_TRUE(b1_to_c1_message_filter->MessageReceived());
  ASSERT_TRUE(b2_to_c2_message_filter->MessageReceived());
  EXPECT_EQ(b1_to_c1_message_filter->GetIntersectionState()
                ->outermost_main_frame_scroll_position,
            gfx::Point(10, 0));
  EXPECT_EQ(b2_to_c2_message_filter->GetIntersectionState()
                ->outermost_main_frame_scroll_position,
            gfx::Point(20, 0));
  b1_to_c1_message_filter->Clear();
  b2_to_c2_message_filter->Clear();

  // If we scroll the main frame, it should propagate IPC's which re-synchronize
  // the values for all child frames.
  ASSERT_TRUE(EvalJsAfterLifecycleUpdate(a_node->current_frame_host(),
                                         "window.scrollTo(0, 5)", "")
                  .error.empty());
  flush_ipcs(b1_node);
  flush_ipcs(b2_node);
  ASSERT_TRUE(b1_to_c1_message_filter->MessageReceived());
  ASSERT_TRUE(b2_to_c2_message_filter->MessageReceived());

  // Window scroll offset will be scaled by device scale factor
  const float device_scale_factor = a_node->render_manager()
                                        ->GetRenderWidgetHostView()
                                        ->GetDeviceScaleFactor();
  float expected_y = device_scale_factor * 5.0;
  EXPECT_NEAR(b1_to_c1_message_filter->GetIntersectionState()
                  ->outermost_main_frame_scroll_position.y(),
              expected_y, 1.f);
  EXPECT_NEAR(b2_to_c2_message_filter->GetIntersectionState()
                  ->outermost_main_frame_scroll_position.y(),
              expected_y, 1.f);
}

IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest,
                       FrameViewportIntersectionTestAggregate) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b,c,a,b)"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // Each immediate child is sized to 100% width and 75% height.
  LayoutNonRecursiveForTestingViewportIntersection(shell()->web_contents());

  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();

  // Child 2 does not intersect, but shares widget with the main frame.
  FrameTreeNode* node = root->child_at(2);
  RenderProcessHostPriorityClient::Priority priority =
      node->current_frame_host()->GetRenderWidgetHost()->GetPriority();
  EXPECT_TRUE(priority.intersects_viewport);
  EXPECT_TRUE(
      node->current_frame_host()->GetProcess()->GetIntersectsViewport());

  // Child 3 does not intersect, but shares a process with child 0.
  node = root->child_at(3);
  priority = node->current_frame_host()->GetRenderWidgetHost()->GetPriority();
  EXPECT_FALSE(priority.intersects_viewport);
  EXPECT_TRUE(
      node->current_frame_host()->GetProcess()->GetIntersectsViewport());
}

// Tests that when a non-root frame in an iframe, performs a RAF to emulate a
// scroll, that metrics are reported.
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest, ScrollByRAF) {
  base::HistogramTester histogram_tester;
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b(b))"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();
  ASSERT_EQ(1U, root->child_count());

  EXPECT_EQ(
      " Site A ------------ proxies for B\n"
      "   +--Site B ------- proxies for A\n"
      "        +--Site B -- proxies for A\n"
      "Where A = http://a.com/\n"
      "      B = http://b.com/",
      DepictFrameTree(root));

  // Layout all three frames, so that the animation has a region to mark dirty.
  LayoutNonRecursiveForTestingViewportIntersection(root->current_frame_host());
  LayoutNonRecursiveForTestingViewportIntersection(
      root->child_at(0)->current_frame_host());
  LayoutNonRecursiveForTestingViewportIntersection(
      root->child_at(0)->child_at(0)->current_frame_host());

  // Add a div to the nested iframe, so that it can be animated.
  RenderFrameSubmissionObserver frame_observer(root->child_at(0)->child_at(0));
  std::string addContent(R"(
      var d = document.createElement('div');
      d.id = 'animationtarget';
      d.innerHTML = 'Hey Listen!';
      document.body.appendChild(d);
    )");
  ASSERT_TRUE(
      EvalJsAfterLifecycleUpdate(
          root->child_at(0)->child_at(0)->current_frame_host(), "", addContent)
          .error.empty());
  frame_observer.WaitForAnyFrameSubmission();

  // Fetch the initial metrics, as adding a div can incidentally trigger RAF
  // metrics.
  FetchHistogramsFromChildProcesses();
  auto initial_samples = histogram_tester.GetAllSamples(
      "Graphics.Smoothness.PercentDroppedFrames3.MainThread.RAF");
  ASSERT_EQ(initial_samples.size(), 0u);

  const int pre_scroll_frame_count = frame_observer.render_frame_count();

  // Run a RAF that takes more than one frame, as metrics due to not track
  // frames where WillBeginMainFrame occurs before it is triggered. Subsequent
  // RAFs in the sequence will be measured.
  std::string scrollByRAF(R"(
     var offset = 0;
      function run() {
        let child = document.getElementById("animationtarget");
        var rect = child.getBoundingClientRect();
        child.style = 'transform: translateY(' + parseInt(offset)+'px);';
        offset += 1;
        requestAnimationFrame(run);
      }
      run();
     )");
  ASSERT_TRUE(
      EvalJsAfterLifecycleUpdate(
          root->child_at(0)->child_at(0)->current_frame_host(), scrollByRAF, "")
          .error.empty());

  // There will have been one frame before the RAF sequence. The minimum for
  // reporting if 100 frames, however we need to wait at least one extra frame.
  // On Android the animation begins during the initial call to
  // EvalJsAfterLifecycleUpdate. However on Linux the first translate is not
  // applied until the subsequent frame. So we wait for the minimum, then verify
  // afterwards.
  const int kExpectedNumberFrames = 101 + pre_scroll_frame_count;
  while (frame_observer.render_frame_count() < kExpectedNumberFrames)
    frame_observer.WaitForAnyFrameSubmission();

  // We now wait for FrameSequenceTracker to time out in order for it to report.
  // This will occur once the minimum 100 frames have been produced, and 5s have
  // passed. If the test times out then the bug is back.
  while (histogram_tester
             .GetAllSamples(
                 "Graphics.Smoothness.PercentDroppedFrames3.MainThread.RAF")
             .empty()) {
    frame_observer.WaitForAnyFrameSubmission();
    FetchHistogramsFromChildProcesses();
  }
}

// Make sure that when a relevant feature of the main frame changes, e.g. the
// frame width, that the browser is notified.
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest, TextAutosizerPageInfo) {
  UpdateTextAutosizerInfoProxyObserver update_text_autosizer_info_observer;

  blink::web_pref::WebPreferences prefs =
      web_contents()->GetOrCreateWebPreferences();
  prefs.text_autosizing_enabled = true;

  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b)"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));
  web_contents()->SetWebPreferences(prefs);

  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();
  ASSERT_EQ(1U, root->child_count());
  FrameTreeNode* b_child = root->child_at(0);

  blink::mojom::TextAutosizerPageInfo received_page_info;
  auto interceptor = std::make_unique<TextAutosizerPageInfoInterceptor>(
      web_contents()->GetPrimaryMainFrame());
#if BUILDFLAG(IS_ANDROID)
  prefs.device_scale_adjustment += 0.05f;
  // Change the device scale adjustment to trigger a RemotePageInfo update.
  web_contents()->SetWebPreferences(prefs);
  // Make sure we receive a ViewHostMsg from the main frame's renderer.
  interceptor->WaitForPageInfo(std::optional<int>(),
                               prefs.device_scale_adjustment);
  // Make sure the correct page message is sent to the child.
  base::RunLoop().RunUntilIdle();
  received_page_info = interceptor->GetTextAutosizerPageInfo();
  EXPECT_EQ(prefs.device_scale_adjustment,
            received_page_info.device_scale_adjustment);
#else
  // Resize the main frame, then wait to observe that the RemotePageInfo message
  // arrives.
  auto* view = web_contents()->GetRenderWidgetHostView();
  gfx::Rect old_bounds = view->GetViewBounds();
  gfx::Rect new_bounds(
      old_bounds.origin(),
      gfx::Size(old_bounds.width() - 20, old_bounds.height() - 20));

  view->SetBounds(new_bounds);
  // Make sure we receive a ViewHostMsg from the main frame's renderer.
  interceptor->WaitForPageInfo(new_bounds.width(), std::optional<float>());
  // Make sure the correct page message is sent to the child.
  base::RunLoop().RunUntilIdle();
  received_page_info = interceptor->GetTextAutosizerPageInfo();
  EXPECT_EQ(new_bounds.width(), received_page_info.main_frame_width);
#endif  // BUILDFLAG(IS_ANDROID)

  // Dynamically create a new, cross-process frame to test sending the cached
  // TextAutosizerPageInfo.

  GURL c_url = embedded_test_server()->GetURL("c.com", "/title1.html");
  // The following is a hack so we can get an IPC watcher connected to the
  // RenderProcessHost for C before the `blink::WebView` is created for it, and
  // the TextAutosizerPageInfo IPC is sent to it.
  scoped_refptr<SiteInstance> c_site =
      web_contents()->GetSiteInstance()->GetRelatedSiteInstance(c_url);
  // Force creation of a render process for c's SiteInstance, this will get
  // used when we dynamically create the new frame.
  auto* c_rph = static_cast<RenderProcessHostImpl*>(c_site->GetProcess());
  ASSERT_TRUE(c_rph);
  ASSERT_NE(c_rph, root->current_frame_host()->GetProcess());
  ASSERT_NE(c_rph, b_child->current_frame_host()->GetProcess());

  // Create the subframe now.
  std::string create_frame_script = base::StringPrintf(
      "var new_iframe = document.createElement('iframe');"
      "new_iframe.src = '%s';"
      "document.body.appendChild(new_iframe);",
      c_url.spec().c_str());
  EXPECT_TRUE(ExecJs(root, create_frame_script));
  ASSERT_EQ(2U, root->child_count());

  // Ensure IPC is sent.
  base::RunLoop().RunUntilIdle();
  blink::mojom::TextAutosizerPageInfo page_info_sent_to_remote_main_frames =
      update_text_autosizer_info_observer.TextAutosizerPageInfo(
          web_contents()
              ->GetRenderManager()
              ->GetAllProxyHostsForTesting()
              .begin()
              ->second.get());

  EXPECT_EQ(received_page_info.main_frame_width,
            page_info_sent_to_remote_main_frames.main_frame_width);
  EXPECT_EQ(received_page_info.main_frame_layout_width,
            page_info_sent_to_remote_main_frames.main_frame_layout_width);
  EXPECT_EQ(received_page_info.device_scale_adjustment,
            page_info_sent_to_remote_main_frames.device_scale_adjustment);
}

// Test that the physical backing size and view bounds for a scaled out-of-
// process iframe are set and updated correctly.
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest,
                       CompositorViewportPixelSizeTest) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/frame_tree/page_with_scaled_frame.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();

  ASSERT_EQ(1U, root->child_count());

  FrameTreeNode* parent_iframe_node = root->child_at(0);

  EXPECT_EQ(
      " Site A ------------ proxies for B\n"
      "   +--Site A ------- proxies for B\n"
      "        +--Site B -- proxies for A\n"
      "Where A = http://a.com/\n"
      "      B = http://baz.com/",
      DepictFrameTree(root));

  FrameTreeNode* nested_iframe_node = parent_iframe_node->child_at(0);
  RenderFrameProxyHost* proxy_to_parent =
      nested_iframe_node->render_manager()->GetProxyToParent();
  CrossProcessFrameConnector* connector =
      proxy_to_parent->cross_process_frame_connector();
  RenderWidgetHostViewBase* rwhv_nested =
      static_cast<RenderWidgetHostViewBase*>(
          nested_iframe_node->current_frame_host()
              ->GetRenderWidgetHost()
              ->GetView());

  RenderFrameSubmissionObserver frame_observer(nested_iframe_node);
  frame_observer.WaitForMetadataChange();

  // Verify that applying a CSS scale transform does not impact the size of the
  // content of the nested iframe.
  // The screen_space_rect_in_dip may be off by 1 due to rounding. There is no
  // good way to avoid this due to various device-scale-factor. (e.g. when
  // dsf=3.375, ceil(round(50 * 3.375) / 3.375) = 51. Thus, we allow the screen
  // size in dip to be off by 1 here.
  EXPECT_NEAR(50, connector->rect_in_parent_view_in_dip().size().width(), 1);
  EXPECT_NEAR(50, connector->rect_in_parent_view_in_dip().size().height(), 1);
  EXPECT_EQ(gfx::Size(100, 100), rwhv_nested->GetViewBounds().size());
  EXPECT_EQ(gfx::Size(100, 100), connector->local_frame_size_in_dip());
  EXPECT_EQ(connector->local_frame_size_in_pixels(),
            rwhv_nested->GetCompositorViewportPixelSize());
}

// Verify an OOPIF resize handler doesn't fire immediately after load without
// the frame having been resized. See https://crbug.com/826457.
// TODO(crbug.com/40809978): Test is very flaky on many platforms.
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest,
                       DISABLED_NoResizeAfterIframeLoad) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(a)"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();

  FrameTreeNode* iframe = root->child_at(0);
  GURL site_url =
      embedded_test_server()->GetURL("b.com", "/page_with_resize_handler.html");
  EXPECT_TRUE(NavigateToURLFromRenderer(iframe, site_url));
  base::RunLoop().RunUntilIdle();

  // Should be zero because the iframe only has its initial size from parent.
  EXPECT_EQ(0, EvalJs(iframe->current_frame_host(), "resize_count;"));
}

// Test that the view bounds for an out-of-process iframe are set and updated
// correctly, including accounting for local frame offsets in the parent and
// scroll positions.
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest, ViewBoundsInNestedFrameTest) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(a)"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();
  RenderWidgetHostViewBase* rwhv_root = static_cast<RenderWidgetHostViewBase*>(
      root->current_frame_host()->GetRenderWidgetHost()->GetView());
  ASSERT_EQ(1U, root->child_count());

  FrameTreeNode* parent_iframe_node = root->child_at(0);
  GURL site_url(embedded_test_server()->GetURL(
      "a.com", "/frame_tree/page_with_positioned_frame.html"));
  EXPECT_TRUE(NavigateToURLFromRenderer(parent_iframe_node, site_url));
  RenderFrameSubmissionObserver frame_observer(shell()->web_contents());

  EXPECT_EQ(
      " Site A ------------ proxies for B\n"
      "   +--Site A ------- proxies for B\n"
      "        +--Site B -- proxies for A\n"
      "Where A = http://a.com/\n"
      "      B = http://baz.com/",
      DepictFrameTree(root));

  FrameTreeNode* nested_iframe_node = parent_iframe_node->child_at(0);
  RenderWidgetHostViewBase* rwhv_nested =
      static_cast<RenderWidgetHostViewBase*>(
          nested_iframe_node->current_frame_host()
              ->GetRenderWidgetHost()
              ->GetView());
  WaitForHitTestData(nested_iframe_node->current_frame_host());

  float scale_factor =
      frame_observer.LastRenderFrameMetadata().page_scale_factor;

  // Get the view bounds of the nested iframe, which should account for the
  // relative offset of its direct parent within the root frame.
  gfx::Rect bounds = rwhv_nested->GetViewBounds();

  RenderFrameProxyHost* parent_iframe_proxy =
      nested_iframe_node->render_manager()->GetProxyToParent();
  auto interceptor = std::make_unique<SynchronizeVisualPropertiesInterceptor>(
      parent_iframe_proxy);

  // Scroll the parent frame downward to verify that the child rect gets updated
  // correctly.
  blink::WebMouseWheelEvent scroll_event(
      blink::WebInputEvent::Type::kMouseWheel,
      blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests());

  scroll_event.SetPositionInWidget(
      std::floor((bounds.x() - rwhv_root->GetViewBounds().x() - 5) *
                 scale_factor),
      std::floor((bounds.y() - rwhv_root->GetViewBounds().y() - 5) *
                 scale_factor));
  scroll_event.delta_x = 0.0f;
  scroll_event.delta_y = -30.0f;
  scroll_event.phase = blink::WebMouseWheelEvent::kPhaseBegan;
  rwhv_root->ProcessMouseWheelEvent(scroll_event, ui::LatencyInfo());
  interceptor->WaitForRect();

  // The precise amount of scroll for the first view position update is not
  // deterministic, so this simply verifies that the OOPIF moved from its
  // earlier position.
  gfx::Rect update_rect = interceptor->last_rect();
  EXPECT_LT(update_rect.y(), bounds.y() - rwhv_root->GetViewBounds().y());
}

// Verify that "scrolling" property on frame elements propagates to child frames
// correctly.
// Does not work on android since android has scrollbars overlaid.
// TODO(bokan): Pretty soon most/all platforms will use overlay scrollbars. This
// test should find a better way to check for scrollability. crbug.com/662196.
// Flaky on Linux. crbug.com/790929.
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
#define MAYBE_FrameOwnerPropertiesPropagationScrolling \
  DISABLED_FrameOwnerPropertiesPropagationScrolling
#else
#define MAYBE_FrameOwnerPropertiesPropagationScrolling \
  FrameOwnerPropertiesPropagationScrolling
#endif
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest,
                       MAYBE_FrameOwnerPropertiesPropagationScrolling) {
#if BUILDFLAG(IS_MAC)
  ui::test::ScopedPreferredScrollerStyle scroller_style_override(false);
#endif
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/frame_owner_properties_scrolling.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();
  ASSERT_EQ(1u, root->child_count());

  EXPECT_EQ(
      " Site A ------------ proxies for B\n"
      "   +--Site B ------- proxies for A\n"
      "Where A = http://a.com/\n"
      "      B = http://b.com/",
      DepictFrameTree(root));

  FrameTreeNode* child = root->child_at(0);

  // If the available client width within the iframe is smaller than the
  // frame element's width, we assume there's a scrollbar.
  // Also note that just comparing clientHeight and scrollHeight of the frame's
  // document will not work.
  auto has_scrollbar = [](RenderFrameHostImpl* rfh) {
    int client_width = EvalJs(rfh, "document.body.clientWidth").ExtractInt();
    const int kFrameElementWidth = 200;
    return client_width < kFrameElementWidth;
  };

  auto set_scrolling_property = [](RenderFrameHostImpl* parent_rfh,
                                   const std::string& value) {
    EXPECT_TRUE(ExecJs(
        parent_rfh,
        base::StringPrintf("document.getElementById('child-1').setAttribute("
                           "    'scrolling', '%s');",
                           value.c_str())));
  };

  // Run the test over variety of parent/child cases.
  GURL urls[] = {// Remote to remote.
                 embedded_test_server()->GetURL("c.com", "/tall_page.html"),
                 // Remote to local.
                 embedded_test_server()->GetURL("a.com", "/tall_page.html"),
                 // Local to remote.
                 embedded_test_server()->GetURL("b.com", "/tall_page.html")};
  const std::string scrolling_values[] = {"yes", "auto", "no"};

  for (const auto& scrolling_value : scrolling_values) {
    bool expect_scrollbar = scrolling_value != "no";
    set_scrolling_property(root->current_frame_host(), scrolling_value);
    for (const auto& url : urls) {
      EXPECT_TRUE(NavigateToURLFromRenderer(child, url));
      EXPECT_EQ(expect_scrollbar, has_scrollbar(child->current_frame_host()));
    }
  }
}

// Verify that "marginwidth" and "marginheight" properties on frame elements
// propagate to child frames correctly.
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest,
                       FrameOwnerPropertiesPropagationMargin) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/frame_owner_properties_margin.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();
  ASSERT_EQ(1u, root->child_count());

  EXPECT_EQ(
      " Site A ------------ proxies for B\n"
      "   +--Site B ------- proxies for A\n"
      "Where A = http://a.com/\n"
      "      B = http://b.com/",
      DepictFrameTree(root));

  FrameTreeNode* child = root->child_at(0);

  EXPECT_EQ("10", EvalJs(child, "document.body.getAttribute('marginwidth');"));
  EXPECT_EQ("50", EvalJs(child, "document.body.getAttribute('marginheight');"));

  // Run the test over variety of parent/child cases.
  GURL urls[] = {// Remote to remote.
                 embedded_test_server()->GetURL("c.com", "/title2.html"),
                 // Remote to local.
                 embedded_test_server()->GetURL("a.com", "/title1.html"),
                 // Local to remote.
                 embedded_test_server()->GetURL("b.com", "/title2.html")};

  int current_margin_width = 15;
  int current_margin_height = 25;

  // Before each navigation, we change the marginwidth and marginheight
  // properties of the frame. We then check whether those properties are applied
  // correctly after the navigation has completed.
  for (const auto& url : urls) {
    // Change marginwidth and marginheight before navigating.
    EXPECT_TRUE(ExecJs(
        root,
        base::StringPrintf("var child = document.getElementById('child-1');"
                           "child.setAttribute('marginwidth', '%d');"
                           "child.setAttribute('marginheight', '%d');",
                           current_margin_width, current_margin_height)));

    EXPECT_TRUE(NavigateToURLFromRenderer(child, url));

    EXPECT_EQ(base::NumberToString(current_margin_width),
              EvalJs(child, "document.body.getAttribute('marginwidth');"));
    EXPECT_EQ(base::NumberToString(current_margin_height),
              EvalJs(child, "document.body.getAttribute('marginheight');"));

    current_margin_width += 5;
    current_margin_height += 10;
  }
}

// Verify that "csp" property on frame elements propagates to child frames
// correctly. See https://crbug.com/647588
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest,
                       FrameOwnerPropertiesPropagationCSP) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/frame_owner_properties_csp.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();
  ASSERT_EQ(1u, root->child_count());

  // The document in the iframe is blocked by CSPEE. An error page is loaded, it
  // stays in the process of the main document.
  EXPECT_EQ(
      " Site A\n"
      "   +--Site A\n"
      "Where A = http://a.com/",
      DepictFrameTree(root));

  FrameTreeNode* child = root->child_at(0);

  EXPECT_EQ(
      "object-src \'none\'",
      EvalJs(root, "document.getElementById('child-1').getAttribute('csp');"));

  // Run the test over variety of parent/child cases.
  struct {
    std::string csp_value;
    GURL url;
    bool should_block;
  } testCases[]{
      // Remote to remote.
      {"default-src a.com",
       embedded_test_server()->GetURL("c.com", "/title2.html"), true},
      // Remote to local.
      {"default-src b.com",
       embedded_test_server()->GetURL("a.com", "/title1.html"), true},
      // Local to remote.
      {"img-src c.com", embedded_test_server()->GetURL("b.com", "/title2.html"),
       true},
  };

  // Before each navigation, we change the csp property of the frame.
  // We then check whether that property is applied
  // correctly after the navigation has completed.
  for (const auto& testCase : testCases) {
    // Change csp before navigating.
    EXPECT_TRUE(ExecJs(
        root,
        base::StringPrintf("document.getElementById('child-1').setAttribute("
                           "    'csp', '%s');",
                           testCase.csp_value.c_str())));

    NavigateFrameToURL(child, testCase.url);
    EXPECT_EQ(testCase.csp_value, child->csp_attribute()->header->header_value);
    // TODO(amalika): add checks that the CSP replication takes effect

    const url::Origin child_origin =
        child->current_frame_host()->GetLastCommittedOrigin();

    EXPECT_EQ(testCase.should_block, child_origin.opaque());
    EXPECT_EQ(url::Origin::Create(testCase.url.DeprecatedGetOriginAsURL())
                  .GetTupleOrPrecursorTupleIfOpaque(),
              child_origin.GetTupleOrPrecursorTupleIfOpaque());
  }
}

// This test verifies that changing the CSS visibility of a cross-origin
// <iframe> is forwarded to its corresponding RenderWidgetHost and all other
// RenderWidgetHosts corresponding to the nested cross-origin frame.
// TODO(crbug.com/40865141): Flaky on mac, linux-lacros, android.
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_CHROMEOS_LACROS)
#define MAYBE_CSSVisibilityChanged DISABLED_CSSVisibilityChanged
#else
#define MAYBE_CSSVisibilityChanged CSSVisibilityChanged
#endif
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest, MAYBE_CSSVisibilityChanged) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b(b(c(d(d(a))))))"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // Find all child RenderWidgetHosts.
  std::vector<RenderWidgetHostImpl*> child_widget_hosts;
  FrameTreeNode* first_cross_process_child =
      web_contents()->GetPrimaryFrameTree().root()->child_at(0);
  for (auto* ftn : web_contents()->GetPrimaryFrameTree().SubtreeNodes(
           first_cross_process_child)) {
    RenderFrameHostImpl* frame_host = ftn->current_frame_host();
    if (!frame_host->is_local_root())
      continue;

    child_widget_hosts.push_back(frame_host->GetRenderWidgetHost());
  }

  // Ignoring the root, there is exactly 4 local roots and hence 5
  // RenderWidgetHosts on the page.
  EXPECT_EQ(4U, child_widget_hosts.size());

  // Initially all the RenderWidgetHosts should be visible.
  for (size_t index = 0; index < child_widget_hosts.size(); ++index) {
    EXPECT_FALSE(child_widget_hosts[index]->is_hidden())
        << "The RWH at distance " << index + 1U
        << " from root RWH should not be hidden.";
  }

  std::string show_script =
      "document.querySelector('iframe').style.visibility = 'visible';";
  std::string hide_script =
      "document.querySelector('iframe').style.visibility = 'hidden';";

  // Define observers for notifications about hiding child RenderWidgetHosts.
  std::vector<std::unique_ptr<RenderWidgetHostVisibilityObserver>>
      hide_widget_host_observers(child_widget_hosts.size());
  for (size_t index = 0U; index < child_widget_hosts.size(); ++index) {
    hide_widget_host_observers[index] =
        std::make_unique<RenderWidgetHostVisibilityObserver>(
            child_widget_hosts[index], false);
  }

  EXPECT_TRUE(ExecJs(shell(), hide_script));
  for (size_t index = 0U; index < child_widget_hosts.size(); ++index) {
    EXPECT_TRUE(hide_widget_host_observers[index]->WaitUntilSatisfied())
        << "Expected RenderWidgetHost at distance " << index + 1U
        << " from root RenderWidgetHost to become hidden.";
  }

  // Define observers for notifications about showing child RenderWidgetHosts.
  std::vector<std::unique_ptr<RenderWidgetHostVisibilityObserver>>
      show_widget_host_observers(child_widget_hosts.size());
  for (size_t index = 0U; index < child_widget_hosts.size(); ++index) {
    show_widget_host_observers[index] =
        std::make_unique<RenderWidgetHostVisibilityObserver>(
            child_widget_hosts[index], true);
  }

  EXPECT_TRUE(ExecJs(shell(), show_script));
  for (size_t index = 0U; index < child_widget_hosts.size(); ++index) {
    EXPECT_TRUE(show_widget_host_observers[index]->WaitUntilSatisfied())
        << "Expected RenderWidgetHost at distance " << index + 1U
        << " from root RenderWidgetHost to become shown.";
  }
}

// This test verifies that hiding an OOPIF in CSS will stop generating
// compositor frames for the OOPIF and any nested OOPIFs inside it. This holds
// even when the whole page is shown.
#if BUILDFLAG(IS_MAC)
// Flaky on Mac. https://crbug.com/1505297
#define MAYBE_HiddenOOPIFWillNotGenerateCompositorFrames \
  DISABLED_HiddenOOPIFWillNotGenerateCompositorFrames
#else
#define MAYBE_HiddenOOPIFWillNotGenerateCompositorFrames \
  HiddenOOPIFWillNotGenerateCompositorFrames
#endif
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest,
                       MAYBE_HiddenOOPIFWillNotGenerateCompositorFrames) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/frame_tree/page_with_two_frames.html"));
  ASSERT_TRUE(NavigateToURL(shell(), main_url));
  ASSERT_EQ(shell()->web_contents()->GetLastCommittedURL(), main_url);

  GURL cross_site_url_b =
      embedded_test_server()->GetURL("b.com", "/counter.html");

  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();

  EXPECT_TRUE(NavigateToURLFromRenderer(root->child_at(0), cross_site_url_b));

  EXPECT_TRUE(NavigateToURLFromRenderer(root->child_at(1), cross_site_url_b));

  // Now inject code in the first frame to create a nested OOPIF.
  RenderFrameHostCreatedObserver new_frame_created_observer(
      shell()->web_contents(), 1);
  ASSERT_TRUE(
      ExecJs(root->child_at(0)->current_frame_host(),
             "document.body.appendChild(document.createElement('iframe'));"));
  new_frame_created_observer.Wait();

  GURL cross_site_url_a =
      embedded_test_server()->GetURL("a.com", "/counter.html");

  // Navigate the nested frame.
  TestFrameNavigationObserver observer(root->child_at(0)->child_at(0));
  ASSERT_TRUE(ExecJs(root->child_at(0)->current_frame_host(),
                     JsReplace("document.querySelector('iframe').src = $1",
                               cross_site_url_a)));
  observer.Wait();

  RenderWidgetHostViewChildFrame* first_child_view =
      static_cast<RenderWidgetHostViewChildFrame*>(
          root->child_at(0)->current_frame_host()->GetView());
  RenderWidgetHostViewChildFrame* second_child_view =
      static_cast<RenderWidgetHostViewChildFrame*>(
          root->child_at(1)->current_frame_host()->GetView());
  RenderWidgetHostViewChildFrame* nested_child_view =
      static_cast<RenderWidgetHostViewChildFrame*>(
          root->child_at(0)->child_at(0)->current_frame_host()->GetView());

  RenderFrameSubmissionObserver first_frame_counter(
      first_child_view->host_->render_frame_metadata_provider());
  RenderFrameSubmissionObserver second_frame_counter(
      second_child_view->host_->render_frame_metadata_provider());
  RenderFrameSubmissionObserver third_frame_counter(
      nested_child_view->host_->render_frame_metadata_provider());

  const int kFrameCountLimit = 20;

  // Wait for a minimum number of compositor frames for the second frame.
  while (second_frame_counter.render_frame_count() < kFrameCountLimit)
    second_frame_counter.WaitForAnyFrameSubmission();
  ASSERT_LE(kFrameCountLimit, second_frame_counter.render_frame_count());

  // Now make sure all frames have roughly the counter value in the sense that
  // no counter value is more than twice any other.
  float ratio = static_cast<float>(first_frame_counter.render_frame_count()) /
                static_cast<float>(second_frame_counter.render_frame_count());
  EXPECT_GT(2.5f, ratio + 1 / ratio) << "Ratio is: " << ratio;

  ratio = static_cast<float>(first_frame_counter.render_frame_count()) /
          static_cast<float>(third_frame_counter.render_frame_count());
  EXPECT_GT(2.5f, ratio + 1 / ratio) << "Ratio is: " << ratio;

  // Make sure all views can become visible.
  EXPECT_TRUE(first_child_view->CanBecomeVisible());
  EXPECT_TRUE(second_child_view->CanBecomeVisible());
  EXPECT_TRUE(nested_child_view->CanBecomeVisible());

  // Hide the first frame and wait for the notification to be posted by its
  // RenderWidgetHost.
  RenderWidgetHostVisibilityObserver hide_observer(
      root->child_at(0)->current_frame_host()->GetRenderWidgetHost(), false);

  // Hide the first frame.
  ASSERT_TRUE(ExecJs(
      shell(),
      "document.getElementsByName('frame1')[0].style.visibility = 'hidden'"));
  ASSERT_TRUE(hide_observer.WaitUntilSatisfied());
  EXPECT_TRUE(first_child_view->FrameConnectorForTesting()->IsHidden());

  // Verify that only the second view can become visible now.
  EXPECT_FALSE(first_child_view->CanBecomeVisible());
  EXPECT_TRUE(second_child_view->CanBecomeVisible());
  EXPECT_FALSE(nested_child_view->CanBecomeVisible());

  // Now hide and show the WebContents (to simulate a tab switch).
  shell()->web_contents()->WasHidden();
  shell()->web_contents()->WasShown();

  first_frame_counter.ResetCounter();
  second_frame_counter.ResetCounter();
  third_frame_counter.ResetCounter();

  // We expect the second counter to keep running.
  while (second_frame_counter.render_frame_count() < kFrameCountLimit)
    second_frame_counter.WaitForAnyFrameSubmission();
  ASSERT_LT(kFrameCountLimit, second_frame_counter.render_frame_count() + 1);

  // Verify that the counter for other two frames did not count much.
  ratio = static_cast<float>(first_frame_counter.render_frame_count()) /
          static_cast<float>(second_frame_counter.render_frame_count());
  EXPECT_GT(0.5f, ratio) << "Ratio is: " << ratio;

  ratio = static_cast<float>(third_frame_counter.render_frame_count()) /
          static_cast<float>(second_frame_counter.render_frame_count());
  EXPECT_GT(0.5f, ratio) << "Ratio is: " << ratio;
}

// This test verifies that navigating a hidden OOPIF to cross-origin will not
// lead to creating compositor frames for the new OOPIF renderer.
IN_PROC_BROWSER_TEST_P(
    SitePerProcessBrowserTest,
    HiddenOOPIFWillNotGenerateCompositorFramesAfterNavigation) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/frame_tree/page_with_two_frames.html"));
  ASSERT_TRUE(NavigateToURL(shell(), main_url));
  ASSERT_EQ(shell()->web_contents()->GetLastCommittedURL(), main_url);

  GURL cross_site_url_b =
      embedded_test_server()->GetURL("b.com", "/counter.html");

  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();

  EXPECT_TRUE(NavigateToURLFromRenderer(root->child_at(0), cross_site_url_b));

  EXPECT_TRUE(NavigateToURLFromRenderer(root->child_at(1), cross_site_url_b));

  // Hide the first frame and wait for the notification to be posted by its
  // RenderWidgetHost.
  RenderWidgetHostVisibilityObserver hide_observer(
      root->child_at(0)->current_frame_host()->GetRenderWidgetHost(), false);

  // Hide the first frame.
  ASSERT_TRUE(ExecJs(
      shell(),
      "document.getElementsByName('frame1')[0].style.visibility = 'hidden'"));
  ASSERT_TRUE(hide_observer.WaitUntilSatisfied());

  // Now navigate the first frame to another OOPIF process.
  TestFrameNavigationObserver navigation_observer(
      root->child_at(0)->current_frame_host());
  GURL cross_site_url_c =
      embedded_test_server()->GetURL("c.com", "/counter.html");
  ASSERT_TRUE(
      ExecJs(web_contents(),
             JsReplace("document.getElementsByName('frame1')[0].src = $1",
                       cross_site_url_c)));
  navigation_observer.Wait();

  // Now investigate compositor frame creation.
  RenderWidgetHostViewChildFrame* first_child_view =
      static_cast<RenderWidgetHostViewChildFrame*>(
          root->child_at(0)->current_frame_host()->GetView());

  RenderWidgetHostViewChildFrame* second_child_view =
      static_cast<RenderWidgetHostViewChildFrame*>(
          root->child_at(1)->current_frame_host()->GetView());

  EXPECT_FALSE(first_child_view->CanBecomeVisible());

  RenderFrameSubmissionObserver first_frame_counter(
      first_child_view->host_->render_frame_metadata_provider());
  RenderFrameSubmissionObserver second_frame_counter(
      second_child_view->host_->render_frame_metadata_provider());

  const int kFrameCountLimit = 20;

  // Wait for a certain number of swapped compositor frames generated for the
  // second child view. During the same interval the first frame should not have
  // swapped any compositor frames.
  while (second_frame_counter.render_frame_count() < kFrameCountLimit)
    second_frame_counter.WaitForAnyFrameSubmission();
  ASSERT_LT(kFrameCountLimit, second_frame_counter.render_frame_count() + 1);

  float ratio = static_cast<float>(first_frame_counter.render_frame_count()) /
                static_cast<float>(second_frame_counter.render_frame_count());
  EXPECT_GT(0.5f, ratio) << "Ratio is: " << ratio;
}

IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest, ScreenCoordinates) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b)"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();
  FrameTreeNode* child = root->child_at(0);

  const char* properties[] = {"screenX", "screenY", "outerWidth",
                              "outerHeight"};

  for (const char* property : properties) {
    std::string script = base::StringPrintf("window.%s;", property);
    int root_value = EvalJs(root, script).ExtractInt();
    int child_value = EvalJs(child, script).ExtractInt();
    EXPECT_EQ(root_value, child_value);
  }
}

// Tests that an out-of-process iframe receives the visibilitychange event.
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest, VisibilityChange) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b)"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();

  EXPECT_EQ(
      " Site A ------------ proxies for B\n"
      "   +--Site B ------- proxies for A\n"
      "Where A = http://a.com/\n"
      "      B = http://b.com/",
      DepictFrameTree(root));

  EXPECT_TRUE(
      ExecJs(root->child_at(0),
             "var event_fired = 0;\n"
             "document.addEventListener('visibilitychange',\n"
             "                          function() { event_fired++; });\n"));

  shell()->web_contents()->WasHidden();

  EXPECT_EQ(1, EvalJs(root->child_at(0), "event_fired"));

  shell()->web_contents()->WasShown();

  EXPECT_EQ(2, EvalJs(root->child_at(0), "event_fired"));
}

// This test verifies that the main-frame's page scale factor propagates to
// the compositor layertrees in each of the child processes.
// Flaky on all platforms: https://crbug.com/1116774
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest,
                       DISABLED_PageScaleFactorPropagatesToOOPIFs) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b(c),d)"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();
  ASSERT_EQ(2u, root->child_count());
  FrameTreeNode* child_b = root->child_at(0);
  FrameTreeNode* child_c = root->child_at(1);
  ASSERT_EQ(1U, child_b->child_count());
  FrameTreeNode* child_d = child_b->child_at(0);

  ASSERT_TRUE(child_b);
  ASSERT_TRUE(child_c);
  ASSERT_TRUE(child_d);

  EXPECT_EQ(
      " Site A ------------ proxies for B C D\n"
      "   |--Site B ------- proxies for A C D\n"
      "   |    +--Site C -- proxies for A B D\n"
      "   +--Site D ------- proxies for A B C\n"
      "Where A = http://a.com/\n"
      "      B = http://b.com/\n"
      "      C = http://c.com/\n"
      "      D = http://d.com/",
      DepictFrameTree(root));

  RenderFrameSubmissionObserver observer_a(root);
  RenderFrameSubmissionObserver observer_b(child_b);
  RenderFrameSubmissionObserver observer_c(child_c);
  RenderFrameSubmissionObserver observer_d(child_d);

  // Monitor visual sync messages coming from the mainframe to make sure
  // |is_pinch_gesture_active| goes true during the pinch gesture.
  RenderFrameProxyHost* root_proxy_host =
      child_d->render_manager()->GetProxyToParent();
  auto interceptor_mainframe =
      std::make_unique<SynchronizeVisualPropertiesInterceptor>(root_proxy_host);

  // Monitor frame sync messages coming from child_b as it will need to
  // relay them to child_d.
  RenderFrameProxyHost* child_b_proxy_host =
      child_c->render_manager()->GetProxyToParent();
  auto interceptor_child_b =
      std::make_unique<SynchronizeVisualPropertiesInterceptor>(
          child_b_proxy_host);

  // We need to observe a root frame submission to pick up the initial page
  // scale factor.
  observer_a.WaitForAnyFrameSubmission();

  const float kPageScaleDelta = 2.f;
  // On desktop systems we expect |current_page_scale| to be 1.f, but on
  // Android it will typically be less than 1.f, and may take on arbitrary
  // values.
  float current_page_scale =
      observer_a.LastRenderFrameMetadata().page_scale_factor;
  float target_page_scale = current_page_scale * kPageScaleDelta;

  SyntheticPinchGestureParams params;
  auto* host = static_cast<RenderWidgetHostImpl*>(
      root->current_frame_host()->GetRenderWidgetHost());
  gfx::Rect bounds(host->GetView()->GetViewBounds().size());
  // The synthetic gesture code expects a location in root-view coordinates.
  params.anchor = gfx::PointF(bounds.CenterPoint());
  // In SyntheticPinchGestureParams, |scale_factor| is really a delta.
  params.scale_factor = kPageScaleDelta;
#if BUILDFLAG(IS_MAC)
  auto synthetic_pinch_gesture =
      std::make_unique<SyntheticTouchpadPinchGesture>(params);
#else
  auto synthetic_pinch_gesture =
      std::make_unique<SyntheticTouchscreenPinchGesture>(params);
#endif

  // Send pinch gesture and verify we receive the ack.
  InputEventAckWaiter ack_waiter(host,
                                 blink::WebInputEvent::Type::kGesturePinchEnd);
  host->QueueSyntheticGesture(
      std::move(synthetic_pinch_gesture),
      base::BindOnce([](SyntheticGesture::Result result) {
        EXPECT_EQ(SyntheticGesture::GESTURE_FINISHED, result);
      }));
  ack_waiter.Wait();

  // Make sure all the page scale values behave as expected.
  const float kScaleTolerance = 0.1f;
  observer_a.WaitForPageScaleFactor(target_page_scale, kScaleTolerance);
  observer_b.WaitForExternalPageScaleFactor(target_page_scale, kScaleTolerance);
  observer_c.WaitForExternalPageScaleFactor(target_page_scale, kScaleTolerance);
  observer_d.WaitForExternalPageScaleFactor(target_page_scale, kScaleTolerance);

  // The change in |is_pinch_gesture_active| that signals the end of the pinch
  // gesture will occur sometime after the ack for GesturePinchEnd, so we need
  // to wait for it from each renderer. If it's never seen, the test fails by
  // timing out.
  interceptor_mainframe->WaitForPinchGestureEnd();
  interceptor_child_b->WaitForPinchGestureEnd();
}

// Test that the compositing scale factor for an out-of-process iframe are set
// and updated correctly, including accounting for all intermediate transforms.
// TODO(crbug.com/40163506): Flaky test.
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest,
                       DISABLED_CompositingScaleFactorInNestedFrameTest) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/frame_tree/page_with_scaled_frame.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();

  ASSERT_EQ(1U, root->child_count());
  FrameTreeNode* child_b = root->child_at(0);

  EXPECT_TRUE(NavigateToURLFromRenderer(
      child_b, embedded_test_server()->GetURL(
                   "b.com", "/frame_tree/page_with_transformed_iframe.html")));

  ASSERT_EQ(1U, child_b->child_count());
  FrameTreeNode* child_c = child_b->child_at(0);

  EXPECT_TRUE(NavigateToURLFromRenderer(
      child_c, embedded_test_server()->GetURL(
                   "c.com", "/frame_tree/page_with_scaled_frame.html")));

  ASSERT_EQ(1U, child_c->child_count());
  FrameTreeNode* child_d = child_c->child_at(0);

  EXPECT_TRUE(NavigateToURLFromRenderer(
      child_d, embedded_test_server()->GetURL("d.com", "/simple_page.html")));

  EXPECT_EQ(
      " Site A ------------ proxies for B C D\n"
      "   +--Site B ------- proxies for A C D\n"
      "        +--Site C -- proxies for A B D\n"
      "             +--Site D -- proxies for A B C\n"
      "Where A = http://a.com/\n"
      "      B = http://b.com/\n"
      "      C = http://c.com/\n"
      "      D = http://d.com/",
      DepictFrameTree(root));

  // Wait for b.com's frame to have its compositing scale factor set to 0.5,
  // which is the scale factor for b.com's iframe element in the main frame.
  while (true) {
    auto* rwh_b = child_b->current_frame_host()->GetRenderWidgetHost();
    std::optional<blink::VisualProperties> properties =
        rwh_b->LastComputedVisualProperties();
    if (properties && cc::MathUtil::IsFloatNearlyTheSame(
                          properties->compositing_scale_factor, 0.5f)) {
      break;
    }
    base::RunLoop().RunUntilIdle();
  }

  // Wait for c.com's frame to have its compositing scale factor set to 0.5,
  // which is the accumulated scale factor of c.com to the main frame obtained
  // by multiplying the scale factor of c.com's iframe element (1 since
  // transform is rotation only without scale) with the scale factor of its
  // parent frame b.com (0.5).
  while (true) {
    auto* rwh_c = child_c->current_frame_host()->GetRenderWidgetHost();
    std::optional<blink::VisualProperties> properties =
        rwh_c->LastComputedVisualProperties();
    if (properties && cc::MathUtil::IsFloatNearlyTheSame(
                          properties->compositing_scale_factor, 0.5f)) {
      break;
    }
    base::RunLoop().RunUntilIdle();
  }

  // Wait for d.com's frame to have its compositing scale factor set to 0.25,
  // which is the accumulated scale factor of d.com to the main frame obtained
  // by combining the scale factor of d.com's iframe element (0.5) with the
  // scale factor of its parent d.com (0.5).
  while (true) {
    auto* rwh_d = child_d->current_frame_host()->GetRenderWidgetHost();
    std::optional<blink::VisualProperties> properties =
        rwh_d->LastComputedVisualProperties();
    if (properties && cc::MathUtil::IsFloatNearlyTheSame(
                          properties->compositing_scale_factor, 0.25f)) {
      break;
    }
    base::RunLoop().RunUntilIdle();
  }
}

// Test that the compositing scale factor for an out-of-process iframe is set
// to a non-zero value even if intermediate CSS transform has zero scale.
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest,
                       CompositingScaleFactorWithZeroScaleTransform) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/frame_tree/page_with_scaled_frame.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();

  ASSERT_EQ(1U, root->child_count());
  FrameTreeNode* child_b = root->child_at(0);

  EXPECT_TRUE(NavigateToURLFromRenderer(
      child_b,
      embedded_test_server()->GetURL("b.com", "/frame_tree/simple_page.html")));

  EXPECT_EQ(
      " Site A ------------ proxies for B\n"
      "   +--Site B ------- proxies for A\n"
      "Where A = http://a.com/\n"
      "      B = http://b.com/",
      DepictFrameTree(root));

  // Wait for b.com's frame to have its compositing scale factor set to 0.5,
  // which is the scale factor for b.com's iframe element in the main frame.
  while (true) {
    auto* rwh_b = child_b->current_frame_host()->GetRenderWidgetHost();
    std::optional<blink::VisualProperties> properties =
        rwh_b->LastComputedVisualProperties();
    if (properties && cc::MathUtil::IsFloatNearlyTheSame(
                          properties->compositing_scale_factor, 0.5f)) {
      break;
    }
    base::RunLoop().RunUntilIdle();
  }

  // Set iframe transform scale to 0.
  EXPECT_TRUE(
      EvalJs(root->current_frame_host(),
             "document.querySelector('iframe').style.transform = 'scale(0)'")
          .error.empty());

  // Wait for b.com frame's compositing scale factor to change, and check that
  // the final value is non-zero.
  while (true) {
    auto* rwh_b = child_b->current_frame_host()->GetRenderWidgetHost();
    std::optional<blink::VisualProperties> properties =
        rwh_b->LastComputedVisualProperties();
    if (properties && !cc::MathUtil::IsFloatNearlyTheSame(
                          properties->compositing_scale_factor, 0.5f)) {
      EXPECT_GT(properties->compositing_scale_factor, 0.0f);
      break;
    }
    base::RunLoop().RunUntilIdle();
  }
}

// Check that when a frame changes a subframe's size twice and then sends a
// postMessage to the subframe, the subframe's onmessage handler sees the new
// size.  In particular, ensure that the postMessage won't get reordered with
// the second resize, which might be throttled if the first resize is still in
// progress. See https://crbug.com/828529.
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest,
                       ResizeAndCrossProcessPostMessagePreserveOrder) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b)"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));
  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();

  // Add an onmessage handler to the subframe to send back its width.
  EXPECT_TRUE(ExecJs(root->child_at(0),
                     WaitForMessageScript("document.body.clientWidth")));

  // Drop the visual properties ACKs from the child renderer.  To do this,
  // unsubscribe the child's RenderWidgetHost from its
  // RenderFrameMetadataProvider, which ensures that
  // DidUpdateVisualProperties() won't be called on it, and the ACK won't be
  // reset.  This simulates that the ACK for the first resize below does not
  // arrive before the second resize IPC arrives from the
  // parent, and that the second resize IPC early-exits in
  // SynchronizeVisualProperties() due to the pending visual properties ACK.
  RenderWidgetHostImpl* rwh =
      root->child_at(0)->current_frame_host()->GetRenderWidgetHost();
  rwh->render_frame_metadata_provider_.RemoveObserver(rwh);

  // Now, resize the subframe twice from the main frame and send it a
  // postMessage. The postMessage handler should see the second updated size.
  EXPECT_TRUE(ExecJs(root, R"(
      var f = document.querySelector('iframe');
      f.width = 500;
      f.offsetTop; // force layout; this sends a resize IPC for width of 500.
      f.width = 700;
      f.offsetTop; // force layout; this sends a resize IPC for width of 700.
      f.contentWindow.postMessage('foo', '*');)"));
  EXPECT_EQ(700, EvalJs(root->child_at(0), "onMessagePromise"));
}

// This test verifies that when scrolling an OOPIF in a pinched-zoomed page,
// that the scroll-delta matches the distance between TouchStart/End as seen
// by the oopif, i.e. the oopif content 'sticks' to the finger during scrolling.
// The relation is not exact, but should be close.
// TODO(crbug.com/40697699): Re-enable the flaky test.
#if BUILDFLAG(IS_ANDROID)
#define MAYBE_DisableScrollOopifInPinchZoomedPage \
  DISABLED_ScrollOopifInPinchZoomedPage
#else
#define MAYBE_DisableScrollOopifInPinchZoomedPage ScrollOopifInPinchZoomedPage
#endif
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest,
                       MAYBE_DisableScrollOopifInPinchZoomedPage) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b)"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();
  ASSERT_EQ(1u, root->child_count());
  FrameTreeNode* child = root->child_at(0);
  ASSERT_TRUE(child);

  EXPECT_EQ(
      " Site A ------------ proxies for B\n"
      "   +--Site B ------- proxies for A\n"
      "Where A = http://a.com/\n"
      "      B = http://b.com/",
      DepictFrameTree(root));

  // Make B scrollable. The call to document.write will erase the html inside
  // the OOPIF, leaving just a vertical column of 'Hello's.
  std::string script =
      "var s = '<div>Hello</div>\\n';\n"
      "document.write(s.repeat(200));";
  EXPECT_TRUE(ExecJs(child, script));

  RenderFrameSubmissionObserver observer_a(root);
  RenderFrameSubmissionObserver observer_b(child);

  // We need to observe a root frame submission to pick up the initial page
  // scale factor.
  observer_a.WaitForAnyFrameSubmission();

  const float kPageScaleDelta = 2.f;
  // On desktop systems we expect |current_page_scale| to be 1.f, but on
  // Android it will typically be less than 1.f, and may take on arbitrary
  // values.
  float original_page_scale =
      observer_a.LastRenderFrameMetadata().page_scale_factor;
  float target_page_scale = original_page_scale * kPageScaleDelta;

  SyntheticPinchGestureParams params;
  auto* host = static_cast<RenderWidgetHostImpl*>(
      root->current_frame_host()->GetRenderWidgetHost());
  RenderWidgetHostViewBase* root_view = host->GetView();
  RenderWidgetHostViewBase* child_view =
      static_cast<RenderWidgetHostImpl*>(
          child->current_frame_host()->GetRenderWidgetHost())
          ->GetView();
  gfx::Rect bounds(root_view->GetViewBounds().size());
  // The synthetic gesture code expects a location in root-view coordinates.
  params.anchor = gfx::PointF(bounds.CenterPoint().x(), 70.f);
  // In SyntheticPinchGestureParams, |scale_factor| is really a delta.
  params.scale_factor = kPageScaleDelta;
#if BUILDFLAG(IS_MAC)
  auto synthetic_pinch_gesture =
      std::make_unique<SyntheticTouchpadPinchGesture>(params);
#else
  auto synthetic_pinch_gesture =
      std::make_unique<SyntheticTouchscreenPinchGesture>(params);
#endif

  // Send pinch gesture and verify we receive the ack.
  {
    InputEventAckWaiter ack_waiter(
        host, blink::WebInputEvent::Type::kGesturePinchEnd);
    host->QueueSyntheticGesture(
        std::move(synthetic_pinch_gesture),
        base::BindOnce([](SyntheticGesture::Result result) {
          EXPECT_EQ(SyntheticGesture::GESTURE_FINISHED, result);
        }));
    ack_waiter.Wait();
  }

  // Make sure all the page scale values behave as expected.
  const float kScaleTolerance = 0.07f;
  observer_a.WaitForPageScaleFactor(target_page_scale, kScaleTolerance);
  observer_b.WaitForExternalPageScaleFactor(target_page_scale, kScaleTolerance);
  float final_page_scale =
      observer_a.LastRenderFrameMetadata().page_scale_factor;

  // Verify scroll position of OOPIF.
  float initial_child_scroll = EvalJs(child, "window.scrollY").ExtractDouble();

  // Send touch-initiated gesture scroll sequence to OOPIF.
  // TODO(wjmaclean): GetViewBounds() is broken for OOPIFs when PSF != 1.f, so
  // we calculate it manually. This will need to be update when GetViewBounds()
  // in RenderWidgetHostViewBase is fixed. See https://crbug.com/928825.
  auto child_bounds = child_view->GetViewBounds();
  gfx::PointF child_upper_left =
      child_view->TransformPointToRootCoordSpaceF(gfx::PointF(0, 0));
  gfx::PointF child_lower_right = child_view->TransformPointToRootCoordSpaceF(
      gfx::PointF(child_bounds.width(), child_bounds.height()));
  gfx::PointF scroll_start_location_in_screen =
      gfx::PointF((child_upper_left.x() + child_lower_right.x()) / 2.f,
                  child_lower_right.y() - 10);
  const float kScrollDelta = 100.f;
  gfx::PointF scroll_end_location_in_screen =
      scroll_start_location_in_screen + gfx::Vector2dF(0, -kScrollDelta);

  // Create touch move sequence with discrete touch moves. Include a brief
  // pause at the end to avoid the scroll flinging.
  static constexpr char kActionsTemplate[] = R"HTML(
      [{
        "source" : "touch",
        "actions" : [
          { "name": "pointerDown", "x": %f, "y": %f},
          { "name": "pointerMove", "x": %f, "y": %f},
          { "name": "pause", "duration": 300 },
          { "name": "pointerUp"}
        ]
      }]
  )HTML";
  std::string touch_move_sequence_json = base::StringPrintf(
      kActionsTemplate, scroll_start_location_in_screen.x(),
      scroll_start_location_in_screen.y(), scroll_end_location_in_screen.x(),
      scroll_end_location_in_screen.y());
  ASSERT_OK_AND_ASSIGN(
      auto parsed_json,
      base::JSONReader::ReadAndReturnValueWithError(touch_move_sequence_json));
  ActionsParser actions_parser(std::move(parsed_json));

  ASSERT_TRUE(actions_parser.Parse());
  auto synthetic_scroll_gesture = std::make_unique<SyntheticPointerAction>(
      actions_parser.pointer_action_params());

  {
    auto* child_host = static_cast<RenderWidgetHostImpl*>(
        child->current_frame_host()->GetRenderWidgetHost());
    InputEventAckWaiter ack_waiter(
        child_host, blink::WebInputEvent::Type::kGestureScrollEnd);
    host->QueueSyntheticGesture(
        std::move(synthetic_scroll_gesture),
        base::BindOnce([](SyntheticGesture::Result result) {
          EXPECT_EQ(SyntheticGesture::GESTURE_FINISHED, result);
        }));
    ack_waiter.Wait();
  }

  // Verify new scroll position of OOPIF, should match touch sequence delta.
  float expected_scroll_delta = kScrollDelta / final_page_scale;
  float actual_scroll_delta =
      EvalJs(child, "window.scrollY").ExtractDouble() - initial_child_scroll;

  const float kScrollTolerance = 0.2f;
  EXPECT_GT((1.f + kScrollTolerance) * expected_scroll_delta,
            actual_scroll_delta);
  EXPECT_LT((1.f - kScrollTolerance) * expected_scroll_delta,
            actual_scroll_delta);
}

INSTANTIATE_TEST_SUITE_P(All,
                         SitePerProcessHighDPIBrowserTest,
                         testing::ValuesIn(RenderDocumentFeatureLevelValues()));
}  // namespace content
