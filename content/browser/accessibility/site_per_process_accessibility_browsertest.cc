// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "content/browser/accessibility/browser_accessibility.h"
#include "content/browser/accessibility/browser_accessibility_manager.h"
#include "content/browser/accessibility/browser_accessibility_state_impl.h"
#include "content/browser/frame_host/cross_process_frame_connector.h"
#include "content/browser/frame_host/frame_tree.h"
#include "content/browser/frame_host/render_frame_proxy_host.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_view_child_frame.h"
#include "content/browser/site_per_process_browsertest.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/accessibility_notification_waiter.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "content/shell/browser/shell.h"
#include "content/test/content_browser_test_utils_internal.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "url/gurl.h"
#include "url/url_constants.h"

// These tests time out on Android.
#if defined(OS_ANDROID)
#define MAYBE_SitePerProcessAccessibilityBrowserTest \
  DISABLED_SitePerProcessAccessibilityBrowserTest
#else
#define MAYBE_SitePerProcessAccessibilityBrowserTest \
  SitePerProcessAccessibilityBrowserTest
#endif

namespace content {

class MAYBE_SitePerProcessAccessibilityBrowserTest
    : public SitePerProcessBrowserTest {
 public:
  MAYBE_SitePerProcessAccessibilityBrowserTest() {}

  void LoadCrossSitePageIntoFrame(FrameTreeNode* frame_tree_node,
                                  const std::string& relative_url,
                                  const std::string& host) {
    // Load cross-site page into iframe.
    RenderFrameHostImpl* child_rfh =
        frame_tree_node->render_manager()->current_frame_host();
    RenderFrameDeletedObserver deleted_observer(child_rfh);
    GURL cross_site_url(embedded_test_server()->GetURL(host, relative_url));
    NavigateFrameToURL(frame_tree_node, cross_site_url);

    // Ensure that we have created a new process for the subframe.
    SiteInstance* site_instance =
        frame_tree_node->current_frame_host()->GetSiteInstance();
    EXPECT_NE(shell()->web_contents()->GetSiteInstance(), site_instance);

    // Wait until the iframe completes the swap.
    deleted_observer.WaitUntilDeleted();
  }
};

IN_PROC_BROWSER_TEST_F(MAYBE_SitePerProcessAccessibilityBrowserTest,
                       CrossSiteIframeAccessibility) {
  // Enable full accessibility for all current and future WebContents.
  BrowserAccessibilityState::GetInstance()->EnableAccessibility();

  GURL main_url(embedded_test_server()->GetURL("/site_per_process_main.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetFrameTree()
                            ->root();

  // Load same-site page into iframe.
  FrameTreeNode* child = root->child_at(0);
  GURL http_url(embedded_test_server()->GetURL("/title1.html"));
  NavigateFrameToURL(child, http_url);

  // Load cross-site page into iframe and wait for text from that
  // page to appear in the accessibility tree.
  LoadCrossSitePageIntoFrame(child, "/title2.html", "foo.com");
  WaitForAccessibilityTreeToContainNodeWithName(shell()->web_contents(),
                                                "Title Of Awesomeness");

  RenderFrameHostImpl* main_frame = static_cast<RenderFrameHostImpl*>(
      shell()->web_contents()->GetMainFrame());
  BrowserAccessibilityManager* main_frame_manager =
      main_frame->browser_accessibility_manager();
  VLOG(1) << "Main frame accessibility tree:\n"
          << main_frame_manager->SnapshotAXTreeForTesting().ToString();

  // Assert that we can walk from the main frame down into the child frame
  // directly, getting correct roles and data along the way.
  BrowserAccessibility* ax_root = main_frame_manager->GetRoot();
  EXPECT_EQ(ax::mojom::Role::kRootWebArea, ax_root->GetRole());
  ASSERT_EQ(1U, ax_root->PlatformChildCount());

  BrowserAccessibility* ax_group = ax_root->PlatformGetChild(0);
  EXPECT_EQ(ax::mojom::Role::kGenericContainer, ax_group->GetRole());
  ASSERT_EQ(2U, ax_group->PlatformChildCount());

  BrowserAccessibility* ax_iframe = ax_group->PlatformGetChild(0);
  EXPECT_EQ(ax::mojom::Role::kIframe, ax_iframe->GetRole());
  ASSERT_EQ(1U, ax_iframe->PlatformChildCount());

  BrowserAccessibility* ax_child_frame_root = ax_iframe->PlatformGetChild(0);
  EXPECT_EQ(ax::mojom::Role::kRootWebArea, ax_child_frame_root->GetRole());
  ASSERT_EQ(1U, ax_child_frame_root->PlatformChildCount());
  EXPECT_EQ("Title Of Awesomeness", ax_child_frame_root->GetStringAttribute(
                                        ax::mojom::StringAttribute::kName));

  BrowserAccessibility* ax_child_frame_group =
      ax_child_frame_root->PlatformGetChild(0);
  EXPECT_EQ(ax::mojom::Role::kGenericContainer,
            ax_child_frame_group->GetRole());
  ASSERT_EQ(1U, ax_child_frame_group->PlatformChildCount());

  BrowserAccessibility* ax_child_frame_static_text =
      ax_child_frame_group->PlatformGetChild(0);
  EXPECT_EQ(ax::mojom::Role::kStaticText,
            ax_child_frame_static_text->GetRole());
  ASSERT_EQ(0U, ax_child_frame_static_text->PlatformChildCount());

  // Last, check that the parent of the child frame root is correct.
  EXPECT_EQ(ax_child_frame_root->PlatformGetParent(), ax_iframe);
}

// TODO(aboxhall): Flaky test, discuss with dmazzoni
IN_PROC_BROWSER_TEST_F(MAYBE_SitePerProcessAccessibilityBrowserTest,
                       DISABLED_TwoCrossSiteNavigations) {
  // Enable full accessibility for all current and future WebContents.
  BrowserAccessibilityState::GetInstance()->EnableAccessibility();

  GURL main_url(embedded_test_server()->GetURL("/site_per_process_main.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetFrameTree()
                            ->root();

  // Load first cross-site page into iframe and wait for text from that
  // page to appear in the accessibility tree.
  FrameTreeNode* child = root->child_at(0);
  LoadCrossSitePageIntoFrame(child, "/title1.html", "foo.com");
  WaitForAccessibilityTreeToContainNodeWithName(shell()->web_contents(),
                                                "This page has no title.");

  // Load second cross-site page into iframe and wait for text from that
  // page to appear in the accessibility tree. If this succeeds and doesn't
  // time out, the test passes.
  LoadCrossSitePageIntoFrame(child, "/title2.html", "bar.com");
  WaitForAccessibilityTreeToContainNodeWithName(shell()->web_contents(),
                                                "Title Of Awesomeness");
}

// Ensure that enabling accessibility and doing a remote-to-local main frame
// navigation doesn't crash.  See https://crbug.com/762824.
IN_PROC_BROWSER_TEST_F(MAYBE_SitePerProcessAccessibilityBrowserTest,
                       RemoteToLocalMainFrameNavigation) {
  // Enable full accessibility for all current and future WebContents.
  BrowserAccessibilityState::GetInstance()->EnableAccessibility();

  GURL main_url(embedded_test_server()->GetURL("a.com", "/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));
  WaitForAccessibilityTreeToContainNodeWithName(shell()->web_contents(),
                                                "This page has no title.");

  // Open a new tab for b.com and wait for it to load.  This will create a
  // proxy in b.com for the original (opener) tab.
  GURL b_url(embedded_test_server()->GetURL("b.com", "/title2.html"));
  Shell* new_shell = OpenPopup(shell()->web_contents(), b_url, "popup");
  WaitForAccessibilityTreeToContainNodeWithName(new_shell->web_contents(),
                                                "Title Of Awesomeness");

  // Navigate the original tab to b.com as well.  This performs a
  // remote-to-local main frame navigation in b.com and shouldn't crash.
  EXPECT_TRUE(NavigateToURL(shell(), b_url));
  WaitForAccessibilityTreeToContainNodeWithName(shell()->web_contents(),
                                                "Title Of Awesomeness");
}

}  // namespace content
