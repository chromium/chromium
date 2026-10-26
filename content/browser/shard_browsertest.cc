// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/command_line.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/strcat.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "content/browser/bad_message.h"
#include "content/browser/process_lock.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/browser/site_instance_impl.h"
#include "content/browser/site_shard.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/common/content_navigation_policy.h"
#include "content/public/browser/back_forward_cache.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/navigation_throttle.h"
#include "content/public/browser/navigation_throttle_registry.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/site_isolation_policy.h"
#include "content/public/common/child_process_id.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/back_forward_cache_util.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_content_browser_client.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_navigation_throttle.h"
#include "content/public/test/test_navigation_throttle_inserter.h"
#include "content/public/test/test_utils.h"
#include "content/shell/browser/shell.h"
#include "content/test/content_browser_test_utils_internal.h"
#include "net/base/net_errors.h"
#include "net/base/schemeful_site.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"
#include "url/scheme_host_port.h"
#include "url/url_constants.h"

namespace content {

// Tests for shards (see BrowserContext::SetSitesForEmbedderShard()). These are
// currently only supported when site isolation is disabled and default
// SiteInstances (rather than default SiteInstanceGroups) are used, which is the
// case for Android WebView. These tests put the isolated.com and isolated2.com
// sites in the embedder shard. All other sites are in the default shard.
class ShardBrowserTest : public ContentBrowserTest {
 public:
  ShardBrowserTest() {
    feature_list_.InitAndDisableFeature(features::kDefaultSiteInstanceGroups);
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    ContentBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kDisableSiteIsolation);
    command_line->RemoveSwitch(switches::kSitePerProcess);
  }

  void SetUpOnMainThread() override {
    ContentBrowserTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Start());
    // This must happen before the BrowserContext is used for any navigation.
    shell()->web_contents()->GetBrowserContext()->SetSitesForEmbedderShard({
        net::SchemefulSite(GURL("http://isolated.com")),
        net::SchemefulSite(GURL("http://isolated2.com")),
    });

    ASSERT_FALSE(ShouldUseDefaultSiteInstanceGroup());
    // Subclasses may enable site isolation (see
    // SiteIsolationShardBrowserTest).
    if (base::CommandLine::ForCurrentProcess()->HasSwitch(
            switches::kDisableSiteIsolation)) {
      ASSERT_FALSE(AreAllSitesIsolatedForTesting());
    }
  }

 protected:
  WebContentsImpl* web_contents() const {
    return static_cast<WebContentsImpl*>(shell()->web_contents());
  }

  GURL GetURL(std::string_view host, std::string_view path = "/title1.html") {
    return embedded_test_server()->GetURL(host, path);
  }

  // Returns the shard recorded on the ProcessLock of `rfh`'s process (see
  // ProcessLock::shard()), or std::nullopt if the process hasn't been used yet
  // (in which case it isn't assigned to a shard).
  static std::optional<SiteShard> GetProcessShard(RenderFrameHost* rfh) {
    ProcessLock lock = rfh->GetProcess()->GetProcessLock();
    if (lock.is_unused()) {
      return std::nullopt;
    }
    return lock.shard();
  }

  // Returns the shard of `rfh`'s SiteInstance.
  static SiteShard GetSiteInstanceShard(RenderFrameHost* rfh) {
    return static_cast<SiteInstanceImpl*>(rfh->GetSiteInstance())
        ->GetSiteInfo()
        .shard();
  }

  // Checks that `rfh` is alive, i.e. that its renderer was not killed.
  static void ExpectAlive(RenderFrameHost* rfh) {
    EXPECT_TRUE(rfh->IsRenderFrameLive());
    EXPECT_EQ(2, EvalJs(rfh, "1 + 1"));
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Tests that subframes in different shards are put in different processes,
// while subframes in the same shard share a default SiteInstance and process.
IN_PROC_BROWSER_TEST_F(ShardBrowserTest, Subframes) {
  GURL main_url(GetURL("unisolated.com",
                       "/cross_site_iframe_factory.html?unisolated.com("
                       "other.com,isolated.com,sub.isolated.com)"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();
  ASSERT_EQ(3U, root->child_count());
  RenderFrameHostImpl* main_rfh = root->current_frame_host();
  RenderFrameHostImpl* other_rfh = root->child_at(0)->current_frame_host();
  RenderFrameHostImpl* isolated_rfh = root->child_at(1)->current_frame_host();
  RenderFrameHostImpl* isolated_sub_rfh =
      root->child_at(2)->current_frame_host();

  // Frames in the default shard share the default SiteInstance.
  EXPECT_TRUE(main_rfh->GetSiteInstance()->IsDefaultSiteInstance());
  EXPECT_EQ(main_rfh->GetSiteInstance(), other_rfh->GetSiteInstance());
  EXPECT_EQ(SiteShard::kDefault, GetSiteInstanceShard(main_rfh));
  EXPECT_EQ(SiteShard::kDefault, GetProcessShard(main_rfh));

  // Frames in the embedder shard share a different default SiteInstance, in the
  // same BrowsingInstance, and a different process.
  SiteInstanceImpl* isolated_instance = isolated_rfh->GetSiteInstance();
  EXPECT_TRUE(isolated_instance->IsDefaultSiteInstance());
  EXPECT_NE(main_rfh->GetSiteInstance(), isolated_instance);
  EXPECT_TRUE(
      main_rfh->GetSiteInstance()->IsRelatedSiteInstance(isolated_instance));
  EXPECT_EQ(isolated_instance, isolated_sub_rfh->GetSiteInstance());
  EXPECT_NE(main_rfh->GetProcess(), isolated_rfh->GetProcess());
  EXPECT_EQ(SiteShard::kEmbedderSites, GetSiteInstanceShard(isolated_rfh));
  EXPECT_EQ(SiteShard::kEmbedderSites, GetProcessShard(isolated_rfh));

  // Both processes allow any site, but their ProcessLocks differ because of
  // their shards.
  EXPECT_TRUE(main_rfh->GetProcess()->GetProcessLock().AllowsAnySite());
  EXPECT_TRUE(isolated_rfh->GetProcess()->GetProcessLock().AllowsAnySite());
  EXPECT_NE(main_rfh->GetProcess()->GetProcessLock(),
            isolated_rfh->GetProcess()->GetProcessLock());

  for (RenderFrameHostImpl* rfh :
       {main_rfh, other_rfh, isolated_rfh, isolated_sub_rfh}) {
    ExpectAlive(rfh);
  }

  // Navigating an isolated subframe to a URL in the default shard moves it
  // back to the main frame's process.
  EXPECT_TRUE(
      NavigateToURLFromRenderer(root->child_at(1), GetURL("other.com")));
  EXPECT_EQ(main_rfh->GetProcess(),
            root->child_at(1)->current_frame_host()->GetProcess());
  ExpectAlive(root->child_at(1)->current_frame_host());
}

// Tests that main frame navigations between shards swap processes, including
// for popups which stay in the same BrowsingInstance.
IN_PROC_BROWSER_TEST_F(ShardBrowserTest, CrossShardPopupAndNavigation) {
  EXPECT_TRUE(NavigateToURL(shell(), GetURL("unisolated.com")));
  RenderFrameHostImpl* default_rfh = web_contents()->GetPrimaryMainFrame();
  const ChildProcessId default_process_id = default_rfh->GetProcess()->GetID();
  EXPECT_EQ(SiteShard::kDefault, GetSiteInstanceShard(default_rfh));
  EXPECT_EQ(SiteShard::kDefault, GetProcessShard(default_rfh));

  // Open a popup to a URL in a different shard.
  Shell* popup = OpenPopup(default_rfh, GetURL("isolated2.com"), "popup");
  ASSERT_TRUE(popup);
  RenderFrameHostImpl* popup_rfh = static_cast<RenderFrameHostImpl*>(
      popup->web_contents()->GetPrimaryMainFrame());
  EXPECT_TRUE(default_rfh->GetSiteInstance()->IsRelatedSiteInstance(
      popup_rfh->GetSiteInstance()));
  EXPECT_NE(default_rfh->GetSiteInstance(), popup_rfh->GetSiteInstance());
  EXPECT_NE(default_process_id, popup_rfh->GetProcess()->GetID());
  EXPECT_EQ(SiteShard::kEmbedderSites, GetSiteInstanceShard(popup_rfh));
  EXPECT_EQ(SiteShard::kEmbedderSites, GetProcessShard(popup_rfh));
  ExpectAlive(popup_rfh);

  // The popup and its opener can still communicate across processes.
  EXPECT_TRUE(ExecJs(default_rfh,
                     "window.messageOrigin = new Promise(resolve => {"
                     "  window.onmessage = e => resolve(e.origin);"
                     "});"
                     // Don't wait for the promise to resolve here.
                     "true;"));
  EXPECT_TRUE(ExecJs(popup_rfh, "window.opener.postMessage('hi', '*');"));
  EXPECT_EQ(url::Origin::Create(GetURL("isolated2.com")).Serialize(),
            EvalJs(default_rfh, "window.messageOrigin"));

  // Navigating the main frame from the default shard to a URL in a different
  // shard swaps processes.
  EXPECT_TRUE(NavigateToURLFromRenderer(shell(), GetURL("sub.isolated2.com")));
  RenderFrameHostImpl* isolated_rfh = web_contents()->GetPrimaryMainFrame();
  const ChildProcessId isolated_process_id =
      isolated_rfh->GetProcess()->GetID();
  EXPECT_NE(default_process_id, isolated_process_id);
  EXPECT_EQ(SiteShard::kEmbedderSites, GetSiteInstanceShard(isolated_rfh));
  EXPECT_EQ(SiteShard::kEmbedderSites, GetProcessShard(isolated_rfh));
  // The popup is in the same BrowsingInstance and shard, so the main frame now
  // shares its default SiteInstance and process.
  EXPECT_EQ(popup_rfh->GetSiteInstance(), isolated_rfh->GetSiteInstance());
  EXPECT_EQ(popup_rfh->GetProcess(), isolated_rfh->GetProcess());
  ExpectAlive(isolated_rfh);

  // And back again.
  EXPECT_TRUE(NavigateToURLFromRenderer(shell(), GetURL("unisolated.com")));
  RenderFrameHostImpl* default_rfh2 = web_contents()->GetPrimaryMainFrame();
  EXPECT_NE(isolated_process_id, default_rfh2->GetProcess()->GetID());
  EXPECT_EQ(SiteShard::kDefault, GetSiteInstanceShard(default_rfh2));
  EXPECT_EQ(SiteShard::kDefault, GetProcessShard(default_rfh2));
  ExpectAlive(default_rfh2);
}

// Tests that a data: URL subframe stays in its initiator's shard and process,
// and that the process can continue to be used for content in that shard
// afterwards.
IN_PROC_BROWSER_TEST_F(ShardBrowserTest, DataUrlSubframe) {
  EXPECT_TRUE(NavigateToURL(shell(), GetURL("isolated.com")));
  RenderFrameHostImpl* main_rfh = web_contents()->GetPrimaryMainFrame();
  EXPECT_EQ(SiteShard::kEmbedderSites, GetSiteInstanceShard(main_rfh));
  EXPECT_EQ(SiteShard::kEmbedderSites, GetProcessShard(main_rfh));

  // Create a data: URL subframe from the isolated document.
  TestNavigationObserver observer(web_contents());
  EXPECT_TRUE(ExecJs(main_rfh,
                     "var f = document.createElement('iframe');"
                     "f.src = 'data:text/html,<p>foo</p>';"
                     "document.body.appendChild(f);"));
  observer.Wait();
  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();
  ASSERT_EQ(1U, root->child_count());
  RenderFrameHostImpl* data_rfh = root->child_at(0)->current_frame_host();
  EXPECT_TRUE(data_rfh->GetLastCommittedURL().SchemeIs(url::kDataScheme));
  EXPECT_TRUE(data_rfh->GetLastCommittedOrigin().opaque());

  // The data: subframe is in the same process and shard as its initiator, and
  // the process is still assigned to that shard.
  EXPECT_EQ(main_rfh->GetProcess(), data_rfh->GetProcess());
  EXPECT_EQ(SiteShard::kEmbedderSites, GetSiteInstanceShard(data_rfh));
  EXPECT_EQ(SiteShard::kEmbedderSites, GetProcessShard(main_rfh));
  ExpectAlive(main_rfh);
  ExpectAlive(data_rfh);

  // The process can still commit other content from the same shard.
  EXPECT_TRUE(
      NavigateToURLFromRenderer(root->child_at(0), GetURL("sub.isolated.com")));
  EXPECT_EQ(main_rfh->GetProcess(),
            root->child_at(0)->current_frame_host()->GetProcess());
  ExpectAlive(root->child_at(0)->current_frame_host());

  EXPECT_TRUE(NavigateToURLFromRenderer(shell(), GetURL("sub.isolated.com")));
  EXPECT_EQ(SiteShard::kEmbedderSites,
            GetSiteInstanceShard(web_contents()->GetPrimaryMainFrame()));
  EXPECT_EQ(SiteShard::kEmbedderSites,
            GetProcessShard(web_contents()->GetPrimaryMainFrame()));
  ExpectAlive(web_contents()->GetPrimaryMainFrame());
}

// Tests that a browser-initiated navigation to about:blank from a document in
// a shard works, and that the resulting process can still be used for that
// shard afterwards. Android WebView apps commonly load about:blank to clear a
// WebView. about:blank isn't assigned a site (see
// SiteInstance::ShouldAssignSiteForURL()), so it doesn't determine a shard
// itself.
IN_PROC_BROWSER_TEST_F(ShardBrowserTest, BrowserInitiatedAboutBlank) {
  // TODO(crbug.com/577777639): The expectations below assume that the
  // about:blank navigation proactively swaps BrowsingInstances, which doesn't
  // happen when BackForwardCache is disabled (e.g. on linux-bfcache-rel).
  if (!CanCrossSiteNavigationsProactivelySwapBrowsingInstances()) {
    GTEST_SKIP() << "Requires proactive BrowsingInstance swaps.";
  }
  EXPECT_TRUE(NavigateToURL(shell(), GetURL("isolated.com")));
  RenderFrameHostImpl* isolated_rfh = web_contents()->GetPrimaryMainFrame();
  EXPECT_EQ(SiteShard::kEmbedderSites, GetSiteInstanceShard(isolated_rfh));
  EXPECT_EQ(SiteShard::kEmbedderSites, GetProcessShard(isolated_rfh));
  scoped_refptr<SiteInstanceImpl> isolated_instance =
      isolated_rfh->GetSiteInstance();
  const ChildProcessId isolated_process_id =
      isolated_rfh->GetProcess()->GetID();

  EXPECT_TRUE(NavigateToURL(shell(), GURL(url::kAboutBlankURL)));
  RenderFrameHostImpl* blank_rfh = web_contents()->GetPrimaryMainFrame();
  EXPECT_TRUE(blank_rfh->GetLastCommittedOrigin().opaque());
  EXPECT_FALSE(blank_rfh->GetLastCommittedOrigin()
                   .GetTupleOrPrecursorTupleIfOpaque()
                   .IsValid());
  // Since this is a browser-initiated navigation that isn't same-site with the
  // current document, it swaps BrowsingInstances (see
  // RenderFrameHostManager::ShouldSwapBrowsingInstancesForNavigation()), as it
  // would without shards. So the about:blank document doesn't stay in the
  // isolated.com document's SiteInstance or process. Instead, it uses a new
  // SiteInstance without a site, whose SiteInfo is empty and so has the default
  // shard, in a new process that isn't locked yet and so isn't assigned to a
  // shard.
  EXPECT_FALSE(blank_rfh->GetSiteInstance()->HasSite());
  EXPECT_FALSE(blank_rfh->GetSiteInstance()->IsRelatedSiteInstance(
      isolated_instance.get()));
  EXPECT_NE(isolated_process_id, blank_rfh->GetProcess()->GetID());
  EXPECT_EQ(SiteShard::kDefault, GetSiteInstanceShard(blank_rfh));
  EXPECT_EQ(std::nullopt, GetProcessShard(blank_rfh));
  const ChildProcessId blank_process_id = blank_rfh->GetProcess()->GetID();
  ExpectAlive(blank_rfh);

  // The about:blank document's SiteInstance and process can then be used for
  // the embedder shard.
  EXPECT_TRUE(NavigateToURLFromRenderer(shell(), GetURL("sub.isolated.com")));
  RenderFrameHostImpl* isolated_sub_rfh = web_contents()->GetPrimaryMainFrame();
  EXPECT_EQ(blank_process_id, isolated_sub_rfh->GetProcess()->GetID());
  EXPECT_EQ(SiteShard::kEmbedderSites, GetSiteInstanceShard(isolated_sub_rfh));
  EXPECT_EQ(SiteShard::kEmbedderSites, GetProcessShard(isolated_sub_rfh));
  ExpectAlive(isolated_sub_rfh);

  EXPECT_TRUE(NavigateToURLFromRenderer(shell(), GetURL("unisolated.com")));
  EXPECT_EQ(SiteShard::kDefault,
            GetSiteInstanceShard(web_contents()->GetPrimaryMainFrame()));
  EXPECT_EQ(SiteShard::kDefault,
            GetProcessShard(web_contents()->GetPrimaryMainFrame()));
  ExpectAlive(web_contents()->GetPrimaryMainFrame());
}

// Tests that LoadDataWithBaseURL navigations use the shard of the base URL,
// since that is the origin that is committed.
IN_PROC_BROWSER_TEST_F(ShardBrowserTest, LoadDataWithBaseURL) {
  EXPECT_TRUE(NavigateToURL(shell(), GetURL("unisolated.com")));
  const ChildProcessId unisolated_process_id =
      web_contents()->GetPrimaryMainFrame()->GetProcess()->GetID();

  const GURL kIsolatedBaseUrl("http://isolated.com/");
  {
    TestNavigationObserver observer(web_contents());
    shell()->LoadDataWithBaseURL(
        /*url=*/GURL(), "<html><body>foo</body></html>", kIsolatedBaseUrl);
    observer.Wait();
    EXPECT_TRUE(observer.last_navigation_succeeded());
  }
  RenderFrameHostImpl* isolated_rfh = web_contents()->GetPrimaryMainFrame();
  EXPECT_EQ(url::Origin::Create(kIsolatedBaseUrl),
            isolated_rfh->GetLastCommittedOrigin());
  const ChildProcessId isolated_process_id =
      isolated_rfh->GetProcess()->GetID();
  EXPECT_NE(unisolated_process_id, isolated_process_id);
  EXPECT_EQ(SiteShard::kEmbedderSites, GetSiteInstanceShard(isolated_rfh));
  EXPECT_EQ(SiteShard::kEmbedderSites, GetProcessShard(isolated_rfh));
  ExpectAlive(isolated_rfh);

  // Loading data with a base URL in the default shard uses a process in
  // the default shard.
  const GURL kUnisolatedBaseUrl("http://unisolated2.com/");
  {
    TestNavigationObserver observer(web_contents());
    shell()->LoadDataWithBaseURL(
        /*url=*/GURL(), "<html><body>bar</body></html>", kUnisolatedBaseUrl);
    observer.Wait();
    EXPECT_TRUE(observer.last_navigation_succeeded());
  }
  RenderFrameHostImpl* unisolated_rfh = web_contents()->GetPrimaryMainFrame();
  EXPECT_EQ(url::Origin::Create(kUnisolatedBaseUrl),
            unisolated_rfh->GetLastCommittedOrigin());
  EXPECT_NE(isolated_process_id, unisolated_rfh->GetProcess()->GetID());
  EXPECT_EQ(SiteShard::kDefault, GetSiteInstanceShard(unisolated_rfh));
  EXPECT_EQ(SiteShard::kDefault, GetProcessShard(unisolated_rfh));
  ExpectAlive(unisolated_rfh);
}

// Tests that processes are never shared across shards, even when over the
// renderer process limit (Android WebView uses a limit of 1), while processes
// are still shared within a shard.
IN_PROC_BROWSER_TEST_F(ShardBrowserTest, ProcessLimit) {
  RenderProcessHost::SetMaxRendererProcessCount(1);

  const struct {
    GURL url;
    SiteShard expected_shard;
  } kTestCases[] = {
      {GetURL("a.com"), SiteShard::kDefault},
      {GetURL("isolated.com"), SiteShard::kEmbedderSites},
      {GetURL("b.com"), SiteShard::kDefault},
      {GetURL("sub.isolated.com"), SiteShard::kEmbedderSites},
      {GetURL("isolated2.com"), SiteShard::kEmbedderSites},
  };

  struct Result {
    GURL url;
    SiteShard shard;
    raw_ptr<RenderFrameHost> rfh;
  };
  std::vector<Result> results;
  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(test_case.url);
    // Use a separate window (and thus BrowsingInstance) for each URL.
    Shell* new_shell = results.empty() ? shell() : CreateBrowser();
    EXPECT_TRUE(NavigateToURL(new_shell, test_case.url));
    RenderFrameHost* rfh = new_shell->web_contents()->GetPrimaryMainFrame();
    EXPECT_EQ(test_case.expected_shard, GetSiteInstanceShard(rfh));
    EXPECT_EQ(test_case.expected_shard, GetProcessShard(rfh));
    ExpectAlive(rfh);
    results.push_back({test_case.url, test_case.expected_shard, rfh});
  }

  // Since the process limit has been reached, processes are shared within a
  // shard, but never across shards. RenderProcessHostImpl::IsSuitableHost()
  // agrees: a process is only suitable for SiteInstances in its own shard.
  for (const Result& a : results) {
    for (const Result& b : results) {
      SCOPED_TRACE(testing::Message() << a.url << " vs " << b.url);
      if (a.shard == b.shard) {
        EXPECT_EQ(a.rfh->GetProcess(), b.rfh->GetProcess());
      } else {
        EXPECT_NE(a.rfh->GetProcess(), b.rfh->GetProcess());
      }
      SiteInstanceImpl* b_instance =
          static_cast<SiteInstanceImpl*>(b.rfh->GetSiteInstance());
      EXPECT_EQ(a.shard == b.shard,
                RenderProcessHostImpl::IsSuitableHost(
                    a.rfh->GetProcess(), b_instance->GetIsolationContext(),
                    b_instance->GetSiteInfo()));
    }
  }

  RenderProcessHost::SetMaxRendererProcessCount(0);
}

// Tests that file: URLs, which are never put in a default SiteInstance, are in
// the default shard, and that subframes from a shard are put in a different
// process. Android WebView apps commonly load file: URLs (e.g.
// file:///android_asset/).
IN_PROC_BROWSER_TEST_F(ShardBrowserTest, FileUrl) {
  const GURL file_url = GetTestUrl("", "title1.html");
  ASSERT_TRUE(file_url.SchemeIsFile());
  EXPECT_TRUE(NavigateToURL(shell(), file_url));
  RenderFrameHostImpl* file_rfh = web_contents()->GetPrimaryMainFrame();
  EXPECT_FALSE(file_rfh->GetSiteInstance()->IsDefaultSiteInstance());
  EXPECT_EQ(SiteShard::kDefault, GetSiteInstanceShard(file_rfh));
  EXPECT_EQ(SiteShard::kDefault, GetProcessShard(file_rfh));
  ExpectAlive(file_rfh);

  // Add subframes in a shard and in the default shard.
  CreateSubframe(web_contents(), "isolated", GetURL("isolated.com"),
                 /*wait_for_navigation=*/true);
  CreateSubframe(web_contents(), "unisolated", GetURL("unisolated.com"),
                 /*wait_for_navigation=*/true);
  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();
  ASSERT_EQ(2U, root->child_count());
  RenderFrameHostImpl* isolated_rfh = root->child_at(0)->current_frame_host();
  RenderFrameHostImpl* unisolated_rfh = root->child_at(1)->current_frame_host();

  // The isolated subframe is in its shard's default SiteInstance, in a
  // different process from the file: document.
  EXPECT_TRUE(isolated_rfh->GetSiteInstance()->IsDefaultSiteInstance());
  EXPECT_NE(file_rfh->GetProcess(), isolated_rfh->GetProcess());
  EXPECT_EQ(SiteShard::kEmbedderSites, GetSiteInstanceShard(isolated_rfh));
  EXPECT_EQ(SiteShard::kEmbedderSites, GetProcessShard(isolated_rfh));

  // The unisolated subframe is in the same shard as the file: document, so
  // it stays in the parent's SiteInstance and process, as it would without
  // shards (see RenderFrameHostManager's DetermineSiteInstanceForURL()).
  EXPECT_EQ(file_rfh->GetSiteInstance(), unisolated_rfh->GetSiteInstance());
  EXPECT_EQ(file_rfh->GetProcess(), unisolated_rfh->GetProcess());
  EXPECT_EQ(SiteShard::kDefault, GetSiteInstanceShard(unisolated_rfh));
  EXPECT_EQ(SiteShard::kDefault, GetProcessShard(unisolated_rfh));

  for (RenderFrameHostImpl* rfh : {file_rfh, isolated_rfh, unisolated_rfh}) {
    ExpectAlive(rfh);
  }

  // Browser-initiated navigations between file: URLs and a shard swap
  // processes.
  EXPECT_TRUE(NavigateToURL(shell(), GetURL("isolated.com")));
  RenderFrameHostImpl* isolated_main_rfh =
      web_contents()->GetPrimaryMainFrame();
  const ChildProcessId isolated_process_id =
      isolated_main_rfh->GetProcess()->GetID();
  EXPECT_EQ(SiteShard::kEmbedderSites, GetSiteInstanceShard(isolated_main_rfh));
  EXPECT_EQ(SiteShard::kEmbedderSites, GetProcessShard(isolated_main_rfh));
  ExpectAlive(isolated_main_rfh);

  EXPECT_TRUE(NavigateToURL(shell(), file_url));
  RenderFrameHostImpl* file_rfh2 = web_contents()->GetPrimaryMainFrame();
  EXPECT_NE(isolated_process_id, file_rfh2->GetProcess()->GetID());
  EXPECT_EQ(SiteShard::kDefault, GetSiteInstanceShard(file_rfh2));
  EXPECT_EQ(SiteShard::kDefault, GetProcessShard(file_rfh2));
  ExpectAlive(file_rfh2);
}

// Tests that a sandboxed data: URL subframe, which has an opaque origin whose
// precursor is its initiator, stays in its initiator's shard and process, and
// that the process can continue to be used for content in that shard
// afterwards.
//
// Since site isolation is disabled, sandboxed frames aren't isolated (see
// SiteIsolationPolicy::AreIsolatedSandboxedIframesEnabled()), so
// UrlInfo::is_sandboxed is false. As a result, the sandboxed data: subframe
// takes the same path as an unsandboxed one in RenderFrameHostManager's
// DetermineSiteInstanceForURL(): it gets its own SiteInstance in the
// initiator's SiteInstanceGroup (see ShouldCreateSiteInstanceForDataUrls()),
// whose shard comes from the precursor of its opaque origin.
IN_PROC_BROWSER_TEST_F(ShardBrowserTest, SandboxedDataUrlSubframe) {
  const struct {
    std::string_view host;
    SiteShard expected_shard;
  } kTestCases[] = {
      {"isolated.com", SiteShard::kEmbedderSites},
      {"unisolated.com", SiteShard::kDefault},
  };
  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(test_case.host);
    const GURL main_url = GetURL(test_case.host);
    EXPECT_TRUE(NavigateToURL(shell(), main_url));
    RenderFrameHostImpl* main_rfh = web_contents()->GetPrimaryMainFrame();
    EXPECT_EQ(test_case.expected_shard, GetSiteInstanceShard(main_rfh));
    EXPECT_EQ(test_case.expected_shard, GetProcessShard(main_rfh));

    // Create a sandboxed data: URL subframe from the main document.
    TestNavigationObserver observer(web_contents());
    EXPECT_TRUE(ExecJs(main_rfh,
                       "var f = document.createElement('iframe');"
                       "f.sandbox = '';"
                       "f.src = 'data:text/html,<p>foo</p>';"
                       "document.body.appendChild(f);"));
    observer.Wait();
    FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();
    ASSERT_EQ(1U, root->child_count());
    RenderFrameHostImpl* data_rfh = root->child_at(0)->current_frame_host();
    EXPECT_TRUE(data_rfh->GetLastCommittedURL().SchemeIs(url::kDataScheme));
    const url::Origin& data_origin = data_rfh->GetLastCommittedOrigin();
    EXPECT_TRUE(data_origin.opaque());
    EXPECT_EQ(url::SchemeHostPort(main_url),
              data_origin.GetTupleOrPrecursorTupleIfOpaque());
    EXPECT_FALSE(data_rfh->GetSiteInstance()->GetSiteInfo().IsSandboxed());

    // The data: subframe has its own SiteInstance in the main frame's
    // SiteInstanceGroup, so it is in the same process and shard as the main
    // frame.
    EXPECT_NE(main_rfh->GetSiteInstance(), data_rfh->GetSiteInstance());
    EXPECT_EQ(main_rfh->GetSiteInstance()->group(),
              data_rfh->GetSiteInstance()->group());
    EXPECT_EQ(main_rfh->GetProcess(), data_rfh->GetProcess());
    EXPECT_EQ(test_case.expected_shard, GetSiteInstanceShard(data_rfh));
    EXPECT_EQ(test_case.expected_shard, GetProcessShard(main_rfh));
    ExpectAlive(main_rfh);
    ExpectAlive(data_rfh);

    // The process can still commit other content from the same shard.
    EXPECT_TRUE(NavigateToURLFromRenderer(
        root->child_at(0), GetURL(base::StrCat({"sub.", test_case.host}))));
    EXPECT_EQ(main_rfh->GetProcess(),
              root->child_at(0)->current_frame_host()->GetProcess());
    ExpectAlive(root->child_at(0)->current_frame_host());
  }
}

// Tests that a main frame document sandboxed with a CSP header, which has an
// opaque origin whose precursor is the URL's origin, is in the shard of that
// precursor, and that navigating away to another shard swaps processes.
IN_PROC_BROWSER_TEST_F(ShardBrowserTest, CspSandboxedMainFrame) {
  const struct {
    std::string_view host;
    std::string_view other_host;
    SiteShard expected_shard;
    SiteShard other_shard;
  } kTestCases[] = {
      {"isolated.com", "unisolated.com", SiteShard::kEmbedderSites,
       SiteShard::kDefault},
      {"unisolated.com", "isolated.com", SiteShard::kDefault,
       SiteShard::kEmbedderSites},
  };
  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(test_case.host);
    const GURL sandboxed_url =
        GetURL(test_case.host, "/set-header?Content-Security-Policy: sandbox");
    EXPECT_TRUE(NavigateToURL(shell(), sandboxed_url));
    RenderFrameHostImpl* sandboxed_rfh = web_contents()->GetPrimaryMainFrame();
    const url::Origin& sandboxed_origin =
        sandboxed_rfh->GetLastCommittedOrigin();
    EXPECT_TRUE(sandboxed_origin.opaque());
    EXPECT_EQ(url::SchemeHostPort(sandboxed_url),
              sandboxed_origin.GetTupleOrPrecursorTupleIfOpaque());
    EXPECT_EQ(test_case.expected_shard, GetSiteInstanceShard(sandboxed_rfh));
    EXPECT_EQ(test_case.expected_shard, GetProcessShard(sandboxed_rfh));
    ExpectAlive(sandboxed_rfh);
    const ChildProcessId sandboxed_process_id =
        sandboxed_rfh->GetProcess()->GetID();

    // A browser-initiated navigation to another shard swaps processes.
    EXPECT_TRUE(NavigateToURL(shell(), GetURL(test_case.other_host)));
    RenderFrameHostImpl* other_rfh = web_contents()->GetPrimaryMainFrame();
    EXPECT_NE(sandboxed_process_id, other_rfh->GetProcess()->GetID());
    EXPECT_EQ(test_case.other_shard, GetSiteInstanceShard(other_rfh));
    EXPECT_EQ(test_case.other_shard, GetProcessShard(other_rfh));
    ExpectAlive(other_rfh);
  }
}

// Tests that sandboxed subframes, which have opaque origins whose precursor is
// their URL's origin, are in the shard of that precursor: a same-site sandboxed
// subframe stays in its parent's process, while a sandboxed subframe from
// another shard is put in a different process.
IN_PROC_BROWSER_TEST_F(ShardBrowserTest, SandboxedIframes) {
  EXPECT_TRUE(NavigateToURL(shell(), GetURL("isolated.com")));
  RenderFrameHostImpl* main_rfh = web_contents()->GetPrimaryMainFrame();
  EXPECT_EQ(SiteShard::kEmbedderSites, GetSiteInstanceShard(main_rfh));
  EXPECT_EQ(SiteShard::kEmbedderSites, GetProcessShard(main_rfh));

  CreateSubframe(main_rfh, "same_site", GetURL("isolated.com"),
                 /*wait_for_navigation=*/true,
                 {.sandbox_flags = "allow-scripts"});
  CreateSubframe(main_rfh, "cross_shard", GetURL("unisolated.com"),
                 /*wait_for_navigation=*/true,
                 {.sandbox_flags = "allow-scripts"});
  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();
  ASSERT_EQ(2U, root->child_count());
  RenderFrameHostImpl* same_site_rfh = root->child_at(0)->current_frame_host();
  RenderFrameHostImpl* cross_shard_rfh =
      root->child_at(1)->current_frame_host();
  for (RenderFrameHostImpl* rfh : {same_site_rfh, cross_shard_rfh}) {
    EXPECT_TRUE(rfh->GetLastCommittedOrigin().opaque());
    EXPECT_EQ(url::SchemeHostPort(rfh->GetLastCommittedURL()),
              rfh->GetLastCommittedOrigin().GetTupleOrPrecursorTupleIfOpaque());
  }

  // The same-site sandboxed subframe is in the main frame's process and shard.
  EXPECT_EQ(main_rfh->GetProcess(), same_site_rfh->GetProcess());
  EXPECT_EQ(SiteShard::kEmbedderSites, GetSiteInstanceShard(same_site_rfh));
  EXPECT_EQ(SiteShard::kEmbedderSites, GetProcessShard(same_site_rfh));

  // The sandboxed subframe from the default shard is in a different process.
  EXPECT_NE(main_rfh->GetProcess(), cross_shard_rfh->GetProcess());
  EXPECT_EQ(SiteShard::kDefault, GetSiteInstanceShard(cross_shard_rfh));
  EXPECT_EQ(SiteShard::kDefault, GetProcessShard(cross_shard_rfh));

  for (RenderFrameHostImpl* rfh : {main_rfh, same_site_rfh, cross_shard_rfh}) {
    ExpectAlive(rfh);
  }
}

// Tests that an about:srcdoc subframe, which inherits its parent's origin,
// stays in its parent's SiteInstance, process and shard.
IN_PROC_BROWSER_TEST_F(ShardBrowserTest, AboutSrcdocSubframe) {
  const struct {
    std::string_view host;
    SiteShard expected_shard;
  } kTestCases[] = {
      {"isolated.com", SiteShard::kEmbedderSites},
      {"unisolated.com", SiteShard::kDefault},
  };
  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(test_case.host);
    EXPECT_TRUE(NavigateToURL(shell(), GetURL(test_case.host)));
    RenderFrameHostImpl* main_rfh = web_contents()->GetPrimaryMainFrame();

    TestNavigationObserver observer(web_contents());
    EXPECT_TRUE(ExecJs(main_rfh,
                       "var f = document.createElement('iframe');"
                       "f.srcdoc = '<p>hi</p>';"
                       "document.body.appendChild(f);"));
    observer.Wait();
    FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();
    ASSERT_EQ(1U, root->child_count());
    RenderFrameHostImpl* srcdoc_rfh = root->child_at(0)->current_frame_host();
    EXPECT_TRUE(srcdoc_rfh->GetLastCommittedURL().IsAboutSrcdoc());
    EXPECT_EQ(main_rfh->GetLastCommittedOrigin(),
              srcdoc_rfh->GetLastCommittedOrigin());

    EXPECT_EQ(main_rfh->GetSiteInstance(), srcdoc_rfh->GetSiteInstance());
    EXPECT_EQ(main_rfh->GetProcess(), srcdoc_rfh->GetProcess());
    EXPECT_EQ(test_case.expected_shard, GetSiteInstanceShard(srcdoc_rfh));
    EXPECT_EQ(test_case.expected_shard, GetProcessShard(srcdoc_rfh));
    ExpectAlive(main_rfh);
    ExpectAlive(srcdoc_rfh);
  }
}

// Tests that renderer-initiated about:blank documents (in a subframe and in a
// popup), which inherit their initiator's origin, stay in the initiator's
// process and shard, and that the popup can later navigate to another shard.
IN_PROC_BROWSER_TEST_F(ShardBrowserTest, RendererInitiatedAboutBlank) {
  const GURL main_url = GetURL("isolated.com");
  EXPECT_TRUE(NavigateToURL(shell(), main_url));
  RenderFrameHostImpl* main_rfh = web_contents()->GetPrimaryMainFrame();
  EXPECT_EQ(SiteShard::kEmbedderSites, GetSiteInstanceShard(main_rfh));
  EXPECT_EQ(SiteShard::kEmbedderSites, GetProcessShard(main_rfh));
  const GURL blank_url(url::kAboutBlankURL);

  // An about:blank subframe inherits its parent's origin and stays in its
  // parent's process and shard.
  CreateSubframe(web_contents(), "blank", blank_url,
                 /*wait_for_navigation=*/true);
  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();
  ASSERT_EQ(1U, root->child_count());
  RenderFrameHostImpl* blank_rfh = root->child_at(0)->current_frame_host();
  EXPECT_EQ(blank_url, blank_rfh->GetLastCommittedURL());
  EXPECT_EQ(url::Origin::Create(main_url), blank_rfh->GetLastCommittedOrigin());
  EXPECT_EQ(main_rfh->GetSiteInstance(), blank_rfh->GetSiteInstance());
  EXPECT_EQ(main_rfh->GetProcess(), blank_rfh->GetProcess());
  EXPECT_EQ(SiteShard::kEmbedderSites, GetSiteInstanceShard(blank_rfh));
  EXPECT_EQ(SiteShard::kEmbedderSites, GetProcessShard(blank_rfh));
  ExpectAlive(blank_rfh);

  // An about:blank popup inherits its opener's origin and stays in its
  // opener's SiteInstance, process and shard.
  Shell* popup = OpenPopup(main_rfh, blank_url, "popup");
  ASSERT_TRUE(popup);
  RenderFrameHostImpl* popup_rfh = static_cast<RenderFrameHostImpl*>(
      popup->web_contents()->GetPrimaryMainFrame());
  EXPECT_EQ(url::Origin::Create(main_url), popup_rfh->GetLastCommittedOrigin());
  EXPECT_EQ(main_rfh->GetSiteInstance(), popup_rfh->GetSiteInstance());
  EXPECT_EQ(main_rfh->GetProcess(), popup_rfh->GetProcess());
  EXPECT_EQ(SiteShard::kEmbedderSites, GetSiteInstanceShard(popup_rfh));
  EXPECT_EQ(SiteShard::kEmbedderSites, GetProcessShard(popup_rfh));

  // The opener can script the popup, since they are same-origin.
  EXPECT_TRUE(ExecJs(main_rfh,
                     "window.open('', 'popup').document.body.innerHTML = "
                     "'<p id=\"written\">hi</p>';"));
  EXPECT_EQ("hi", EvalJs(popup_rfh,
                         "document.getElementById('written')"
                         ".textContent"));
  ExpectAlive(popup_rfh);

  // Navigating the popup to another shard swaps processes.
  EXPECT_TRUE(NavigateToURLFromRenderer(popup, GetURL("unisolated.com")));
  RenderFrameHostImpl* unisolated_rfh = static_cast<RenderFrameHostImpl*>(
      popup->web_contents()->GetPrimaryMainFrame());
  EXPECT_NE(main_rfh->GetProcess(), unisolated_rfh->GetProcess());
  EXPECT_EQ(SiteShard::kDefault, GetSiteInstanceShard(unisolated_rfh));
  EXPECT_EQ(SiteShard::kDefault, GetProcessShard(unisolated_rfh));
  ExpectAlive(unisolated_rfh);
  ExpectAlive(main_rfh);
}

// Tests that server redirects between shards commit the final document in the
// destination's shard, and in a different process from the one the navigation
// started in. This covers browser- and renderer-initiated main frame
// navigations, as well as subframe navigations.
IN_PROC_BROWSER_TEST_F(ShardBrowserTest, CrossShardRedirects) {
  const struct {
    std::string_view start_host;
    SiteShard start_shard;
    std::string_view dest_host;
    SiteShard dest_shard;
  } kTestCases[] = {
      {"unisolated.com", SiteShard::kDefault, "isolated.com",
       SiteShard::kEmbedderSites},
      {"isolated.com", SiteShard::kEmbedderSites, "unisolated.com",
       SiteShard::kDefault},
  };
  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(
        base::StrCat({test_case.start_host, " -> ", test_case.dest_host}));
    const GURL start_url = GetURL(test_case.start_host);
    const GURL dest_url = GetURL(test_case.dest_host, "/title2.html");
    const GURL redirect_url =
        GetURL(test_case.start_host,
               base::StrCat({"/server-redirect?", dest_url.spec()}));

    // Browser-initiated main frame navigation.
    {
      SCOPED_TRACE("browser-initiated main frame");
      EXPECT_TRUE(NavigateToURL(shell(), start_url));
      RenderFrameHostImpl* start_rfh = web_contents()->GetPrimaryMainFrame();
      EXPECT_EQ(test_case.start_shard, GetSiteInstanceShard(start_rfh));
      EXPECT_EQ(test_case.start_shard, GetProcessShard(start_rfh));
      const ChildProcessId start_process_id = start_rfh->GetProcess()->GetID();

      EXPECT_TRUE(NavigateToURL(shell(), redirect_url, dest_url));
      RenderFrameHostImpl* rfh = web_contents()->GetPrimaryMainFrame();
      EXPECT_EQ(dest_url, rfh->GetLastCommittedURL());
      EXPECT_NE(start_process_id, rfh->GetProcess()->GetID());
      EXPECT_EQ(test_case.dest_shard, GetSiteInstanceShard(rfh));
      EXPECT_EQ(test_case.dest_shard, GetProcessShard(rfh));
      ExpectAlive(rfh);
    }

    // Renderer-initiated main frame navigation. The redirect starts out
    // same-site, so the process is only picked once the redirect is followed.
    {
      SCOPED_TRACE("renderer-initiated main frame");
      EXPECT_TRUE(NavigateToURL(shell(), start_url));
      RenderFrameHostImpl* start_rfh = web_contents()->GetPrimaryMainFrame();
      EXPECT_EQ(test_case.start_shard, GetSiteInstanceShard(start_rfh));
      EXPECT_EQ(test_case.start_shard, GetProcessShard(start_rfh));
      const ChildProcessId start_process_id = start_rfh->GetProcess()->GetID();

      EXPECT_TRUE(NavigateToURLFromRenderer(shell(), redirect_url, dest_url));
      RenderFrameHostImpl* rfh = web_contents()->GetPrimaryMainFrame();
      EXPECT_EQ(dest_url, rfh->GetLastCommittedURL());
      EXPECT_NE(start_process_id, rfh->GetProcess()->GetID());
      EXPECT_EQ(test_case.dest_shard, GetSiteInstanceShard(rfh));
      EXPECT_EQ(test_case.dest_shard, GetProcessShard(rfh));
      ExpectAlive(rfh);
    }

    // Renderer-initiated main frame navigation that starts out in the other
    // shard and redirects back to the current shard.
    {
      SCOPED_TRACE("renderer-initiated main frame, redirect back");
      EXPECT_TRUE(NavigateToURL(shell(), start_url));
      const GURL back_url = GetURL(test_case.start_host, "/title3.html");
      const GURL redirect_back_url =
          GetURL(test_case.dest_host,
                 base::StrCat({"/server-redirect?", back_url.spec()}));
      EXPECT_TRUE(
          NavigateToURLFromRenderer(shell(), redirect_back_url, back_url));
      RenderFrameHostImpl* rfh = web_contents()->GetPrimaryMainFrame();
      EXPECT_EQ(back_url, rfh->GetLastCommittedURL());
      EXPECT_EQ(test_case.start_shard, GetSiteInstanceShard(rfh));
      EXPECT_EQ(test_case.start_shard, GetProcessShard(rfh));
      ExpectAlive(rfh);
    }

    // Subframe navigation, from a subframe in its parent's shard and process.
    {
      SCOPED_TRACE("subframe");
      EXPECT_TRUE(NavigateToURL(shell(), start_url));
      RenderFrameHostImpl* main_rfh = web_contents()->GetPrimaryMainFrame();
      CreateSubframe(web_contents(), "child", start_url,
                     /*wait_for_navigation=*/true);
      FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();
      ASSERT_EQ(1U, root->child_count());
      FrameTreeNode* child = root->child_at(0);
      EXPECT_EQ(main_rfh->GetProcess(),
                child->current_frame_host()->GetProcess());

      EXPECT_TRUE(NavigateToURLFromRenderer(child, redirect_url, dest_url));
      RenderFrameHostImpl* child_rfh = child->current_frame_host();
      EXPECT_EQ(dest_url, child_rfh->GetLastCommittedURL());
      EXPECT_NE(main_rfh->GetProcess(), child_rfh->GetProcess());
      EXPECT_EQ(test_case.dest_shard, GetSiteInstanceShard(child_rfh));
      EXPECT_EQ(test_case.dest_shard, GetProcessShard(child_rfh));
      EXPECT_EQ(test_case.start_shard, GetProcessShard(main_rfh));
      ExpectAlive(main_rfh);
      ExpectAlive(child_rfh);
    }
  }
}

// Runs with full Site Isolation (--site-per-process), where every site requires
// a dedicated process. Content that requires a dedicated process always stays
// in the default shard (see BrowserContext::SetSitesForEmbedderShard()), so
// shards have no effect.
class SiteIsolationShardBrowserTest : public ShardBrowserTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    ShardBrowserTest::SetUpCommandLine(command_line);
    command_line->RemoveSwitch(switches::kDisableSiteIsolation);
    command_line->AppendSwitch(switches::kSitePerProcess);
  }

  void SetUpOnMainThread() override {
    ShardBrowserTest::SetUpOnMainThread();
    ASSERT_TRUE(AreAllSitesIsolatedForTesting());
  }
};

// Tests that sites in the embedder shard aren't sharded with full Site
// Isolation, for main frames and cross-site subframes: each site gets its own
// SiteInstance and a process locked to it, all of which are in the default
// shard.
IN_PROC_BROWSER_TEST_F(SiteIsolationShardBrowserTest, NoSharding) {
  // Main frames.
  for (std::string_view host : {"isolated.com", "unisolated.com"}) {
    SCOPED_TRACE(host);
    EXPECT_TRUE(NavigateToURL(shell(), GetURL(host)));
    RenderFrameHostImpl* rfh = web_contents()->GetPrimaryMainFrame();
    EXPECT_FALSE(rfh->GetSiteInstance()->IsDefaultSiteInstance());
    EXPECT_TRUE(rfh->GetProcess()->GetProcessLock().IsLockedToSite());
    EXPECT_EQ(SiteShard::kDefault, GetSiteInstanceShard(rfh));
    EXPECT_EQ(SiteShard::kDefault, GetProcessShard(rfh));
    ExpectAlive(rfh);
  }

  // Cross-site subframes.
  GURL main_url(GetURL("unisolated.com",
                       "/cross_site_iframe_factory.html?unisolated.com("
                       "isolated.com,isolated2.com)"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));
  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();
  ASSERT_EQ(2U, root->child_count());
  RenderFrameHostImpl* main_rfh = root->current_frame_host();
  RenderFrameHostImpl* isolated_rfh = root->child_at(0)->current_frame_host();
  RenderFrameHostImpl* isolated2_rfh = root->child_at(1)->current_frame_host();
  for (RenderFrameHostImpl* rfh : {main_rfh, isolated_rfh, isolated2_rfh}) {
    SCOPED_TRACE(rfh->GetLastCommittedURL());
    EXPECT_FALSE(rfh->GetSiteInstance()->IsDefaultSiteInstance());
    EXPECT_TRUE(rfh->GetProcess()->GetProcessLock().IsLockedToSite());
    EXPECT_EQ(SiteShard::kDefault, GetSiteInstanceShard(rfh));
    EXPECT_EQ(SiteShard::kDefault, GetProcessShard(rfh));
    ExpectAlive(rfh);
  }
  // Each site is in its own process, including the two sites in the embedder
  // shard.
  EXPECT_NE(main_rfh->GetProcess(), isolated_rfh->GetProcess());
  EXPECT_NE(main_rfh->GetProcess(), isolated2_rfh->GetProcess());
  EXPECT_NE(isolated_rfh->GetProcess(), isolated2_rfh->GetProcess());
}

// Runs with partial Site Isolation, where isolated.com is an isolated origin
// (and so requires a dedicated process), but other sites don't require one.
class IsolatedOriginShardBrowserTest : public ShardBrowserTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    ShardBrowserTest::SetUpCommandLine(command_line);
    command_line->RemoveSwitch(switches::kDisableSiteIsolation);
    command_line->AppendSwitchASCII(switches::kIsolateOrigins,
                                    "http://isolated.com");
  }

  void SetUpOnMainThread() override {
    browser_client_ = std::make_unique<NoSiteIsolationContentBrowserClient>();
    ShardBrowserTest::SetUpOnMainThread();
    ASSERT_FALSE(AreAllSitesIsolatedForTesting());
  }

  void TearDownOnMainThread() override {
    browser_client_.reset();
    ShardBrowserTest::TearDownOnMainThread();
  }

 private:
  class NoSiteIsolationContentBrowserClient
      : public ContentBrowserTestContentBrowserClient {
   public:
    bool ShouldEnableStrictSiteIsolation() override { return false; }
  };

  std::unique_ptr<NoSiteIsolationContentBrowserClient> browser_client_;
};

// Tests that an isolated origin in the embedder shard isn't sharded, since it
// requires a dedicated process, while a site in the embedder shard that isn't
// an isolated origin is still sharded.
IN_PROC_BROWSER_TEST_F(IsolatedOriginShardBrowserTest,
                       NoShardingForIsolatedOrigin) {
  GURL main_url(GetURL("unisolated.com",
                       "/cross_site_iframe_factory.html?unisolated.com("
                       "isolated.com,isolated2.com)"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));
  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();
  ASSERT_EQ(2U, root->child_count());
  RenderFrameHostImpl* main_rfh = root->current_frame_host();
  RenderFrameHostImpl* isolated_rfh = root->child_at(0)->current_frame_host();
  RenderFrameHostImpl* isolated2_rfh = root->child_at(1)->current_frame_host();

  // isolated.com is an isolated origin, so it gets its own SiteInstance and a
  // process locked to it, in the default shard, even though it's in the
  // embedder shard.
  EXPECT_FALSE(isolated_rfh->GetSiteInstance()->IsDefaultSiteInstance());
  EXPECT_TRUE(isolated_rfh->GetProcess()->GetProcessLock().IsLockedToSite());
  EXPECT_EQ(SiteShard::kDefault, GetSiteInstanceShard(isolated_rfh));
  EXPECT_EQ(SiteShard::kDefault, GetProcessShard(isolated_rfh));

  // isolated2.com isn't an isolated origin, so it's in the embedder shard's
  // default SiteInstance, in an unlocked process.
  EXPECT_TRUE(isolated2_rfh->GetSiteInstance()->IsDefaultSiteInstance());
  EXPECT_TRUE(isolated2_rfh->GetProcess()->GetProcessLock().AllowsAnySite());
  EXPECT_EQ(SiteShard::kEmbedderSites, GetSiteInstanceShard(isolated2_rfh));
  EXPECT_EQ(SiteShard::kEmbedderSites, GetProcessShard(isolated2_rfh));

  // unisolated.com is in the default shard's default SiteInstance, in an
  // unlocked process.
  EXPECT_TRUE(main_rfh->GetSiteInstance()->IsDefaultSiteInstance());
  EXPECT_TRUE(main_rfh->GetProcess()->GetProcessLock().AllowsAnySite());
  EXPECT_EQ(SiteShard::kDefault, GetSiteInstanceShard(main_rfh));
  EXPECT_EQ(SiteShard::kDefault, GetProcessShard(main_rfh));

  EXPECT_NE(main_rfh->GetProcess(), isolated_rfh->GetProcess());
  EXPECT_NE(main_rfh->GetProcess(), isolated2_rfh->GetProcess());
  EXPECT_NE(isolated_rfh->GetProcess(), isolated2_rfh->GetProcess());
  for (RenderFrameHostImpl* rfh : {main_rfh, isolated_rfh, isolated2_rfh}) {
    ExpectAlive(rfh);
  }

  // The same applies to main frames.
  EXPECT_TRUE(NavigateToURL(shell(), GetURL("isolated.com")));
  RenderFrameHostImpl* rfh = web_contents()->GetPrimaryMainFrame();
  EXPECT_TRUE(rfh->GetProcess()->GetProcessLock().IsLockedToSite());
  EXPECT_EQ(SiteShard::kDefault, GetSiteInstanceShard(rfh));
  EXPECT_EQ(SiteShard::kDefault, GetProcessShard(rfh));
  ExpectAlive(rfh);

  EXPECT_TRUE(NavigateToURL(shell(), GetURL("isolated2.com")));
  rfh = web_contents()->GetPrimaryMainFrame();
  EXPECT_TRUE(rfh->GetProcess()->GetProcessLock().AllowsAnySite());
  EXPECT_EQ(SiteShard::kEmbedderSites, GetSiteInstanceShard(rfh));
  EXPECT_EQ(SiteShard::kEmbedderSites, GetProcessShard(rfh));
  ExpectAlive(rfh);
}

// Tests that a renderer process is killed if it tries to commit a URL and
// origin from another shard, e.g. if it's compromised. Without shards, the
// process isn't locked (since site isolation is disabled), so it could commit
// any such URL and origin. With shards, the Citadel checks in
// ChildProcessSecurityPolicyImpl::CanAccessMaybeOpaqueOrigin() only allow an
// unlocked process to commit origins from its own shard.
class KillShardBrowserTest : public ShardBrowserTest {
 protected:
  void TestCommitInOtherShardKillsRenderer(std::string_view host,
                                           SiteShard shard,
                                           std::string_view same_shard_host,
                                           std::string_view other_host) {
    // As a control, check that a commit claiming a URL and origin from another
    // site in the same shard passes ChildProcessSecurityPolicyImpl's checks,
    // since the process isn't locked to a site. It's only caught by the later
    // check that the origin matches the one the browser expected to commit,
    // which gives a different kill reason. This uses a separate window, since
    // PwnCommitIPC() only supports one interception per WebContents.
    {
      SCOPED_TRACE("same shard");
      Shell* control_window = CreateBrowser();
      EXPECT_TRUE(NavigateToURL(control_window, GetURL(host)));
      RenderFrameHost* control_rfh =
          control_window->web_contents()->GetPrimaryMainFrame();
      EXPECT_EQ(shard, GetSiteInstanceShard(control_rfh));
      EXPECT_EQ(shard, GetProcessShard(control_rfh));
      const GURL control_target_url = GetURL(host, "/title2.html");
      const GURL same_shard_url = GetURL(same_shard_host, "/title2.html");
      PwnCommitIPC(control_window->web_contents(), control_target_url,
                   same_shard_url, url::Origin::Create(same_shard_url));
      RenderProcessHostBadIpcMessageWaiter control_kill_waiter(
          control_rfh->GetProcess());
      EXPECT_TRUE(ExecJs(control_rfh,
                         JsReplace("location.href = $1;", control_target_url)));
      EXPECT_EQ(bad_message::RFH_ORIGIN_TO_COMMIT_MISMATCH,
                control_kill_waiter.Wait());
    }

    EXPECT_TRUE(NavigateToURL(shell(), GetURL(host)));
    RenderFrameHostImpl* rfh = web_contents()->GetPrimaryMainFrame();
    EXPECT_EQ(shard, GetSiteInstanceShard(rfh));
    EXPECT_EQ(shard, GetProcessShard(rfh));

    // Navigate to a same-site URL, which stays in the same process, but make
    // the renderer claim that it committed a URL and origin from the other
    // shard.
    const GURL target_url = GetURL(host, "/title2.html");
    const GURL other_shard_url = GetURL(other_host, "/title2.html");
    PwnCommitIPC(web_contents(), target_url, other_shard_url,
                 url::Origin::Create(other_shard_url));
    RenderProcessHostBadIpcMessageWaiter kill_waiter(rfh->GetProcess());
    EXPECT_TRUE(ExecJs(rfh, JsReplace("location.href = $1;", target_url)));

    // The URL fails ChildProcessSecurityPolicyImpl::CanCommitURL(), because
    // of the shard mismatch. Since the process isn't locked to a site, that's
    // the only reason it could fail for an HTTP URL, and it's checked before
    // the origin mismatch that killed the renderer in the control case above.
    EXPECT_EQ(bad_message::RFH_CAN_COMMIT_URL_BLOCKED, kill_waiter.Wait());
  }
};

IN_PROC_BROWSER_TEST_F(KillShardBrowserTest,
                       CommitDefaultShardUrlInEmbedderShardProcess) {
  TestCommitInOtherShardKillsRenderer("isolated.com", SiteShard::kEmbedderSites,
                                      "isolated2.com", "unisolated.com");
}

IN_PROC_BROWSER_TEST_F(KillShardBrowserTest,
                       CommitEmbedderShardUrlInDefaultShardProcess) {
  TestCommitInOtherShardKillsRenderer("unisolated.com", SiteShard::kDefault,
                                      "unisolated2.com", "isolated.com");
}

// Tests back/forward navigations across shards: default -> embedder shard ->
// default, then back twice and forward twice. Each history navigation must
// commit in the right shard, and in a different process from the previous
// document, whether or not the documents are restored from the back/forward
// cache.
class HistoryShardBrowserTest : public ShardBrowserTest,
                                public testing::WithParamInterface<bool> {
 protected:
  bool IsBackForwardCacheUsed() const { return GetParam(); }
};

INSTANTIATE_TEST_SUITE_P(All,
                         HistoryShardBrowserTest,
                         testing::Bool(),
                         [](const testing::TestParamInfo<bool>& info) {
                           return info.param ? "WithBackForwardCache"
                                             : "WithoutBackForwardCache";
                         });

IN_PROC_BROWSER_TEST_P(HistoryShardBrowserTest, BackForwardAcrossShards) {
  if (IsBackForwardCacheUsed()) {
    if (!IsBackForwardCacheEnabled()) {
      GTEST_SKIP() << "The back/forward cache is not enabled.";
    }
  } else {
    DisableBackForwardCacheForTesting(
        web_contents(), BackForwardCache::TEST_REQUIRES_NO_CACHING);
  }

  const GURL url_a = GetURL("unisolated.com");
  const GURL url_b = GetURL("isolated.com");
  const GURL url_c = GetURL("unisolated2.com");

  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImplWrapper rfh_a(web_contents()->GetPrimaryMainFrame());
  EXPECT_EQ(SiteShard::kDefault, GetSiteInstanceShard(rfh_a.get()));
  EXPECT_EQ(SiteShard::kDefault, GetProcessShard(rfh_a.get()));
  const ChildProcessId process_a = rfh_a->GetProcess()->GetID();

  EXPECT_TRUE(NavigateToURL(shell(), url_b));
  RenderFrameHostImplWrapper rfh_b(web_contents()->GetPrimaryMainFrame());
  EXPECT_EQ(SiteShard::kEmbedderSites, GetSiteInstanceShard(rfh_b.get()));
  EXPECT_EQ(SiteShard::kEmbedderSites, GetProcessShard(rfh_b.get()));
  const ChildProcessId process_b = rfh_b->GetProcess()->GetID();
  EXPECT_NE(process_a, process_b);

  EXPECT_TRUE(NavigateToURL(shell(), url_c));
  RenderFrameHostImplWrapper rfh_c(web_contents()->GetPrimaryMainFrame());
  EXPECT_EQ(SiteShard::kDefault, GetSiteInstanceShard(rfh_c.get()));
  EXPECT_EQ(SiteShard::kDefault, GetProcessShard(rfh_c.get()));
  EXPECT_NE(process_b, rfh_c->GetProcess()->GetID());

  if (IsBackForwardCacheUsed()) {
    EXPECT_TRUE(rfh_a->IsInBackForwardCache());
    EXPECT_TRUE(rfh_b->IsInBackForwardCache());
  } else {
    EXPECT_TRUE(rfh_a.WaitUntilRenderFrameDeleted());
    EXPECT_TRUE(rfh_b.WaitUntilRenderFrameDeleted());
  }

  const struct {
    bool back;
    GURL url;
    SiteShard shard;
    raw_ptr<RenderFrameHostImplWrapper> cached_rfh;
  } kSteps[] = {
      {true, url_b, SiteShard::kEmbedderSites, &rfh_b},
      {true, url_a, SiteShard::kDefault, &rfh_a},
      {false, url_b, SiteShard::kEmbedderSites, &rfh_b},
      {false, url_c, SiteShard::kDefault, &rfh_c},
  };
  ChildProcessId previous_process_id = rfh_c->GetProcess()->GetID();
  for (const auto& step : kSteps) {
    SCOPED_TRACE(step.url);
    TestNavigationObserver observer(web_contents());
    if (step.back) {
      web_contents()->GetController().GoBack();
    } else {
      web_contents()->GetController().GoForward();
    }
    observer.Wait();
    EXPECT_TRUE(observer.last_navigation_succeeded());

    RenderFrameHostImpl* rfh = web_contents()->GetPrimaryMainFrame();
    EXPECT_EQ(step.url, rfh->GetLastCommittedURL());
    EXPECT_EQ(step.shard, GetSiteInstanceShard(rfh));
    EXPECT_EQ(step.shard, GetProcessShard(rfh));
    EXPECT_NE(previous_process_id, rfh->GetProcess()->GetID());
    if (IsBackForwardCacheUsed()) {
      // The document was restored from the back/forward cache.
      EXPECT_EQ(step.cached_rfh->get(), rfh);
    }
    ExpectAlive(rfh);
    previous_process_id = rfh->GetProcess()->GetID();
  }
}

// Tests that with error page isolation, which content_shell enables for main
// frames (unlike Android WebView), an error page for a URL in the embedder
// shard commits in an error page SiteInstance and process. Error page
// SiteInfos are in the default shard (see SiteInfo::CreateForErrorPage()), and
// error page processes are locked, so they aren't affected by shards.
IN_PROC_BROWSER_TEST_F(ShardBrowserTest, IsolatedMainFrameErrorPage) {
  ASSERT_TRUE(SiteIsolationPolicy::IsErrorPageIsolationEnabled(
      /*in_main_frame=*/true));
  EXPECT_TRUE(NavigateToURL(shell(), GetURL("isolated.com")));
  const ChildProcessId isolated_process_id =
      web_contents()->GetPrimaryMainFrame()->GetProcess()->GetID();

  const GURL error_url = GetURL("isolated.com", "/close-socket");
  TestNavigationObserver observer(web_contents());
  shell()->LoadURL(error_url);
  observer.Wait();
  EXPECT_FALSE(observer.last_navigation_succeeded());
  EXPECT_EQ(net::ERR_EMPTY_RESPONSE, observer.last_net_error_code());

  RenderFrameHostImpl* error_rfh = web_contents()->GetPrimaryMainFrame();
  EXPECT_TRUE(error_rfh->IsErrorDocument());
  EXPECT_TRUE(error_rfh->GetSiteInstance()->GetSiteInfo().is_error_page());
  EXPECT_TRUE(error_rfh->GetProcess()->GetProcessLock().is_error_page());
  EXPECT_NE(isolated_process_id, error_rfh->GetProcess()->GetID());
  EXPECT_EQ(SiteShard::kDefault, GetSiteInstanceShard(error_rfh));
  EXPECT_EQ(SiteShard::kDefault, GetProcessShard(error_rfh));
  ExpectAlive(error_rfh);
  const ChildProcessId error_process_id = error_rfh->GetProcess()->GetID();

  // A successful navigation to the same URL afterwards goes back to the
  // embedder shard.
  EXPECT_TRUE(NavigateToURL(shell(), GetURL("isolated.com")));
  RenderFrameHostImpl* rfh = web_contents()->GetPrimaryMainFrame();
  EXPECT_NE(error_process_id, rfh->GetProcess()->GetID());
  EXPECT_EQ(SiteShard::kEmbedderSites, GetSiteInstanceShard(rfh));
  EXPECT_EQ(SiteShard::kEmbedderSites, GetProcessShard(rfh));
  ExpectAlive(rfh);
}

// Like ShardBrowserTest, but without error page isolation in main frames or
// subframes, as in Android WebView (see
// AwContentBrowserClient::ShouldIsolateErrorPage()).
class NoErrorPageIsolationShardBrowserTest : public ShardBrowserTest {
 public:
  void SetUpOnMainThread() override {
    ShardBrowserTest::SetUpOnMainThread();
    client_ = std::make_unique<NoErrorPageIsolationContentBrowserClient>();
    ASSERT_FALSE(SiteIsolationPolicy::IsErrorPageIsolationEnabled(
        /*in_main_frame=*/true));
    ASSERT_FALSE(SiteIsolationPolicy::IsErrorPageIsolationEnabled(
        /*in_main_frame=*/false));
  }

  void TearDownOnMainThread() override {
    client_.reset();
    ShardBrowserTest::TearDownOnMainThread();
  }

 protected:
  // Runs a scenario that used to crash the browser: a subframe navigation to
  // `blocked_host`, which is blocked by CSP, from an about:blank document
  // whose SiteInstance has no site, but whose process is shared with a
  // document from `first_host` in another shard because of the process limit.
  //
  // ComputeErrorPageProcess() used to return kCurrentProcess for the blocked
  // navigation, so the error page reused the about:blank document's
  // SiteInstance, and then set its site to the blocked URL's. That assigned
  // the SiteInstance to the blocked URL's shard, which differs from the shared
  // process's shard, and hit a CHECK in ProcessLock::SetIsUsed(). Since
  // shards are peers, blocked error pages now commit in the destination's
  // process instead when the BrowserContext has shards.
  //
  // If `load_about_blank_first` is true, window 2 loads about:blank before
  // window 1 loads `first_host`, rather than afterwards. This is needed when
  // `first_host` is in the embedder shard: SiteInstances without a site are
  // treated as being in the default shard when looking for a process to reuse,
  // so about:blank won't reuse a process that's already in the embedder shard.
  // Instead, window 1's document reuses window 2's unused process.
  void TestBlockedSubframeErrorPageInSharedProcess(
      std::string_view first_host,
      SiteShard first_shard,
      std::string_view blocked_host,
      SiteShard blocked_shard,
      bool load_about_blank_first) {
    ASSERT_NE(first_shard, blocked_shard);
    RenderProcessHost::SetMaxRendererProcessCount(1);

    // Window 2: load about:blank in a new window, and thus a new
    // BrowsingInstance. Its SiteInstance has no site, and because of the
    // process limit, it shares a process with window 1.
    Shell* window2 = CreateBrowser();
    auto* window2_contents =
        static_cast<WebContentsImpl*>(window2->web_contents());
    auto load_window2 = [&] {
      EXPECT_TRUE(NavigateToURL(window2, GURL(url::kAboutBlankURL)));
      EXPECT_FALSE(window2_contents->GetPrimaryMainFrame()
                       ->GetSiteInstance()
                       ->HasSite());
    };
    if (load_about_blank_first) {
      load_window2();
    }

    // Window 1: load a document in `first_shard`. Its process is now used, and
    // assigned to that shard.
    EXPECT_TRUE(NavigateToURL(shell(), GetURL(first_host)));
    RenderFrameHostImpl* window1_rfh = web_contents()->GetPrimaryMainFrame();
    RenderProcessHost* shared_process = window1_rfh->GetProcess();
    EXPECT_EQ(first_shard, GetSiteInstanceShard(window1_rfh));
    EXPECT_EQ(first_shard, GetProcessShard(window1_rfh));

    if (!load_about_blank_first) {
      load_window2();
    }
    RenderFrameHostImpl* window2_rfh = window2_contents->GetPrimaryMainFrame();
    EXPECT_FALSE(window2_rfh->GetSiteInstance()->HasSite());
    EXPECT_EQ(shared_process, window2_rfh->GetProcess());

    // Block subframe navigations in window 2.
    EXPECT_TRUE(ExecJs(window2_rfh,
                       "const meta = document.createElement('meta');"
                       "meta.httpEquiv = 'Content-Security-Policy';"
                       "meta.content = \"frame-src 'none'\";"
                       "document.head.appendChild(meta);"));

    // Add a subframe for a URL in `blocked_shard`, which is blocked by CSP.
    const GURL blocked_url = GetURL(blocked_host);
    TestNavigationObserver observer(window2_contents);
    EXPECT_TRUE(ExecJs(window2_rfh,
                       JsReplace("const f = document.createElement('iframe');"
                                 "f.src = $1;"
                                 "document.body.appendChild(f);",
                                 blocked_url)));
    observer.Wait();
    EXPECT_FALSE(observer.last_navigation_succeeded());
    EXPECT_EQ(net::ERR_BLOCKED_BY_CSP, observer.last_net_error_code());
    EXPECT_EQ(blocked_url, observer.last_navigation_url());

    FrameTreeNode* root = window2_contents->GetPrimaryFrameTree().root();
    ASSERT_EQ(1U, root->child_count());
    RenderFrameHostImpl* error_rfh = root->child_at(0)->current_frame_host();
    EXPECT_TRUE(error_rfh->IsErrorDocument());

    // The error page commits in a process in the blocked URL's shard, rather
    // than in the shared process. Its opaque origin's precursor is the blocked
    // URL, which belongs in that shard.
    EXPECT_TRUE(error_rfh->GetLastCommittedOrigin().opaque());
    EXPECT_EQ(
        url::SchemeHostPort(blocked_url),
        error_rfh->GetLastCommittedOrigin().GetTupleOrPrecursorTupleIfOpaque());
    EXPECT_NE(shared_process, error_rfh->GetProcess());
    EXPECT_EQ(blocked_shard, GetSiteInstanceShard(error_rfh));
    EXPECT_EQ(blocked_shard, GetProcessShard(error_rfh));

    // Window 2's main frame stays in the shared process, which is still in
    // `first_shard`.
    EXPECT_EQ(window2_rfh, window2_contents->GetPrimaryMainFrame());
    EXPECT_EQ(shared_process, window2_rfh->GetProcess());
    EXPECT_EQ(first_shard, GetProcessShard(window2_rfh));

    for (RenderFrameHostImpl* rfh : {window1_rfh, window2_rfh, error_rfh}) {
      ExpectAlive(rfh);
    }

    RenderProcessHost::SetMaxRendererProcessCount(0);
  }

 private:
  class NoErrorPageIsolationContentBrowserClient
      : public ContentBrowserTestContentBrowserClient {
   public:
    bool ShouldIsolateErrorPage(bool in_main_frame) override { return false; }
  };

  std::unique_ptr<NoErrorPageIsolationContentBrowserClient> client_;
};

// Regression test for the crash in TestBlockedSubframeErrorPageInSharedProcess,
// where the blocked subframe is in the embedder shard.
IN_PROC_BROWSER_TEST_F(NoErrorPageIsolationShardBrowserTest,
                       BlockedSubframeErrorPageInSharedProcess) {
  TestBlockedSubframeErrorPageInSharedProcess(
      "unisolated.com", SiteShard::kDefault, "isolated.com",
      SiteShard::kEmbedderSites, /*load_about_blank_first=*/false);
}

// Like BlockedSubframeErrorPageInSharedProcess, but the shared process is in
// the embedder shard and the blocked subframe is in the default shard.
IN_PROC_BROWSER_TEST_F(NoErrorPageIsolationShardBrowserTest,
                       BlockedDefaultShardSubframeErrorPageInSharedProcess) {
  TestBlockedSubframeErrorPageInSharedProcess(
      "isolated.com", SiteShard::kEmbedderSites, "unisolated.com",
      SiteShard::kDefault, /*load_about_blank_first=*/true);
}

// Like BlockedSubframeErrorPageInSharedProcess, but for a renderer-initiated
// main frame navigation from the about:blank document, which is blocked by a
// NavigationThrottle. This used to hit the same CHECK, since the error page
// reused the about:blank document's SiteInstance.
IN_PROC_BROWSER_TEST_F(NoErrorPageIsolationShardBrowserTest,
                       BlockedMainFrameErrorPageInSharedProcess) {
  RenderProcessHost::SetMaxRendererProcessCount(1);

  // Window 1: load a document in the default shard.
  EXPECT_TRUE(NavigateToURL(shell(), GetURL("unisolated.com")));
  RenderFrameHostImpl* window1_rfh = web_contents()->GetPrimaryMainFrame();
  RenderProcessHost* shared_process = window1_rfh->GetProcess();
  EXPECT_EQ(SiteShard::kDefault, GetSiteInstanceShard(window1_rfh));
  EXPECT_EQ(SiteShard::kDefault, GetProcessShard(window1_rfh));

  // Window 2: load about:blank, which reuses window 1's process.
  Shell* window2 = CreateBrowser();
  EXPECT_TRUE(NavigateToURL(window2, GURL(url::kAboutBlankURL)));
  auto* window2_contents =
      static_cast<WebContentsImpl*>(window2->web_contents());
  RenderFrameHostImpl* window2_rfh = window2_contents->GetPrimaryMainFrame();
  EXPECT_FALSE(window2_rfh->GetSiteInstance()->HasSite());
  EXPECT_EQ(shared_process, window2_rfh->GetProcess());

  // Block the navigation to a URL in the embedder shard.
  const GURL blocked_url = GetURL("isolated.com");
  TestNavigationThrottleInserter throttle_inserter(
      window2_contents,
      base::BindLambdaForTesting([&](NavigationThrottleRegistry& registry) {
        if (registry.GetNavigationHandle().GetURL() != blocked_url) {
          return;
        }
        auto throttle = std::make_unique<TestNavigationThrottle>(registry);
        throttle->SetResponse(TestNavigationThrottle::WILL_START_REQUEST,
                              TestNavigationThrottle::SYNCHRONOUS,
                              NavigationThrottle::BLOCK_REQUEST);
        registry.AddThrottle(std::move(throttle));
      }));

  // Navigate window 2's main frame from the renderer.
  TestNavigationObserver observer(window2_contents);
  EXPECT_TRUE(
      ExecJs(window2_rfh, JsReplace("location.href = $1;", blocked_url)));
  observer.Wait();
  EXPECT_FALSE(observer.last_navigation_succeeded());
  EXPECT_EQ(net::ERR_BLOCKED_BY_CLIENT, observer.last_net_error_code());

  // The error page commits in a process in the embedder shard.
  RenderFrameHostImpl* error_rfh = window2_contents->GetPrimaryMainFrame();
  EXPECT_TRUE(error_rfh->IsErrorDocument());
  EXPECT_EQ(
      url::SchemeHostPort(blocked_url),
      error_rfh->GetLastCommittedOrigin().GetTupleOrPrecursorTupleIfOpaque());
  EXPECT_NE(shared_process, error_rfh->GetProcess());
  EXPECT_EQ(SiteShard::kEmbedderSites, GetSiteInstanceShard(error_rfh));
  EXPECT_EQ(SiteShard::kEmbedderSites, GetProcessShard(error_rfh));
  ExpectAlive(error_rfh);

  // The shared process is still in the default shard.
  EXPECT_EQ(SiteShard::kDefault, GetProcessShard(window1_rfh));
  ExpectAlive(window1_rfh);

  RenderProcessHost::SetMaxRendererProcessCount(0);
}

// Tests where a main frame error page commits, without error page isolation,
// when a navigation to a URL in another shard fails with a network error.
// Network errors aren't blocked requests, so ComputeErrorPageProcess() returns
// kDestinationProcess, and the error page commits in the destination's shard
// and process, with an opaque origin whose precursor is the failed URL.
IN_PROC_BROWSER_TEST_F(NoErrorPageIsolationShardBrowserTest,
                       FailedMainFrameNavigationToOtherShard) {
  const struct {
    std::string_view start_host;
    std::string_view dest_host;
    SiteShard dest_shard;
  } kTestCases[] = {
      {"unisolated.com", "isolated.com", SiteShard::kEmbedderSites},
      {"isolated.com", "unisolated.com", SiteShard::kDefault},
  };
  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(test_case.dest_host);
    EXPECT_TRUE(NavigateToURL(shell(), GetURL(test_case.start_host)));
    const ChildProcessId start_process_id =
        web_contents()->GetPrimaryMainFrame()->GetProcess()->GetID();

    const GURL error_url = GetURL(test_case.dest_host, "/close-socket");
    TestNavigationObserver observer(web_contents());
    shell()->LoadURL(error_url);
    observer.Wait();
    EXPECT_FALSE(observer.last_navigation_succeeded());
    EXPECT_EQ(net::ERR_EMPTY_RESPONSE, observer.last_net_error_code());

    RenderFrameHostImpl* error_rfh = web_contents()->GetPrimaryMainFrame();
    EXPECT_TRUE(error_rfh->IsErrorDocument());
    EXPECT_FALSE(error_rfh->GetSiteInstance()->GetSiteInfo().is_error_page());
    EXPECT_TRUE(error_rfh->GetLastCommittedOrigin().opaque());
    EXPECT_EQ(
        url::SchemeHostPort(error_url),
        error_rfh->GetLastCommittedOrigin().GetTupleOrPrecursorTupleIfOpaque());
    EXPECT_NE(start_process_id, error_rfh->GetProcess()->GetID());
    EXPECT_EQ(test_case.dest_shard, GetSiteInstanceShard(error_rfh));
    EXPECT_EQ(test_case.dest_shard, GetProcessShard(error_rfh));
    ExpectAlive(error_rfh);
  }
}

// Tests that a blocked subframe navigation to another site in the same shard as
// its parent, without error page isolation, commits the error page in the
// parent's process, so blocked error pages only need a new process when the
// shard changes. Since the BrowserContext has shards, the error page commits
// in the destination's process (see ComputeErrorPageProcess()), so its opaque
// origin's precursor is the blocked URL.
IN_PROC_BROWSER_TEST_F(NoErrorPageIsolationShardBrowserTest,
                       BlockedSameShardSubframeErrorPage) {
  const struct {
    std::string_view main_host;
    std::string_view blocked_host;
    SiteShard shard;
  } kTestCases[] = {
      {"unisolated.com", "unisolated2.com", SiteShard::kDefault},
      {"isolated.com", "isolated2.com", SiteShard::kEmbedderSites},
  };
  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(test_case.blocked_host);
    EXPECT_TRUE(NavigateToURL(shell(), GetURL(test_case.main_host)));
    RenderFrameHostImpl* main_rfh = web_contents()->GetPrimaryMainFrame();

    // Block subframe navigations with CSP.
    EXPECT_TRUE(ExecJs(main_rfh,
                       "const meta = document.createElement('meta');"
                       "meta.httpEquiv = 'Content-Security-Policy';"
                       "meta.content = \"frame-src 'none'\";"
                       "document.head.appendChild(meta);"));

    const GURL blocked_url = GetURL(test_case.blocked_host);
    TestNavigationObserver observer(web_contents());
    EXPECT_TRUE(
        ExecJs(main_rfh, JsReplace("const f = document.createElement('iframe');"
                                   "f.src = $1;"
                                   "document.body.appendChild(f);",
                                   blocked_url)));
    observer.Wait();
    EXPECT_EQ(net::ERR_BLOCKED_BY_CSP, observer.last_net_error_code());

    FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();
    ASSERT_EQ(1U, root->child_count());
    RenderFrameHostImpl* error_rfh = root->child_at(0)->current_frame_host();
    EXPECT_TRUE(error_rfh->IsErrorDocument());
    EXPECT_EQ(
        url::SchemeHostPort(blocked_url),
        error_rfh->GetLastCommittedOrigin().GetTupleOrPrecursorTupleIfOpaque());
    EXPECT_EQ(main_rfh->GetProcess(), error_rfh->GetProcess());
    EXPECT_EQ(test_case.shard, GetSiteInstanceShard(error_rfh));
    EXPECT_EQ(test_case.shard, GetProcessShard(error_rfh));
    ExpectAlive(main_rfh);
    ExpectAlive(error_rfh);
  }
}

// Tests an about:srcdoc subframe in an about:blank document whose SiteInstance
// has no site, but whose process has since been used for the embedder shard by
// another window, because of the process limit. The srcdoc document inherits
// the about:blank document's opaque origin, which has no precursor, so it is in
// the default shard.
//
// TODO(crbug.com/577044583): This currently crashes the browser with
// "Attempting to change the shard of a process" in ProcessLock::SetIsUsed().
// The srcdoc navigation reuses the site-less source SiteInstance without a
// suitability check (see
// RenderFrameHostManager::DetermineSiteInstanceForURL()). Then
// NavigationRequest::SelectFrameHostForCrossDocumentNavigationWithNoUrlLoader()
// calls ConvertToDefaultOrSetSite(), which assigns the SiteInstance to the
// default shard while its process is already used for the embedder shard. This
// is a separate issue from blocked error pages (see
// TestBlockedSubframeErrorPageInSharedProcess()). Enable this test once it's
// fixed.
IN_PROC_BROWSER_TEST_F(ShardBrowserTest,
                       DISABLED_SrcdocSubframeInSitelessSharedProcess) {
  RenderProcessHost::SetMaxRendererProcessCount(1);

  // Window 2 loads about:blank first, so that window 1's document in the
  // embedder shard reuses its unused process (see
  // TestBlockedSubframeErrorPageInSharedProcess()).
  Shell* window2 = CreateBrowser();
  EXPECT_TRUE(NavigateToURL(window2, GURL(url::kAboutBlankURL)));
  auto* window2_contents =
      static_cast<WebContentsImpl*>(window2->web_contents());

  EXPECT_TRUE(NavigateToURL(shell(), GetURL("isolated.com")));
  RenderFrameHostImpl* window1_rfh = web_contents()->GetPrimaryMainFrame();
  RenderProcessHost* shared_process = window1_rfh->GetProcess();
  EXPECT_EQ(SiteShard::kEmbedderSites, GetSiteInstanceShard(window1_rfh));
  EXPECT_EQ(SiteShard::kEmbedderSites, GetProcessShard(window1_rfh));

  RenderFrameHostImpl* window2_rfh = window2_contents->GetPrimaryMainFrame();
  EXPECT_FALSE(window2_rfh->GetSiteInstance()->HasSite());
  EXPECT_EQ(shared_process, window2_rfh->GetProcess());

  TestNavigationObserver observer(window2_contents);
  EXPECT_TRUE(ExecJs(window2_rfh,
                     "const f = document.createElement('iframe');"
                     "f.srcdoc = '<p>hi</p>';"
                     "document.body.appendChild(f);"));
  observer.Wait();
  FrameTreeNode* root = window2_contents->GetPrimaryFrameTree().root();
  ASSERT_EQ(1U, root->child_count());
  RenderFrameHostImpl* srcdoc_rfh = root->child_at(0)->current_frame_host();
  EXPECT_TRUE(srcdoc_rfh->GetLastCommittedURL().IsAboutSrcdoc());

  // The srcdoc document stays in its parent's SiteInstance and process, and
  // its SiteInstance's shard must match its process's shard.
  // TODO(crbug.com/577044583): The expected shard depends on how the crash is
  // fixed, so for now this only checks that the SiteInstance and process are
  // consistent. Check for a specific shard once it's fixed.
  EXPECT_EQ(window2_rfh->GetSiteInstance(), srcdoc_rfh->GetSiteInstance());
  EXPECT_EQ(window2_rfh->GetProcess(), srcdoc_rfh->GetProcess());
  EXPECT_EQ(GetSiteInstanceShard(srcdoc_rfh), GetProcessShard(srcdoc_rfh));
  ExpectAlive(window1_rfh);
  ExpectAlive(window2_rfh);
  ExpectAlive(srcdoc_rfh);

  RenderProcessHost::SetMaxRendererProcessCount(0);
}

}  // namespace content
