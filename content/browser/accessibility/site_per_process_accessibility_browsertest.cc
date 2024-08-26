// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "content/browser/renderer_host/cross_process_frame_connector.h"
#include "content/browser/renderer_host/frame_tree.h"
#include "content/browser/renderer_host/render_frame_proxy_host.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_view_child_frame.h"
#include "content/browser/site_per_process_browsertest.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/accessibility_notification_waiter.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/scoped_accessibility_mode_override.h"
#include "content/public/test/test_utils.h"
#include "content/shell/browser/shell.h"
#include "content/test/content_browser_test_utils_internal.h"
#include "content/test/render_document_feature.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "ui/accessibility/ax_mode.h"
#include "ui/accessibility/platform/browser_accessibility.h"
#include "ui/accessibility/platform/browser_accessibility_manager.h"
#include "url/gurl.h"
#include "url/url_constants.h"

// These tests time out on Android.
#if BUILDFLAG(IS_ANDROID)
#define MAYBE_SitePerProcessAccessibilityBrowserTest \
  DISABLED_SitePerProcessAccessibilityBrowserTest
#else
#define MAYBE_SitePerProcessAccessibilityBrowserTest \
  SitePerProcessAccessibilityBrowserTest
#endif
// "All/DISABLED_SitePerProcessAccessibilityBrowserTest" does not work. We need
// "DISABLED_All/...". TODO(crbug.com/40136187) delete when fixed.
#if BUILDFLAG(IS_ANDROID)
#define MAYBE_All DISABLED_All
#else
#define MAYBE_All All
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
    EXPECT_TRUE(NavigateToURLFromRenderer(frame_tree_node, cross_site_url));

    // Ensure that we have created a new process for the subframe.
    SiteInstance* site_instance =
        frame_tree_node->current_frame_host()->GetSiteInstance();
    EXPECT_NE(shell()->web_contents()->GetSiteInstance(), site_instance);

    // Wait until the iframe completes the swap.
    deleted_observer.WaitUntilDeleted();
  }
};

IN_PROC_BROWSER_TEST_P(MAYBE_SitePerProcessAccessibilityBrowserTest,
                       CrossSiteIframeAccessibility) {
  // Enable full accessibility for all current and future WebContents.
  ScopedAccessibilityModeOverride mode_override(ui::kAXModeComplete);

  GURL main_url(embedded_test_server()->GetURL("/site_per_process_main.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();

  // Load same-site page into iframe.
  FrameTreeNode* child = root->child_at(0);
  GURL http_url(embedded_test_server()->GetURL("/title1.html"));
  EXPECT_TRUE(NavigateToURLFromRenderer(child, http_url));

  // Load cross-site page into iframe and wait for text from that
  // page to appear in the accessibility tree.
  LoadCrossSitePageIntoFrame(child, "/title2.html", "foo.com");
  WaitForAccessibilityTreeToContainNodeWithName(shell()->web_contents(),
                                                "Title Of Awesomeness");

  RenderFrameHostImpl* main_frame = static_cast<RenderFrameHostImpl*>(
      shell()->web_contents()->GetPrimaryMainFrame());
  ui::BrowserAccessibilityManager* main_frame_manager =
      main_frame->browser_accessibility_manager();
  VLOG(1) << "Main frame accessibility tree:\n"
          << main_frame_manager->SnapshotAXTreeForTesting().ToString();

  // Assert that we can walk from the main frame down into the child frame
  // directly, getting correct roles and data along the way.
  ui::BrowserAccessibility* ax_root =
      main_frame_manager->GetBrowserAccessibilityRoot();
  EXPECT_EQ(ax::mojom::Role::kRootWebArea, ax_root->GetRole());
  ASSERT_EQ(1U, ax_root->PlatformChildCount());

  ui::BrowserAccessibility* ax_group = ax_root->PlatformGetChild(0);
  EXPECT_EQ(ax::mojom::Role::kGenericContainer, ax_group->GetRole());
  ASSERT_EQ(2U, ax_group->PlatformChildCount());

  ui::BrowserAccessibility* ax_iframe = ax_group->PlatformGetChild(0);
  EXPECT_EQ(ax::mojom::Role::kIframe, ax_iframe->GetRole());
  ASSERT_EQ(1U, ax_iframe->PlatformChildCount());

  ui::BrowserAccessibility* ax_child_frame_root =
      ax_iframe->PlatformGetChild(0);
  EXPECT_EQ(ax::mojom::Role::kRootWebArea, ax_child_frame_root->GetRole());
  ASSERT_EQ(1U, ax_child_frame_root->PlatformChildCount());
  EXPECT_EQ("Title Of Awesomeness", ax_child_frame_root->GetStringAttribute(
                                        ax::mojom::StringAttribute::kName));

  ui::BrowserAccessibility* ax_child_frame_group =
      ax_child_frame_root->PlatformGetChild(0);
  EXPECT_EQ(ax::mojom::Role::kGenericContainer,
            ax_child_frame_group->GetRole());
  ASSERT_EQ(1U, ax_child_frame_group->PlatformChildCount());

  ui::BrowserAccessibility* ax_child_frame_static_text =
      ax_child_frame_group->PlatformGetChild(0);
  EXPECT_EQ(ax::mojom::Role::kStaticText,
            ax_child_frame_static_text->GetRole());
  ASSERT_EQ(0U, ax_child_frame_static_text->PlatformChildCount());

  // Last, check that the parent of the child frame root is correct.
  EXPECT_EQ(ax_child_frame_root->PlatformGetParent(), ax_iframe);
}

// TODO(aboxhall): Flaky test, discuss with dmazzoni
IN_PROC_BROWSER_TEST_P(MAYBE_SitePerProcessAccessibilityBrowserTest,
                       DISABLED_TwoCrossSiteNavigations) {
  // Enable full accessibility for all current and future WebContents.
  ScopedAccessibilityModeOverride mode_override(ui::kAXModeComplete);

  GURL main_url(embedded_test_server()->GetURL("/site_per_process_main.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();

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
IN_PROC_BROWSER_TEST_P(MAYBE_SitePerProcessAccessibilityBrowserTest,
                       RemoteToLocalMainFrameNavigation) {
  // Enable full accessibility for all current and future WebContents.
  ScopedAccessibilityModeOverride mode_override(ui::kAXModeComplete);

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

INSTANTIATE_TEST_SUITE_P(
    MAYBE_All,
    MAYBE_SitePerProcessAccessibilityBrowserTest,
    ::testing::ValuesIn(RenderDocumentFeatureLevelValues()));

class MAYBE_SitePerProcessAccessibilityDeviceScaleFactorBrowserTest
    : public MAYBE_SitePerProcessAccessibilityBrowserTest {
 public:
  MAYBE_SitePerProcessAccessibilityDeviceScaleFactorBrowserTest() = default;

  void SetUp() override {
    EnablePixelOutput(device_scale_factor_);
    MAYBE_SitePerProcessAccessibilityBrowserTest::SetUp();
  }

 protected:
  static constexpr float device_scale_factor_ = 2.f;
};

IN_PROC_BROWSER_TEST_P(
    MAYBE_SitePerProcessAccessibilityDeviceScaleFactorBrowserTest,
    CrossSiteIframeCoordinates) {
  // Enable full accessibility for all current and future WebContents.
  ScopedAccessibilityModeOverride mode_override(ui::kAXModeComplete);

  GURL main_url(embedded_test_server()->GetURL("/site_per_process_main.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();

  // Load cross-site page into iframe and wait for text from that
  // page to appear in the accessibility tree.
  FrameTreeNode* child = root->child_at(0);
  LoadCrossSitePageIntoFrame(child, "/title2.html", "foo.com");
  WaitForAccessibilityTreeToContainNodeWithName(shell()->web_contents(),
                                                "Title Of Awesomeness");
  child = root->child_at(0);

  RenderFrameHostImpl* main_frame = static_cast<RenderFrameHostImpl*>(
      shell()->web_contents()->GetPrimaryMainFrame());
  ui::BrowserAccessibilityManager* main_frame_manager =
      main_frame->browser_accessibility_manager();
  VLOG(1) << "Main frame accessibility tree:\n"
          << main_frame_manager->SnapshotAXTreeForTesting().ToString();

  // Assert that we can walk from the main frame down into the child frame
  // directly, getting correct roles and data along the way.
  ui::BrowserAccessibility* ax_root =
      main_frame_manager->GetBrowserAccessibilityRoot();
  EXPECT_EQ(ax::mojom::Role::kRootWebArea, ax_root->GetRole());
  ASSERT_EQ(1U, ax_root->PlatformChildCount());

  ui::BrowserAccessibility* ax_group = ax_root->PlatformGetChild(0);
  EXPECT_EQ(ax::mojom::Role::kGenericContainer, ax_group->GetRole());
  ASSERT_EQ(2U, ax_group->PlatformChildCount());

  ui::BrowserAccessibility* ax_iframe = ax_group->PlatformGetChild(0);
  EXPECT_EQ(ax::mojom::Role::kIframe, ax_iframe->GetRole());
  ASSERT_EQ(1U, ax_iframe->PlatformChildCount());

  ui::BrowserAccessibility* ax_child_frame_root =
      ax_iframe->PlatformGetChild(0);
  EXPECT_EQ(ax::mojom::Role::kRootWebArea, ax_child_frame_root->GetRole());
  ASSERT_EQ(1U, ax_child_frame_root->PlatformChildCount());

  // Get the relative iframe rect in blink pixels.
  gfx::Rect iframe_rect_root_relative_blink_pixels = ax_iframe->GetBoundsRect(
      ui::AXCoordinateSystem::kRootFrame, ui::AXClippingBehavior::kUnclipped);
  gfx::Rect iframe_rect_root_relative_physical_pixels;
  iframe_rect_root_relative_physical_pixels =
      iframe_rect_root_relative_blink_pixels;

  // Get the view bounds in screen coordinate DIPs, then ensure the offsetting
  // done by ui::AXCoordinateSystem::kScreenPhysicalPixels produces the correct
  // rect.
  gfx::Rect view_bounds = main_frame->GetView()->GetViewBounds();
  gfx::Rect iframe_rect_physical_screen_pixels =
      iframe_rect_root_relative_physical_pixels +
      gfx::ScaleToFlooredPoint(view_bounds.origin(), device_scale_factor_)
          .OffsetFromOrigin();

  EXPECT_EQ(iframe_rect_physical_screen_pixels.origin(),
            ax_child_frame_root
                ->GetBoundsRect(ui::AXCoordinateSystem::kScreenPhysicalPixels,
                                ui::AXClippingBehavior::kUnclipped)
                .origin());
}

INSTANTIATE_TEST_SUITE_P(
    MAYBE_All,
    MAYBE_SitePerProcessAccessibilityDeviceScaleFactorBrowserTest,
    ::testing::ValuesIn(RenderDocumentFeatureLevelValues()));
}  // namespace content
