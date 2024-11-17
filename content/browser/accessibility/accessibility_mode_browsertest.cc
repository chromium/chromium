// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_view_base.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/browser_accessibility_state.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/accessibility_notification_waiter.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/scoped_accessibility_mode_override.h"
#include "content/shell/browser/shell.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/ax_mode.h"
#include "ui/accessibility/platform/browser_accessibility.h"

namespace content {

const char kMinimalPageDataURL[] =
    "data:text/html,<html><head></head><body>Hello, world</body></html>";

class AccessibilityModeTest : public ContentBrowserTest {
 protected:
  WebContentsImpl* web_contents() {
    return static_cast<WebContentsImpl*>(shell()->web_contents());
  }

 protected:
  const ui::BrowserAccessibility* FindNode(ax::mojom::Role role,
                                           const std::string& name) {
    const ui::BrowserAccessibility* root =
        GetManager()->GetBrowserAccessibilityRoot();
    CHECK(root);
    return FindNodeInSubtree(*root, role, name);
  }

  ui::BrowserAccessibilityManager* GetManager() {
    WebContentsImpl* web_contents =
        static_cast<WebContentsImpl*>(shell()->web_contents());
    return web_contents->GetRootBrowserAccessibilityManager();
  }

 private:
  const ui::BrowserAccessibility* FindNodeInSubtree(
      const ui::BrowserAccessibility& node,
      ax::mojom::Role role,
      const std::string& name) {
    if (node.GetRole() == role &&
        node.GetStringAttribute(ax::mojom::StringAttribute::kName) == name) {
      return &node;
    }
    for (unsigned int i = 0; i < node.PlatformChildCount(); ++i) {
      const ui::BrowserAccessibility* result =
          FindNodeInSubtree(*node.PlatformGetChild(i), role, name);
      if (result) {
        return result;
      }
    }
    return nullptr;
  }
};

IN_PROC_BROWSER_TEST_F(AccessibilityModeTest, AccessibilityModeOff) {
  EXPECT_TRUE(NavigateToURL(shell(), GURL(kMinimalPageDataURL)));
  auto accessibility_mode = web_contents()->GetAccessibilityMode();
  bool hasNativeAPIs = accessibility_mode.has_mode(ui::AXMode::kNativeAPIs);
  // Strip off kNativeAPIs, which may be set in some situations.
  accessibility_mode.set_mode(ui::AXMode::kNativeAPIs, false);
  EXPECT_TRUE(accessibility_mode.is_mode_off());
  if (!hasNativeAPIs) {
    EXPECT_EQ(nullptr, GetManager());
  }
}

IN_PROC_BROWSER_TEST_F(AccessibilityModeTest, AccessibilityModeComplete) {
  EXPECT_TRUE(NavigateToURL(shell(), GURL(kMinimalPageDataURL)));
  auto accessibility_mode = web_contents()->GetAccessibilityMode();
  // Strip off kNativeAPIs, which may be set in some situations.
  accessibility_mode.set_mode(ui::AXMode::kNativeAPIs, false);
  ASSERT_TRUE(accessibility_mode.is_mode_off());

  AccessibilityNotificationWaiter waiter(shell()->web_contents());
  ScopedAccessibilityModeOverride scoped_accessibility_mode(
      web_contents(), ui::kAXModeComplete);
  EXPECT_EQ(web_contents()->GetAccessibilityMode(), ui::kAXModeComplete);
  ASSERT_TRUE(waiter.WaitForNotification());
  EXPECT_NE(nullptr, GetManager());
}

// Tests that adding kAXModeComplete via ui::AXPlatformNode gives the flags
// to an active WebContents.
IN_PROC_BROWSER_TEST_F(AccessibilityModeTest,
                       AccessibilityModeCompleteViaNode) {
  EXPECT_TRUE(NavigateToURL(shell(), GURL(kMinimalPageDataURL)));
  auto accessibility_mode = web_contents()->GetAccessibilityMode();
  // Strip off kNativeAPIs, which may be set in some situations.
  accessibility_mode.set_mode(ui::AXMode::kNativeAPIs, false);
  ASSERT_TRUE(accessibility_mode.is_mode_off());

  AccessibilityNotificationWaiter waiter(shell()->web_contents());
  ui::AXPlatformNode::NotifyAddAXModeFlags(ui::kAXModeComplete);
  ASSERT_TRUE(waiter.WaitForNotification());
  EXPECT_EQ(web_contents()->GetAccessibilityMode(), ui::kAXModeComplete);
  EXPECT_NE(nullptr, GetManager());
}

// Tests that adding kAXModeComplete via BrowserAccessibilityState gives the
// flags to an active WebContents.
IN_PROC_BROWSER_TEST_F(AccessibilityModeTest,
                       AccessibilityModeCompleteViaContent) {
  EXPECT_TRUE(NavigateToURL(shell(), GURL(kMinimalPageDataURL)));
  auto accessibility_mode = web_contents()->GetAccessibilityMode();
  // Strip off kNativeAPIs, which may be set in some situations.
  accessibility_mode.set_mode(ui::AXMode::kNativeAPIs, false);
  ASSERT_TRUE(accessibility_mode.is_mode_off());

  AccessibilityNotificationWaiter waiter(shell()->web_contents());
  BrowserAccessibilityState::GetInstance()->AddAccessibilityModeFlags(
      ui::kAXModeComplete);
  ASSERT_TRUE(waiter.WaitForNotification());
  EXPECT_EQ(web_contents()->GetAccessibilityMode(), ui::kAXModeComplete);
  EXPECT_NE(nullptr, GetManager());
}

IN_PROC_BROWSER_TEST_F(AccessibilityModeTest,
                       AccessibilityModeWebContentsOnly) {
  EXPECT_TRUE(NavigateToURL(shell(), GURL(kMinimalPageDataURL)));
  auto accessibility_mode = web_contents()->GetAccessibilityMode();
  // Strip off kNativeAPIs, which may be set in some situations.
  accessibility_mode.set_mode(ui::AXMode::kNativeAPIs, false);
  ASSERT_TRUE(accessibility_mode.is_mode_off());

  AccessibilityNotificationWaiter waiter(shell()->web_contents());
  ScopedAccessibilityModeOverride scoped_accessibility_mode(
      web_contents(), ui::kAXModeWebContentsOnly);

  accessibility_mode = web_contents()->GetAccessibilityMode();
  bool hasNativeAPIs = accessibility_mode.has_mode(ui::AXMode::kNativeAPIs);
  // Strip off kNativeAPIs, which may be set in some situations.
  accessibility_mode.set_mode(ui::AXMode::kNativeAPIs, false);
  EXPECT_EQ(accessibility_mode, ui::kAXModeWebContentsOnly);
  ASSERT_TRUE(waiter.WaitForNotification());
  // No BrowserAccessibilityManager if kNativeAPIs isn't set.
  if (!hasNativeAPIs) {
    EXPECT_EQ(nullptr, GetManager());
  }
}

IN_PROC_BROWSER_TEST_F(AccessibilityModeTest, AddingModes) {
  EXPECT_TRUE(NavigateToURL(shell(), GURL(kMinimalPageDataURL)));

  AccessibilityNotificationWaiter waiter(shell()->web_contents());
  ScopedAccessibilityModeOverride scoped_accessibility_mode(
      web_contents(), ui::kAXModeWebContentsOnly);
  auto accessibility_mode = web_contents()->GetAccessibilityMode();
  // Strip off kNativeAPIs, which may be set in some situations.
  accessibility_mode.set_mode(ui::AXMode::kNativeAPIs, false);
  EXPECT_TRUE(accessibility_mode == ui::kAXModeWebContentsOnly);
  ASSERT_TRUE(waiter.WaitForNotification());
  EXPECT_EQ(nullptr, GetManager());

  AccessibilityNotificationWaiter waiter2(shell()->web_contents());
  ScopedAccessibilityModeOverride scoped_accessibility_mode2(
      web_contents(), ui::kAXModeComplete);
  EXPECT_EQ(web_contents()->GetAccessibilityMode(), ui::kAXModeComplete);
  ASSERT_TRUE(waiter2.WaitForNotification());
  EXPECT_NE(nullptr, GetManager());
}

IN_PROC_BROWSER_TEST_F(AccessibilityModeTest,
                       FullAccessibilityHasInlineTextBoxes) {
  // TODO(dmazzoni): On Android we use an ifdef to disable inline text boxes,
  // we should do it with accessibility flags instead. http://crbug.com/672205
#if !BUILDFLAG(IS_ANDROID)
  EXPECT_TRUE(NavigateToURL(shell(), GURL(url::kAboutBlankURL)));

  AccessibilityNotificationWaiter waiter(shell()->web_contents(),
                                         ui::kAXModeComplete,
                                         ax::mojom::Event::kLoadComplete);
  GURL url("data:text/html,<p>Para</p>");
  EXPECT_TRUE(NavigateToURL(shell(), url));
  ASSERT_TRUE(waiter.WaitForNotification());

  const ui::BrowserAccessibility* text =
      FindNode(ax::mojom::Role::kStaticText, "Para");
  ASSERT_NE(nullptr, text);
  ASSERT_EQ(1U, text->InternalChildCount());
  ui::BrowserAccessibility* inline_text = text->InternalGetChild(0);
  ASSERT_NE(nullptr, inline_text);
  EXPECT_EQ(ax::mojom::Role::kInlineTextBox, inline_text->GetRole());
#endif  // !BUILDFLAG(IS_ANDROID)
}

IN_PROC_BROWSER_TEST_F(AccessibilityModeTest,
                       MinimalAccessibilityModeHasNoInlineTextBoxes) {
  // TODO(dmazzoni): On Android we use an ifdef to disable inline text boxes,
  // we should do it with accessibility flags instead. http://crbug.com/672205
#if !BUILDFLAG(IS_ANDROID)
  EXPECT_TRUE(NavigateToURL(shell(), GURL(url::kAboutBlankURL)));

  AccessibilityNotificationWaiter waiter(
      shell()->web_contents(),
      ui::AXMode::kNativeAPIs | ui::AXMode::kWebContents,
      ax::mojom::Event::kLoadComplete);
  GURL url("data:text/html,<p>Para</p>");
  EXPECT_TRUE(NavigateToURL(shell(), url));
  ASSERT_TRUE(waiter.WaitForNotification());

  const ui::BrowserAccessibility* text =
      FindNode(ax::mojom::Role::kStaticText, "Para");
  ASSERT_NE(nullptr, text);
  EXPECT_EQ(0U, text->InternalChildCount());
#endif  // !BUILDFLAG(IS_ANDROID)
}

IN_PROC_BROWSER_TEST_F(AccessibilityModeTest, AddScreenReaderModeFlag) {
  EXPECT_TRUE(NavigateToURL(shell(), GURL(url::kAboutBlankURL)));

  AccessibilityNotificationWaiter waiter(
      shell()->web_contents(),
      ui::AXMode::kNativeAPIs | ui::AXMode::kWebContents,
      ax::mojom::Event::kLoadComplete);
  GURL url("data:text/html,<input aria-label=Foo placeholder=Bar>");
  EXPECT_TRUE(NavigateToURL(shell(), url));
  ASSERT_TRUE(waiter.WaitForNotification());

  const ui::BrowserAccessibility* textbox =
      FindNode(ax::mojom::Role::kTextField, "Foo");
  ASSERT_NE(nullptr, textbox);
  EXPECT_FALSE(
      textbox->HasStringAttribute(ax::mojom::StringAttribute::kPlaceholder));
  int original_id = textbox->GetId();

  AccessibilityNotificationWaiter waiter2(shell()->web_contents(), ui::AXMode(),
                                          ax::mojom::Event::kLoadComplete);
  ScopedAccessibilityModeOverride ax_mode_override(ui::AXMode::kScreenReader);
  ASSERT_TRUE(waiter2.WaitForNotification());

  const ui::BrowserAccessibility* textbox2 =
      FindNode(ax::mojom::Role::kTextField, "Foo");
  ASSERT_NE(nullptr, textbox2);
  EXPECT_TRUE(
      textbox2->HasStringAttribute(ax::mojom::StringAttribute::kPlaceholder));
  EXPECT_EQ(original_id, textbox2->GetId());
}

IN_PROC_BROWSER_TEST_F(AccessibilityModeTest,
                       ReEnablingAccessibilityDoesNotTimeout) {
  EXPECT_TRUE(NavigateToURL(shell(), GURL(kMinimalPageDataURL)));
  auto accessibility_mode = web_contents()->GetAccessibilityMode();
  // Strip off kNativeAPIs, which may be set in some situations.
  accessibility_mode.set_mode(ui::AXMode::kNativeAPIs, false);
  ASSERT_TRUE(accessibility_mode.is_mode_off());

  AccessibilityNotificationWaiter waiter(shell()->web_contents());
  ScopedAccessibilityModeOverride scoped_accessibility_mode(
      web_contents(), ui::kAXModeWebContentsOnly);
  accessibility_mode = web_contents()->GetAccessibilityMode();
  bool hasNativeAPIs = accessibility_mode.has_mode(ui::AXMode::kNativeAPIs);
  // Strip off kNativeAPIs, which may be set in some situations.
  accessibility_mode.set_mode(ui::AXMode::kNativeAPIs, false);
  EXPECT_TRUE(accessibility_mode == ui::kAXModeWebContentsOnly);
  ASSERT_TRUE(waiter.WaitForNotification());
  EXPECT_EQ(nullptr, GetManager());

  AccessibilityNotificationWaiter waiter2(shell()->web_contents());
  ScopedAccessibilityModeOverride scoped_accessibility_mode2(
      web_contents(), ui::kAXModeComplete);
  EXPECT_TRUE(web_contents()->GetAccessibilityMode() == ui::kAXModeComplete);
  ASSERT_TRUE(waiter2.WaitForNotification());
  if (!hasNativeAPIs) {
    EXPECT_NE(nullptr, GetManager());
  }
}

// Test platform node ids on OS's that have platform nodes.
#if !BUILDFLAG(IS_CHROMEOS) && !BUILDFLAG(IS_ANDROID)
IN_PROC_BROWSER_TEST_F(AccessibilityModeTest, ReEnablingDoesNotAlterUniqueIds) {
  ASSERT_TRUE(NavigateToURL(shell(), GURL(R"HTML(
      data:text/html,<!DOCTYPE html>
      <html>
      <body>
        <button>Button 1</button>
        <iframe srcdoc="
          <!DOCTYPE html>
          <html>
          <body>
            <button>Button 2</button>
          </body>
          </html>
        "></iframe>
        <button>Button 3</button>
      </body>
      </html>)HTML")));

  auto accessibility_mode = web_contents()->GetAccessibilityMode();
  // Strip off kNativeAPIs, which may be set in some situations.
  accessibility_mode.set_mode(ui::AXMode::kNativeAPIs, false);
  // Accessibility should now be off.
  ASSERT_TRUE(accessibility_mode.is_mode_off());
  EXPECT_EQ(nullptr, GetManager());

  // Turn accessibility on.
  AccessibilityNotificationWaiter waiter(shell()->web_contents());
  BrowserAccessibilityState::GetInstance()->AddAccessibilityModeFlags(
      ui::kAXModeComplete);
  ASSERT_TRUE(waiter.WaitForNotification());
  WaitForAccessibilityTreeToContainNodeWithName(shell()->web_contents(),
                                                "Button 2");

  // Save unique ids.
  accessibility_mode = web_contents()->GetAccessibilityMode();
  ASSERT_TRUE(accessibility_mode.has_mode(ui::AXMode::kNativeAPIs));
  ASSERT_TRUE(accessibility_mode.has_mode(ui::AXMode::kWebContents));
  EXPECT_NE(nullptr, GetManager());
  const ui::BrowserAccessibility* button_1 =
      FindNode(ax::mojom::Role::kButton, "Button 1");
  ASSERT_NE(nullptr, button_1);
  const ui::BrowserAccessibility* button_2 =
      FindNode(ax::mojom::Role::kButton, "Button 2");
  ASSERT_NE(nullptr, button_2);

  int32_t unique_id_1 = button_1->GetAXPlatformNode()->GetUniqueId();
  int32_t unique_id_2 = button_2->GetAXPlatformNode()->GetUniqueId();

  // Turn accessibility off again.
  BrowserAccessibilityState::GetInstance()->ResetAccessibilityMode();
  accessibility_mode = web_contents()->GetAccessibilityMode();
  ASSERT_TRUE(accessibility_mode.is_mode_off());
  EXPECT_EQ(nullptr, GetManager());

  // Turn accessibility on again.
  AccessibilityNotificationWaiter waiter_3(shell()->web_contents());
  BrowserAccessibilityState::GetInstance()->AddAccessibilityModeFlags(
      ui::kAXModeBasic);
  ASSERT_TRUE(waiter_3.WaitForNotification());
  WaitForAccessibilityTreeToContainNodeWithName(shell()->web_contents(),
                                                "Button 2");

  // Compare unique id for newly created a11y nodes with previous unique ids.
  accessibility_mode = web_contents()->GetAccessibilityMode();
  ASSERT_TRUE(accessibility_mode.has_mode(ui::AXMode::kNativeAPIs));
  ASSERT_TRUE(accessibility_mode.has_mode(ui::AXMode::kWebContents));
  EXPECT_NE(nullptr, GetManager());
  const ui::BrowserAccessibility* button_1_refresh =
      FindNode(ax::mojom::Role::kButton, "Button 1");
  ASSERT_NE(nullptr, button_1_refresh);
  // button_1 is now a dangling pointer for the old button.
  // The pointers are not the same, proving that button_1_refresh is new.
  ASSERT_NE(button_1, button_1_refresh);
  const ui::BrowserAccessibility* button_2_refresh =
      FindNode(ax::mojom::Role::kButton, "Button 2");
  ASSERT_NE(nullptr, button_2_refresh);
  // button_2 is now a dangling pointer for the old button.
  // The pointers are not the same, proving that button_2_refresh is new.
  ASSERT_NE(button_2, button_2_refresh);

  EXPECT_EQ(unique_id_1, button_1_refresh->GetAXPlatformNode()->GetUniqueId());
  EXPECT_EQ(unique_id_2, button_2_refresh->GetAXPlatformNode()->GetUniqueId());
}
#endif  // #if !BUILDFLAG(IS_CHROMEOS) && !BUILDFLAG(IS_ANDROID)

}  // namespace content
