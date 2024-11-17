// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/test/accessibility_notification_waiter.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/platform/browser_accessibility.h"
#include "ui/accessibility/platform/browser_accessibility_manager.h"

namespace content {

class AccessibilityLineLayoutBrowserTest : public ContentBrowserTest {
 public:
  AccessibilityLineLayoutBrowserTest() = default;
  ~AccessibilityLineLayoutBrowserTest() override = default;

 protected:
  ui::BrowserAccessibility* FindButton(ui::BrowserAccessibility* node) {
    if (node->GetRole() == ax::mojom::Role::kButton) {
      return node;
    }
    for (unsigned i = 0; i < node->PlatformChildCount(); i++) {
      if (ui::BrowserAccessibility* button =
              FindButton(node->PlatformGetChild(i))) {
        return button;
      }
    }
    return nullptr;
  }

  int CountNextPreviousOnLineLinks(ui::BrowserAccessibility* node,
                                   bool do_not_count_inline_text) {
    int line_link_count = 0;

    if (do_not_count_inline_text &&
        node->GetRole() == ax::mojom::Role::kInlineTextBox) {
      int next_on_line_id =
          node->GetIntAttribute(ax::mojom::IntAttribute::kNextOnLineId);
      if (next_on_line_id != ui::kInvalidAXNodeID) {
        ui::BrowserAccessibility* other =
            node->manager()->GetFromID(next_on_line_id);
        EXPECT_NE(nullptr, other) << "Next on line link is invalid.";
        line_link_count++;
      }
      int previous_on_line_id =
          node->GetIntAttribute(ax::mojom::IntAttribute::kPreviousOnLineId);
      if (previous_on_line_id != ui::kInvalidAXNodeID) {
        ui::BrowserAccessibility* other =
            node->manager()->GetFromID(previous_on_line_id);
        EXPECT_NE(nullptr, other) << "Previous on line link is invalid.";
        line_link_count++;
      }
    }

    for (auto it = node->InternalChildrenBegin();
         it != node->InternalChildrenEnd(); ++it) {
      line_link_count +=
          CountNextPreviousOnLineLinks(it.get(), do_not_count_inline_text);
    }

    return line_link_count;
  }
};

// http://crbug.com/868830 - the patch that enabled this test to pass
// caused a performance regression.
IN_PROC_BROWSER_TEST_F(AccessibilityLineLayoutBrowserTest,
                       DISABLED_WholeBlockIsUpdated) {
  ASSERT_TRUE(embedded_test_server()->Start());

  AccessibilityNotificationWaiter waiter(shell()->web_contents(),
                                         ui::kAXModeComplete,
                                         ax::mojom::Event::kLoadComplete);
  GURL url(embedded_test_server()->GetURL("/accessibility/lines/lines.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url));
  ASSERT_TRUE(waiter.WaitForNotification());

  WebContentsImpl* web_contents =
      static_cast<WebContentsImpl*>(shell()->web_contents());
  ui::BrowserAccessibilityManager* manager =
      web_contents->GetRootBrowserAccessibilityManager();

  // There should be at least 2 links between nodes on the same line.
  int line_link_count = CountNextPreviousOnLineLinks(
      manager->GetBrowserAccessibilityRoot(), false);
  ASSERT_GE(line_link_count, 2);

  // Find the button and click it.
  ui::BrowserAccessibility* button =
      FindButton(manager->GetBrowserAccessibilityRoot());
  ASSERT_NE(nullptr, button);
  manager->DoDefaultAction(*button);

  // When done the page will change the button text to "Done".
  WaitForAccessibilityTreeToContainNodeWithName(web_contents, "Done");

  // There should be at least 2 links between nodes on the same line,
  // though not necessarily the same as before.
  line_link_count = CountNextPreviousOnLineLinks(
      manager->GetBrowserAccessibilityRoot(), false);
  ASSERT_GE(line_link_count, 2);
}

// http://crbug.com/868830 - the patch that enabled this test to pass caused a
// performance regression.  (Android doesn't generate InlineTextBoxes
// immediately; we can wait for them but without the aforementioned fix the
// updated tree isn't processed to create the Next/PreviousOnLine links.)
#if !BUILDFLAG(IS_ANDROID)
IN_PROC_BROWSER_TEST_F(AccessibilityLineLayoutBrowserTest,
                       NestedLayoutNGInlineFormattingContext) {
  ASSERT_TRUE(embedded_test_server()->Start());

  AccessibilityNotificationWaiter waiter(shell()->web_contents(),
                                         ui::kAXModeComplete,
                                         ax::mojom::Event::kLoadComplete);
  GURL url(embedded_test_server()->GetURL(
      "/accessibility/lines/lines-inline-nested.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url));
  ASSERT_TRUE(waiter.WaitForNotification());

  WebContentsImpl* web_contents =
      static_cast<WebContentsImpl*>(shell()->web_contents());
  ui::BrowserAccessibilityManager* manager =
      web_contents->GetRootBrowserAccessibilityManager();

  AccessibilityNotificationWaiter waiter2(shell()->web_contents(),
                                          ui::kAXModeComplete,
                                          ax::mojom::Event::kTreeChanged);
  manager->LoadInlineTextBoxes(*manager->GetBrowserAccessibilityRoot());
  ASSERT_TRUE(waiter2.WaitForNotification());

  // There are three pieces of text, and they should be cross-linked:
  //   before <-> inside <-> after
  int line_link_count = CountNextPreviousOnLineLinks(
      manager->GetBrowserAccessibilityRoot(), true);
  ASSERT_EQ(line_link_count, 2);
}
#endif  // !BUILDFLAG(IS_ANDROID)

}  // namespace content
