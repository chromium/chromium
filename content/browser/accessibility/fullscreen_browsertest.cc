// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/web_contents/web_contents_impl.h"
#include "content/common/features.h"
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

class AccessibilityFullscreenBrowserTest : public ContentBrowserTest {
 public:
  AccessibilityFullscreenBrowserTest() = default;
  ~AccessibilityFullscreenBrowserTest() override = default;

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

  int CountLinks(ui::BrowserAccessibility* node) {
    if (node->GetRole() == ax::mojom::Role::kLink) {
      return 1;
    }
    int links_in_children = 0;
    for (unsigned i = 0; i < node->PlatformChildCount(); i++) {
      links_in_children += CountLinks(node->PlatformGetChild(i));
    }
    return links_in_children;
  }
};

namespace {

// FakeFullscreenDelegate simply stores the latest requested mod and reports it
// back, which is all that is required for the renderer to enter fullscreen.
class FakeFullscreenDelegate : public WebContentsDelegate {
 public:
  FakeFullscreenDelegate() = default;

  FakeFullscreenDelegate(const FakeFullscreenDelegate&) = delete;
  FakeFullscreenDelegate& operator=(const FakeFullscreenDelegate&) = delete;

  ~FakeFullscreenDelegate() override = default;

  void EnterFullscreenModeForTab(
      RenderFrameHost*,
      const blink::mojom::FullscreenOptions&) override {
    is_fullscreen_ = true;
  }

  void ExitFullscreenModeForTab(WebContents*) override {
    is_fullscreen_ = false;
  }

  bool IsFullscreenForTabOrPending(const WebContents*) override {
    return is_fullscreen_;
  }

 private:
  bool is_fullscreen_ = false;
};

}  // namespace

IN_PROC_BROWSER_TEST_F(AccessibilityFullscreenBrowserTest,
                       IgnoreElementsOutsideFullscreenElement) {
  ASSERT_TRUE(embedded_test_server()->Start());

  FakeFullscreenDelegate delegate;
  shell()->web_contents()->SetDelegate(&delegate);

  AccessibilityNotificationWaiter waiter(shell()->web_contents(),
                                         ui::kAXModeComplete,
                                         ax::mojom::Event::kLoadComplete);
  GURL url(
      embedded_test_server()->GetURL("/accessibility/fullscreen/links.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url));
  ASSERT_TRUE(waiter.WaitForNotification());

  WebContentsImpl* web_contents =
      static_cast<WebContentsImpl*>(shell()->web_contents());
  ui::BrowserAccessibilityManager* manager =
      web_contents->GetRootBrowserAccessibilityManager();

  // Initially there are 3 links in the accessibility tree.
  EXPECT_EQ(3, CountLinks(manager->GetBrowserAccessibilityRoot()));

  // Enter fullscreen by finding the button and performing the default action,
  // which is to click it.
  ui::BrowserAccessibility* button =
      FindButton(manager->GetBrowserAccessibilityRoot());
  ASSERT_NE(nullptr, button);
  manager->DoDefaultAction(*button);

  // Upon entering fullscreen, the page will change the button text to "Done".
  WaitForAccessibilityTreeToContainNodeWithName(web_contents, "Done");

  // Now, the two links outside of the fullscreen element are gone.
  EXPECT_EQ(1, CountLinks(manager->GetBrowserAccessibilityRoot()));
}

// Fails flakily on all platforms: crbug.com/825735
IN_PROC_BROWSER_TEST_F(AccessibilityFullscreenBrowserTest,
                       DISABLED_InsideIFrame) {
  ASSERT_TRUE(embedded_test_server()->Start());

  FakeFullscreenDelegate delegate;
  shell()->web_contents()->SetDelegate(&delegate);

  AccessibilityNotificationWaiter waiter(shell()->web_contents(),
                                         ui::kAXModeComplete,
                                         ax::mojom::Event::kLoadComplete);
  GURL url(
      embedded_test_server()->GetURL("/accessibility/fullscreen/iframe.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url));
  ASSERT_TRUE(waiter.WaitForNotification());

  WebContentsImpl* web_contents =
      static_cast<WebContentsImpl*>(shell()->web_contents());
  ui::BrowserAccessibilityManager* manager =
      web_contents->GetRootBrowserAccessibilityManager();

  // Initially there's just one link, in the top frame.
  EXPECT_EQ(1, CountLinks(manager->GetBrowserAccessibilityRoot()));

  // Enter fullscreen by finding the button and performing the default action,
  // which is to click it.
  ui::BrowserAccessibility* button =
      FindButton(manager->GetBrowserAccessibilityRoot());
  ASSERT_NE(nullptr, button);
  manager->DoDefaultAction(*button);

  // After entering fullscreen, the page will add an iframe with a link inside
  // in the inert part of the page, then exit fullscreen and change the button
  // text to "Done". Then the link inside the iframe should also be exposed.
  WaitForAccessibilityTreeToContainNodeWithName(web_contents, "Done");
  EXPECT_EQ(2, CountLinks(manager->GetBrowserAccessibilityRoot()));
}

}  // namespace content
