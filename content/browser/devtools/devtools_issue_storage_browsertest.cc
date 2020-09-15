// Copyright 2020 The Chromium Authors. All rights reserved.
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
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_base.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
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
    return web_contents()->GetFrameTree()->GetMainFrame();
  }
};

namespace {

void ReportDummyIssue(RenderFrameHostImpl* rfh) {
  auto issueDetails = protocol::Audits::InspectorIssueDetails::Create();
  auto inspector_issue =
      protocol::Audits::InspectorIssue::Create()
          .SetCode(
              protocol::Audits::InspectorIssueCodeEnum::SameSiteCookieIssue)
          .SetDetails(issueDetails.Build())
          .Build();
  devtools_instrumentation::ReportBrowserInitiatedIssue(rfh,
                                                        inspector_issue.get());
}

}  // namespace

IN_PROC_BROWSER_TEST_F(DevToolsIssueStorageBrowserTest,
                       DevToolsReceivesBrowserIssues) {
  // 1) Navigate to about:blank.
  EXPECT_TRUE(NavigateToURL(shell(), GURL("about:blank")));

  // 2) Report an empty SameSite cookie issue.
  ReportDummyIssue(main_frame_host());

  // 3) Open DevTools.
  Attach();

  // 4) Verify we haven't received any Issues yet.
  ASSERT_TRUE(notifications_.empty());

  // 5) Enable audits domain.
  SendCommand("Audits.enable", std::make_unique<base::DictionaryValue>());

  // 6) Verify we have received the SameSite issue.
  WaitForNotification("Audits.issueAdded", true);
}

IN_PROC_BROWSER_TEST_F(DevToolsIssueStorageBrowserTest,
                       DevToolsReceivesBrowserIssuesWhileAttached) {
  // 1) Navigate to about:blank.
  EXPECT_TRUE(NavigateToURL(shell(), GURL("about:blank")));

  // 2) Open DevTools and enable Audits domain.
  Attach();
  SendCommand("Audits.enable", std::make_unique<base::DictionaryValue>());

  // 3) Verify we haven't received any Issues yet.
  ASSERT_TRUE(notifications_.empty());

  // 4) Report an empty SameSite cookie issue.
  ReportDummyIssue(main_frame_host());

  // 5) Verify we have received the SameSite issue.
  WaitForNotification("Audits.issueAdded", true);
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
  SendCommand("Audits.enable", std::make_unique<base::DictionaryValue>());

  // 5) Verify we have received the SameSite issue on the main target.
  WaitForNotification("Audits.issueAdded", true);
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
  SendCommand("Audits.enable", std::make_unique<base::DictionaryValue>());

  // 5) Verify that we haven't received any notifications.
  ASSERT_TRUE(notifications_.empty());
}

class DevToolsIssueStorageWithBackForwardCacheBrowserTest
    : public DevToolsIssueStorageBrowserTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    std::vector<base::test::ScopedFeatureList::FeatureAndParams>
        enabled_features;

    // Enable BackForwardCache, omitting this feature results in a crash.
    std::map<std::string, std::string> params = {
        {"TimeToLiveInBackForwardCacheInSeconds", "3600"}};
    enabled_features.emplace_back(features::kBackForwardCache, params);
    feature_list_.InitWithFeaturesAndParameters(enabled_features, {});
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
  SendCommand("Audits.enable", std::make_unique<base::DictionaryValue>());

  // 6) Verify we have received the SameSite issue on the main target.
  WaitForNotification("Audits.issueAdded", true);
}

}  // namespace content
