// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/run_loop.h"
#include "build/build_config.h"
#include "content/browser/devtools/render_frame_devtools_agent_host.h"
#include "content/browser/frame_host/frame_tree.h"
#include "content/browser/site_per_process_browsertest.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/download_manager.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "content/shell/browser/shell.h"
#include "content/shell/browser/shell_download_manager_delegate.h"
#include "content/test/content_browser_test_utils_internal.h"
#include "net/dns/mock_host_resolver.h"

namespace content {

class SitePerProcessDevToolsBrowserTest : public SitePerProcessBrowserTest {
 public:
  SitePerProcessDevToolsBrowserTest() {}
  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    SitePerProcessBrowserTest::SetUpOnMainThread();
  }
};

class TestClient: public DevToolsAgentHostClient {
 public:
  TestClient() : closed_(false), waiting_for_reply_(false) {}
  ~TestClient() override {}

  bool closed() { return closed_; }

  void DispatchProtocolMessage(
      DevToolsAgentHost* agent_host,
      const std::string& message) override {
    if (waiting_for_reply_) {
      waiting_for_reply_ = false;
      base::RunLoop::QuitCurrentDeprecated();
    }
  }

  void AgentHostClosed(DevToolsAgentHost* agent_host) override {
    closed_ = true;
  }

  void WaitForReply() {
    waiting_for_reply_ = true;
    base::RunLoop().Run();
  }

 private:
  bool closed_;
  bool waiting_for_reply_;
};

// Fails on Android, http://crbug.com/464993.
#if defined(OS_ANDROID)
#define MAYBE_CrossSiteIframeAgentHost DISABLED_CrossSiteIframeAgentHost
#else
#define MAYBE_CrossSiteIframeAgentHost CrossSiteIframeAgentHost
#endif
IN_PROC_BROWSER_TEST_F(SitePerProcessDevToolsBrowserTest,
                       MAYBE_CrossSiteIframeAgentHost) {
  DevToolsAgentHost::List list;
  GURL main_url(embedded_test_server()->GetURL("/site_per_process_main.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root =
      static_cast<WebContentsImpl*>(shell()->web_contents())->
          GetFrameTree()->root();

  list = DevToolsAgentHost::GetOrCreateAll();
  EXPECT_EQ(1U, list.size());
  EXPECT_EQ(DevToolsAgentHost::kTypePage, list[0]->GetType());
  EXPECT_EQ(main_url.spec(), list[0]->GetURL().spec());

  // Load same-site page into iframe.
  FrameTreeNode* child = root->child_at(0);
  GURL http_url(embedded_test_server()->GetURL("/title1.html"));
  NavigateFrameToURL(child, http_url);

  list = DevToolsAgentHost::GetOrCreateAll();
  EXPECT_EQ(1U, list.size());
  EXPECT_EQ(DevToolsAgentHost::kTypePage, list[0]->GetType());
  EXPECT_EQ(main_url.spec(), list[0]->GetURL().spec());

  // Load cross-site page into iframe.
  GURL::Replacements replace_host;
  GURL cross_site_url(embedded_test_server()->GetURL("/title2.html"));
  replace_host.SetHostStr("foo.com");
  cross_site_url = cross_site_url.ReplaceComponents(replace_host);
  NavigateFrameToURL(root->child_at(0), cross_site_url);

  list = DevToolsAgentHost::GetOrCreateAll();
  EXPECT_EQ(2U, list.size());
  EXPECT_EQ(DevToolsAgentHost::kTypePage, list[0]->GetType());
  EXPECT_EQ(main_url.spec(), list[0]->GetURL().spec());
  EXPECT_EQ(DevToolsAgentHost::kTypeFrame, list[1]->GetType());
  EXPECT_EQ(cross_site_url.spec(), list[1]->GetURL().spec());
  EXPECT_EQ(std::string(), list[0]->GetParentId());
  EXPECT_EQ(list[0]->GetId(), list[1]->GetParentId());
  EXPECT_NE(list[1]->GetId(), list[0]->GetId());

  // Attaching to both agent hosts.
  scoped_refptr<DevToolsAgentHost> child_host = list[1];
  TestClient child_client;
  child_host->AttachClient(&child_client);
  scoped_refptr<DevToolsAgentHost> parent_host = list[0];
  TestClient parent_client;
  parent_host->AttachClient(&parent_client);

  // Send message to parent and child frames and get result back.
  char message[] = "{\"id\": 0, \"method\": \"incorrect.method\"}";
  child_host->DispatchProtocolMessage(&child_client, message);
  child_client.WaitForReply();
  parent_host->DispatchProtocolMessage(&parent_client, message);
  parent_client.WaitForReply();

  // Load back same-site page into iframe.
  NavigateFrameToURL(root->child_at(0), http_url);

  list = DevToolsAgentHost::GetOrCreateAll();
  EXPECT_EQ(1U, list.size());
  EXPECT_EQ(DevToolsAgentHost::kTypePage, list[0]->GetType());
  EXPECT_EQ(main_url.spec(), list[0]->GetURL().spec());
  EXPECT_TRUE(child_client.closed());
  child_host->DetachClient(&child_client);
  child_host = nullptr;
  EXPECT_FALSE(parent_client.closed());
  parent_host->DetachClient(&parent_client);
  parent_host = nullptr;
}

IN_PROC_BROWSER_TEST_F(SitePerProcessDevToolsBrowserTest, AgentHostForFrames) {
  GURL main_url(embedded_test_server()->GetURL("/site_per_process_main.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  scoped_refptr<DevToolsAgentHost> page_agent =
      DevToolsAgentHost::GetOrCreateFor(shell()->web_contents());

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root =
      static_cast<WebContentsImpl*>(shell()->web_contents())->
          GetFrameTree()->root();

  scoped_refptr<DevToolsAgentHost> main_frame_agent =
      RenderFrameDevToolsAgentHost::GetOrCreateFor(root);
  EXPECT_EQ(page_agent.get(), main_frame_agent.get());

  // Load same-site page into iframe.
  FrameTreeNode* child = root->child_at(0);
  GURL http_url(embedded_test_server()->GetURL("/title1.html"));
  NavigateFrameToURL(child, http_url);

  scoped_refptr<DevToolsAgentHost> child_frame_agent =
      RenderFrameDevToolsAgentHost::GetOrCreateFor(child);
  EXPECT_EQ(page_agent.get(), child_frame_agent.get());

  // Load cross-site page into iframe.
  GURL::Replacements replace_host;
  GURL cross_site_url(embedded_test_server()->GetURL("/title2.html"));
  replace_host.SetHostStr("foo.com");
  cross_site_url = cross_site_url.ReplaceComponents(replace_host);
  NavigateFrameToURL(root->child_at(0), cross_site_url);

  child_frame_agent = RenderFrameDevToolsAgentHost::GetOrCreateFor(child);
  EXPECT_NE(page_agent.get(), child_frame_agent.get());
  EXPECT_EQ(child_frame_agent->GetParentId(), page_agent->GetId());
  EXPECT_NE(child_frame_agent->GetId(), page_agent->GetId());
}

IN_PROC_BROWSER_TEST_F(SitePerProcessDevToolsBrowserTest,
    AgentHostForPageEqualsOneForMainFrame) {
  GURL main_url(embedded_test_server()->GetURL("/site_per_process_main.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root =
      static_cast<WebContentsImpl*>(shell()->web_contents())->
          GetFrameTree()->root();
  FrameTreeNode* child = root->child_at(0);

  // Load cross-site page into iframe.
  GURL::Replacements replace_host;
  GURL cross_site_url(embedded_test_server()->GetURL("/title2.html"));
  replace_host.SetHostStr("foo.com");
  cross_site_url = cross_site_url.ReplaceComponents(replace_host);
  NavigateFrameToURL(child, cross_site_url);

  // First ask for child frame, then for main frame.
  scoped_refptr<DevToolsAgentHost> child_frame_agent =
      RenderFrameDevToolsAgentHost::GetOrCreateFor(child);
  scoped_refptr<DevToolsAgentHost> main_frame_agent =
      RenderFrameDevToolsAgentHost::GetOrCreateFor(root);
  EXPECT_NE(main_frame_agent.get(), child_frame_agent.get());
  EXPECT_EQ(child_frame_agent->GetParentId(), main_frame_agent->GetId());
  EXPECT_NE(child_frame_agent->GetId(), main_frame_agent->GetId());

  // Agent for web contents should be the the main frame's one.
  scoped_refptr<DevToolsAgentHost> page_agent =
      DevToolsAgentHost::GetOrCreateFor(shell()->web_contents());
  EXPECT_EQ(page_agent.get(), main_frame_agent.get());
}

class SitePerProcessDownloadDevToolsBrowserTest
    : public SitePerProcessBrowserTest {
 public:
  SitePerProcessDownloadDevToolsBrowserTest() {}

  void SetUpOnMainThread() override {
    SitePerProcessBrowserTest::SetUpOnMainThread();
    ASSERT_TRUE(downloads_directory_.CreateUniqueTempDir());
    DownloadManager* download_manager = BrowserContext::GetDownloadManager(
        shell()->web_contents()->GetBrowserContext());
    ShellDownloadManagerDelegate* download_delegate =
        static_cast<ShellDownloadManagerDelegate*>(
            download_manager->GetDelegate());
    download_delegate->SetDownloadBehaviorForTesting(
        downloads_directory_.GetPath());
  }

  base::ScopedTempDir downloads_directory_;
};

IN_PROC_BROWSER_TEST_F(SitePerProcessDownloadDevToolsBrowserTest,
                       NotCommittedNavigationDoesNotBlockAgent) {
  ASSERT_TRUE(
      NavigateToURL(shell(), embedded_test_server()->GetURL("/title1.html")));
  scoped_refptr<DevToolsAgentHost> agent =
      DevToolsAgentHost::GetOrCreateFor(shell()->web_contents());
  TestClient client;
  agent->AttachClient(&client);
  char message[] = "{\"id\": 0, \"method\": \"incorrect.method\"}";
  // Check that client is responsive.
  agent->DispatchProtocolMessage(&client, message);
  client.WaitForReply();

  // Do cross process navigation that ends up being download, which will be
  // not committed navigation in that web contents/render frame.
  GURL::Replacements replace_host;
  GURL cross_site_url(embedded_test_server()->GetURL("/download/empty.bin"));
  replace_host.SetHostStr("foo.com");
  cross_site_url = cross_site_url.ReplaceComponents(replace_host);
  ASSERT_TRUE(NavigateToURLAndExpectNoCommit(shell(), cross_site_url));

  // Check that client is still responding after not committed navigation
  // is finished.
  agent->DispatchProtocolMessage(&client, message);
  client.WaitForReply();
  ASSERT_TRUE(agent->DetachClient(&client));
}

}  // namespace content
