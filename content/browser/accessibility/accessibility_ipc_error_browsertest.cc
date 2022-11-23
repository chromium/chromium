// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include "build/build_config.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/ax_event_notification_details.h"
#include "content/public/test/accessibility_notification_waiter.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "ui/accessibility/ax_common.h"
#include "ui/accessibility/ax_node.h"
#include "ui/accessibility/ax_tree.h"

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
      if (attribute_pair.first == attr)
        return attribute_pair.second;
    }
    return std::string();
  }
};

// Failed on Android x86 in crbug.com/1123641.
// Do not test on AX_FAIL_FAST_BUILDS, where the BAD IPC will simply assert.
#if (BUILDFLAG(IS_ANDROID) && defined(ARCH_CPU_X86)) || \
    defined(AX_FAIL_FAST_BUILD)
#define MAYBE_ResetBrowserAccessibilityManager \
  DISABLED_ResetBrowserAccessibilityManager
#else
#define MAYBE_ResetBrowserAccessibilityManager ResetBrowserAccessibilityManager
#endif
IN_PROC_BROWSER_TEST_F(AccessibilityIpcErrorBrowserTest,
                       MAYBE_ResetBrowserAccessibilityManager) {
  // Allow accessibility to reset itself on error, rather than failing.
  RenderFrameHostImpl::max_accessibility_resets_ = 99;

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
                                           ax::mojom::Event::kLayoutComplete);
    ASSERT_TRUE(waiter.WaitForNotification());
  }

  // Make sure we still didn't create a BrowserAccessibilityManager.
  // This means that at least one accessibility IPC was lost.
  ASSERT_EQ(nullptr, frame->GetOrCreateBrowserAccessibilityManager());

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
    AccessibilityNotificationWaiter waiter(shell()->web_contents(),
                                           ui::kAXModeComplete,
                                           ax::mojom::Event::kLayoutComplete);
    ASSERT_TRUE(ExecJs(
        shell(), "document.getElementById('p1').style.display = 'none';"));
    ASSERT_TRUE(waiter.WaitForNotification());
  }

  // Show that accessibility was reset because the frame doesn't have a
  // BrowserAccessibilityManager anymore.
  ASSERT_EQ(nullptr, frame->browser_accessibility_manager());

  // Finally, allow creating a new accessibility manager and
  // ensure that we didn't kill the renderer; we can still send it messages.
  frame->set_no_create_browser_accessibility_manager_for_testing(false);
  const ui::AXTree* tree = nullptr;
  {
    // Because we missed one IPC message, AXTree::Unserialize() will fail.
    AccessibilityNotificationWaiter waiter(
        shell()->web_contents(), ui::kAXModeComplete, ax::mojom::Event::kFocus);
    ASSERT_TRUE(ExecJs(shell(), "document.getElementById('button').focus();"));
    ASSERT_TRUE(waiter.WaitForNotification());
    tree = &waiter.GetAXTree();
  }

  // Get the accessibility tree, ensure it reflects the final state of the
  // document.
  const ui::AXNode* root = tree->root();

  // Use this for debugging if the test fails.
  VLOG(1) << tree->ToString();

  EXPECT_EQ(ax::mojom::Role::kRootWebArea, root->GetRole());
  ASSERT_EQ(2u, root->GetUnignoredChildCount());

  const ui::AXNode* live_region = root->GetUnignoredChildAtIndex(0);
  ASSERT_EQ(1u, live_region->GetUnignoredChildCount());
  EXPECT_EQ(ax::mojom::Role::kGenericContainer, live_region->GetRole());

  const ui::AXNode* para = live_region->GetUnignoredChildAtIndex(0);
  EXPECT_EQ(ax::mojom::Role::kParagraph, para->GetRole());

  const ui::AXNode* button = root->GetUnignoredChildAtIndex(1);
  EXPECT_EQ(ax::mojom::Role::kButton, button->GetRole());
}

// Do not test on AX_FAIL_FAST_BUILDS, where the BAD IPC will simply assert.
#if BUILDFLAG(IS_LINUX) && !defined(AX_FAIL_FAST_BUILD)
#define MAYBE_MultipleBadAccessibilityIPCsKillsRenderer \
  MultipleBadAccessibilityIPCsKillsRenderer
#else
// http://crbug.com/542704, http://crbug.com/1281355
#define MAYBE_MultipleBadAccessibilityIPCsKillsRenderer \
  DISABLED_MultipleBadAccessibilityIPCsKillsRenderer
#endif
IN_PROC_BROWSER_TEST_F(AccessibilityIpcErrorBrowserTest,
                       MAYBE_MultipleBadAccessibilityIPCsKillsRenderer) {
  // We should be able to reset accessibility |max_iterations-1| times
  // (see render_frame_host_impl.cc - max_accessibility_resets_),
  // but the subsequent time the renderer should be killed.
  int max_iterations = RenderFrameHostImpl::max_accessibility_resets_ + 1;

  // Create a data url and load it.
  const char url_str[] =
      "data:text/html,"
      "<button id='button'>Button</button>";
  GURL url(url_str);
  EXPECT_TRUE(NavigateToURL(shell(), url));
  RenderFrameHostImpl* frame = static_cast<RenderFrameHostImpl*>(
      shell()->web_contents()->GetPrimaryMainFrame());

  {
    // Enable accessibility (passing ui::kAXModeComplete to
    // AccessibilityNotificationWaiter does this automatically) and wait for
    // the first event.
    AccessibilityNotificationWaiter waiter(shell()->web_contents(),
                                           ui::kAXModeComplete,
                                           ax::mojom::Event::kLayoutComplete);
    ASSERT_TRUE(waiter.WaitForNotification());
  }

  // Construct a bad accessibility message that BrowserAccessibilityManager
  // will reject.  Note that BrowserAccessibilityManager is hosted in a
  // renderer process - the test verifies that the renderer process will crash
  // (i.e. the scenario under test does not involve mojo::ReportBadMessage
  // or content::bad_message::ReceivedBadMessage).
  AXEventNotificationDetails bad_accessibility_event;
  bad_accessibility_event.updates.resize(1);
  bad_accessibility_event.updates[0].root_id = 1;
  bad_accessibility_event.updates[0].nodes.resize(1);
  bad_accessibility_event.updates[0].nodes[0].id = 1;
  bad_accessibility_event.updates[0].nodes[0].child_ids.push_back(999);

  for (int iteration = 0; iteration < max_iterations; iteration++) {
    // Make sure the manager has been created.
    frame->GetOrCreateBrowserAccessibilityManager();
    ASSERT_NE(nullptr, frame->browser_accessibility_manager());

    // Send the bad message to the manager.
    frame->SendAccessibilityEventsToManager(bad_accessibility_event);

    // Now the frame should have deleted the BrowserAccessibilityManager.
    ASSERT_EQ(nullptr, frame->browser_accessibility_manager());

    if (iteration == max_iterations - 1)
      break;

    AccessibilityNotificationWaiter waiter(shell()->web_contents(),
                                           ui::kAXModeComplete,
                                           ax::mojom::Event::kLoadComplete);
    ASSERT_TRUE(waiter.WaitForNotification());
  }

  // Wait for the renderer to be killed.
  if (frame->IsRenderFrameLive()) {
    RenderProcessHostWatcher render_process_watcher(
        frame->GetProcess(), RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
    render_process_watcher.Wait();
  }
  ASSERT_FALSE(frame->IsRenderFrameLive());
}

}  // namespace content
