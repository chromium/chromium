// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/test/scoped_feature_list.h"
#include "content/browser/devtools/devtools_instrumentation.h"
#include "content/browser/devtools/protocol/audits.h"
#include "content/browser/devtools/protocol/devtools_protocol_test_support.h"
#include "content/browser/devtools/render_frame_devtools_agent_host.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/devtools_agent_host_client.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/back_forward_cache_util.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_base.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/fenced_frame_test_util.h"
#include "content/public/test/prerender_test_util.h"
#include "content/public/test/test_utils.h"
#include "content/shell/browser/shell.h"
#include "net/dns/mock_host_resolver.h"

namespace content {

class DevToolsIssueStorageBrowserTest : public DevToolsProtocolTest {
 public:
  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    SetupCrossSiteRedirector(embedded_test_server());
  }

 protected:
  WebContentsImpl* web_contents() {
    return static_cast<WebContentsImpl*>(shell()->web_contents());
  }

  RenderFrameHostImpl* main_frame_host() {
    return web_contents()->GetPrimaryFrameTree().GetMainFrame();
  }

  void WaitForDummyIssueNotification() {
    base::Value::Dict notification =
        WaitForNotification("Audits.issueAdded", true);
    EXPECT_EQ(*notification.FindStringByDottedPath("issue.code"),
              protocol::Audits::InspectorIssueCodeEnum::CookieIssue);
  }
};

namespace {

void ReportDummyIssue(RenderFrameHostImpl* rfh) {
  auto issueDetails = protocol::Audits::InspectorIssueDetails::Create();
  auto inspector_issue =
      protocol::Audits::InspectorIssue::Create()
          .SetCode(protocol::Audits::InspectorIssueCodeEnum::CookieIssue)
          .SetDetails(issueDetails.Build())
          .Build();
  devtools_instrumentation::ReportBrowserInitiatedIssue(rfh,
                                                        inspector_issue.get());
}

}  // namespace

IN_PROC_BROWSER_TEST_F(DevToolsIssueStorageBrowserTest,
                       DevToolsReceivesBrowserIssues) {
  EXPECT_TRUE(NavigateToURL(shell(), GURL("about:blank")));
  // Report an empty SameSite cookie issue.
  ReportDummyIssue(main_frame_host());
  Attach();
  SendCommandSync("Audits.enable");
  // Verify we have received the SameSite issue.
  WaitForDummyIssueNotification();
}

IN_PROC_BROWSER_TEST_F(DevToolsIssueStorageBrowserTest,
                       DevToolsReceivesBrowserIssuesWhileAttached) {
  EXPECT_TRUE(NavigateToURL(shell(), GURL("about:blank")));
  Attach();
  SendCommandSync("Audits.enable");
  // Report an empty SameSite cookie issue.
  ReportDummyIssue(main_frame_host());
  // Verify we have received the SameSite issue.
  WaitForDummyIssueNotification();
}

IN_PROC_BROWSER_TEST_F(DevToolsIssueStorageBrowserTest,
                       DeleteSubframeWithIssue) {
  // 1) Navigate to a page with an OOP iframe.
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL test_url =
      embedded_test_server()->GetURL("/devtools/page-with-oopif.html");
  EXPECT_TRUE(NavigateToURL(shell(), test_url));

  // 2) Report an empty SameSite cookie issue in the iframe.
  RenderFrameHostImpl* main_frame = main_frame_host();
  EXPECT_EQ(main_frame->child_count(), static_cast<unsigned>(1));
  RenderFrameHostImpl* iframe = main_frame->child_at(0)->current_frame_host();
  EXPECT_FALSE(iframe->is_main_frame());

  ReportDummyIssue(iframe);

  // 3) Delete the iframe from the page. This should cause the issue to be
  // re-assigned
  //    to the root frame.
  main_frame->RemoveChild(iframe->frame_tree_node());

  // 4) Open DevTools and enable Audits domain.
  Attach();
  SendCommandSync("Audits.enable");

  // 5) Verify we have received the SameSite issue on the main target.
  WaitForDummyIssueNotification();
}

IN_PROC_BROWSER_TEST_F(DevToolsIssueStorageBrowserTest,
                       MainFrameNavigationClearsIssues) {
  // 1) Navigate to about:blank.
  EXPECT_TRUE(NavigateToURL(shell(), GURL("about:blank")));

  // 2) Report an empty SameSite cookie issue.
  ReportDummyIssue(main_frame_host());

  // 3) Navigate to /devtools/navigation.html
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL test_url = embedded_test_server()->GetURL("/devtools/navigation.html");
  EXPECT_TRUE(NavigateToURL(shell(), test_url));

  // 4) Open DevTools and enable Audits domain.
  Attach();
  SendCommandSync("Audits.enable");

  // 5) Verify that we haven't received any notifications.
  ASSERT_FALSE(HasExistingNotification());
}

class DevToolsIssueStorageWithBackForwardCacheBrowserTest
    : public DevToolsIssueStorageBrowserTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    // Enable BackForwardCache, omitting this feature results in a crash.
    feature_list_.InitWithFeaturesAndParameters(
        GetDefaultEnabledBackForwardCacheFeaturesForTesting(),
        GetDefaultDisabledBackForwardCacheFeaturesForTesting());
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(DevToolsIssueStorageWithBackForwardCacheBrowserTest,
                       BackForwardCacheGoBack) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  // 1) Navigate to A.
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImpl* rfh_a = main_frame_host();
  RenderFrameDeletedObserver rfh_a_deleted(rfh_a);

  // 2) Report an empty SameSite cookie issue.
  ReportDummyIssue(rfh_a);

  // 3) Navigate to B.
  //    The previous test verifies that the issue storage is cleared at
  //    this point.
  EXPECT_TRUE(NavigateToURL(shell(), url_b));
  EXPECT_TRUE(rfh_a->IsInBackForwardCache());

  // 4) Go back to A and expect that it is restored from the back-forward cache.
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  EXPECT_FALSE(rfh_a_deleted.deleted());
  EXPECT_EQ(main_frame_host(), rfh_a);

  // 5) Open DevTools and enable Audits domain.
  Attach();
  SendCommandSync("Audits.enable");

  // 6) Verify we have received the SameSite issue on the main target.
  WaitForDummyIssueNotification();
}

class DevToolsIssueStorageWithPrerenderBrowserTest
    : public DevToolsIssueStorageBrowserTest {
 public:
  DevToolsIssueStorageWithPrerenderBrowserTest()
      : prerender_test_helper_(base::BindRepeating(
            &DevToolsIssueStorageWithPrerenderBrowserTest::GetWebContents,
            base::Unretained(this))) {}

  void SetUp() override {
    prerender_test_helper().RegisterServerRequestMonitor(
        embedded_test_server());
    DevToolsIssueStorageBrowserTest::SetUp();
  }

  test::PrerenderTestHelper& prerender_test_helper() {
    return prerender_test_helper_;
  }

 private:
  WebContents* GetWebContents() { return web_contents(); }
  test::PrerenderTestHelper prerender_test_helper_;
};

IN_PROC_BROWSER_TEST_F(DevToolsIssueStorageWithPrerenderBrowserTest,
                       IssueWhilePrerendering) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL main_url(embedded_test_server()->GetURL("/empty.html"));
  GURL prerender_url(embedded_test_server()->GetURL("/title1.html"));

  // 1) Navigate to |main_url|.
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // 2) Prerender |prerender_url|.
  FrameTreeNodeId host_id = prerender_test_helper().AddPrerender(prerender_url);
  RenderFrameHostImpl* prerender_rfh = static_cast<RenderFrameHostImpl*>(
      prerender_test_helper().GetPrerenderedMainFrameHost(host_id));

  // 3) Report an empty SameSite cookie issue in prerendering page.
  ReportDummyIssue(prerender_rfh);

  // 4) Activate prerendering page.
  prerender_test_helper().NavigatePrimaryPage(prerender_url);

  // 5) Open DevTools and enable Audits domain.
  Attach();
  SendCommandSync("Audits.enable");

  // 6) Verify we have received the SameSite issue on the main target.
  WaitForDummyIssueNotification();
}

class DevToolsIssueStorageFencedFrameTest
    : public DevToolsIssueStorageBrowserTest {
 public:
  content::test::FencedFrameTestHelper& fenced_frame_test_helper() {
    return fenced_frame_helper_;
  }

 protected:
  content::test::FencedFrameTestHelper fenced_frame_helper_;
};

IN_PROC_BROWSER_TEST_F(DevToolsIssueStorageFencedFrameTest,
                       DeleteFencedFrameWithIssue) {
  // 1) Navigate to a page.
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL test_url = embedded_test_server()->GetURL("/title1.html");
  EXPECT_TRUE(NavigateToURL(shell(), test_url));

  // 2) Create a fenced frame.
  GURL fenced_frame_url =
      embedded_test_server()->GetURL("/fenced_frames/title1.html");
  content::RenderFrameHostImpl* fenced_frame_rfh =
      static_cast<RenderFrameHostImpl*>(
          fenced_frame_test_helper().CreateFencedFrame(
              web_contents()->GetPrimaryMainFrame(), fenced_frame_url));
  EXPECT_NE(nullptr, fenced_frame_rfh);

  // 3) Report an empty SameSite cookie issue in the fenced frame.
  ReportDummyIssue(fenced_frame_rfh);

  // 4) Delete the fenced frame from the page. This should cause the issue to be
  // re-assigned to the primary root frame.
  EXPECT_TRUE(ExecJs(shell()->web_contents(),
                     "document.querySelector('fencedframe').remove()"));

  // 5) Open DevTools and enable Audits domain.
  Attach();
  SendCommandSync("Audits.enable");

  // 6) Verify we have received the SameSite issue on the main target.
  WaitForDummyIssueNotification();
}

}  // namespace content
