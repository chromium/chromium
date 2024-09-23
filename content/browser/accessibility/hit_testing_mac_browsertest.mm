// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/accessibility/hit_testing_browsertest.h"
#include "content/public/test/accessibility_notification_waiter.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "ui/accessibility/platform/browser_accessibility_cocoa.h"
#include "ui/accessibility/platform/browser_accessibility_mac.h"
#include "ui/gfx/mac/coordinate_conversion.h"

namespace content {

#define EXPECT_ACCESSIBILITY_MAC_HIT_TEST_RESULT(css_point, expected_element, \
                                                 hit_element)                 \
  SCOPED_TRACE(GetScopedTrace(css_point));                                    \
  EXPECT_EQ([expected_element owner]->GetId(), [hit_element owner]->GetId());

class AccessibilityHitTestingMacBrowserTest
    : public AccessibilityHitTestingBrowserTest {
 public:
  BrowserAccessibilityCocoa* GetWebContentRoot() {
    return GetRootBrowserAccessibilityManager()
        ->GetBrowserAccessibilityRoot()
        ->GetNativeViewAccessible();
  }
};

INSTANTIATE_TEST_SUITE_P(
    All,
    AccessibilityHitTestingMacBrowserTest,
    ::testing::Values(1, 2),
    AccessibilityHitTestingBrowserTest::TestPassToString());

IN_PROC_BROWSER_TEST_P(AccessibilityHitTestingMacBrowserTest,
                       AccessibilityHitTest) {
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

  BrowserAccessibilityCocoa* root = GetWebContentRoot();

  // Test a hit on a rect in the main frame.
  {
    // Hit testing in mac expects a point in DIPs space.
    gfx::Point rect_2_point(49, 20);
    BrowserAccessibilityCocoa* hit_element = [root
        accessibilityHitTest:NSMakePoint(rect_2_point.x(), rect_2_point.y())];
    BrowserAccessibilityCocoa* expected_element =
        FindNode(ax::mojom::Role::kGenericContainer, "rect2")
            ->GetNativeViewAccessible();
    EXPECT_ACCESSIBILITY_MAC_HIT_TEST_RESULT(rect_2_point, expected_element,
                                             hit_element);
  }

  // Test a hit on a rect in the iframe.
  {
    gfx::Point rect_b_point(79, 79);
    BrowserAccessibilityCocoa* hit_element = [root
        accessibilityHitTest:NSMakePoint(rect_b_point.x(), rect_b_point.y())];
    BrowserAccessibilityCocoa* expected_element =
        FindNode(ax::mojom::Role::kGenericContainer, "rectB")
            ->GetNativeViewAccessible();
    EXPECT_ACCESSIBILITY_MAC_HIT_TEST_RESULT(rect_b_point, expected_element,
                                             hit_element);
  }
}

}
