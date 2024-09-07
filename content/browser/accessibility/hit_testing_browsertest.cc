// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/accessibility/hit_testing_browsertest.h"

#include "base/check.h"
#include "base/command_line.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "build/build_config.h"
#include "build/chromecast_buildflags.h"
#include "build/chromeos_buildflags.h"
#include "content/browser/accessibility/accessibility_tree_formatter_blink.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/ax_inspect_factory.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/accessibility_notification_waiter.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "content/shell/browser/shell.h"
#include "content/test/content_browser_test_utils_internal.h"
#include "net/dns/mock_host_resolver.h"
#include "ui/accessibility/ax_clipping_behavior.h"
#include "ui/accessibility/ax_coordinate_system.h"
#include "ui/accessibility/ax_node_id_forward.h"
#include "ui/accessibility/platform/ax_platform_node_base.h"
#include "ui/accessibility/platform/ax_platform_tree_manager.h"
#include "ui/accessibility/platform/browser_accessibility.h"
#include "ui/accessibility/platform/browser_accessibility_manager.h"
#include "ui/display/display_switches.h"
#include "ui/gfx/geometry/vector2d_conversions.h"

namespace content {

using ui::AXTreeFormatter;

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
  auto device_scale_factor = GetParam();
  command_line->AppendSwitchASCII(
      switches::kForceDeviceScaleFactor,
      base::StringPrintf("%.2f", device_scale_factor));
}

std::string AccessibilityHitTestingBrowserTest::TestPassToString::operator()(
    const ::testing::TestParamInfo<double>& info) const {
  auto device_scale_factor = info.param;
  std::string name = base::StringPrintf("ZoomFactor%g", device_scale_factor);

  // The test harness only allows alphanumeric characters and underscores
  // in param names.
  std::string sanitized_name;
  base::ReplaceChars(name, ".", "_", &sanitized_name);
  return sanitized_name;
}

ui::BrowserAccessibilityManager*
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
// Device scale factor gets applied going from
// CSS to page pixels, i.e. before view offset.
gfx::Point AccessibilityHitTestingBrowserTest::CSSToFramePoint(
    gfx::Point css_point) {
  gfx::Point page_point;
  page_point = ScaleToRoundedPoint(css_point, GetDeviceScaleFactor());

  gfx::Point frame_point = page_point - scroll_offset_.OffsetFromOrigin();
  return frame_point;
}

gfx::Point AccessibilityHitTestingBrowserTest::FrameToCSSPoint(
    gfx::Point frame_point) {
  gfx::Point page_point = frame_point + scroll_offset_.OffsetFromOrigin();

  gfx::Point css_point;
  css_point = ScaleToRoundedPoint(page_point, 1.0 / GetDeviceScaleFactor());
  return css_point;
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
  physical_pixel_point = screen_point;

  return physical_pixel_point;
}

ui::BrowserAccessibility*
AccessibilityHitTestingBrowserTest::HitTestAndWaitForResultWithEvent(
    const gfx::Point& point,
    ax::mojom::Event event_to_fire) {
  ui::BrowserAccessibilityManager* manager =
      GetRootBrowserAccessibilityManager();

  AccessibilityNotificationWaiter event_waiter(
      shell()->web_contents(), ui::kAXModeComplete, event_to_fire);
  ui::AXActionData action_data;
  action_data.action = ax::mojom::Action::kHitTest;
  action_data.target_point = CSSToFramePoint(point);
  action_data.hit_test_event_to_fire = event_to_fire;
  manager->delegate()->AccessibilityHitTest(CSSToFramePoint(point),
                                            event_to_fire, 0, {});
  EXPECT_TRUE(event_waiter.WaitForNotification());

  ui::BrowserAccessibilityManager* target_manager =
      event_waiter.event_browser_accessibility_manager();
  int event_target_id = event_waiter.event_target_id();
  ui::BrowserAccessibility* hit_node =
      target_manager->GetFromID(event_target_id);
  return hit_node;
}

ui::BrowserAccessibility*
AccessibilityHitTestingBrowserTest::HitTestAndWaitForResult(
    const gfx::Point& point) {
  return HitTestAndWaitForResultWithEvent(point, ax::mojom::Event::kHover);
}

ui::BrowserAccessibility*
AccessibilityHitTestingBrowserTest::AsyncHitTestAndWaitForCallback(
    const gfx::Point& point) {
  ui::BrowserAccessibilityManager* manager =
      GetRootBrowserAccessibilityManager();

  gfx::Point target_point = CSSToFramePoint(point);
  base::RunLoop run_loop;
  ui::AXPlatformTreeManager* hit_manager = nullptr;
  ui::AXNodeID hit_node_id = ui::kInvalidAXNodeID;

  auto callback = [&](ui::AXPlatformTreeManager* manager,
                      ui::AXNodeID node_id) {
    hit_manager = manager;
    hit_node_id = node_id;
    run_loop.QuitClosure().Run();
  };
  manager->delegate()->AccessibilityHitTest(
      target_point, ax::mojom::Event::kNone, 0,
      base::BindLambdaForTesting(callback));
  run_loop.Run();

  ui::BrowserAccessibility* hit_node =
      static_cast<ui::BrowserAccessibilityManager*>(hit_manager)
          ->GetFromID(hit_node_id);
  return hit_node;
}

ui::BrowserAccessibility*
AccessibilityHitTestingBrowserTest::CallCachingAsyncHitTest(
    const gfx::Point& page_point) {
  gfx::Point screen_point = CSSToPhysicalPixelPoint(page_point);

  // Each call to CachingAsyncHitTest results in at least one HOVER
  // event received. Block until we receive it. CachingAsyncHitTestNearestLeaf
  // will call CachingAsyncHitTest.
  AccessibilityNotificationWaiter hover_waiter(
      shell()->web_contents(), ui::kAXModeComplete, ax::mojom::Event::kHover);

  ui::BrowserAccessibility* result =
      GetRootBrowserAccessibilityManager()->CachingAsyncHitTest(screen_point);

  EXPECT_TRUE(hover_waiter.WaitForNotification());
  return result;
}

ui::BrowserAccessibility*
AccessibilityHitTestingBrowserTest::CallNearestLeafNode(
    const gfx::Point& page_point) {
  gfx::Point screen_point = CSSToPhysicalPixelPoint(page_point);
  ui::BrowserAccessibilityManager* manager =
      static_cast<WebContentsImpl*>(shell()->web_contents())
          ->GetRootBrowserAccessibilityManager();

  // Each call to CachingAsyncHitTest results in at least one HOVER
  // event received. Block until we receive it. CachingAsyncHitTest
  // will call CachingAsyncHitTest.
  AccessibilityNotificationWaiter hover_waiter(
      shell()->web_contents(), ui::kAXModeComplete, ax::mojom::Event::kHover);
  ui::AXPlatformNodeBase* platform_node = nullptr;
  if (manager->GetBrowserAccessibilityRoot()->GetAXPlatformNode()) {
    platform_node =
        static_cast<ui::AXPlatformNodeBase*>(
            manager->GetBrowserAccessibilityRoot()->GetAXPlatformNode())
            ->NearestLeafToPoint(screen_point);
  }
  EXPECT_TRUE(hover_waiter.WaitForNotification());
  if (platform_node) {
    return ui::BrowserAccessibility::FromAXPlatformNodeDelegate(
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
  if (render_frame_metadata.root_scroll_offset) {
    scroll_offset_ =
        gfx::ToRoundedPoint(render_frame_metadata.root_scroll_offset.value());
  } else {
    scroll_offset_ = gfx::Point();
  }

  // Ensure we get an accessibility update reflecting the new scale factor.
  // TODO(crbug.com/40844856): Investigate why this does not return true.
  ASSERT_TRUE(accessibility_waiter.WaitForNotification());
}

std::string
AccessibilityHitTestingBrowserTest::FormatHitTestAccessibilityTree() {
  std::unique_ptr<AXTreeFormatter> accessibility_tree_formatter =
      AXInspectFactory::CreateBlinkFormatter();
  accessibility_tree_formatter->set_show_ids(true);
  accessibility_tree_formatter->SetPropertyFilters(
      {{"name=*", ui::AXPropertyFilter::ALLOW},
       {"location=*", ui::AXPropertyFilter::ALLOW},
       {"size=*", ui::AXPropertyFilter::ALLOW}});
  std::string accessibility_tree;
  return accessibility_tree_formatter->Format(GetRootAndAssertNonNull());
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
    ::testing::Values(1, 2),
    AccessibilityHitTestingBrowserTest::TestPassToString());

INSTANTIATE_TEST_SUITE_P(
    All,
    AccessibilityHitTestingCrossProcessBrowserTest,
    ::testing::Values(1, 2),
    AccessibilityHitTestingBrowserTest::TestPassToString());

#if defined(THREAD_SANITIZER)
// TODO(crbug.com/40775546): Times out flakily on TSAN builds.
#define MAYBE_CachingAsyncHitTest DISABLED_CachingAsyncHitTest
#else
#define MAYBE_CachingAsyncHitTest CachingAsyncHitTest
#endif
IN_PROC_BROWSER_TEST_P(AccessibilityHitTestingBrowserTest,
                       MAYBE_CachingAsyncHitTest) {
  ASSERT_TRUE(embedded_test_server()->Start());

  EXPECT_TRUE(NavigateToURL(shell(), GURL(url::kAboutBlankURL)));

  AccessibilityNotificationWaiter waiter(shell()->web_contents(),
                                         ui::kAXModeComplete,
                                         ax::mojom::Event::kLoadComplete);
  GURL url(embedded_test_server()->GetURL(
      "/accessibility/hit_testing/simple_rectangles.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url));
  ASSERT_TRUE(waiter.WaitForNotification());

  WaitForAccessibilityTreeToContainNodeWithName(shell()->web_contents(),
                                                "rectA");

  // Test a hit on a rect in the main frame.
  {
    gfx::Point rect_2_point(49, 20);
    ui::BrowserAccessibility* hit_node = CallCachingAsyncHitTest(rect_2_point);
    ui::BrowserAccessibility* expected_node =
        FindNode(ax::mojom::Role::kGenericContainer, "rect2");
    EXPECT_ACCESSIBILITY_HIT_TEST_RESULT(rect_2_point, expected_node, hit_node);
  }

  // Test a hit on a rect in the iframe.
  {
    gfx::Point rect_b_point(79, 79);
    ui::BrowserAccessibility* hit_node = CallCachingAsyncHitTest(rect_b_point);
    ui::BrowserAccessibility* expected_node =
        FindNode(ax::mojom::Role::kGenericContainer, "rectB");
    EXPECT_ACCESSIBILITY_HIT_TEST_RESULT(rect_b_point, expected_node, hit_node);
  }
}

#if defined(THREAD_SANITIZER)
// TODO(crbug.com/40775516): Times out flakily on TSAN builds.
#define MAYBE_HitTest DISABLED_HitTest
#else
#define MAYBE_HitTest HitTest
#endif
IN_PROC_BROWSER_TEST_P(AccessibilityHitTestingBrowserTest, MAYBE_HitTest) {
  ASSERT_TRUE(embedded_test_server()->Start());

  EXPECT_TRUE(NavigateToURL(shell(), GURL(url::kAboutBlankURL)));

  AccessibilityNotificationWaiter waiter(shell()->web_contents(),
                                         ui::kAXModeComplete,
                                         ax::mojom::Event::kLoadComplete);
  GURL url(embedded_test_server()->GetURL(
      "/accessibility/hit_testing/simple_rectangles.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url));
  ASSERT_TRUE(waiter.WaitForNotification());

  WaitForAccessibilityTreeToContainNodeWithName(shell()->web_contents(),
                                                "rectA");

  // Test a hit on a rect in the main frame.
  {
    gfx::Point rect_2_point(49, 20);
    ui::BrowserAccessibility* hit_node = HitTestAndWaitForResult(rect_2_point);
    ui::BrowserAccessibility* expected_node =
        FindNode(ax::mojom::Role::kGenericContainer, "rect2");
    EXPECT_ACCESSIBILITY_HIT_TEST_RESULT(rect_2_point, expected_node, hit_node);

    // Try callback API.
    hit_node = AsyncHitTestAndWaitForCallback(rect_2_point);
    EXPECT_ACCESSIBILITY_HIT_TEST_RESULT(rect_2_point, expected_node, hit_node);
  }

  // Test a hit on a rect in the iframe.
  {
    gfx::Point rect_b_point(79, 79);
    ui::BrowserAccessibility* hit_node = HitTestAndWaitForResult(rect_b_point);
    ui::BrowserAccessibility* expected_node =
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

// Web popups don't exist on Android, so this test doesn't have to be run on
// this platform.
#if !BUILDFLAG(IS_ANDROID)

// crbug.com/1317505: Flaky on Lacros and Linux Wayland
#if BUILDFLAG(IS_CHROMEOS_LACROS) || BUILDFLAG(IS_LINUX)
#define MAYBE_HitTestInPopup DISABLED_HitTestInPopup
#else
#define MAYBE_HitTestInPopup HitTestInPopup
#endif
IN_PROC_BROWSER_TEST_P(AccessibilityHitTestingBrowserTest,
                       MAYBE_HitTestInPopup) {
  ASSERT_TRUE(embedded_test_server()->Start());

  EXPECT_TRUE(NavigateToURL(shell(), GURL(url::kAboutBlankURL)));

  AccessibilityNotificationWaiter waiter(shell()->web_contents(),
                                         ui::kAXModeComplete,
                                         ax::mojom::Event::kLoadComplete);
  GURL url(embedded_test_server()->GetURL(
      "/accessibility/hit_testing/input-color-with-popup-open.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url));
  ASSERT_TRUE(waiter.WaitForNotification());

  WaitForAccessibilityTreeToContainNodeWithName(shell()->web_contents(),
                                                "color picker");

  AccessibilityNotificationWaiter click_waiter(
      shell()->web_contents(), ui::kAXModeComplete, ax::mojom::Event::kClicked);
  auto* input = FindNode(ax::mojom::Role::kColorWell, "color picker");
  ASSERT_TRUE(input);
  ui::AXActionData action_data;
  action_data.action = ax::mojom::Action::kDoDefault;
  input->AccessibilityPerformAction(action_data);

  ASSERT_TRUE(click_waiter.WaitForNotification());

  auto* popup_root = GetRootBrowserAccessibilityManager()->GetPopupRoot();
  ASSERT_NE(nullptr, popup_root);

  auto* format_toggler =
      FindNode(ax::mojom::Role::kSpinButton, "Format toggler");
  ASSERT_TRUE(format_toggler);

  gfx::Rect bounds = format_toggler->GetBoundsRect(
      ui::AXCoordinateSystem::kFrame, ui::AXClippingBehavior::kUnclipped);
  auto* hit_node =
      HitTestAndWaitForResult(FrameToCSSPoint(bounds.CenterPoint()));

  ASSERT_EQ(hit_node, format_toggler);
}
#endif

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
  ASSERT_TRUE(waiter.WaitForNotification());

  gfx::Point out_of_bounds_point(-1, -1);

  ui::BrowserAccessibility* hit_node =
      HitTestAndWaitForResult(out_of_bounds_point);
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
  ASSERT_TRUE(waiter.WaitForNotification());
  WaitForAccessibilityTreeToContainNodeWithName(shell()->web_contents(),
                                                "rectA");

  auto* web_contents = static_cast<WebContentsImpl*>(shell()->web_contents());
  FrameTreeNode* root = web_contents->GetPrimaryFrameTree().root();
  ASSERT_EQ(1U, root->child_count());

  FrameTreeNode* child = root->child_at(0);
  EXPECT_TRUE(NavigateToURLFromRenderer(child, url_b));
  EXPECT_EQ(url_b, child->current_url());
  WaitForAccessibilityTreeToContainNodeWithName(shell()->web_contents(),
                                                "rectF");

  EXPECT_EQ(
      " Site A ------------ proxies for B\n"
      "   +--Site B ------- proxies for A\n"
      "Where A = http://a.com/\n"
      "      B = http://b.com/",
      DepictFrameTree(*root));

  // Before scrolling.
  {
    gfx::Point rect_b_point(79, 79);
    ui::BrowserAccessibility* hit_node = HitTestAndWaitForResult(rect_b_point);
    ui::BrowserAccessibility* expected_node =
        FindNode(ax::mojom::Role::kGenericContainer, "rectB");
    EXPECT_ACCESSIBILITY_HIT_TEST_RESULT(rect_b_point, expected_node, hit_node);

    // Try callback API.
    hit_node = AsyncHitTestAndWaitForCallback(rect_b_point);
    EXPECT_ACCESSIBILITY_HIT_TEST_RESULT(rect_b_point, expected_node, hit_node);
  }

  // Scroll div up 100px.
  int scroll_delta = 100;
  std::string scroll_string = base::StringPrintf(
      "window.scrollTo(0, %d); window.scrollY;", scroll_delta);
  EXPECT_NEAR(
      EvalJs(child->current_frame_host(), scroll_string).ExtractDouble(),
      static_cast<double>(scroll_delta), 1.0);

  // After scrolling.
  {
    gfx::Point rect_g_point(79, 89);
    ui::BrowserAccessibility* hit_node = HitTestAndWaitForResult(rect_g_point);
    ui::BrowserAccessibility* expected_node =
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
  ASSERT_TRUE(waiter.WaitForNotification());

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
    ui::BrowserAccessibility* hit_node = CallCachingAsyncHitTest(rect_2_point);
    ui::BrowserAccessibility* expected_node =
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
    ui::BrowserAccessibility* hit_node = CallCachingAsyncHitTest(rect_b_point);
    ui::BrowserAccessibility* expected_node =
        FindNode(ax::mojom::Role::kGenericContainer, "rectB");
    EXPECT_NE(expected_node->GetName(), hit_node->GetName());

    // Call again and we should get the correct element.
    hit_node = CallCachingAsyncHitTest(rect_b_point);
    EXPECT_ACCESSIBILITY_HIT_TEST_RESULT(rect_b_point, expected_node, hit_node);
  }
}

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_MAC)
// Fails flakily with compared ID differences. TODO(crbug.com/40715277):
// Re-enable this test.
IN_PROC_BROWSER_TEST_P(AccessibilityHitTestingBrowserTest,
                       DISABLED_CachingAsyncHitTest_WithPinchZoom) {
  ASSERT_TRUE(embedded_test_server()->Start());

  EXPECT_TRUE(NavigateToURL(shell(), GURL(url::kAboutBlankURL)));

  AccessibilityNotificationWaiter waiter(shell()->web_contents(),
                                         ui::kAXModeComplete,
                                         ax::mojom::Event::kLoadComplete);

  GURL url(embedded_test_server()->GetURL(
      "/accessibility/hit_testing/simple_rectangles.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url));
  SynchronizeThreads();
  // TODO(crbug.com/40844856): Investigate why this does not return
  // true.
  ASSERT_TRUE(waiter.WaitForNotification());

  WaitForAccessibilityTreeToContainNodeWithName(shell()->web_contents(),
                                                "rectA");

  // Apply pinch zoom.
  SimulatePinchZoom(1.25f);

  // Test a hit on a rect in the main frame.
  {
    gfx::Point rect_2_point(49, 20);
    ui::BrowserAccessibility* hit_node = CallCachingAsyncHitTest(rect_2_point);
    ui::BrowserAccessibility* expected_node =
        FindNode(ax::mojom::Role::kGenericContainer, "rect2");
    EXPECT_ACCESSIBILITY_HIT_TEST_RESULT(rect_2_point, expected_node, hit_node);
  }

  // Test a hit on a rect in the iframe.
  {
    gfx::Point rect_b_point(79, 79);
    ui::BrowserAccessibility* hit_node = CallCachingAsyncHitTest(rect_b_point);
    ui::BrowserAccessibility* expected_node =
        FindNode(ax::mojom::Role::kGenericContainer, "rectB");
    EXPECT_ACCESSIBILITY_HIT_TEST_RESULT(rect_b_point, expected_node, hit_node);
  }
}

// TODO(crbug.com/40775545): Times out flakily on TSAN builds.
// TODO(crbug.com/40919503): Times out flakily on ASan builds.
// TODO(crbug.com/40921699): Times out flakily on win-asan.
IN_PROC_BROWSER_TEST_P(AccessibilityHitTestingBrowserTest,
                       DISABLED_HitTest_WithPinchZoom) {
  ASSERT_TRUE(embedded_test_server()->Start());

  EXPECT_TRUE(NavigateToURL(shell(), GURL(url::kAboutBlankURL)));

  AccessibilityNotificationWaiter waiter(shell()->web_contents(),
                                         ui::kAXModeComplete,
                                         ax::mojom::Event::kLoadComplete);

  GURL url(embedded_test_server()->GetURL(
      "/accessibility/hit_testing/simple_rectangles.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url));
  SynchronizeThreads();
  // TODO(crbug.com/40844856): Investigate why this does not return
  // true.
  ASSERT_TRUE(waiter.WaitForNotification());

  WaitForAccessibilityTreeToContainNodeWithName(shell()->web_contents(),
                                                "rectA");

  // Apply pinch zoom.
  SimulatePinchZoom(1.25f);

  // Test a hit on a rect in the main frame.
  {
    gfx::Point rect_2_point(49, 20);
    ui::BrowserAccessibility* hit_node = HitTestAndWaitForResult(rect_2_point);
    ui::BrowserAccessibility* expected_node =
        FindNode(ax::mojom::Role::kGenericContainer, "rect2");
    EXPECT_ACCESSIBILITY_HIT_TEST_RESULT(rect_2_point, expected_node, hit_node);

    // Try callback API.
    hit_node = AsyncHitTestAndWaitForCallback(rect_2_point);
    EXPECT_ACCESSIBILITY_HIT_TEST_RESULT(rect_2_point, expected_node, hit_node);
  }

  // Test a hit on a rect in the iframe.
  {
    gfx::Point rect_b_point(79, 79);
    ui::BrowserAccessibility* hit_node = HitTestAndWaitForResult(rect_b_point);
    ui::BrowserAccessibility* expected_node =
        FindNode(ax::mojom::Role::kGenericContainer, "rectB");
    EXPECT_ACCESSIBILITY_HIT_TEST_RESULT(rect_b_point, expected_node, hit_node);

    // Try callback API.
    hit_node = AsyncHitTestAndWaitForCallback(rect_b_point);
    EXPECT_ACCESSIBILITY_HIT_TEST_RESULT(rect_b_point, expected_node, hit_node);
  }
}

// Timeouts on Linux. TODO(crbug.com/40692703): Enable this test.
IN_PROC_BROWSER_TEST_P(
    AccessibilityHitTestingBrowserTest,
    DISABLED_CachingAsyncHitTestMissesElement_WithPinchZoom) {
  ASSERT_TRUE(embedded_test_server()->Start());

  EXPECT_TRUE(NavigateToURL(shell(), GURL(url::kAboutBlankURL)));

  AccessibilityNotificationWaiter waiter(shell()->web_contents(),
                                         ui::kAXModeComplete,
                                         ax::mojom::Event::kLoadComplete);
  GURL url(embedded_test_server()->GetURL(
      "/accessibility/hit_testing/simple_rectangles_with_curtain.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url));
  ASSERT_TRUE(waiter.WaitForNotification());

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
    ui::BrowserAccessibility* hit_node = CallCachingAsyncHitTest(rect_2_point);
    ui::BrowserAccessibility* expected_node =
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
    ui::BrowserAccessibility* hit_node = CallCachingAsyncHitTest(rect_b_point);
    ui::BrowserAccessibility* expected_node =
        FindNode(ax::mojom::Role::kGenericContainer, "rectB");
    EXPECT_NE(expected_node->GetName(), hit_node->GetName());

    // Call again and we should get the correct element.
    hit_node = CallCachingAsyncHitTest(rect_b_point);
    EXPECT_ACCESSIBILITY_HIT_TEST_RESULT(rect_b_point, expected_node, hit_node);
  }
}

#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_MAC)

// GetAXPlatformNode is currently only supported on windows and linux (excluding
// Chrome OS or Chromecast)
#if BUILDFLAG(IS_WIN) || (BUILDFLAG(IS_LINUX) && !BUILDFLAG(IS_CASTOS))
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
  ASSERT_TRUE(waiter.WaitForNotification());

  WaitForAccessibilityTreeToContainNodeWithName(shell()->web_contents(),
                                                "rectA");

  // Test a hit on text in the main frame.
  {
    gfx::Point rect_2_point(70, 20);
    ui::BrowserAccessibility* hit_node = CallNearestLeafNode(rect_2_point);
    ui::BrowserAccessibility* expected_node =
        FindNode(ax::mojom::Role::kStaticText, "2");
    EXPECT_ACCESSIBILITY_HIT_TEST_RESULT(rect_2_point, expected_node, hit_node);
  }

  // Test a hit on text in the iframe.
  {
    gfx::Point rect_b_point(100, 100);
    ui::BrowserAccessibility* hit_node = CallNearestLeafNode(rect_b_point);
    ui::BrowserAccessibility* expected_node =
        FindNode(ax::mojom::Role::kStaticText, "B");
    EXPECT_ACCESSIBILITY_HIT_TEST_RESULT(rect_b_point, expected_node, hit_node);
  }
}
#endif

}  // namespace content
