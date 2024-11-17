// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include "build/build_config.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/test/accessibility_notification_waiter.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "ui/accessibility/ax_common.h"
#include "ui/accessibility/ax_node.h"
#include "ui/accessibility/ax_tree.h"
#include "ui/accessibility/ax_updates_and_events.h"

namespace content {

class AccessibilityIpcErrorBrowserTest : public ContentBrowserTest {
 public:
  AccessibilityIpcErrorBrowserTest() {}

  AccessibilityIpcErrorBrowserTest(const AccessibilityIpcErrorBrowserTest&) =
      delete;
  AccessibilityIpcErrorBrowserTest& operator=(
      const AccessibilityIpcErrorBrowserTest&) = delete;

 protected:
  // Convenience method to get the value of a particular AXNode
  // attribute as a UTF-8 string.
  std::string GetAttr(const ui::AXNode* node,
                      const ax::mojom::StringAttribute attr) {
    for (const auto& attribute_pair : node->GetStringAttributes()) {
      if (attribute_pair.first == attr) {
        return attribute_pair.second;
      }
    }
    return std::string();
  }
};

// Failed on Android x86 in crbug.com/1123641.
// Do not test on AX_FAIL_FAST_BUILDS, where the BAD IPC will simply assert.
#if (BUILDFLAG(IS_ANDROID) && defined(ARCH_CPU_X86)) || \
    defined(AX_FAIL_FAST_BUILD)
#define MAYBE_UnrecoverableAccessibilityErrorDisallowReenabling \
  DISABLED_UnrecoverableAccessibilityErrorDisallowReenabling
#else
#define MAYBE_UnrecoverableAccessibilityErrorDisallowReenabling \
  UnrecoverableAccessibilityErrorDisallowReenabling
#endif
IN_PROC_BROWSER_TEST_F(
    AccessibilityIpcErrorBrowserTest,
    MAYBE_UnrecoverableAccessibilityErrorDisallowReenabling) {
  // Create a data url and load it.
  const char url_str[] =
      "data:text/html,"
      "<div aria-live='polite'>"
      "  <p id='p1'>Paragraph One</p>"
      "  <p id='p2'>Paragraph Two</p>"
      "</div>"
      "<button id='button'>Button</button>";
  GURL url(url_str);
  EXPECT_TRUE(NavigateToURL(shell(), url));

  // Simulate a condition where the RFH can't create a
  // BrowserAccessibilityManager - like if there's no view.
  RenderFrameHostImpl* frame = static_cast<RenderFrameHostImpl*>(
      shell()->web_contents()->GetPrimaryMainFrame());
  frame->set_no_create_browser_accessibility_manager_for_testing(true);
  ASSERT_EQ(nullptr, frame->GetOrCreateBrowserAccessibilityManager());

  {
    // Enable accessibility (passing ui::kAXModeComplete to
    // AccessibilityNotificationWaiter does this automatically) and wait for
    // the first event.
    AccessibilityNotificationWaiter waiter(shell()->web_contents(),
                                           ui::kAXModeComplete,
                                           ax::mojom::Event::kLoadComplete);
    ASSERT_TRUE(waiter.WaitForNotification());
  }

  // Make sure we still didn't create a BrowserAccessibilityManager.
  // This means that at least one accessibility IPC was lost.
  ASSERT_EQ(nullptr, frame->GetOrCreateBrowserAccessibilityManager());

  // Verify the current mode.
  EXPECT_EQ(ui::kAXModeComplete,
            shell()->web_contents()->GetAccessibilityMode());

  // Now create a BrowserAccessibilityManager, simulating what would happen
  // if the RFH's view is created now - but then disallow recreating the
  // BrowserAccessibilityManager so that we can test that this one gets
  // destroyed.
  frame->set_no_create_browser_accessibility_manager_for_testing(false);
  ASSERT_TRUE(frame->GetOrCreateBrowserAccessibilityManager() != nullptr);
  frame->set_no_create_browser_accessibility_manager_for_testing(true);

  {
    // Hide one of the elements on the page, and wait for an accessibility
    // notification triggered by the hide.
    AccessibilityNotificationWaiter waiter(shell()->web_contents());
    ASSERT_TRUE(ExecJs(
        shell(), "document.getElementById('p1').style.display = 'none';"));
    ASSERT_TRUE(waiter.WaitForNotification());
  }

  // Show that accessibility was reset because the frame doesn't have a
  // BrowserAccessibilityManager anymore.
  ASSERT_EQ(nullptr, frame->browser_accessibility_manager());

  // Accessibility suffered a fatal error so we intentionally disallow
  // re-enablement.
  content::WebContentsImpl* impl =
      static_cast<content::WebContentsImpl*>(shell()->web_contents());
  EXPECT_TRUE(impl->GetAccessibilityMode().is_mode_off());
  impl->SetAccessibilityMode(ui::kAXModeComplete);
  EXPECT_TRUE(impl->GetAccessibilityMode().is_mode_off());
}

}  // namespace content
