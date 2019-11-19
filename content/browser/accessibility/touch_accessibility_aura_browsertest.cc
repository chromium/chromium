// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/macros.h"
#include "base/strings/string_number_conversions.h"
#include "content/browser/accessibility/browser_accessibility.h"
#include "content/browser/renderer_host/render_widget_host_view_child_frame.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/test/accessibility_notification_waiter.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/hit_test_region_observer.h"
#include "content/shell/browser/shell.h"
#include "content/test/content_browser_test_utils_internal.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/events/event.h"
#include "ui/events/event_sink.h"
#include "ui/events/event_utils.h"

namespace content {

class TouchAccessibilityBrowserTest : public ContentBrowserTest {
 public:
  TouchAccessibilityBrowserTest() {}

 protected:
  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    SetupCrossSiteRedirector(embedded_test_server());
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  void NavigateToUrlAndWaitForAccessibilityTree(const GURL& url) {
    AccessibilityNotificationWaiter waiter(shell()->web_contents(),
                                           ui::kAXModeComplete,
                                           ax::mojom::Event::kLoadComplete);
    EXPECT_TRUE(NavigateToURL(shell(), url));
    waiter.WaitForNotification();
  }

  void SendTouchExplorationEvent(int x, int y) {
    aura::Window* window = shell()->web_contents()->GetContentNativeView();
    ui::EventSink* sink = window->GetHost()->event_sink();
    gfx::Rect bounds = window->GetBoundsInRootWindow();
    gfx::Point location(bounds.x() + x, bounds.y() + y);
    int flags = ui::EF_TOUCH_ACCESSIBILITY;
    std::unique_ptr<ui::Event> mouse_move_event(
        new ui::MouseEvent(ui::ET_MOUSE_MOVED, location, location,
                           ui::EventTimeForNow(), flags, 0));
    ignore_result(sink->OnEventFromSource(mouse_move_event.get()));
  }

  DISALLOW_COPY_AND_ASSIGN(TouchAccessibilityBrowserTest);
};

IN_PROC_BROWSER_TEST_F(TouchAccessibilityBrowserTest,
                       TouchExplorationSendsHoverEvents) {
  // Create HTML with a 7 x 5 table, each exactly 50 x 50 pixels.
  std::string html_url =
      "data:text/html,"
      "<style>"
      "  body { margin: 0; }"
      "  table { border-spacing: 0; border-collapse: collapse; }"
      "  td { width: 50px; height: 50px; padding: 0; }"
      "</style>"
      "<body>"
      "<table>";
  int cell = 0;
  for (int row = 0; row < 5; ++row) {
    html_url += "<tr>";
    for (int col = 0; col < 7; ++col) {
      html_url += "<td>" + base::NumberToString(cell) + "</td>";
      ++cell;
    }
    html_url += "</tr>";
  }
  html_url += "</table></body>";

  NavigateToUrlAndWaitForAccessibilityTree(GURL(html_url));

  // Get the BrowserAccessibilityManager.
  WebContentsImpl* web_contents =
      static_cast<WebContentsImpl*>(shell()->web_contents());
  BrowserAccessibilityManager* manager =
      web_contents->GetRootBrowserAccessibilityManager();
  ASSERT_NE(nullptr, manager);

  // Loop over all of the cells in the table. For each one, send a simulated
  // touch exploration event in the center of that cell, and assert that we
  // get an accessibility hover event fired in the correct cell.
  AccessibilityNotificationWaiter waiter(
      shell()->web_contents(), ui::kAXModeComplete, ax::mojom::Event::kHover);
  for (int row = 0; row < 5; ++row) {
    for (int col = 0; col < 7; ++col) {
      std::string expected_cell_text = base::NumberToString(row * 7 + col);
      VLOG(1) << "Sending event in row " << row << " col " << col
              << " with text " << expected_cell_text;
      SendTouchExplorationEvent(50 * col + 25, 50 * row + 25);

      // Wait until we get a hover event in the expected grid cell.
      // Tolerate additional events, keep looping until we get the expected one.
      std::string cell_text;
      do {
        waiter.WaitForNotification();
        int target_id = waiter.event_target_id();
        BrowserAccessibility* hit = manager->GetFromID(target_id);
        BrowserAccessibility* child = hit->PlatformGetChild(0);
        ASSERT_NE(nullptr, child);
        cell_text = child->GetData().GetStringAttribute(
            ax::mojom::StringAttribute::kName);
        VLOG(1) << "Got hover event in cell with text: " << cell_text;
      } while (cell_text != expected_cell_text);
    }
  }
}

IN_PROC_BROWSER_TEST_F(TouchAccessibilityBrowserTest,
                       TouchExplorationInIframe) {
  NavigateToUrlAndWaitForAccessibilityTree(embedded_test_server()->GetURL(
      "/accessibility/html/iframe-coordinates.html"));

  WaitForAccessibilityTreeToContainNodeWithName(shell()->web_contents(),
                                                "Ordinary Button");

  // Get the BrowserAccessibilityManager for the first child frame.
  RenderFrameHostImpl* main_frame = static_cast<RenderFrameHostImpl*>(
      shell()->web_contents()->GetMainFrame());
  RenderFrameHostImpl* child_frame =
      main_frame->frame_tree_node()->child_at(0)->current_frame_host();
  BrowserAccessibilityManager* child_manager =
      child_frame->GetOrCreateBrowserAccessibilityManager();
  ASSERT_NE(nullptr, child_manager);

  // Send a touch exploration event to the button in the first iframe.
  // A touch exploration event is just a mouse move event with
  // the ui::EF_TOUCH_ACCESSIBILITY flag set.
  AccessibilityNotificationWaiter waiter(shell()->web_contents(), ui::AXMode(),
                                         ax::mojom::Event::kHover);
  SendTouchExplorationEvent(50, 350);
  waiter.WaitForNotification();
  int target_id = waiter.event_target_id();
  BrowserAccessibility* hit = child_manager->GetFromID(target_id);
  EXPECT_EQ(ax::mojom::Role::kButton, hit->GetData().role);
  std::string text =
      hit->GetData().GetStringAttribute(ax::mojom::StringAttribute::kName);
  EXPECT_EQ("Ordinary Button", text);
}

IN_PROC_BROWSER_TEST_F(TouchAccessibilityBrowserTest,
                       TouchExplorationInCrossSiteIframe) {
  NavigateToUrlAndWaitForAccessibilityTree(embedded_test_server()->GetURL(
      "/accessibility/html/iframe-coordinates-cross-process.html"));

  WaitForAccessibilityTreeToContainNodeWithName(shell()->web_contents(),
                                                "Ordinary Button");

  // Get the BrowserAccessibilityManager for the first child frame.
  RenderFrameHostImpl* main_frame = static_cast<RenderFrameHostImpl*>(
      shell()->web_contents()->GetMainFrame());
  RenderFrameHostImpl* child_frame =
      main_frame->frame_tree_node()->child_at(0)->current_frame_host();
  BrowserAccessibilityManager* child_manager =
      child_frame->GetOrCreateBrowserAccessibilityManager();
  ASSERT_NE(nullptr, child_manager);

  // If OOPIFs are enabled, wait until hit testing data is ready, otherwise the
  // touch event will not get sent to the correct renderer process. However the
  // |child_frame| being used here is not actually a
  // RenderWidgetHostViewChildFrame.
  WaitForHitTestData(child_frame);

  // Send a touch exploration event to the button in the first iframe.
  // A touch exploration event is just a mouse move event with
  // the ui::EF_TOUCH_ACCESSIBILITY flag set.
  AccessibilityNotificationWaiter waiter(shell()->web_contents(), ui::AXMode(),
                                         ax::mojom::Event::kHover);
  SendTouchExplorationEvent(50, 350);
  waiter.WaitForNotification();
  int target_id = waiter.event_target_id();
  BrowserAccessibility* hit = child_manager->GetFromID(target_id);
  EXPECT_EQ(ax::mojom::Role::kButton, hit->GetData().role);
  std::string text =
      hit->GetData().GetStringAttribute(ax::mojom::StringAttribute::kName);
  EXPECT_EQ("Ordinary Button", text);
}

}  // namespace content
