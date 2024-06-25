// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "content/browser/renderer_host/render_frame_host_manager_browsertest.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "content/shell/browser/shell.h"
#include "content/test/content_browser_test_utils_internal.h"
#include "content/test/render_document_feature.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

namespace content {

// Fully enable RenderDocument for speculative RenderViewHost tests. This is
// done by parameterizing this test suite with a single test parameter.
using SpeculativeRenderViewHostTest = RenderFrameHostManagerTest;

// Check that FrameTree::speculative_render_view_host_ is used correctly.
IN_PROC_BROWSER_TEST_P(SpeculativeRenderViewHostTest,
                       SameSiteInstanceGroupCase) {
  // Disable BFCache because otherwise the BrowsingInstances will be proactively
  // swapped and the navigation will not be same-SiteInstanceGroup, which is
  // the only case a speculative RenderViewHost is used.
  DisableBackForwardCache(
      BackForwardCacheImpl::TEST_ASSUMES_NO_RENDER_FRAME_CHANGE);

  StartEmbeddedServer();

  // Open a page in SiteInstanceGroup A.
  GURL url1(embedded_test_server()->GetURL("a.com", "/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url1));

  // Open a different page in SiteInstanceGroup A. Stop the navigation when
  // there's a speculative RenderFrameHost for page 2.
  WebContentsImpl* web_contents =
      static_cast<WebContentsImpl*>(shell()->web_contents());
  GURL url2(embedded_test_server()->GetURL("a.com", "/title2.html"));
  TestNavigationManager navigation(web_contents, url2);
  shell()->LoadURL(url2);
  navigation.WaitForSpeculativeRenderFrameHostCreation();

  // Check that the speculative RenderViewHost exists, and that it matches the
  // RenderViewHost of the speculative RenderFrameHost.
  FrameTree& frame_tree = web_contents->GetPrimaryFrameTree();
  FrameTreeNode* root = frame_tree.root();
  RenderFrameHostImpl* speculative_rfh =
      root->render_manager()->speculative_frame_host();
  EXPECT_TRUE(speculative_rfh);
  EXPECT_TRUE(frame_tree.speculative_render_view_host());
  RenderViewHostImpl* speculative_rvh = speculative_rfh->render_view_host();
  EXPECT_EQ(speculative_rvh, frame_tree.speculative_render_view_host());
  EXPECT_EQ(speculative_rvh->rvh_map_id(),
            frame_tree.GetRenderViewHostMapId(
                speculative_rfh->GetSiteInstance()->group()));

  // The speculative RenderViewHost and the current main frame RenderViewHost
  // should have the same RenderViewHostMap ID, since they're in the same
  // SiteInstanceGroup, but are two different RenderViewHosts.
  EXPECT_NE(speculative_rvh, root->current_frame_host()->render_view_host());
  EXPECT_EQ(speculative_rvh->rvh_map_id(),
            root->current_frame_host()->render_view_host()->rvh_map_id());

  // Expect that the speculative RenderViewHost was swapped in correctly.
  EXPECT_TRUE(navigation.WaitForNavigationFinished());
  EXPECT_EQ(root->current_frame_host()->render_view_host(), speculative_rvh);
  EXPECT_FALSE(frame_tree.speculative_render_view_host());
}

// Test that cross-SiteInstanceGroup navigations do not yet use speculative
// RenderViewHosts.
IN_PROC_BROWSER_TEST_P(SpeculativeRenderViewHostTest,
                       CrossSiteInstanceGroupNavigation) {
  StartEmbeddedServer();

  // Open a page in SiteInstanceGroup A.
  GURL url1(embedded_test_server()->GetURL("a.com", "/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url1));

  // Open a different page in SiteInstanceGroup B. Stop the navigation when
  // there's a speculative RenderFrameHost for page 2.
  WebContentsImpl* web_contents =
      static_cast<WebContentsImpl*>(shell()->web_contents());
  GURL url2(embedded_test_server()->GetURL("b.com", "/title1.html"));
  TestNavigationManager navigation(web_contents, url2);
  shell()->LoadURL(url2);
  navigation.WaitForSpeculativeRenderFrameHostCreation();

  // There should be a speculative RenderFrameHost with a RenderViewHost, but
  // the RenderViewHost should not be speculative.
  FrameTree& frame_tree = web_contents->GetPrimaryFrameTree();
  RenderFrameHostImpl* speculative_rfh =
      frame_tree.root()->render_manager()->speculative_frame_host();
  EXPECT_TRUE(speculative_rfh);
  EXPECT_TRUE(speculative_rfh->render_view_host());
  EXPECT_FALSE(frame_tree.speculative_render_view_host());

  EXPECT_TRUE(navigation.WaitForNavigationFinished());
}

// Check that FrameTree::speculative_render_view_host_ is removed when the
// navigation is cancelled before completing.
IN_PROC_BROWSER_TEST_P(SpeculativeRenderViewHostTest,
                       SpeculativeRenderViewHostCreatedNotUsed) {
  // Disable BFCache because otherwise the BrowsingInstances will be proactively
  // swapped and the navigation will not be same-SiteInstanceGroup, which is
  // the only case a speculative RenderViewHost is used.
  DisableBackForwardCache(
      BackForwardCacheImpl::TEST_ASSUMES_NO_RENDER_FRAME_CHANGE);

  StartEmbeddedServer();

  // Open a page in SiteInstanceGroup A.
  GURL url1(embedded_test_server()->GetURL("a.com", "/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url1));

  // Open a different page in SiteInstanceGroup A. Stop the navigation when
  // there's a speculative RenderFrameHost for page2.
  WebContentsImpl* web_contents =
      static_cast<WebContentsImpl*>(shell()->web_contents());
  GURL url2(embedded_test_server()->GetURL("a.com", "/title2.html"));
  TestNavigationManager navigation(web_contents, url2);
  shell()->LoadURL(url2);
  navigation.WaitForSpeculativeRenderFrameHostCreation();
  FrameTree& frame_tree = web_contents->GetPrimaryFrameTree();
  EXPECT_TRUE(frame_tree.speculative_render_view_host());

  // Cancel the navigation while there's still a speculative RenderFrameHost and
  // RenderViewHost.
  FrameTreeNode* root = frame_tree.root();
  root->navigator().CancelNavigation(
      root, NavigationDiscardReason::kExplicitCancellation);

  // Expect that the navigation finishes but doesn't commit. There should no
  // longer be a speculative RenderViewHost or RenderFrameHost. The current
  // RenderFrameHost's RenderViewHost should still be around.
  EXPECT_TRUE(navigation.WaitForNavigationFinished());
  EXPECT_FALSE(navigation.was_committed());
  EXPECT_FALSE(frame_tree.speculative_render_view_host());
  EXPECT_FALSE(root->render_manager()->speculative_frame_host());
  EXPECT_TRUE(root->current_frame_host()->render_view_host());
}

// With RenderDocument, a same-SiteInstanceGroup history navigation page should
// not be reusing RenderViewHosts, so check that a new speculative
// RenderViewHost is being created for history navigations.
IN_PROC_BROWSER_TEST_P(SpeculativeRenderViewHostTest, HistoryNavigation) {
  // Disable BFCache because otherwise the BrowsingInstances will be proactively
  // swapped and the navigation will not be same-SiteInstanceGroup, which is
  // the only case a speculative RenderViewHost is used.
  DisableBackForwardCache(
      BackForwardCacheImpl::TEST_ASSUMES_NO_RENDER_FRAME_CHANGE);

  StartEmbeddedServer();

  // Open two pages in SiteInstanceGroup A, one after the other..
  GURL url1(embedded_test_server()->GetURL("a.com", "/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url1));
  GURL url2(embedded_test_server()->GetURL("a.com", "/title2.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url2));

  // Go back to the first page, but pause before it commits.
  WebContentsImpl* web_contents =
      static_cast<WebContentsImpl*>(shell()->web_contents());
  NavigationControllerImpl& controller =
      static_cast<NavigationControllerImpl&>(web_contents->GetController());
  TestNavigationManager navigation(web_contents, url1);
  ASSERT_TRUE(controller.CanGoBack());
  controller.GoBack();
  navigation.WaitForSpeculativeRenderFrameHostCreation();

  // Check that the speculative RenderViewHost exists, and that it matches the
  // RenderViewHost of the speculative RenderFrameHost.
  RenderFrameHostImpl* speculative_rfh = web_contents->GetPrimaryFrameTree()
                                             .root()
                                             ->render_manager()
                                             ->speculative_frame_host();
  FrameTree& frame_tree = web_contents->GetPrimaryFrameTree();
  EXPECT_TRUE(speculative_rfh);
  RenderViewHostImpl* speculative_rvh = speculative_rfh->render_view_host();
  EXPECT_TRUE(frame_tree.speculative_render_view_host());
  EXPECT_EQ(speculative_rvh->rvh_map_id(),
            frame_tree.GetRenderViewHostMapId(
                speculative_rfh->GetSiteInstance()->group()));
  EXPECT_NE(frame_tree.root()->current_frame_host()->render_view_host(),
            speculative_rvh);

  // Expect that the former speculative RenderViewHost is now the RenderViewHost
  // of the current main frame RenderFrameHost.
  EXPECT_TRUE(navigation.WaitForNavigationFinished());
  EXPECT_EQ(frame_tree.root()->current_frame_host()->render_view_host(),
            speculative_rvh);
}

// Check that opener proxies are unaffected in a navigation that has a
// speculative RenderViewHost. When navigating a tab with an opener proxy, the
// opener proxy to the tab shouldn't change when the page that was opened
// navigates.
IN_PROC_BROWSER_TEST_P(SpeculativeRenderViewHostTest, OpenerProxies) {
  // Disable BFCache because otherwise the BrowsingInstances will be proactively
  // swapped and the navigation will not be same-SiteInstanceGroup, which is
  // the only case a speculative RenderViewHost is used.
  DisableBackForwardCache(
      BackForwardCacheImpl::TEST_ASSUMES_NO_RENDER_FRAME_CHANGE);

  StartEmbeddedServer();
  IsolateAllSitesForTesting(base::CommandLine::ForCurrentProcess());

  // Open page A in a tab. From there, window.open page B in another tab.
  GURL url1(embedded_test_server()->GetURL("a.com", "/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url1));
  WebContentsImpl* contents_a =
      static_cast<WebContentsImpl*>(shell()->web_contents());
  FrameTreeNode* root_a = contents_a->GetPrimaryFrameTree().root();
  RenderViewHost* rvh_a = root_a->current_frame_host()->render_view_host();

  GURL url2(embedded_test_server()->GetURL("b.com", "/title1.html"));
  ShellAddedObserver new_shell_observer;
  EXPECT_TRUE(ExecJs(root_a, JsReplace("window.open($1)", url2)));
  Shell* new_shell = new_shell_observer.GetShell();
  WebContentsImpl* contents_b =
      static_cast<WebContentsImpl*>(new_shell->web_contents());
  EXPECT_TRUE(WaitForLoadStop(contents_b));

  // Store the current proxies.
  FrameTreeNode* root_b = contents_b->GetPrimaryFrameTree().root();
  RenderViewHost* rvh_b = root_b->current_frame_host()->render_view_host();
  SiteInstanceGroup* site_instance_group_a =
      root_a->current_frame_host()->GetSiteInstance()->group();
  SiteInstanceGroup* site_instance_group_b =
      root_b->current_frame_host()->GetSiteInstance()->group();
  RenderFrameProxyHost* proxy_of_root_a_in_sig_b =
      root_a->current_frame_host()
          ->browsing_context_state()
          ->GetRenderFrameProxyHost(site_instance_group_b);
  RenderFrameProxyHost* proxy_of_root_b_in_sig_a =
      root_b->current_frame_host()
          ->browsing_context_state()
          ->GetRenderFrameProxyHost(site_instance_group_a);

  // The RenderViewHosts of the proxies and the pages should be different.
  EXPECT_NE(proxy_of_root_a_in_sig_b->GetRenderViewHost(), rvh_a);
  EXPECT_NE(proxy_of_root_b_in_sig_a->GetRenderViewHost(), rvh_b);

  // Do a same-SiteInstanceGroup navigation on B's tab. Check that a speculative
  // RenderViewHost is created. See SpeculativeRenderViewHost for more detailed
  // checks on the speculative RenderViewHost.
  GURL url3(embedded_test_server()->GetURL("b.com", "/title2.html"));
  TestNavigationManager navigation(contents_b, url3);
  new_shell->LoadURL(url3);
  navigation.WaitForSpeculativeRenderFrameHostCreation();
  EXPECT_TRUE(contents_b->GetPrimaryFrameTree().speculative_render_view_host());
  EXPECT_TRUE(navigation.WaitForNavigationFinished());

  // Check that the proxies are still the same.
  EXPECT_EQ(site_instance_group_b,
            root_b->current_frame_host()->GetSiteInstance()->group());
  EXPECT_EQ(proxy_of_root_a_in_sig_b,
            root_a->current_frame_host()
                ->browsing_context_state()
                ->GetRenderFrameProxyHost(site_instance_group_b));
  EXPECT_EQ(proxy_of_root_b_in_sig_a,
            root_b->current_frame_host()
                ->browsing_context_state()
                ->GetRenderFrameProxyHost(site_instance_group_a));

  // The RenderViewHosts of the proxies and the pages should still be different.
  EXPECT_NE(proxy_of_root_a_in_sig_b->GetRenderViewHost(),
            root_a->current_frame_host()->render_view_host());
  EXPECT_NE(proxy_of_root_b_in_sig_a->GetRenderViewHost(),
            root_b->current_frame_host()->render_view_host());

  // Since page B had a navigation, that should result in a new RenderViewHost
  // being created. Page A didn't change, so the RenderViewHost should be the
  // same as before B navigated.
  EXPECT_NE(rvh_b, root_b->current_frame_host()->render_view_host());
  EXPECT_EQ(rvh_a, root_a->current_frame_host()->render_view_host());
}

// Make sure any subframe proxies are deleted after a navigation that uses
// speculative RenderViewHosts.
IN_PROC_BROWSER_TEST_P(SpeculativeRenderViewHostTest, SubframeProxies) {
  // Disable BFCache because otherwise the BrowsingInstances will be proactively
  // swapped and the navigation will not be same-SiteInstanceGroup, which is
  // the only case a speculative RenderViewHost is used.
  DisableBackForwardCache(
      BackForwardCacheImpl::TEST_ASSUMES_NO_RENDER_FRAME_CHANGE);

  StartEmbeddedServer();

  IsolateAllSitesForTesting(base::CommandLine::ForCurrentProcess());
  // Open page A with a cross site subframe.
  GURL url1(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b)"));
  EXPECT_TRUE(NavigateToURL(shell(), url1));

  // Expect that there are proxies to the subframe.
  WebContentsImpl* web_contents =
      static_cast<WebContentsImpl*>(shell()->web_contents());
  FrameTreeNode* root_a = web_contents->GetPrimaryFrameTree().root();
  FrameTreeNode* subframe = root_a->current_frame_host()->child_at(0);
  SiteInstanceGroup* site_instance_group_b =
      subframe->current_frame_host()->GetSiteInstance()->group();
  EXPECT_TRUE(root_a->current_frame_host()
                  ->browsing_context_state()
                  ->GetRenderFrameProxyHost(site_instance_group_b));

  // Open a different page, without subframes, in SiteInstanceGroup A. Stop the
  // navigation when there's a speculative RenderFrameHost for page 2.
  GURL url2(embedded_test_server()->GetURL("a.com", "/title2.html"));
  TestNavigationManager navigation(web_contents, url2);
  shell()->LoadURL(url2);
  navigation.WaitForSpeculativeRenderFrameHostCreation();

  // Make sure a speculative RenderViewHost is involved.
  FrameTree& frame_tree = web_contents->GetPrimaryFrameTree();
  EXPECT_TRUE(frame_tree.speculative_render_view_host());

  // Complete the navigation. Expect that there is no longer a speculative
  // RenderViewHost, or any proxies.
  EXPECT_TRUE(navigation.WaitForNavigationFinished());
  EXPECT_FALSE(frame_tree.speculative_render_view_host());
  EXPECT_EQ(0u, root_a->current_frame_host()
                    ->browsing_context_state()
                    ->proxy_hosts()
                    .size());
}

// Crash a page, then navigate to a same-site URL. The new navigation should use
// a new RenderFrameHost and speculative RenderViewHost.
// TODO(crbug.com/1336305, yangsharon): This navigation is not using a
// speculative RenderViewHost, when it should be. Fix and enable this test.
IN_PROC_BROWSER_TEST_P(SpeculativeRenderViewHostTest,
                       DISABLED_CrashedRenderFrameHost) {
  // Disable BFCache because otherwise the BrowsingInstances will be proactively
  // swapped and the navigation will not be same-SiteInstanceGroup, which is
  // the only case a speculative RenderViewHost is used.
  DisableBackForwardCache(
      BackForwardCacheImpl::TEST_ASSUMES_NO_RENDER_FRAME_CHANGE);

  StartEmbeddedServer();

  // Open a page in SiteInstanceGroup A.
  GURL url(embedded_test_server()->GetURL("a.com", "/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url));

  WebContentsImpl* web_contents =
      static_cast<WebContentsImpl*>(shell()->web_contents());
  TestNavigationObserver observer(web_contents);

  // Crash the page.
  RenderProcessHost* process =
      web_contents->GetPrimaryMainFrame()->GetProcess();
  RenderProcessHostWatcher crash_observer(
      process, RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
  process->Shutdown(0);
  crash_observer.Wait();

  // Navigate to a same SiteInstanceGroup URL.
  GURL url2(embedded_test_server()->GetURL("a.com", "/title2.html"));
  TestNavigationManager navigation(web_contents, url2);
  shell()->LoadURL(url);
  EXPECT_TRUE(navigation.WaitForRequestStart());

  // Check that a speculative RenderViewHost is used for the navigation.
  FrameTree& frame_tree = web_contents->GetPrimaryFrameTree();
  RenderFrameHostImpl* speculative_rfh =
      frame_tree.root()->render_manager()->speculative_frame_host();
  EXPECT_TRUE(speculative_rfh);
  EXPECT_TRUE(frame_tree.speculative_render_view_host());
  RenderViewHostImpl* speculative_rvh = speculative_rfh->render_view_host();
  EXPECT_EQ(speculative_rvh->rvh_map_id(),
            frame_tree.GetRenderViewHostMapId(
                speculative_rfh->GetSiteInstance()->group()));

  // Expect that the speculative RenderViewHost was swapped in correctly.
  EXPECT_TRUE(navigation.WaitForNavigationFinished());
  EXPECT_EQ(frame_tree.root()->current_frame_host()->render_view_host(),
            speculative_rvh);
  EXPECT_FALSE(frame_tree.speculative_render_view_host());
}

// Check that FrameTree::ForEachRenderViewHost includes the speculative
// RenderViewHost when one exists.
IN_PROC_BROWSER_TEST_P(SpeculativeRenderViewHostTest, ForEachRenderViewHost) {
  // Disable BFCache because otherwise the BrowsingInstances will be proactively
  // swapped and the navigation will not be same-SiteInstanceGroup, which is
  // the only case a speculative RenderViewHost is used.
  DisableBackForwardCache(
      BackForwardCacheImpl::TEST_ASSUMES_NO_RENDER_FRAME_CHANGE);

  StartEmbeddedServer();

  // Open a page in SiteInstanceGroup A.
  GURL url1(embedded_test_server()->GetURL("a.com", "/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url1));

  // Navigate to a different page in SiteInstanceGroup A. Stop the navigation
  // when there's a speculative RenderFrameHost for page 2.
  WebContentsImpl* web_contents =
      static_cast<WebContentsImpl*>(shell()->web_contents());
  GURL url2(embedded_test_server()->GetURL("a.com", "/title2.html"));
  TestNavigationManager navigation(web_contents, url2);
  shell()->LoadURL(url2);
  navigation.WaitForSpeculativeRenderFrameHostCreation();

  // Check that the speculative RenderViewHost exists, and that it matches the
  // RenderViewHost of the speculative RenderFrameHost.
  FrameTree& frame_tree = web_contents->GetPrimaryFrameTree();
  RenderFrameHostImpl* speculative_rfh =
      frame_tree.root()->render_manager()->speculative_frame_host();
  EXPECT_TRUE(speculative_rfh);
  RenderViewHostImpl* speculative_rvh =
      frame_tree.speculative_render_view_host();
  EXPECT_TRUE(speculative_rvh);

  // Iterate over all RenderViewHosts, which should include the speculative
  // RenderViewHost.
  bool is_speculative = false;
  frame_tree.ForEachRenderViewHost(
      [&is_speculative, speculative_rvh](RenderViewHostImpl* rvh) {
        if (rvh->is_speculative()) {
          EXPECT_EQ(rvh, speculative_rvh);
          is_speculative = true;
        }
      });
  EXPECT_TRUE(is_speculative);

  // Once the navigation finishes, there should no longer be a speculative
  // RenderViewHost to iterate over.
  EXPECT_TRUE(navigation.WaitForNavigationFinished());
  is_speculative = false;
  frame_tree.ForEachRenderViewHost([&is_speculative](RenderViewHostImpl* rvh) {
    if (rvh->is_speculative()) {
      is_speculative = true;
    }
  });
  EXPECT_FALSE(is_speculative);
}

INSTANTIATE_TEST_SUITE_P(
    All,
    SpeculativeRenderViewHostTest,
    testing::ValuesIn(RenderDocumentFeatureFullyEnabled()));

}  // namespace content
