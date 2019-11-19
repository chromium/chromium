// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/accessibility/browser_accessibility.h"
#include "content/browser/accessibility/browser_accessibility_state_impl.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_view_base.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/accessibility_notification_waiter.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/ax_mode.h"

namespace content {

const char kMinimalPageDataURL[] =
    "data:text/html,<html><head></head><body>Hello, world</body></html>";

class AccessibilityModeTest : public ContentBrowserTest {
 protected:
  WebContentsImpl* web_contents() {
    return static_cast<WebContentsImpl*>(shell()->web_contents());
  }

 protected:
  const BrowserAccessibility* FindNode(ax::mojom::Role role,
                                       const std::string& name) {
    const BrowserAccessibility* root = GetManager()->GetRoot();
    CHECK(root);
    return FindNodeInSubtree(*root, role, name);
  }

  BrowserAccessibilityManager* GetManager() {
    WebContentsImpl* web_contents =
        static_cast<WebContentsImpl*>(shell()->web_contents());
    return web_contents->GetRootBrowserAccessibilityManager();
  }

 private:
  const BrowserAccessibility* FindNodeInSubtree(
      const BrowserAccessibility& node,
      ax::mojom::Role role,
      const std::string& name) {
    if (node.GetRole() == role &&
        node.GetStringAttribute(ax::mojom::StringAttribute::kName) == name)
      return &node;
    for (unsigned int i = 0; i < node.PlatformChildCount(); ++i) {
      const BrowserAccessibility* result =
          FindNodeInSubtree(*node.PlatformGetChild(i), role, name);
      if (result)
        return result;
    }
    return nullptr;
  }
};

IN_PROC_BROWSER_TEST_F(AccessibilityModeTest, AccessibilityModeOff) {
  EXPECT_TRUE(NavigateToURL(shell(), GURL(kMinimalPageDataURL)));
  EXPECT_TRUE(web_contents()->GetAccessibilityMode().is_mode_off());
  EXPECT_EQ(nullptr, GetManager());
}

IN_PROC_BROWSER_TEST_F(AccessibilityModeTest, AccessibilityModeComplete) {
  EXPECT_TRUE(NavigateToURL(shell(), GURL(kMinimalPageDataURL)));
  ASSERT_TRUE(web_contents()->GetAccessibilityMode().is_mode_off());

  AccessibilityNotificationWaiter waiter(shell()->web_contents());
  web_contents()->AddAccessibilityMode(ui::kAXModeComplete);
  EXPECT_TRUE(web_contents()->GetAccessibilityMode() == ui::kAXModeComplete);
  waiter.WaitForNotification();
  EXPECT_NE(nullptr, GetManager());
}

IN_PROC_BROWSER_TEST_F(AccessibilityModeTest,
                       AccessibilityModeWebContentsOnly) {
  EXPECT_TRUE(NavigateToURL(shell(), GURL(kMinimalPageDataURL)));
  ASSERT_TRUE(web_contents()->GetAccessibilityMode().is_mode_off());

  AccessibilityNotificationWaiter waiter(shell()->web_contents());
  web_contents()->AddAccessibilityMode(ui::kAXModeWebContentsOnly);
  EXPECT_TRUE(web_contents()->GetAccessibilityMode() ==
              ui::kAXModeWebContentsOnly);
  waiter.WaitForNotification();
  // No BrowserAccessibilityManager expected for this mode.
  EXPECT_EQ(nullptr, GetManager());
}

IN_PROC_BROWSER_TEST_F(AccessibilityModeTest, AddingModes) {
  EXPECT_TRUE(NavigateToURL(shell(), GURL(kMinimalPageDataURL)));

  AccessibilityNotificationWaiter waiter(shell()->web_contents());
  web_contents()->AddAccessibilityMode(ui::kAXModeWebContentsOnly);
  EXPECT_TRUE(web_contents()->GetAccessibilityMode() ==
              ui::kAXModeWebContentsOnly);
  waiter.WaitForNotification();
  EXPECT_EQ(nullptr, GetManager());

  AccessibilityNotificationWaiter waiter2(shell()->web_contents());
  web_contents()->AddAccessibilityMode(ui::kAXModeComplete);
  EXPECT_TRUE(web_contents()->GetAccessibilityMode() == ui::kAXModeComplete);
  waiter2.WaitForNotification();
  EXPECT_NE(nullptr, GetManager());
}

IN_PROC_BROWSER_TEST_F(AccessibilityModeTest,
                       FullAccessibilityHasInlineTextBoxes) {
  // TODO(dmazzoni): On Android we use an ifdef to disable inline text boxes,
  // we should do it with accessibility flags instead. http://crbug.com/672205
#if !defined(OS_ANDROID)
  EXPECT_TRUE(NavigateToURL(shell(), GURL(url::kAboutBlankURL)));

  AccessibilityNotificationWaiter waiter(shell()->web_contents(),
                                         ui::kAXModeComplete,
                                         ax::mojom::Event::kLoadComplete);
  GURL url("data:text/html,<p>Para</p>");
  EXPECT_TRUE(NavigateToURL(shell(), url));
  waiter.WaitForNotification();

  const BrowserAccessibility* text =
      FindNode(ax::mojom::Role::kStaticText, "Para");
  ASSERT_NE(nullptr, text);
  ASSERT_EQ(1U, text->InternalChildCount());
  BrowserAccessibility* inline_text = text->InternalGetChild(0);
  ASSERT_NE(nullptr, inline_text);
  EXPECT_EQ(ax::mojom::Role::kInlineTextBox, inline_text->GetRole());
#endif  // !defined(OS_ANDROID)
}

IN_PROC_BROWSER_TEST_F(AccessibilityModeTest,
                       MinimalAccessibilityModeHasNoInlineTextBoxes) {
  // TODO(dmazzoni): On Android we use an ifdef to disable inline text boxes,
  // we should do it with accessibility flags instead. http://crbug.com/672205
#if !defined(OS_ANDROID)
  EXPECT_TRUE(NavigateToURL(shell(), GURL(url::kAboutBlankURL)));

  AccessibilityNotificationWaiter waiter(
      shell()->web_contents(),
      ui::AXMode::kNativeAPIs | ui::AXMode::kWebContents,
      ax::mojom::Event::kLoadComplete);
  GURL url("data:text/html,<p>Para</p>");
  EXPECT_TRUE(NavigateToURL(shell(), url));
  waiter.WaitForNotification();

  const BrowserAccessibility* text =
      FindNode(ax::mojom::Role::kStaticText, "Para");
  ASSERT_NE(nullptr, text);
  EXPECT_EQ(0U, text->InternalChildCount());
#endif  // !defined(OS_ANDROID)
}

IN_PROC_BROWSER_TEST_F(AccessibilityModeTest, AddScreenReaderModeFlag) {
  EXPECT_TRUE(NavigateToURL(shell(), GURL(url::kAboutBlankURL)));

  AccessibilityNotificationWaiter waiter(
      shell()->web_contents(),
      ui::AXMode::kNativeAPIs | ui::AXMode::kWebContents,
      ax::mojom::Event::kLoadComplete);
  GURL url("data:text/html,<input aria-label=Foo placeholder=Bar>");
  EXPECT_TRUE(NavigateToURL(shell(), url));
  waiter.WaitForNotification();

  const BrowserAccessibility* textbox =
      FindNode(ax::mojom::Role::kTextField, "Foo");
  ASSERT_NE(nullptr, textbox);
  EXPECT_FALSE(
      textbox->HasStringAttribute(ax::mojom::StringAttribute::kPlaceholder));
  int original_id = textbox->GetId();

  AccessibilityNotificationWaiter waiter2(shell()->web_contents(), ui::AXMode(),
                                          ax::mojom::Event::kLoadComplete);
  BrowserAccessibilityStateImpl::GetInstance()->AddAccessibilityModeFlags(
      ui::AXMode::kScreenReader);
  waiter2.WaitForNotification();

  const BrowserAccessibility* textbox2 =
      FindNode(ax::mojom::Role::kTextField, "Foo");
  ASSERT_NE(nullptr, textbox2);
  EXPECT_TRUE(
      textbox2->HasStringAttribute(ax::mojom::StringAttribute::kPlaceholder));
  EXPECT_EQ(original_id, textbox2->GetId());
}

}  // namespace content
