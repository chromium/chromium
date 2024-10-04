// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/check.h"
#include "base/command_line.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/accessibility_notification_waiter.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "content/shell/browser/shell.h"
#include "net/base/data_url.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/platform/browser_accessibility.h"
#include "ui/accessibility/platform/browser_accessibility_manager.h"
#include "url/gurl.h"

namespace content {

namespace {

class AccessibilityObjectModelBrowserTest : public ContentBrowserTest {
 public:
  AccessibilityObjectModelBrowserTest() {}
  ~AccessibilityObjectModelBrowserTest() override {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitchASCII(switches::kEnableBlinkFeatures,
                                    "AccessibilityObjectModel");
  }

 protected:
  ui::BrowserAccessibility* FindNode(ax::mojom::Role role,
                                     const std::string& name) {
    ui::BrowserAccessibility* root =
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
  ui::BrowserAccessibility* FindNodeInSubtree(ui::BrowserAccessibility& node,
                                              ax::mojom::Role role,
                                              const std::string& name) {
    if (node.GetRole() == role &&
        node.GetStringAttribute(ax::mojom::StringAttribute::kName) == name) {
      return &node;
    }
    for (unsigned int i = 0; i < node.PlatformChildCount(); ++i) {
      ui::BrowserAccessibility* result =
          FindNodeInSubtree(*node.PlatformGetChild(i), role, name);
      if (result) {
        return result;
      }
    }
    return nullptr;
  }
};

}  // namespace

// TODO(http://crbug.com/1212324): Flaky on various builders.
IN_PROC_BROWSER_TEST_F(AccessibilityObjectModelBrowserTest,
                       DISABLED_EventListenerOnVirtualNode) {
  ASSERT_TRUE(embedded_test_server()->Start());
  EXPECT_TRUE(NavigateToURL(shell(), GURL(url::kAboutBlankURL)));

  AccessibilityNotificationWaiter waiter(shell()->web_contents(),
                                         ui::kAXModeComplete,
                                         ax::mojom::Event::kLoadComplete);
  GURL url(embedded_test_server()->GetURL(
      "/accessibility/aom/event-listener-on-virtual-node.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url));
  ASSERT_TRUE(waiter.WaitForNotification());

  ui::BrowserAccessibility* button =
      FindNode(ax::mojom::Role::kButton, "FocusMe");
  ASSERT_NE(nullptr, button);

  ui::BrowserAccessibility* link = FindNode(ax::mojom::Role::kLink, "ClickMe");
  ASSERT_NE(nullptr, link);

  AccessibilityNotificationWaiter waiter2(
      shell()->web_contents(), ui::kAXModeComplete, ax::mojom::Event::kFocus);
  GetManager()->DoDefaultAction(*link);
  ASSERT_TRUE(waiter2.WaitForNotification());

  ui::BrowserAccessibility* focus = GetManager()->GetFocus();
  ASSERT_NE(nullptr, focus);
  EXPECT_EQ(button->GetId(), focus->GetId());
}

}  // namespace content
