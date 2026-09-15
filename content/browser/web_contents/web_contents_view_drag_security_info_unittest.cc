// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/web_contents/web_contents_view_drag_security_info.h"

#include <memory>

#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/public/common/drop_data.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/test_utils.h"
#include "content/test/test_render_view_host.h"
#include "content/test/test_web_contents.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace content {

class WebContentsViewDragSecurityInfoTest
    : public RenderViewHostImplTestHarness {
 protected:
  static RenderWidgetHostImpl* WidgetFor(RenderFrameHost* rfh) {
    return static_cast<RenderFrameHostImpl*>(rfh)->GetRenderWidgetHost();
  }
};

TEST_F(WebContentsViewDragSecurityInfoTest, DragNotInitiatedByView) {
  NavigateAndCommit(GURL("https://a.test/"));
  WebContentsViewDragSecurityInfo info;
  EXPECT_TRUE(info.IsValidDragTarget(WidgetFor(main_rfh())));
}

TEST_F(WebContentsViewDragSecurityInfoTest, SameSiteInstanceGroupIsValid) {
  NavigateAndCommit(GURL("https://a.test/"));
  WebContentsViewDragSecurityInfo info;
  info.OnDragInitiated(WidgetFor(main_rfh()), DropData());
  EXPECT_TRUE(info.IsValidDragTarget(WidgetFor(main_rfh())));
}

TEST_F(WebContentsViewDragSecurityInfoTest, CrossSiteSubframeIsNotValid) {
  if (!AreAllSitesIsolatedForTesting()) {
    GTEST_SKIP();
  }
  NavigateAndCommit(GURL("https://a.test/"));
  RenderFrameHost* child = NavigationSimulator::NavigateAndCommitFromDocument(
      GURL("https://b.test/"),
      RenderFrameHostTester::For(main_rfh())->AppendChild("child"));
  ASSERT_NE(WidgetFor(child), WidgetFor(main_rfh()));

  WebContentsViewDragSecurityInfo info;
  info.OnDragInitiated(WidgetFor(main_rfh()), DropData());
  EXPECT_FALSE(info.IsValidDragTarget(WidgetFor(child)));

  info.OnDragEnded();
  info.OnDragInitiated(WidgetFor(child), DropData());
  EXPECT_FALSE(info.IsValidDragTarget(WidgetFor(main_rfh())));
}

TEST_F(WebContentsViewDragSecurityInfoTest, OtherWebContentsIsValid) {
  NavigateAndCommit(GURL("https://a.test/"));
  std::unique_ptr<TestWebContents> guest =
      TestWebContents::Create(browser_context(), nullptr);
  guest->NavigateAndCommit(GURL("https://b.test/"));
  ASSERT_NE(WidgetFor(main_rfh())->GetSiteInstanceGroup(),
            WidgetFor(guest->GetPrimaryMainFrame())->GetSiteInstanceGroup());

  WebContentsViewDragSecurityInfo info;
  info.OnDragInitiated(WidgetFor(guest->GetPrimaryMainFrame()), DropData());
  EXPECT_TRUE(info.IsValidDragTarget(WidgetFor(main_rfh())));

  info.OnDragEnded();
  info.OnDragInitiated(WidgetFor(main_rfh()), DropData());
  EXPECT_TRUE(info.IsValidDragTarget(WidgetFor(guest->GetPrimaryMainFrame())));
}

TEST_F(WebContentsViewDragSecurityInfoTest, SourceWidgetGoneIsNotValid) {
  NavigateAndCommit(GURL("https://a.test/"));
  std::unique_ptr<TestWebContents> guest =
      TestWebContents::Create(browser_context(), nullptr);
  guest->NavigateAndCommit(GURL("https://b.test/"));

  WebContentsViewDragSecurityInfo info;
  info.OnDragInitiated(WidgetFor(guest->GetPrimaryMainFrame()), DropData());
  guest.reset();
  EXPECT_FALSE(info.IsValidDragTarget(WidgetFor(main_rfh())));
}

}  // namespace content
