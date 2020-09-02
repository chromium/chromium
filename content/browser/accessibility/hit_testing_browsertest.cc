// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/accessibility/hit_testing_browsertest.h"

#include "base/check.h"
#include "base/test/bind_test_util.h"
#include "build/build_config.h"
#include "build/chromecast_buildflags.h"
#include "content/browser/accessibility/accessibility_tree_formatter_blink.h"
#include "content/browser/accessibility/browser_accessibility.h"
#include "content/browser/accessibility/browser_accessibility_manager.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/use_zoom_for_dsf_policy.h"
#include "content/public/test/accessibility_notification_waiter.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "content/shell/browser/shell.h"
#include "content/test/content_browser_test_utils_internal.h"
#include "net/dns/mock_host_resolver.h"
#include "ui/accessibility/platform/ax_platform_node_base.h"
#include "ui/display/display_switches.h"
#include "ui/gfx/geometry/vector2d_conversions.h"

namespace content {

#define EXPECT_ACCESSIBILITY_HIT_TEST_RESULT(css_point, expected_node, \
                                             hit_node)                 \
  SCOPED_TRACE(GetScopedTrace(css_point));                             \
  EXPECT_EQ(expected_node->GetId(), hit_node->GetId());

AccessibilityHitTestingBrowserTest::AccessibilityHitTestingBrowserTest() =
    default;
AccessibilityHitTestingBrowserTest::~AccessibilityHitTestingBrowserTest() =
    default;

void AccessibilityHitTestingBrowserTest::SetUpCommandLine(
    base::CommandLine* command_line) {
  double device_scale_factor;
  bool use_zoom_for_dsf;
  std::tie(device_scale_factor, use_zoom_for_dsf) = GetParam();
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kForceDeviceScaleFactor,
      base::StringPrintf("%.2f", device_scale_factor));
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kEnableUseZoomForDSF, use_zoom_for_dsf ? "true" : "false");
}

std::string AccessibilityHitTestingBrowserTest::TestPassToString::operator()(
    const ::testing::TestParamInfo<AccessibilityZoomTestParam>& info) const {
  double device_scale_factor;
  bool use_zoom_for_dsf;
  std::tie(device_scale_factor, use_zoom_for_dsf) = info.param;
  std::string name =
      base::StringPrintf("ZoomFactor%g_UseZoomForDSF%s", device_scale_factor,
                         use_zoom_for_dsf ? "On" : "Off");

  // The test harness only allows alphanumeric characters and underscores
  // in param names.
  std::string sanitized_name;
  base::ReplaceChars(name, ".", "_", &sanitized_name);
  return sanitized_name;
}

BrowserAccessibilityManager*
AccessibilityHitTestingBrowserTest::GetRootBrowserAccessibilityManager() {
  WebContentsImpl* web_contents =
      static_cast<WebContentsImpl*>(shell()->web_contents());
  return web_contents->GetRootBrowserAccessibilityManager();
}

float AccessibilityHitTestingBrowserTest::GetDeviceScaleFactor() {
  return GetRootBrowserAccessibilityManager()->device_scale_factor();
}

float AccessibilityHitTestingBrowserTest::GetPageScaleFactor() {
  return GetRootBrowserAccessibilityManager()->GetPageScaleFactor();
}

gfx::Rect
AccessibilityHitTestingBrowserTest::GetViewBoundsInScreenCoordinates() {
  return GetRootBrowserAccessibilityManager()
      ->GetViewBoundsInScreenCoordinates();
}

  // http://www.chromium.org/developers/design-documents/blink-coordinate-spaces
  // If UseZoomForDSF is enabled, device scale factor gets applied going from
  // CSS to page pixels, i.e. before view offset.
  // if UseZoomForDSF is disabled, device scale factor gets applied going from
  // screen to physical pixels, i.e. after view offset.
gfx::Point AccessibilityHitTestingBrowserTest::CSSToFramePoint(
    gfx::Point css_point) {
  gfx::Point page_point;
  if (IsUseZoomForDSFEnabled())
    page_point = ScaleToRoundedPoint(css_point, GetDeviceScaleFactor());
  else
    page_point = css_point;

  gfx::Point frame_point = page_point - scroll_offset_;
  return frame_point;
}

gfx::Point AccessibilityHitTestingBrowserTest::CSSToPhysicalPixelPoint(
    gfx::Point css_point) {
  gfx::Point frame_point = CSSToFramePoint(css_point);
  gfx::Point viewport_point =
      gfx::ScaleToRoundedPoint(frame_point, GetPageScaleFactor());

  gfx::Rect screen_view_bounds = GetViewBoundsInScreenCoordinates();
  gfx::Point screen_point =
      viewport_point + screen_view_bounds.OffsetFromOrigin();

  gfx::Point physical_pixel_point;
  if (IsUseZoomForDSFEnabled()) {
    physical_pixel_point = screen_point;
  } else {
    physical_pixel_point =
        ScaleToRoundedPoint(screen_point, GetDeviceScaleFactor());
  }

  return physical_pixel_point;
}

BrowserAccessibility*
AccessibilityHitTestingBrowserTest::HitTestAndWaitForResultWithEvent(
    const gfx::Point& point,
    ax::mojom::Event event_to_fire) {
  BrowserAccessibilityManager* manager = GetRootBrowserAccessibilityManager();

  AccessibilityNotificationWaiter event_waiter(
      shell()->web_contents(), ui::kAXModeComplete, event_to_fire);
  ui::AXActionData action_data;
  action_data.action = ax::mojom::Action::kHitTest;
  action_data.target_point = CSSToFramePoint(point);
  action_data.hit_test_event_to_fire = event_to_fire;
  manager->delegate()->AccessibilityHitTest(CSSToFramePoint(point),
                                            event_to_fire, 0, {});
  event_waiter.WaitForNotification();

  RenderFrameHostImpl* target_frame = event_waiter.event_render_frame_host();
  BrowserAccessibilityManager* target_manager =
      target_frame->browser_accessibility_manager();
  int event_target_id = event_waiter.event_target_id();
  BrowserAccessibility* hit_node = target_manager->GetFromID(event_target_id);
  return hit_node;
}

BrowserAccessibility*
AccessibilityHitTestingBrowserTest::HitTestAndWaitForResult(
    const gfx::Point& point) {
  return HitTestAndWaitForResultWithEvent(point, ax::mojom::Event::kHover);
}

BrowserAccessibility*
AccessibilityHitTestingBrowserTest::AsyncHitTestAndWaitForCallback(
    const gfx::Point& point) {
  BrowserAccessibilityManager* manager = GetRootBrowserAccessibilityManager();

  gfx::Point target_point = CSSToFramePoint(point);
  base::RunLoop run_loop;
  BrowserAccessibilityManager* hit_manager = nullptr;
  int hit_node_id = 0;

  auto callback = [&](BrowserAccessibilityManager* manager, int node_id) {
    hit_manager = manager;
    hit_node_id = node_id;
    run_loop.QuitClosure().Run();
  };
  manager->delegate()->AccessibilityHitTest(
      target_point, ax::mojom::Event::kNone, 0,
      base::BindLambdaForTesting(callback));
  run_loop.Run();

  BrowserAccessibility* hit_node = hit_manager->GetFromID(hit_node_id);
  return hit_node;
}

BrowserAccessibility*
AccessibilityHitTestingBrowserTest::CallCachingAsyncHitTest(
    const gfx::Point& page_point) {
  gfx::Point screen_point = CSSToPhysicalPixelPoint(page_point);

  // Each call to CachingAsyncHitTest results in at least one HOVER
  // event received. Block until we receive it. CachingAsyncHitTestNearestLeaf
  // will call CachingAsyncHitTest.
  AccessibilityNotificationWaiter hover_waiter(
      shell()->web_contents(), ui::kAXModeComplete, ax::mojom::Event::kHover);

  BrowserAccessibility* result =
      GetRootBrowserAccessibilityManager()->CachingAsyncHitTest(screen_point);

  hover_waiter.WaitForNotification();
  return result;
}

BrowserAccessibility* AccessibilityHitTestingBrowserTest::CallNearestLeafNode(
    const gfx::Point& page_point) {
  gfx::Point screen_point = CSSToPhysicalPixelPoint(page_point);
  BrowserAccessibilityManager* manager =
      static_cast<WebContentsImpl*>(shell()->web_contents())
          ->GetRootBrowserAccessibilityManager();

  // Each call to CachingAsyncHitTest results in at least one HOVER
  // event received. Block until we receive it. CachingAsyncHitTest
  // will call CachingAsyncHitTest.
  AccessibilityNotificationWaiter hover_waiter(
      shell()->web_contents(), ui::kAXModeComplete, ax::mojom::Event::kHover);
  ui::AXPlatformNodeBase* platform_node = nullptr;
  if (manager->GetRoot()->GetAXPlatformNode()) {
    platform_node = static_cast<ui::AXPlatformNodeBase*>(
                        manager->GetRoot()->GetAXPlatformNode())
                        ->NearestLeafToPoint(screen_point);
  }
  hover_waiter.WaitForNotification();
  if (platform_node) {
    return BrowserAccessibility::FromAXPlatformNodeDelegate(
        platform_node->GetDelegate());
  }
  return nullptr;
}

void AccessibilityHitTestingBrowserTest::SynchronizeThreads() {
  MainThreadFrameObserver observer(shell()
                                       ->web_contents()
                                       ->GetRenderWidgetHostView()
                                       ->GetRenderWidgetHost());
  observer.Wait();
}

void AccessibilityHitTestingBrowserTest::SimulatePinchZoom(
    float desired_page_scale) {
  RenderFrameSubmissionObserver observer(shell()->web_contents());
  AccessibilityNotificationWaiter accessibility_waiter(
      shell()->web_contents(), ui::AXMode(),
      ax::mojom::Event::kLocationChanged);

  const gfx::Rect contents_rect = shell()->web_contents()->GetContainerBounds();
  const gfx::Point pinch_position(contents_rect.x(), contents_rect.y());
  SimulateGesturePinchSequence(shell()->web_contents(), pinch_position,
                               desired_page_scale,
                               blink::WebGestureDevice::kTouchscreen);

  // Wait for the gesture to be reflected, then make a note of the new scale
  // factor and any scroll offset that may have been introduced.
  observer.WaitForPageScaleFactor(desired_page_scale, 0);
  const cc::RenderFrameMetadata& render_frame_metadata =
      observer.LastRenderFrameMetadata();
  DCHECK(render_frame_metadata.page_scale_factor == desired_page_scale);
  if (render_frame_metadata.root_scroll_offset)
    scroll_offset_ = gfx::ToRoundedVector2d(
        render_frame_metadata.root_scroll_offset.value());
  else
    scroll_offset_ = gfx::Vector2d();

  // Ensure we get an accessibility update reflecting the new scale factor.
  accessibility_waiter.WaitForNotification();
}

std::string
AccessibilityHitTestingBrowserTest::FormatHitTestAccessibilityTree() {
  std::unique_ptr<AccessibilityTreeFormatter> accessibility_tree_formatter =
      AccessibilityTreeFormatterBlink::CreateBlink();
  accessibility_tree_formatter->set_show_ids(true);
  accessibility_tree_formatter->SetPropertyFilters(
      {{"name=*", AccessibilityTreeFormatter::PropertyFilter::ALLOW},
       {"location=*", AccessibilityTreeFormatter::PropertyFilter::ALLOW},
       {"size=*", AccessibilityTreeFormatter::PropertyFilter::ALLOW}});
  std::string accessibility_tree;
  accessibility_tree_formatter->FormatAccessibilityTreeForTesting(
      GetRootAndAssertNonNull(), &accessibility_tree);
  return accessibility_tree;
}

std::string AccessibilityHitTestingBrowserTest::GetScopedTrace(
    gfx::Point css_point) {
  std::stringstream string_stream;
  string_stream << std::endl
                << "View bounds: "
                << GetRootBrowserAccessibilityManager()
                       ->GetViewBoundsInScreenCoordinates()
                       .ToString()
                << " Page scale: " << GetPageScaleFactor()
                << " Scroll offset: " << scroll_offset_.ToString() << std::endl
                << "Test point CSS: " << css_point.ToString()
                << " Frame: " << CSSToFramePoint(css_point).ToString()
                << " Physical: "
                << CSSToPhysicalPixelPoint(css_point).ToString() << std::endl
                << "Accessibility tree: " << std::endl
                << FormatHitTestAccessibilityTree();
  return string_stream.str();
}

class AccessibilityHitTestingCrossProcessBrowserTest
    : public AccessibilityHitTestingBrowserTest {
 public:
  AccessibilityHitTestingCrossProcessBrowserTest() {}
  ~AccessibilityHitTestingCrossProcessBrowserTest() override {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    IsolateAllSitesForTesting(command_line);
    AccessibilityHitTestingBrowserTest::SetUpCommandLine(command_line);
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    SetupCrossSiteRedirector(embedded_test_server());
    ASSERT_TRUE(embedded_test_server()->Start());
  }
};

INSTANTIATE_TEST_SUITE_P(
    All,
    AccessibilityHitTestingBrowserTest,
    ::testing::Combine(::testing::Values(1, 2), ::testing::Bool()),
    AccessibilityHitTestingBrowserTest::TestPassToString());

INSTANTIATE_TEST_SUITE_P(
    All,
    AccessibilityHitTestingCrossProcessBrowserTest,
    ::testing::Combine(::testing::Values(1, 2), ::testing::Bool()),
    AccessibilityHitTestingBrowserTest::TestPassToString());

IN_PROC_BROWSER_TEST_P(AccessibilityHitTestingBrowserTest,
                       CachingAsyncHitTest) {
  ASSERT_TRUE(embedded_test_server()->Start());

  EXPECT_TRUE(NavigateToURL(shell(), GURL(url::kAboutBlankURL)));

  AccessibilityNotificationWaiter waiter(shell()->web_contents(),
                                         ui::kAXModeComplete,
                                         ax::mojom::Event::kLoadComplete);
  GURL url(embedded_test_server()->GetURL(
      "/accessibility/hit_testing/simple_rectangles.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url));
  waiter.WaitForNotification();

  WaitForAccessibilityTreeToContainNodeWithName(shell()->web_contents(),
                                                "rectA");

  // Test a hit on a rect in the main frame.
  {
    gfx::Point rect_2_point(49, 20);
    BrowserAccessibility* hit_node = CallCachingAsyncHitTest(rect_2_point);
    BrowserAccessibility* expected_node =
        FindNode(ax::mojom::Role::kGenericContainer, "rect2");
    EXPECT_ACCESSIBILITY_HIT_TEST_RESULT(rect_2_point, expected_node, hit_node);
  }

  // Test a hit on a rect in the iframe.
  {
    gfx::Point rect_b_point(79, 79);
    BrowserAccessibility* hit_node = CallCachingAsyncHitTest(rect_b_point);
    BrowserAccessibility* expected_node =
        FindNode(ax::mojom::Role::kGenericContainer, "rectB");
    EXPECT_ACCESSIBILITY_HIT_TEST_RESULT(rect_b_point, expected_node, hit_node);
  }
}

IN_PROC_BROWSER_TEST_P(AccessibilityHitTestingBrowserTest, HitTest) {
  ASSERT_TRUE(embedded_test_server()->Start());

  EXPECT_TRUE(NavigateToURL(shell(), GURL(url::kAboutBlankURL)));

  AccessibilityNotificationWaiter waiter(shell()->web_contents(),
                                         ui::kAXModeComplete,
                                         ax::mojom::Event::kLoadComplete);
  GURL url(embedded_test_server()->GetURL(
      "/accessibility/hit_testing/simple_rectangles.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url));
  waiter.WaitForNotification();

  WaitForAccessibilityTreeToContainNodeWithName(shell()->web_contents(),
                                                "rectA");

  // Test a hit on a rect in the main frame.
  {
    gfx::Point rect_2_point(49, 20);
    BrowserAccessibility* hit_node = HitTestAndWaitForResult(rect_2_point);
    BrowserAccessibility* expected_node =
        FindNode(ax::mojom::Role::kGenericContainer, "rect2");
    EXPECT_ACCESSIBILITY_HIT_TEST_RESULT(rect_2_point, expected_node, hit_node);

    // Try callback API.
    hit_node = AsyncHitTestAndWaitForCallback(rect_2_point);
    EXPECT_ACCESSIBILITY_HIT_TEST_RESULT(rect_2_point, expected_node, hit_node);
  }

  // Test a hit on a rect in the iframe.
  {
    gfx::Point rect_b_point(79, 79);
    BrowserAccessibility* hit_node = HitTestAndWaitForResult(rect_b_point);
    BrowserAccessibility* expected_node =
        FindNode(ax::mojom::Role::kGenericContainer, "rectB");
    EXPECT_ACCESSIBILITY_HIT_TEST_RESULT(rect_b_point, expected_node, hit_node);

    // Try callback API.
    hit_node = AsyncHitTestAndWaitForCallback(rect_b_point);
    EXPECT_ACCESSIBILITY_HIT_TEST_RESULT(rect_b_point, expected_node, hit_node);

    // Test with a different event.
    hit_node = HitTestAndWaitForResultWithEvent(rect_b_point,
                                                ax::mojom::Event::kAlert);
    EXPECT_ACCESSIBILITY_HIT_TEST_RESULT(rect_b_point, expected_node, hit_node);
  }
}

IN_PROC_BROWSER_TEST_P(AccessibilityHitTestingBrowserTest,
                       HitTestOutsideDocumentBoundsReturnsRoot) {
  EXPECT_TRUE(NavigateToURL(shell(), GURL(url::kAboutBlankURL)));

  // Load the page.
  AccessibilityNotificationWaiter waiter(shell()->web_contents(),
                                         ui::kAXModeComplete,
                                         ax::mojom::Event::kLoadComplete);
  const char url_str[] =
      "data:text/html,"
      "<!doctype html>"
      "<html><head><title>Accessibility Test</title></head>"
      "<body>"
      "<a href='#'>"
      "This is some text in a link"
      "</a>"
      "</body></html>";
  GURL url(url_str);
  EXPECT_TRUE(NavigateToURL(shell(), url));
  waiter.WaitForNotification();

  gfx::Point out_of_bounds_point(-1, -1);

  BrowserAccessibility* hit_node = HitTestAndWaitForResult(out_of_bounds_point);
  ASSERT_TRUE(hit_node != nullptr);
  ASSERT_EQ(ax::mojom::Role::kRootWebArea, hit_node->GetRole());

  // Try callback API.
  hit_node = AsyncHitTestAndWaitForCallback(out_of_bounds_point);
  ASSERT_TRUE(hit_node != nullptr);
  ASSERT_EQ(ax::mojom::Role::kRootWebArea, hit_node->GetRole());
}

IN_PROC_BROWSER_TEST_P(AccessibilityHitTestingCrossProcessBrowserTest,
                       HitTestingInCrossProcessIframeWithScrolling) {
  GURL url_a(embedded_test_server()->GetURL(
      "a.com", "/accessibility/hit_testing/simple_rectangles.html"));
  GURL url_b(embedded_test_server()->GetURL(
      "b.com",
      "/accessibility/hit_testing/simple_rectangles_scrolling_iframe.html"));

  EXPECT_TRUE(NavigateToURL(shell(), GURL(url::kAboutBlankURL)));
  AccessibilityNotificationWaiter waiter(shell()->web_contents(),
                                         ui::kAXModeComplete,
                                         ax::mojom::Event::kLoadComplete);

  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  waiter.WaitForNotification();
  WaitForAccessibilityTreeToContainNodeWithName(shell()->web_contents(),
                                                "rectA");

  auto* web_contents = static_cast<WebContentsImpl*>(shell()->web_contents());
  FrameTreeNode* root = web_contents->GetFrameTree()->root();
  ASSERT_EQ(1U, root->child_count());

  FrameTreeNode* child = root->child_at(0);
  NavigateFrameToURL(child, url_b);
  EXPECT_EQ(url_b, child->current_url());
  WaitForAccessibilityTreeToContainNodeWithName(shell()->web_contents(),
                                                "rectF");

  FrameTreeVisualizer visualizer;
  EXPECT_EQ(
      " Site A ------------ proxies for B\n"
      "   +--Site B ------- proxies for A\n"
      "Where A = http://a.com/\n"
      "      B = http://b.com/",
      visualizer.DepictFrameTree(root));

  // Before scrolling.
  {
    gfx::Point rect_b_point(79, 79);
    BrowserAccessibility* hit_node = HitTestAndWaitForResult(rect_b_point);
    BrowserAccessibility* expected_node =
        FindNode(ax::mojom::Role::kGenericContainer, "rectB");
    EXPECT_ACCESSIBILITY_HIT_TEST_RESULT(rect_b_point, expected_node, hit_node);

    // Try callback API.
    hit_node = AsyncHitTestAndWaitForCallback(rect_b_point);
    EXPECT_ACCESSIBILITY_HIT_TEST_RESULT(rect_b_point, expected_node, hit_node);
  }

  // Scroll div up 100px.
  int scroll_delta = 100;
  double actual_scroll_delta = 0;
  std::string scroll_string = base::StringPrintf(
      "window.scrollTo(0, %d); "
      "window.domAutomationController.send(window.scrollY);",
      scroll_delta);
  EXPECT_TRUE(ExecuteScriptAndExtractDouble(
      child->current_frame_host(), scroll_string, &actual_scroll_delta));
  EXPECT_NEAR(static_cast<double>(scroll_delta), actual_scroll_delta, 1.0);

  // After scrolling.
  {
    gfx::Point rect_g_point(79, 89);
    BrowserAccessibility* hit_node = HitTestAndWaitForResult(rect_g_point);
    BrowserAccessibility* expected_node =
        FindNode(ax::mojom::Role::kGenericContainer, "rectG");
    EXPECT_ACCESSIBILITY_HIT_TEST_RESULT(rect_g_point, expected_node, hit_node);

    // Try callback API.
    hit_node = AsyncHitTestAndWaitForCallback(rect_g_point);
    EXPECT_ACCESSIBILITY_HIT_TEST_RESULT(rect_g_point, expected_node, hit_node);
  }
}

IN_PROC_BROWSER_TEST_P(AccessibilityHitTestingBrowserTest,
                       CachingAsyncHitTestMissesElement) {
  ASSERT_TRUE(embedded_test_server()->Start());

  EXPECT_TRUE(NavigateToURL(shell(), GURL(url::kAboutBlankURL)));

  AccessibilityNotificationWaiter waiter(shell()->web_contents(),
                                         ui::kAXModeComplete,
                                         ax::mojom::Event::kLoadComplete);
  GURL url(embedded_test_server()->GetURL(
      "/accessibility/hit_testing/simple_rectangles_with_curtain.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url));
  waiter.WaitForNotification();

  WaitForAccessibilityTreeToContainNodeWithName(shell()->web_contents(),
                                                "rectA");

  // For each point we try, the first time we call CachingAsyncHitTest it
  // should FAIL and return the wrong object, because this test page has
  // been designed to confound local synchronous hit testing using
  // z-indexes. However, calling CachingAsyncHitTest a second time should
  // return the correct result (since CallCachingAsyncHitTest waits for the
  // HOVER event to be received).

  // Test a hit on a rect in the main frame.
  {
    // First call should land on the wrong element.
    gfx::Point rect_2_point(49, 20);
    BrowserAccessibility* hit_node = CallCachingAsyncHitTest(rect_2_point);
    BrowserAccessibility* expected_node =
        FindNode(ax::mojom::Role::kGenericContainer, "rect2");
    EXPECT_NE(expected_node->GetName(), hit_node->GetName());

    // Call again and we should get the correct element.
    hit_node = CallCachingAsyncHitTest(rect_2_point);
    EXPECT_ACCESSIBILITY_HIT_TEST_RESULT(rect_2_point, expected_node, hit_node);
  }

  // Test a hit on a rect in the iframe.
  {
    // First call should land on the wrong element.
    gfx::Point rect_b_point(79, 79);
    BrowserAccessibility* hit_node = CallCachingAsyncHitTest(rect_b_point);
    BrowserAccessibility* expected_node =
        FindNode(ax::mojom::Role::kGenericContainer, "rectB");
    EXPECT_NE(expected_node->GetName(), hit_node->GetName());

    // Call again and we should get the correct element.
    hit_node = CallCachingAsyncHitTest(rect_b_point);
    EXPECT_ACCESSIBILITY_HIT_TEST_RESULT(rect_b_point, expected_node, hit_node);
  }
}

#if !defined(OS_ANDROID) && !defined(OS_MAC)
// Fails flakily with compared ID differences. TODO(crbug.com/1121099): Re-nable
// this test.
#if defined(OS_LINUX) || defined(OS_CHROMEOS)
#define MAYBE_CachingAsyncHitTest_WithPinchZoom \
  DISABLED_CachingAsyncHitTest_WithPinchZoom
#else
#define MAYBE_CachingAsyncHitTest_WithPinchZoom \
  CachingAsyncHitTest_WithPinchZoom
#endif
IN_PROC_BROWSER_TEST_P(AccessibilityHitTestingBrowserTest,
                       MAYBE_CachingAsyncHitTest_WithPinchZoom) {
  ASSERT_TRUE(embedded_test_server()->Start());

  EXPECT_TRUE(NavigateToURL(shell(), GURL(url::kAboutBlankURL)));

  AccessibilityNotificationWaiter waiter(shell()->web_contents(),
                                         ui::kAXModeComplete,
                                         ax::mojom::Event::kLoadComplete);

  GURL url(embedded_test_server()->GetURL(
      "/accessibility/hit_testing/simple_rectangles.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url));
  SynchronizeThreads();
  waiter.WaitForNotification();

  WaitForAccessibilityTreeToContainNodeWithName(shell()->web_contents(),
                                                "rectA");

  // Apply pinch zoom.
  SimulatePinchZoom(1.25f);

  // Test a hit on a rect in the main frame.
  {
    gfx::Point rect_2_point(49, 20);
    BrowserAccessibility* hit_node = CallCachingAsyncHitTest(rect_2_point);
    BrowserAccessibility* expected_node =
        FindNode(ax::mojom::Role::kGenericContainer, "rect2");
    EXPECT_ACCESSIBILITY_HIT_TEST_RESULT(rect_2_point, expected_node, hit_node);
  }

  // Test a hit on a rect in the iframe.
  {
    gfx::Point rect_b_point(79, 79);
    BrowserAccessibility* hit_node = CallCachingAsyncHitTest(rect_b_point);
    BrowserAccessibility* expected_node =
        FindNode(ax::mojom::Role::kGenericContainer, "rectB");
    EXPECT_ACCESSIBILITY_HIT_TEST_RESULT(rect_b_point, expected_node, hit_node);
  }
}

IN_PROC_BROWSER_TEST_P(AccessibilityHitTestingBrowserTest,
                       HitTest_WithPinchZoom) {
  ASSERT_TRUE(embedded_test_server()->Start());

  EXPECT_TRUE(NavigateToURL(shell(), GURL(url::kAboutBlankURL)));

  AccessibilityNotificationWaiter waiter(shell()->web_contents(),
                                         ui::kAXModeComplete,
                                         ax::mojom::Event::kLoadComplete);

  GURL url(embedded_test_server()->GetURL(
      "/accessibility/hit_testing/simple_rectangles.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url));
  SynchronizeThreads();
  waiter.WaitForNotification();

  WaitForAccessibilityTreeToContainNodeWithName(shell()->web_contents(),
                                                "rectA");

  // Apply pinch zoom.
  SimulatePinchZoom(1.25f);

  // Test a hit on a rect in the main frame.
  {
    gfx::Point rect_2_point(49, 20);
    BrowserAccessibility* hit_node = HitTestAndWaitForResult(rect_2_point);
    BrowserAccessibility* expected_node =
        FindNode(ax::mojom::Role::kGenericContainer, "rect2");
    EXPECT_ACCESSIBILITY_HIT_TEST_RESULT(rect_2_point, expected_node, hit_node);

    // Try callback API.
    hit_node = AsyncHitTestAndWaitForCallback(rect_2_point);
    EXPECT_ACCESSIBILITY_HIT_TEST_RESULT(rect_2_point, expected_node, hit_node);
  }

  // Test a hit on a rect in the iframe.
  {
    gfx::Point rect_b_point(79, 79);
    BrowserAccessibility* hit_node = HitTestAndWaitForResult(rect_b_point);
    BrowserAccessibility* expected_node =
        FindNode(ax::mojom::Role::kGenericContainer, "rectB");
    EXPECT_ACCESSIBILITY_HIT_TEST_RESULT(rect_b_point, expected_node, hit_node);

    // Try callback API.
    hit_node = AsyncHitTestAndWaitForCallback(rect_b_point);
    EXPECT_ACCESSIBILITY_HIT_TEST_RESULT(rect_b_point, expected_node, hit_node);
  }
}

// Timeouts on Linux. TODO(crbug.com/1083805): Enable this test.
#if defined(OS_LINUX) || defined(OS_CHROMEOS)
#define MAYBE_CachingAsyncHitTestMissesElement_WithPinchZoom \
  DISABLED_CachingAsyncHitTestMissesElement_WithPinchZoom
#else
#define MAYBE_CachingAsyncHitTestMissesElement_WithPinchZoom \
  CachingAsyncHitTestMissesElement_WithPinchZoom
#endif
IN_PROC_BROWSER_TEST_P(AccessibilityHitTestingBrowserTest,
                       MAYBE_CachingAsyncHitTestMissesElement_WithPinchZoom) {
  ASSERT_TRUE(embedded_test_server()->Start());

  EXPECT_TRUE(NavigateToURL(shell(), GURL(url::kAboutBlankURL)));

  AccessibilityNotificationWaiter waiter(shell()->web_contents(),
                                         ui::kAXModeComplete,
                                         ax::mojom::Event::kLoadComplete);
  GURL url(embedded_test_server()->GetURL(
      "/accessibility/hit_testing/simple_rectangles_with_curtain.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url));
  waiter.WaitForNotification();

  WaitForAccessibilityTreeToContainNodeWithName(shell()->web_contents(),
                                                "rectA");

  // Apply pinch zoom.
  SimulatePinchZoom(1.25f);

  // For each point we try, the first time we call CachingAsyncHitTest it
  // should FAIL and return the wrong object, because this test page has
  // been designed to confound local synchronous hit testing using
  // z-indexes. However, calling CachingAsyncHitTest a second time should
  // return the correct result (since CallCachingAsyncHitTest waits for the
  // HOVER event to be received).

  // Test a hit on a rect in the main frame.
  {
    // First call should land on the wrong element.
    gfx::Point rect_2_point(49, 20);
    BrowserAccessibility* hit_node = CallCachingAsyncHitTest(rect_2_point);
    BrowserAccessibility* expected_node =
        FindNode(ax::mojom::Role::kGenericContainer, "rect2");
    EXPECT_NE(expected_node->GetName(), hit_node->GetName());

    // Call again and we should get the correct element.
    hit_node = CallCachingAsyncHitTest(rect_2_point);
    EXPECT_ACCESSIBILITY_HIT_TEST_RESULT(rect_2_point, expected_node, hit_node);
  }

  // Test a hit on a rect in the iframe.
  {
    // First call should land on the wrong element.
    gfx::Point rect_b_point(79, 79);
    BrowserAccessibility* hit_node = CallCachingAsyncHitTest(rect_b_point);
    BrowserAccessibility* expected_node =
        FindNode(ax::mojom::Role::kGenericContainer, "rectB");
    EXPECT_NE(expected_node->GetName(), hit_node->GetName());

    // Call again and we should get the correct element.
    hit_node = CallCachingAsyncHitTest(rect_b_point);
    EXPECT_ACCESSIBILITY_HIT_TEST_RESULT(rect_b_point, expected_node, hit_node);
  }
}

#endif  // !defined(OS_ANDROID) && !defined(OS_MAC)

// GetAXPlatformNode is currently only supported on windows and linux (excluding
// Chrome OS or Chromecast)
#if defined(OS_WIN) || \
    (defined(OS_LINUX) && !defined(OS_CHROMEOS) && !BUILDFLAG(IS_CHROMECAST))
IN_PROC_BROWSER_TEST_P(AccessibilityHitTestingBrowserTest,
                       NearestLeafInIframes) {
  ASSERT_TRUE(embedded_test_server()->Start());

  EXPECT_TRUE(NavigateToURL(shell(), GURL(url::kAboutBlankURL)));

  AccessibilityNotificationWaiter waiter(shell()->web_contents(),
                                         ui::kAXModeComplete,
                                         ax::mojom::Event::kLoadComplete);
  GURL url(embedded_test_server()->GetURL(
      "/accessibility/hit_testing/text_ranges.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url));
  waiter.WaitForNotification();

  WaitForAccessibilityTreeToContainNodeWithName(shell()->web_contents(),
                                                "rectA");

  // Test a hit on text in the main frame.
  {
    gfx::Point rect_2_point(70, 20);
    BrowserAccessibility* hit_node = CallNearestLeafNode(rect_2_point);
    BrowserAccessibility* expected_node =
        FindNode(ax::mojom::Role::kStaticText, "2");
    EXPECT_ACCESSIBILITY_HIT_TEST_RESULT(rect_2_point, expected_node, hit_node);
  }

  // Test a hit on text in the iframe.
  {
    gfx::Point rect_b_point(100, 100);
    BrowserAccessibility* hit_node = CallNearestLeafNode(rect_b_point);
    BrowserAccessibility* expected_node =
        FindNode(ax::mojom::Role::kStaticText, "B");
    EXPECT_ACCESSIBILITY_HIT_TEST_RESULT(rect_b_point, expected_node, hit_node);
  }
}
#endif
}  // namespace content
