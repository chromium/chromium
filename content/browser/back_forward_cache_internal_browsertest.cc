// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/metrics_hashes.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/test/test_mock_time_task_runner.h"
#include "base/types/expected.h"
#include "build/build_config.h"
#include "content/browser/back_forward_cache_browsertest.h"
#include "content/browser/renderer_host/back_forward_cache_disable.h"
#include "content/browser/renderer_host/back_forward_cache_impl.h"
#include "content/browser/renderer_host/navigation_request.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/common/content_navigation_policy.h"
#include "content/common/features.h"
#include "content/public/browser/browser_accessibility_state.h"
#include "content/public/browser/disallow_activation_reason.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/accessibility_notification_waiter.h"
#include "content/public/test/back_forward_cache_util.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/fenced_frame_test_util.h"
#include "content/public/test/mock_web_contents_observer.h"
#include "content/public/test/scoped_accessibility_mode_override.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "content/shell/browser/shell.h"
#include "content/shell/browser/shell_content_browser_client.h"
#include "content/test/content_browser_test_utils_internal.h"
#include "content/test/did_commit_navigation_interceptor.h"
#include "content/test/echo.test-mojom.h"
#include "media/base/media_switches.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/controllable_http_response.h"
#include "third_party/blink/public/common/features.h"
#include "ui/accessibility/ax_action_data.h"
#include "ui/accessibility/ax_location_and_scroll_updates.h"
#include "ui/accessibility/ax_node_id_forward.h"
#include "ui/accessibility/platform/browser_accessibility.h"

// This file contains back/forward-cache tests that test or use internal
// features, e.g. cache-flushing, crashes, verifying proxies and other
// navigation internals. If you could write the test in JS or using only public
// functions it probably doesn't belong in this file. It was forked from
// https://source.chromium.org/chromium/chromium/src/+/main:content/browser/back_forward_cache_browsertest.cc;drc=db47c3a2e741f8ea55024e64ec932044024cbddc
//
// When adding tests consider also adding WPTs, although for internal tests,
// this is often not an option. See
// third_party/blink/web_tests/external/wpt/html/browsers/browsing-the-web/back-forward-cache/README.md

using testing::_;
using testing::Each;
using ::testing::ElementsAre;
using ::testing::Not;
using ::testing::UnorderedElementsAreArray;

namespace content {

using NotRestoredReason = BackForwardCacheMetrics::NotRestoredReason;

// Ensure flushing the BackForwardCache works properly.
IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest, BackForwardCacheFlush) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  // 1) Navigate to A.
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImpl* rfh_a = current_frame_host();
  RenderFrameDeletedObserver delete_observer_rfh_a(rfh_a);

  // 2) Navigate to B.
  EXPECT_TRUE(NavigateToURL(shell(), url_b));
  RenderFrameHostImpl* rfh_b = current_frame_host();
  RenderFrameDeletedObserver delete_observer_rfh_b(rfh_b);
  EXPECT_FALSE(delete_observer_rfh_a.deleted());

  // 3) Flush A.
  web_contents()->GetController().GetBackForwardCache().Flush();
  delete_observer_rfh_a.WaitUntilDeleted();
  EXPECT_FALSE(delete_observer_rfh_b.deleted());

  // 4) Go back to a new A.
  ASSERT_TRUE(HistoryGoBack(web_contents()));
  EXPECT_FALSE(delete_observer_rfh_b.deleted());

  // 5) Flush B.
  web_contents()->GetController().GetBackForwardCache().Flush();
  delete_observer_rfh_b.WaitUntilDeleted();
}

// Tests that |RenderFrameHost::ForEachRenderFrameHost| and
// |WebContents::ForEachRenderFrameHost| behave correctly with bfcached
// RenderFrameHosts.
IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest, ForEachRenderFrameHost) {
  // There are sometimes unexpected messages from a renderer to the browser,
  // which caused test flakiness on macOS.
  // TODO(crbug.com/40800266): Fix the test flakiness.
  DoNotFailForUnexpectedMessagesWhileCached();

  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b(c),d)"));
  GURL url_e(embedded_test_server()->GetURL("e.com", "/title1.html"));

  std::vector<RenderFrameDeletedObserver*> rfh_observers;

  // 1) Navigate to a(b(c),d).
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImpl* rfh_a = current_frame_host();
  RenderFrameHostImpl* rfh_b = rfh_a->child_at(0)->current_frame_host();
  RenderFrameHostImpl* rfh_c = rfh_b->child_at(0)->current_frame_host();
  RenderFrameHostImpl* rfh_d = rfh_a->child_at(1)->current_frame_host();
  RenderFrameDeletedObserver a_observer(rfh_a), b_observer(rfh_b),
      c_observer(rfh_c), d_observer(rfh_d);
  rfh_observers.insert(rfh_observers.end(),
                       {&a_observer, &b_observer, &c_observer, &d_observer});

  // Ensure the visited frames are what we would expect for the page before
  // entering bfcache.
  EXPECT_THAT(CollectAllRenderFrameHosts(rfh_a),
              ::testing::ElementsAre(rfh_a, rfh_b, rfh_d, rfh_c));
  EXPECT_THAT(CollectAllRenderFrameHosts(web_contents()),
              ::testing::ElementsAre(rfh_a, rfh_b, rfh_d, rfh_c));

  // 2) Navigate to e.
  EXPECT_TRUE(NavigateToURL(shell(), url_e));
  RenderFrameHostImpl* rfh_e = current_frame_host();
  RenderFrameDeletedObserver e_observer(rfh_e);
  rfh_observers.push_back(&e_observer);
  ASSERT_THAT(rfh_observers, Each(Not(Deleted())));
  EXPECT_THAT(Elements({rfh_a, rfh_b, rfh_c, rfh_d}),
              Each(InBackForwardCache()));
  EXPECT_THAT(rfh_e, Not(InBackForwardCache()));

  // When starting iteration from the primary frame, we shouldn't see any of the
  // frames in bfcache.
  EXPECT_THAT(CollectAllRenderFrameHosts(rfh_e), ::testing::ElementsAre(rfh_e));

  // When starting iteration from a bfcached RFH, we should see the frame itself
  // and its descendants in breadth first order.
  EXPECT_THAT(CollectAllRenderFrameHosts(rfh_a),
              ::testing::ElementsAre(rfh_a, rfh_b, rfh_d, rfh_c));

  // Ensure that starting iteration from a subframe of a bfcached frame also
  // works.
  EXPECT_THAT(CollectAllRenderFrameHosts(rfh_b),
              ::testing::ElementsAre(rfh_b, rfh_c));

  // When iterating over all RenderFrameHosts in a WebContents, we should see
  // the RFHs of both the primary page and the bfcached page.
  EXPECT_THAT(
      CollectAllRenderFrameHosts(web_contents()),
      ::testing::UnorderedElementsAre(rfh_a, rfh_b, rfh_c, rfh_d, rfh_e));

  {
    // If we stop iteration in |WebContents::ForEachRenderFrameHost|, we stop
    // the entire iteration, not just iteration in the page being iterated at
    // that point. In this case, if we stop iteration in the primary page, we do
    // not continue to iterate in the bfcached page.
    bool stopped = false;
    web_contents()->ForEachRenderFrameHostWithAction(
        [&](RenderFrameHostImpl* rfh) {
          EXPECT_FALSE(stopped);
          stopped = true;
          return RenderFrameHost::FrameIterationAction::kStop;
        });
  }

  EXPECT_EQ(nullptr, rfh_a->GetParentOrOuterDocument());
  EXPECT_EQ(rfh_a, rfh_b->GetParentOrOuterDocument());
  EXPECT_EQ(rfh_b, rfh_c->GetParentOrOuterDocument());
  EXPECT_EQ(rfh_a, rfh_d->GetParentOrOuterDocument());
  EXPECT_EQ(nullptr, rfh_e->GetParentOrOuterDocument());
  // The outermost document of a bfcached page is the bfcached main
  // RenderFrameHost, not the primary main RenderFrameHost.
  EXPECT_EQ(rfh_a, rfh_a->GetOutermostMainFrame());
  EXPECT_EQ(rfh_a, rfh_b->GetOutermostMainFrame());
  EXPECT_EQ(rfh_a, rfh_c->GetOutermostMainFrame());
  EXPECT_EQ(rfh_a, rfh_d->GetOutermostMainFrame());
  EXPECT_EQ(rfh_e, rfh_e->GetOutermostMainFrame());
  EXPECT_EQ(nullptr, rfh_a->GetParentOrOuterDocumentOrEmbedder());
  EXPECT_EQ(rfh_a, rfh_b->GetParentOrOuterDocumentOrEmbedder());
  EXPECT_EQ(rfh_b, rfh_c->GetParentOrOuterDocumentOrEmbedder());
  EXPECT_EQ(rfh_a, rfh_d->GetParentOrOuterDocumentOrEmbedder());
  EXPECT_EQ(nullptr, rfh_e->GetParentOrOuterDocumentOrEmbedder());
  EXPECT_EQ(rfh_a, rfh_a->GetOutermostMainFrameOrEmbedder());
  EXPECT_EQ(rfh_a, rfh_b->GetOutermostMainFrameOrEmbedder());
  EXPECT_EQ(rfh_a, rfh_c->GetOutermostMainFrameOrEmbedder());
  EXPECT_EQ(rfh_a, rfh_d->GetOutermostMainFrameOrEmbedder());
  EXPECT_EQ(rfh_e, rfh_e->GetOutermostMainFrameOrEmbedder());
}

// Tests that |RenderFrameHostImpl::ForEachRenderFrameHostIncludingSpeculative|
// and |WebContentsImpl::ForEachRenderFrameHostIncludingSpeculative|
// behave correctly when a FrameTreeNode has both a speculative RFH and a
// bfcached RFH.
IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest,
                       ForEachRenderFrameHostWithSpeculative) {
  IsolateAllSitesForTesting(base::CommandLine::ForCurrentProcess());
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));
  GURL url_c(embedded_test_server()->GetURL("c.com", "/title1.html"));

  std::vector<RenderFrameDeletedObserver*> rfh_observers;

  // 1) Navigate to a.
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImpl* rfh_a = current_frame_host();
  RenderFrameDeletedObserver a_observer(rfh_a);
  rfh_observers.push_back(&a_observer);

  // 2) Navigate to b.
  EXPECT_TRUE(NavigateToURL(shell(), url_b));
  RenderFrameHostImpl* rfh_b = current_frame_host();
  RenderFrameDeletedObserver b_observer(rfh_b);
  rfh_observers.push_back(&b_observer);
  ASSERT_THAT(rfh_observers, Each(Not(Deleted())));

  // 3) Begin navigation to c.
  TestNavigationManager nav_manager(web_contents(), url_c);
  shell()->LoadURL(url_c);
  nav_manager.WaitForSpeculativeRenderFrameHostCreation();

  RenderFrameHostImpl* rfh_c =
      rfh_b->frame_tree_node()->render_manager()->speculative_frame_host();
  ASSERT_TRUE(rfh_c);
  EXPECT_EQ(RenderFrameHostImpl::LifecycleStateImpl::kInBackForwardCache,
            rfh_a->lifecycle_state());
  EXPECT_FALSE(rfh_a->GetPage().IsPrimary());
  EXPECT_EQ(RenderFrameHostImpl::LifecycleStateImpl::kActive,
            rfh_b->lifecycle_state());
  EXPECT_TRUE(rfh_b->GetPage().IsPrimary());
  EXPECT_EQ(RenderFrameHostImpl::LifecycleStateImpl::kSpeculative,
            rfh_c->lifecycle_state());
  EXPECT_FALSE(rfh_c->GetPage().IsPrimary());

  // When starting iteration from the bfcached RFH, we should not see the
  // speculative RFH.
  EXPECT_THAT(CollectAllRenderFrameHostsIncludingSpeculative(rfh_a),
              ::testing::ElementsAre(rfh_a));

  // When starting iteration from the primary frame, we shouldn't see the
  // bfcached RFH, but we should see the speculative RFH.
  EXPECT_THAT(CollectAllRenderFrameHostsIncludingSpeculative(rfh_b),
              ::testing::UnorderedElementsAre(rfh_b, rfh_c));

  // When starting iteration from the speculative RFH, we should only see
  // the speculative RFH. In particular, we should not see the bfcached RFH.
  EXPECT_THAT(CollectAllRenderFrameHostsIncludingSpeculative(rfh_c),
              ::testing::ElementsAre(rfh_c));

  // When iterating over all RenderFrameHosts in a WebContents, we should see
  // the RFHs of both the primary page and the bfcached page.
  EXPECT_THAT(CollectAllRenderFrameHostsIncludingSpeculative(web_contents()),
              ::testing::UnorderedElementsAre(rfh_a, rfh_b, rfh_c));
}

IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest,
                       NavigationsAreFullyCommitted) {
  // Sometimes messages arrive from a renderer to browser for the page in
  // back/forward cache (message on content.mojom.FrameHost), because the input
  // task queue is currently not frozen. Do not fail for unexpected messages.
  DoNotFailForUnexpectedMessagesWhileCached();
  ASSERT_TRUE(embedded_test_server()->Start());

  // During a navigation, the document being navigated *away from* can either be
  // deleted or stored into the BackForwardCache. The document being navigated
  // *to* can either be new or restored from the BackForwardCache.
  //
  // This test covers every combination:
  //
  //  1. Navigate to a cacheable page (()->A)
  //  2. Navigate to an uncacheable page (A->B)
  //  3. Go Back to a cached page (B->A)
  //  4. Navigate to a cacheable page (A->C)
  //  5. Go Back to a cached page (C->A)
  //
  // +-+-------+----------------+---------------+
  // |#|nav    | curr_document  | dest_document |
  // +-+-------+----------------+---------------|
  // |1|(()->A)| N/A            | new           |
  // |2|(A->B) | cached         | new           |
  // |3|(B->A) | deleted        | restored      |
  // |4|(A->C) | cached         | new           |
  // |5|(C->A) | cached         | restored      |
  // +-+-------+----------------+---------------+
  //
  // As part of these navigations we check that LastCommittedURL was updated,
  // to verify that the frame wasn't simply swapped in without actually
  // committing.

  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));
  GURL url_c(embedded_test_server()->GetURL("c.com", "/title1.html"));

  // 1. Navigate to a cacheable page (A).
  ASSERT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImplWrapper rfh_a(current_frame_host());

  // 2. Navigate from a cacheable page to an uncacheable page (A->B).
  ASSERT_TRUE(NavigateToURL(shell(), url_b));
  EXPECT_EQ(web_contents()->GetLastCommittedURL(), url_b);
  RenderFrameHostImplWrapper rfh_b(current_frame_host());

  // Page A should be in the cache.
  EXPECT_FALSE(rfh_a.IsDestroyed());
  EXPECT_TRUE(rfh_a->IsInBackForwardCache());

  // 3. Navigate from an uncacheable to a cached page page (B->A).
  ASSERT_TRUE(HistoryGoBack(web_contents()));
  EXPECT_EQ(web_contents()->GetLastCommittedURL(), url_a);

  // Evict page B and wait until it is deleted.
  rfh_b->DisableBackForwardCache(RenderFrameHostDisabledForTestingReason());
  ASSERT_TRUE(rfh_b.WaitUntilRenderFrameDeleted());

  ExpectRestored(FROM_HERE);

  // 4. Navigate from a cacheable page to a cacheable page (A->C).
  ASSERT_TRUE(NavigateToURL(shell(), url_c));
  EXPECT_EQ(web_contents()->GetLastCommittedURL(), url_c);
  RenderFrameHostImpl* rfh_c = current_frame_host();
  RenderFrameDeletedObserver delete_observer_rfh_c(rfh_c);

  // Page A should be in the cache.
  EXPECT_FALSE(rfh_a.IsDestroyed());
  EXPECT_TRUE(rfh_a->IsInBackForwardCache());

  // 5. Navigate from a cacheable page to a cached page (C->A).
  ASSERT_TRUE(HistoryGoBack(web_contents()));
  EXPECT_EQ(web_contents()->GetLastCommittedURL(), url_a);

  // Page C should be in the cache.
  EXPECT_FALSE(delete_observer_rfh_c.deleted());
  EXPECT_TRUE(rfh_c->IsInBackForwardCache());

  ExpectRestored(FROM_HERE);
}

// Disabled due to flakiness on Linux and Mac https://crbug.com/1287467
// Disabled on Chrome OS due to flakiness https://crbug.com/1290834
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_CHROMEOS)
#define MAYBE_ProxiesAreStoredAndRestored DISABLED_ProxiesAreStoredAndRestored
#else
#define MAYBE_ProxiesAreStoredAndRestored ProxiesAreStoredAndRestored
#endif
IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest,
                       MAYBE_ProxiesAreStoredAndRestored) {
  // This test makes assumption about where iframe processes live.
  if (!AreAllSitesIsolatedForTesting())
    return;

  ASSERT_TRUE(embedded_test_server()->Start());

  // During a navigation, the document being navigated *away from* can either be
  // deleted or stored into the BackForwardCache. The document being navigated
  // *to* can either be new or restored from the BackForwardCache.
  //
  // This test covers every combination:
  //
  //  1. Navigate to a cacheable page (()->A)
  //  2. Navigate to an uncacheable page (A->B)
  //  3. Go Back to a cached page (B->A)
  //  4. Navigate to a cacheable page (A->C)
  //  5. Go Back to a cached page (C->A)
  //
  // +-+-------+----------------+---------------+
  // |#|nav    | curr_document  | dest_document |
  // +-+-------+----------------+---------------|
  // |1|(()->A)| N/A            | new           |
  // |2|(A->B) | cached         | new           |
  // |3|(B->A) | deleted        | restored      |
  // |4|(A->C) | cached         | new           |
  // |5|(C->A) | cached         | restored      |
  // +-+-------+----------------+---------------+
  //
  // We use pages with cross process iframes to verify that proxy storage and
  // retrieval works well in every possible combination.

  GURL url_a(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(i,j)"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));
  GURL url_c(embedded_test_server()->GetURL(
      "c.com", "/cross_site_iframe_factory.html?c(k,l,m)"));

  NavigationControllerImpl& controller = web_contents()->GetController();
  BackForwardCacheImpl& cache = controller.GetBackForwardCache();

  // 1. Navigate to a cacheable page (A).
  ASSERT_TRUE(NavigateToURL(shell(), url_a));
  EXPECT_EQ(2u, render_frame_host_manager()
                    ->current_frame_host()
                    ->browsing_context_state()
                    ->GetProxyCount());
  RenderFrameHostImplWrapper rfh_a(current_frame_host());
  std::string frame_tree_a = DepictFrameTree(rfh_a->frame_tree_node());

  // 2. Navigate from a cacheable page to an uncacheable page (A->B).
  ASSERT_TRUE(NavigateToURL(shell(), url_b));
  EXPECT_EQ(0u, render_frame_host_manager()
                    ->current_frame_host()
                    ->browsing_context_state()
                    ->GetProxyCount());
  RenderFrameHostImplWrapper rfh_b(current_frame_host());

  // Page A should be in the cache.
  EXPECT_FALSE(rfh_a.IsDestroyed());
  EXPECT_TRUE(rfh_a->IsInBackForwardCache());

  // Verify proxies are stored as well.
  auto cached_entry = cache.GetOrEvictEntry(rfh_a->nav_entry_id());
  EXPECT_TRUE(cached_entry.has_value());
  EXPECT_EQ(2u, cached_entry.value()->proxy_hosts_size());

  // 3. Navigate from an uncacheable to a cached page page (B->A).
  ASSERT_TRUE(HistoryGoBack(web_contents()));
  // Note: Since we put the page B into BackForwardCache briefly, we do not
  // create a transition proxy. So there should be only proxies for i.com and
  // j.com.
  EXPECT_EQ(2u, render_frame_host_manager()
                    ->current_frame_host()
                    ->browsing_context_state()
                    ->GetProxyCount());

  // Evict page B and wait until it is deleted.
  rfh_b->DisableBackForwardCache(RenderFrameHostDisabledForTestingReason());
  ASSERT_TRUE(rfh_b.WaitUntilRenderFrameDeleted());
  EXPECT_EQ(2u, render_frame_host_manager()
                    ->current_frame_host()
                    ->browsing_context_state()
                    ->GetProxyCount());

  // Page A should still have the correct frame tree.
  EXPECT_EQ(frame_tree_a,
            DepictFrameTree(current_frame_host()->frame_tree_node()));

  // 4. Navigate from a cacheable page to a cacheable page (A->C).
  ASSERT_TRUE(NavigateToURL(shell(), url_c));
  EXPECT_EQ(3u, render_frame_host_manager()
                    ->current_frame_host()
                    ->browsing_context_state()
                    ->GetProxyCount());
  RenderFrameHostImplWrapper rfh_c(current_frame_host());

  // Page A should be in the cache.
  EXPECT_FALSE(rfh_a.IsDestroyed());
  EXPECT_TRUE(rfh_a->IsInBackForwardCache());

  // Verify proxies are stored as well.
  cached_entry = cache.GetOrEvictEntry(rfh_a->nav_entry_id());
  EXPECT_TRUE(cached_entry.has_value());
  EXPECT_EQ(2u, cached_entry.value()->proxy_hosts_size());

  // 5. Navigate from a cacheable page to a cached page (C->A).
  ASSERT_TRUE(HistoryGoBack(web_contents()));
  EXPECT_EQ(2u, render_frame_host_manager()
                    ->current_frame_host()
                    ->browsing_context_state()
                    ->GetProxyCount());

  // Page A should still have the correct frame tree.
  EXPECT_EQ(frame_tree_a,
            DepictFrameTree(current_frame_host()->frame_tree_node()));

  // Page C should be in the cache.
  EXPECT_FALSE(rfh_c.IsDestroyed());
  EXPECT_TRUE(rfh_c->IsInBackForwardCache());

  // Verify proxies are stored as well.
  cached_entry = cache.GetOrEvictEntry(rfh_c->nav_entry_id());
  EXPECT_TRUE(cached_entry.has_value());
  EXPECT_EQ(3u, cached_entry.value()->proxy_hosts_size());
}

IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest,
                       RestoredProxiesAreFunctional) {
  // This test makes assumption about where iframe processes live.
  if (!AreAllSitesIsolatedForTesting())
    return;

  ASSERT_TRUE(embedded_test_server()->Start());

  // Page A is cacheable, while page B is not.
  GURL url_a(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(z)"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));
  GURL url_c(embedded_test_server()->GetURL("c.com", "/title2.html"));

  NavigationControllerImpl& controller = web_contents()->GetController();

  // 1. Navigate to a cacheable page (A).
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImpl* rfh_a = current_frame_host();

  // 2. Navigate from a cacheable page to an uncacheable page (A->B).
  EXPECT_TRUE(NavigateToURL(shell(), url_b));
  RenderFrameHostImpl* rfh_b = current_frame_host();
  DisableBFCacheForRFHForTesting(rfh_b);

  // 3. Navigate from an uncacheable to a cached page page (B->A).
  // This restores the top frame's proxy in the z.com (iframe's) process.
  ASSERT_TRUE(HistoryGoBack(web_contents()));

  // 4. Verify that the main frame's z.com proxy is still functional.
  RenderFrameHostImpl* iframe =
      rfh_a->frame_tree_node()->child_at(0)->current_frame_host();
  EXPECT_TRUE(ExecJs(iframe, "top.location.href = '" + url_c.spec() + "';"));
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));

  // We expect to have navigated through the proxy.
  EXPECT_EQ(url_c, controller.GetLastCommittedEntry()->GetURL());
}

// Flaky on Android, see crbug.com/1135601 and on other platforms, see
// crbug.com/1128772.
IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest,
                       DISABLED_LogIpcPostedToCachedFrame) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // 1) Navigate to a page.
  GURL url(embedded_test_server()->GetURL("/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url));
  RenderFrameHostImpl* rfh_a = current_frame_host();

  // 2) Navigate away. The first page should be in the cache.
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("b.com", "/title1.html")));

  // 3) Post IPC tasks to the page, testing mojo remote objects.

  // Send a message via an associated interface - which will post a task with an
  // IPC hash and will be routed to the per-thread task queue.
  base::RunLoop run_loop;
  rfh_a->RequestTextSurroundingSelection(
      base::BindOnce(
          [](base::RepeatingClosure quit_closure, const std::u16string& str,
             uint32_t num, uint32_t num2) { quit_closure.Run(); },
          run_loop.QuitClosure()),
      1);
  run_loop.Run();

  // 4) Check the histogram.
  base::HistogramBase::Sample sample = base::HistogramBase::Sample(
      base::TaskAnnotator::ScopedSetIpcHash::MD5HashMetricName(
          "blink.mojom.LocalFrame"));

  FetchHistogramsFromChildProcesses();
  EXPECT_TRUE(HistogramContainsIntValue(
      sample, histogram_tester().GetAllSamples(
                  "BackForwardCache.Experimental."
                  "UnexpectedIPCMessagePostedToCachedFrame.MethodHash")));
}

IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest,
                       DisableBackforwardCacheForTesting) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // Disable the BackForwardCache.
  web_contents()->GetController().GetBackForwardCache().DisableForTesting(
      BackForwardCacheImpl::TEST_REQUIRES_NO_CACHING);

  // Navigate to a page that would normally be cacheable.
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("a.com", "/title1.html")));
  RenderFrameDeletedObserver delete_observer_rfh_a(current_frame_host());

  // Navigate away.
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("b.com", "/title1.html")));

  // The page should be deleted (not cached).
  delete_observer_rfh_a.WaitUntilDeleted();
}

// Navigate from A to B, then cause JavaScript execution on A, then go back.
// Test the RenderFrameHost in the cache is evicted by JavaScript.
IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest,
                       EvictionOnJavaScriptExecution) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  // 1) Navigate to A.
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImpl* rfh_a = current_frame_host();
  RenderFrameDeletedObserver delete_observer_rfh_a(rfh_a);

  // 2) Navigate to B.
  EXPECT_TRUE(NavigateToURL(shell(), url_b));
  RenderFrameHostImpl* rfh_b = current_frame_host();
  RenderFrameDeletedObserver delete_observer_rfh_b(rfh_b);

  EXPECT_FALSE(delete_observer_rfh_a.deleted());
  EXPECT_FALSE(delete_observer_rfh_b.deleted());
  EXPECT_TRUE(rfh_a->IsInBackForwardCache());
  EXPECT_FALSE(rfh_b->IsInBackForwardCache());

  // 3) Execute JavaScript on A.
  EvictByJavaScript(rfh_a);

  // RenderFrameHost A is evicted from the BackForwardCache:
  delete_observer_rfh_a.WaitUntilDeleted();

  // 4) Go back to A.
  ASSERT_TRUE(HistoryGoBack(web_contents()));
  ExpectNotRestored({NotRestoredReason::kJavaScriptExecution}, {}, {}, {}, {},
                    FROM_HERE);
}

// Similar to BackForwardCacheBrowserTest.EvictionOnJavaScriptExecution.
// Test case: A(B) -> C -> JS on B -> A(B)
IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest,
                       EvictionOnJavaScriptExecutionIframe) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b)"));
  GURL url_c(embedded_test_server()->GetURL("c.com", "/title1.html"));

  // 1) Navigate to A(B).
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImpl* rfh_a = current_frame_host();
  RenderFrameHostImpl* rfh_b = rfh_a->child_at(0)->current_frame_host();
  RenderFrameDeletedObserver delete_observer_rfh_a(rfh_a);
  RenderFrameDeletedObserver delete_observer_rfh_b(rfh_b);

  // 2) Navigate to C.
  EXPECT_TRUE(NavigateToURL(shell(), url_c));
  RenderFrameHostImpl* rfh_c = current_frame_host();
  RenderFrameDeletedObserver delete_observer_rfh_c(rfh_c);

  EXPECT_FALSE(delete_observer_rfh_a.deleted());
  EXPECT_FALSE(delete_observer_rfh_b.deleted());
  EXPECT_FALSE(delete_observer_rfh_c.deleted());
  EXPECT_TRUE(rfh_a->IsInBackForwardCache());
  EXPECT_TRUE(rfh_b->IsInBackForwardCache());
  EXPECT_FALSE(rfh_c->IsInBackForwardCache());

  // 3) Execute JavaScript on B.
  //
  EvictByJavaScript(rfh_b);

  // The A(B) page is evicted. So A and B are removed:
  delete_observer_rfh_a.WaitUntilDeleted();
  delete_observer_rfh_b.WaitUntilDeleted();

  // 4) Go back to A(B).
  ASSERT_TRUE(HistoryGoBack(web_contents()));
  ExpectNotRestored({NotRestoredReason::kJavaScriptExecution}, {}, {}, {}, {},
                    FROM_HERE);
}

IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest,
                       EvictionOnJavaScriptExecutionInAnotherWorld) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  // 1) Navigate to A.
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImpl* rfh_a = current_frame_host();
  RenderFrameDeletedObserver delete_observer_rfh_a(rfh_a);

  // 2) Execute JavaScript on A in a new world. This ensures a new world.
  const int32_t kNewWorldId = content::ISOLATED_WORLD_ID_CONTENT_END + 1;
  EXPECT_TRUE(ExecJs(rfh_a, "console.log('hi');",
                     EXECUTE_SCRIPT_DEFAULT_OPTIONS, kNewWorldId));

  // 3) Navigate to B.
  EXPECT_TRUE(NavigateToURL(shell(), url_b));
  RenderFrameHostImpl* rfh_b = current_frame_host();
  RenderFrameDeletedObserver delete_observer_rfh_b(rfh_b);

  EXPECT_FALSE(delete_observer_rfh_a.deleted());
  EXPECT_FALSE(delete_observer_rfh_b.deleted());
  EXPECT_TRUE(rfh_a->IsInBackForwardCache());
  EXPECT_FALSE(rfh_b->IsInBackForwardCache());

  // 4) Execute JavaScript on A in the new world.
  EXPECT_FALSE(ExecJs(rfh_a, "console.log('hi');",
                      EXECUTE_SCRIPT_DEFAULT_OPTIONS, kNewWorldId));

  // RenderFrameHost A is evicted from the BackForwardCache:
  delete_observer_rfh_a.WaitUntilDeleted();

  // 5) Go back to A.
  ASSERT_TRUE(HistoryGoBack(web_contents()));
  ExpectNotRestored({NotRestoredReason::kJavaScriptExecution}, {}, {}, {}, {},
                    FROM_HERE);
}

// Navigate from A(B)->C. Send postMessage from A to B upon pagehide, and
IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest, PostMessageDelivered) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b)"));
  GURL url_c(embedded_test_server()->GetURL("c.com", "/title1.html"));

  // 1) Navigate to A(B).
  ASSERT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImplWrapper rfh_a(current_frame_host());
  RenderFrameHostImplWrapper rfh_b(rfh_a->child_at(0)->current_frame_host());
  // Register message handler for b.com.
  ASSERT_TRUE(ExecJs(rfh_b.get(), R"(
      localStorage.setItem('postMessage_dispatched', 'not_dispatched');
      window.addEventListener('message', (event) => {
          console.log(`Received message: ${event.data}`);
          localStorage.setItem('postMessage_dispatched', 'dispatched');
      });
  )"));
  // Register pagehide handler for a.com. Inside pagehide handler, send a
  // postMessage to b.com.
  ASSERT_TRUE(ExecJs(rfh_a.get(), R"(
      window.addEventListener("pagehide", (event) => {
        document.getElementById('child-0')
          .contentWindow.postMessage('foo', '*');
      }, false);
      )"));

  // 2) Navigate to C. This will invoke pagehide handler and postMessage.
  ASSERT_TRUE(NavigateToURL(shell(), url_c));
  // Onmessage event should be queued and not triggered in back/forward cache.
  // Thus JavaScript execution does not happen and the page does not get
  // evicted.
  EXPECT_TRUE(rfh_a->IsInBackForwardCache());

  // 4) Go back to A(B). Make sure that JavaSc
  ASSERT_TRUE(HistoryGoBack(web_contents()));
  ExpectRestored(FROM_HERE);
  EXPECT_EQ("dispatched",
            GetLocalStorage(rfh_b.get(), "postMessage_dispatched"));
}

class BackForwardCacheBrowserTestDisallowBroadcastChannel
    : public BackForwardCacheBrowserTest {
 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    // Disallow broadcastchannel to enter bfcache, because there is no
    // other easy non-sticky feature for testing.
    DisableFeature(blink::features::kBFCacheOpenBroadcastChannel);
    BackForwardCacheBrowserTest::SetUpCommandLine(command_line);
  }
};

// Navigates from page A -> page B -> page C -> page B -> page C. Page B becomes
// ineligible for bfcache in pagehide handler, so Page A stays in bfcache
// without being evicted even after the navigation to Page C.
IN_PROC_BROWSER_TEST_F(
    BackForwardCacheBrowserTestDisallowBroadcastChannel,
    PagehideMakesPageIneligibleForBackForwardCacheAndNotCountedInCacheSize) {
  ASSERT_TRUE(CreateHttpsServer()->Start());
  GURL url_a(https_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(https_server()->GetURL(
      "b.com", "/back_forward_cache/page_with_broadcastchannel.html"));
  GURL url_c(https_server()->GetURL("c.com", "/title1.html"));

  // 1) Navigate to a.com.
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImpl* rfh_a = current_frame_host();

  // 2) Navigate to b.com.
  EXPECT_TRUE(NavigateToURL(shell(), url_b));
  RenderFrameHostImpl* rfh_b = current_frame_host();
  RenderFrameDeletedObserver deleted_observer_rfh_b(rfh_b);
  EXPECT_TRUE(rfh_a->IsInBackForwardCache());
  // Acquire broadcast in pagehide. Now b.com is not eligible for bfcache.
  EXPECT_TRUE(
      ExecJs(rfh_b, "setShouldAcquireBroadcastChannelInPageHide(true);"));

  // 3) Navigate to c.com.
  EXPECT_TRUE(NavigateToURL(shell(), url_c));
  // RenderFrameHostImpl* rfh_c = current_frame_host();
  // Since the b.com is not eligible for bfcache, |rfh_a| should stay in
  // bfcache.
  deleted_observer_rfh_b.WaitUntilDeleted();
  EXPECT_TRUE(rfh_a->IsInBackForwardCache());

  // 4) Navigate back to b.com.
  ASSERT_TRUE(HistoryGoBack(web_contents()));
  ExpectNotRestored(
      {NotRestoredReason::kBlocklistedFeatures},
      {blink::scheduler::WebSchedulerTrackedFeature::kBroadcastChannel}, {}, {},
      {}, FROM_HERE);
  RenderFrameHostImpl* rfh_b_2 = current_frame_host();
  // Do not acquire broadcast channel. Now b.com is eligible for bfcache.
  EXPECT_TRUE(
      ExecJs(rfh_b_2, "setShouldAcquireBroadcastChannelInPageHide(false);"));

  // 5) Navigate forward to c.com.
  ASSERT_TRUE(HistoryGoForward(web_contents()));
  ExpectRestored(FROM_HERE);
  // b.com was eligible for bfcache and should stay in bfcache.
  EXPECT_TRUE(rfh_b_2->IsInBackForwardCache());
}

class BackForwardCacheEntryTimeoutBrowserTest
    : public BackForwardCacheBrowserTest {
 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    BackForwardCacheBrowserTest::SetUpCommandLine(command_line);
    feature_list_.InitAndEnableFeature(features::kBackForwardCacheEntryTimeout);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(BackForwardCacheEntryTimeoutBrowserTest, BusyPagehide) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL("a.test", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.test", "/title1.html"));
  ASSERT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImplWrapper rfh(current_frame_host());
  ASSERT_TRUE(ExecJs(rfh.get(), R"(
      addEventListener("pagehide", () => {while(1){}});
  )"));
  ASSERT_TRUE(NavigateToURL(shell(), url_b));
  ASSERT_TRUE(rfh.WaitUntilRenderFrameDeleted());
  ASSERT_TRUE(HistoryGoBack(web_contents()));
  ExpectNotRestored({NotRestoredReason::kTimeoutPuttingInCache}, {}, {}, {}, {},
                    FROM_HERE);
}

IN_PROC_BROWSER_TEST_F(BackForwardCacheEntryTimeoutBrowserTest,
                       EvictPageWithInfiniteLoop) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  // 1) Navigate to A.
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImplWrapper rfh_a(current_frame_host());
  rfh_a->GetBackForwardCacheMetrics()->SetObserverForTesting(this);

  ExecuteScriptAsync(rfh_a.get(), R"(
    let i = 0;
    while (true) { i++; }
  )");

  RenderProcessHost* process = rfh_a.get()->GetProcess();
  RenderProcessHostWatcher destruction_observer(
      process, RenderProcessHostWatcher::WATCH_FOR_HOST_DESTRUCTION);

  // 2) Navigate to B.
  EXPECT_TRUE(NavigateToURL(shell(), url_b));
  RenderFrameHostImplWrapper rfh_b(current_frame_host());

  // rfh_a should be destroyed (not kept in the cache).
  destruction_observer.Wait();
  EXPECT_TRUE(rfh_a.WaitUntilRenderFrameDeleted());

  // rfh_b should still be the current frame.
  EXPECT_EQ(current_frame_host(), rfh_b.get());

  // 3) Go back to A.
  ASSERT_TRUE(HistoryGoBack(web_contents()));
  ExpectNotRestored({NotRestoredReason::kTimeoutPuttingInCache}, {}, {}, {}, {},
                    FROM_HERE);

  // Make sure that the tree reasons match the flattened reasons.
  EXPECT_THAT(
      GetTreeResult()->GetDocumentResult(),
      MatchesDocumentResult(
          NotRestoredReasons({NotRestoredReason::kTimeoutPuttingInCache}),
          BlockListedFeatures()));
}

// Test the race condition where a document is evicted from the BackForwardCache
// while it is in the middle of being restored and before URL loader starts a
// response.
//
// ┌───────┐                 ┌────────┐
// │Browser│                 │Renderer│
// └───┬───┘                 └───┬────┘
// (Freeze & store the cache)    │
//     │────────────────────────>│
//     │                         │
// (Navigate to cached document) │
//     │──┐                      │
//     │  │                      │
//     │EvictFromBackForwardCache│
//     │<────────────────────────│
//     │  │                      │
//     │  x Navigation cancelled │
//     │    and reissued         │
// ┌───┴───┐                 ┌───┴────┐
// │Browser│                 │Renderer│
// └───────┘                 └────────┘
//
// When the eviction occurs, the in flight NavigationRequest to the cached
// document should be reissued (cancelled and replaced by a normal navigation).
IN_PROC_BROWSER_TEST_F(
    BackForwardCacheBrowserTest,
    ReissuesNavigationIfEvictedDuringNavigation_BeforeResponse) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title2.html"));

  // 1) Navigate to page A.
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImpl* rfh_a = current_frame_host();
  RenderFrameDeletedObserver delete_observer_rfh_a(rfh_a);

  // 2) Navigate to page B.
  EXPECT_TRUE(NavigateToURL(shell(), url_b));
  RenderFrameHostImpl* rfh_b = current_frame_host();
  RenderFrameDeletedObserver delete_observer_rfh_b(rfh_b);
  EXPECT_FALSE(delete_observer_rfh_a.deleted());
  EXPECT_TRUE(rfh_a->IsInBackForwardCache());
  EXPECT_NE(rfh_a, rfh_b);

  // 3) Start navigation to page A, and cause the document to be evicted during
  // the navigation immediately before navigation makes any meaningful progress.
  // The BFCache entry will be evicted before the back navigation completes, so
  // the old navigation will be reset and a new navigation will be restarted.
  // This observer is waiting for the two navigation requests to complete.
  TestNavigationObserver observer(web_contents(),
                                  /* expected_number_of_navigations= */ 2,
                                  MessageLoopRunner::QuitMode::IMMEDIATE,
                                  /* ignore_uncommitted_navigations= */ false);
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(web_contents()->IsLoading());
  EvictByJavaScript(rfh_a);
  EXPECT_FALSE(web_contents()->IsLoading());

  // rfh_a should have been deleted, and page A navigated to normally.
  delete_observer_rfh_a.WaitUntilDeleted();
  observer.Wait();
  RenderFrameHostImpl* rfh_a2 = current_frame_host();
  EXPECT_NE(rfh_a2, rfh_b);
  EXPECT_EQ(rfh_a2->GetLastCommittedURL(), url_a);

  ExpectNotRestored({NotRestoredReason::kJavaScriptExecution,
                     NotRestoredReason::kNavigationCancelledWhileRestoring},
                    {}, {}, {}, {}, FROM_HERE);
}

// Test that the reissued BFCache navigation (see
// `ReissuesNavigationIfEvictedDuringNavigation_BeforeResponse` above) is
// cancelled when there is another navigation request initiated to the same
// `FrameTreeNode` before the restarting task is executed.
IN_PROC_BROWSER_TEST_F(
    BackForwardCacheBrowserTest,
    ReissuedBackForwardCacheNavigationIsCancelledWhenNewNavigationIsCreated) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title2.html"));
  GURL url_c(embedded_test_server()->GetURL("c.com", "/title1.html"));

  // 1) Navigate to page A.
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImpl* rfh_a = current_frame_host();
  RenderFrameDeletedObserver delete_observer_rfh_a(rfh_a);

  // 2) Navigate to page B.
  EXPECT_TRUE(NavigateToURL(shell(), url_b));
  RenderFrameHostImpl* rfh_b = current_frame_host();
  RenderFrameDeletedObserver delete_observer_rfh_b(rfh_b);
  EXPECT_FALSE(delete_observer_rfh_a.deleted());
  EXPECT_TRUE(rfh_a->IsInBackForwardCache());
  EXPECT_NE(rfh_a, rfh_b);

  // 3) Start a back navigation to page A, and cause the document to be evicted
  // during the navigation immediately before the navigation makes any
  // meaningful progress.

  // The BFCache entry will be evicted before the original back navigation to
  // page A completes, so the navigation will be reset and a new non-BFCache
  // navigation to page A will be restarted. Before the restarted navigation
  // task is executed, a new navigation to page C will be manually initiated,
  // which cancels the restarting task.

  // Uses `TestActivationManager` to ensure that the BFCache entry is evicted
  // during the back navigation.
  TestActivationManager activation_manager(web_contents(), url_a);
  web_contents()->GetController().GoBack();
  ASSERT_TRUE(activation_manager.WaitForBeforeChecks());
  EvictByJavaScript(rfh_a);
  activation_manager.WaitForNavigationFinished();
  ASSERT_FALSE(activation_manager.was_committed());
  ASSERT_FALSE(activation_manager.was_activated());
  // `rfh_a` should have been deleted.
  delete_observer_rfh_a.WaitUntilDeleted();

  // 4) Initiate another navigation, so the restarting task will be cancelled.
  // This `observer` is for navigation to page C.
  TestNavigationObserver observer(web_contents());
  web_contents()->GetController().LoadURLWithParams(
      NavigationController::LoadURLParams(url_c));
  observer.WaitForNavigationFinished();
  // Now the destination of the navigation is `url_c` after two navigation
  // requests complete.
  EXPECT_EQ(current_frame_host()->GetLastCommittedURL(), url_c);
}

// Similar to ReissuesNavigationIfEvictedDuringNavigation, except that
// BackForwardCache::Flush is the source of the eviction.
IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest,
                       FlushCacheDuringNavigationToCachedPage) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title2.html"));

  // 1) Navigate to page A.
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImpl* rfh_a1 = current_frame_host();
  RenderFrameDeletedObserver delete_observer_rfh_a1(rfh_a1);

  // 2) Navigate to page B.
  EXPECT_TRUE(NavigateToURL(shell(), url_b));
  RenderFrameHostImpl* rfh_b2 = current_frame_host();
  RenderFrameDeletedObserver delete_observer_rfh_b2(rfh_b2);
  EXPECT_FALSE(delete_observer_rfh_a1.deleted());
  EXPECT_TRUE(rfh_a1->IsInBackForwardCache());
  EXPECT_NE(rfh_a1, rfh_b2);

  // 3) Start navigation to page A, and flush the cache before activation
  // checks finish (i.e. before disabling JS eviction in the renderer).
  // The BFCache entry will be evicted before the back navigation completes,
  // so the old navigation will be reset and a new navigation will be
  // restarted. This observer is waiting for the two navigation requests to
  // complete.
  TestNavigationObserver observer(web_contents(),
                                  /* expected_number_of_navigations= */ 2,
                                  MessageLoopRunner::QuitMode::IMMEDIATE,
                                  /* ignore_uncommitted_navigations= */ false);
  {
    // In a scope to make sure the activation_manager is deleted before the
    // reissued navigation begins.
    TestActivationManager activation_manager(shell()->web_contents(), url_a);

    web_contents()->GetController().GoBack();

    // Wait for the activating navigation to start.
    EXPECT_TRUE(activation_manager.WaitForBeforeChecks());

    // Flush the cache, which contains the document being navigated to.
    web_contents()->GetController().GetBackForwardCache().Flush();

    // The navigation should get canceled, then reissued; ultimately resulting
    // in a successful navigation using a new RenderFrameHost. Ensure the
    // initial activating navigation isn't committed.
    activation_manager.WaitForNavigationFinished();
    EXPECT_FALSE(activation_manager.was_committed());
  }

  // rfh_a should have been deleted, and page A navigated to normally.
  delete_observer_rfh_a1.WaitUntilDeleted();
  observer.Wait();
  EXPECT_TRUE(rfh_b2->IsInBackForwardCache());
  RenderFrameHostImpl* rfh_a3 = current_frame_host();
  EXPECT_EQ(rfh_a3->GetLastCommittedURL(), url_a);
}

// Test that if the renderer process crashes while a document is in the
// BackForwardCache, it gets evicted.
IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest,
                       EvictsFromCacheIfRendererProcessCrashes) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  // 1) Navigate to A.
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImpl* rfh_a = current_frame_host();

  // 2) Navigate to B.
  EXPECT_TRUE(NavigateToURL(shell(), url_b));
  RenderFrameHostImpl* rfh_b = current_frame_host();

  EXPECT_TRUE(rfh_a->IsInBackForwardCache());

  // 3) Crash A's renderer process while it is in the cache.
  {
    RenderProcessHost* process = rfh_a->GetProcess();
    RenderProcessHostWatcher crash_observer(
        process, RenderProcessHostWatcher::WATCH_FOR_HOST_DESTRUCTION);
    EXPECT_TRUE(process->Shutdown(0));
    crash_observer.Wait();
  }

  // rfh_b should still be the current frame.
  EXPECT_EQ(current_frame_host(), rfh_b);

  // 4) Go back to A.
  ASSERT_TRUE(HistoryGoBack(web_contents()));
  ExpectNotRestored({NotRestoredReason::kRendererProcessKilled}, {}, {}, {}, {},
                    FROM_HERE);
}

// The test is simulating a race condition. The scheduler tracked features are
// updated during the "freeze" event in a way that would have prevented the
// document from entering the BackForwardCache in the first place.
//
// TODO(crbug.com/41477477): The document should be evicted.
//
// ┌───────┐                     ┌────────┐
// │browser│                     │renderer│
// └───┬───┘                     └────┬───┘
//  (enter cache)                     │
//     │           Freeze()           │
//     │─────────────────────────────>│
//     │                          (onfreeze)
//     │OnSchedulerTrackedFeaturesUsed│
//     │<─────────────────────────────│
//     │                           (frozen)
//     │                              │
// ┌───┴───┐                     ┌────┴───┐
// │browser│                     │renderer│
// └───────┘                     └────────┘
IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest,
                       SchedulerTrackedFeaturesUpdatedWhileStoring) {
  ASSERT_TRUE(CreateHttpsServer()->Start());

  GURL url_a(https_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(https_server()->GetURL("b.com", "/title1.html"));

  // 1) Navigate to A.
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImpl* rfh_a = current_frame_host();
  RenderFrameDeletedObserver delete_observer_rfh_a(rfh_a);

  // When the page will enter the BackForwardCache, just before being frozen,
  // use a feature that would have been prevented the document from being
  // cached.
  EXPECT_TRUE(ExecJs(rfh_a, R"(
    document.addEventListener('freeze', event => {
      navigator.xr.isSessionSupported('inline');
    });
  )"));

  // 2) Navigate to B.
  EXPECT_TRUE(NavigateToURL(shell(), url_b));

  // rfh_a should be evicted from the cache and destroyed.
  delete_observer_rfh_a.WaitUntilDeleted();
}

// The BackForwardCache caches same-website navigations.
IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest, SameSiteNavigationCaching) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a1(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_a2(embedded_test_server()->GetURL("a.com", "/title2.html"));

  // 1) Navigate to A1.
  EXPECT_TRUE(NavigateToURL(shell(), url_a1));
  RenderFrameHostImpl* rfh_a1 = current_frame_host();
  RenderFrameDeletedObserver delete_rfh_a1(rfh_a1);
  auto browsing_instance_id =
      rfh_a1->GetSiteInstance()->GetBrowsingInstanceId();

  // 2) Navigate to A2.
  EXPECT_TRUE(NavigateToURL(shell(), url_a2));
  RenderFrameHostImpl* rfh_a2 = current_frame_host();
  EXPECT_NE(browsing_instance_id,
            rfh_a2->GetSiteInstance()->GetBrowsingInstanceId());
  EXPECT_TRUE(rfh_a1->IsInBackForwardCache());
  EXPECT_NE(rfh_a1, rfh_a2);
}

// Test that documents are evicted correctly from BackForwardCache after time to
// live.
IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest, TimedEviction) {
  // Inject mock time task runner to be used in the eviction timer, so we can
  // check for the functionality we are interested before and after the time to
  // live. We don't replace SingleThreadTaskRunner::GetCurrentDefault to ensure
  // that it doesn't affect other unrelated callsites.
  scoped_refptr<base::TestMockTimeTaskRunner> task_runner =
      base::MakeRefCounted<base::TestMockTimeTaskRunner>();

  web_contents()->GetController().GetBackForwardCache().SetTaskRunnerForTesting(
      task_runner);

  base::TimeDelta time_to_live_in_back_forward_cache =
      BackForwardCacheImpl::GetTimeToLiveInBackForwardCache(
          BackForwardCacheImpl::kNotInCCNSContext);
  // This should match the value we set in EnableFeatureAndSetParams.
  EXPECT_EQ(time_to_live_in_back_forward_cache, base::Seconds(3600));

  base::TimeDelta delta = base::Milliseconds(1);

  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  // 1) Navigate to A.
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImplWrapper rfh_a(current_frame_host());
  rfh_a->GetBackForwardCacheMetrics()->SetObserverForTesting(this);

  // 2) Navigate to B.
  EXPECT_TRUE(NavigateToURL(shell(), url_b));
  RenderFrameHostImplWrapper rfh_b(current_frame_host());

  // 3) Fast forward to just before eviction is due.
  task_runner->FastForwardBy(time_to_live_in_back_forward_cache - delta);

  // 4) Confirm A is still in BackForwardCache.
  ASSERT_FALSE(rfh_a.IsDestroyed());
  EXPECT_TRUE(rfh_a->IsInBackForwardCache());

  // 5) Fast forward to when eviction is due.
  task_runner->FastForwardBy(delta);

  // 6) Confirm A is evicted.
  EXPECT_EQ(current_frame_host(), rfh_b.get());

  // 7) Go back to A.
  ASSERT_TRUE(HistoryGoBack(web_contents()));
  ExpectNotRestored({NotRestoredReason::kTimeout}, {}, {}, {}, {}, FROM_HERE);
  // Make sure that the tree reasons match the flattened reasons.
  EXPECT_THAT(
      GetTreeResult()->GetDocumentResult(),
      MatchesDocumentResult(NotRestoredReasons({NotRestoredReason::kTimeout}),
                            BlockListedFeatures()));
}

IN_PROC_BROWSER_TEST_F(
    BackForwardCacheBrowserTest,
    DisableBackForwardCachePreventsDocumentsFromBeingCached) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));
  url::Origin origin_a = url::Origin::Create(url_a);
  url::Origin origin_b = url::Origin::Create(url_b);

  // 1) Navigate to A.
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImpl* rfh_a = current_frame_host();
  RenderFrameDeletedObserver delete_observer_rfh_a(rfh_a);
  DisableBFCacheForRFHForTesting(rfh_a);

  // 2) Navigate to B.
  EXPECT_TRUE(NavigateToURL(shell(), url_b));
  delete_observer_rfh_a.WaitUntilDeleted();

  // 3) Go back to A.
  ASSERT_TRUE(HistoryGoBack(web_contents()));
  ExpectNotRestored({NotRestoredReason::kDisableForRenderFrameHostCalled}, {},
                    {}, {RenderFrameHostDisabledForTestingReason()}, {},
                    FROM_HERE);
}

IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest,
                       DisableBackForwardIsNoOpIfRfhIsGone) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));
  url::Origin origin_a = url::Origin::Create(url_a);
  url::Origin origin_b = url::Origin::Create(url_b);

  // 1) Navigate to A.
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImpl* rfh_a = current_frame_host();
  RenderFrameDeletedObserver delete_observer_rfh_a(rfh_a);
  GlobalRenderFrameHostId rfh_a_id = rfh_a->GetGlobalId();
  DisableBFCacheForRFHForTesting(rfh_a_id);

  // 2) Navigate to B.
  EXPECT_TRUE(NavigateToURL(shell(), url_b));
  delete_observer_rfh_a.WaitUntilDeleted();

  // This should not die
  DisableBFCacheForRFHForTesting(rfh_a_id);

  // 3) Go back to A.
  ASSERT_TRUE(HistoryGoBack(web_contents()));
  ExpectNotRestored({NotRestoredReason::kDisableForRenderFrameHostCalled}, {},
                    {}, {RenderFrameHostDisabledForTestingReason()}, {},
                    FROM_HERE);
}

IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest,
                       DisableBackForwardCacheIframe) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b)"));
  GURL url_c(embedded_test_server()->GetURL("b.com", "/title1.html"));

  // 1) Navigate to A.
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImpl* rfh_a = current_frame_host();
  RenderFrameHostImpl* rfh_b = rfh_a->child_at(0)->current_frame_host();
  RenderFrameDeletedObserver delete_observer_rfh_a(rfh_a);
  RenderFrameDeletedObserver delete_observer_rfh_b(rfh_b);

  DisableBFCacheForRFHForTesting(rfh_b);

  // 2) Navigate to C. A and B are deleted.
  EXPECT_TRUE(NavigateToURL(shell(), url_c));
  delete_observer_rfh_a.WaitUntilDeleted();
  delete_observer_rfh_b.WaitUntilDeleted();

  // 3) Go back to A.
  ASSERT_TRUE(HistoryGoBack(web_contents()));
  ExpectNotRestored({NotRestoredReason::kDisableForRenderFrameHostCalled}, {},
                    {}, {RenderFrameHostDisabledForTestingReason()}, {},
                    FROM_HERE);
}

IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest,
                       DisableBackForwardEvictsIfAlreadyInCache) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));
  url::Origin origin_a = url::Origin::Create(url_a);
  url::Origin origin_b = url::Origin::Create(url_b);

  // 1) Navigate to A.
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImpl* rfh_a = current_frame_host();
  RenderFrameDeletedObserver delete_observer_rfh_a(rfh_a);

  // 2) Navigate to B.
  EXPECT_TRUE(NavigateToURL(shell(), url_b));
  EXPECT_TRUE(rfh_a->IsInBackForwardCache());
  EXPECT_FALSE(rfh_a->is_evicted_from_back_forward_cache());

  DisableBFCacheForRFHForTesting(rfh_a);

  delete_observer_rfh_a.WaitUntilDeleted();

  // 3) Go back to A.
  ASSERT_TRUE(HistoryGoBack(web_contents()));
  ExpectNotRestored({NotRestoredReason::kDisableForRenderFrameHostCalled}, {},
                    {}, {RenderFrameHostDisabledForTestingReason()}, {},
                    FROM_HERE);
}

// Confirm that same-document navigation and not history-navigation does not
// record metrics.
IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest, MetricsNotRecorded) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));
  GURL url_b2(embedded_test_server()->GetURL("b.com", "/title1.html#2"));

  // 1) Navigate to A.
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));

  // 2) Navigate to B.
  EXPECT_TRUE(NavigateToURL(shell(), url_b));
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));

  // 3) Navigate to B#2 (same document navigation).
  EXPECT_TRUE(NavigateToURLFromRenderer(shell(), url_b2));

  // 4) Go back to B.
  ASSERT_TRUE(HistoryGoBack(web_contents()));
  ExpectOutcomeDidNotChange(FROM_HERE);

  // 5) Navigate to A.
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  ExpectOutcomeDidNotChange(FROM_HERE);
}

// Test for functionality of domain specific controls in back-forward cache.
class BackForwardCacheBrowserTestWithDomainControlEnabled
    : public BackForwardCacheBrowserTest {
 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    // Sets the allowed websites for testing, additionally adding the params
    // used by BackForwardCacheBrowserTest.
    std::string allowed_websites =
        "https://a.allowed/back_forward_cache/, "
        "https://b.allowed/back_forward_cache/allowed_path.html";
    EnableFeatureAndSetParams(features::kBackForwardCache, "allowed_websites",
                              allowed_websites);

    BackForwardCacheBrowserTest::SetUpCommandLine(command_line);
  }
};

// Check the RenderFrameHost allowed to enter the BackForwardCache are the ones
// matching with the "allowed_websites" feature params.
IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTestWithDomainControlEnabled,
                       CachePagesWithMatchedURLs) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL(
      "a.allowed", "/back_forward_cache/allowed_path.html"));
  GURL url_b(embedded_test_server()->GetURL(
      "b.allowed", "/back_forward_cache/allowed_path.html?query=bar"));

  // 1) Navigate to A.
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImpl* rfh_a = current_frame_host();
  RenderFrameDeletedObserver delete_observer_rfh_a(rfh_a);

  // 2) Navigate to B.
  EXPECT_TRUE(NavigateToURL(shell(), url_b));
  RenderFrameHostImpl* rfh_b = current_frame_host();
  RenderFrameDeletedObserver delete_observer_rfh_b(rfh_b);

  // 3) Check if rfh_a is stored in back-forward cache, since it matches to
  // the list of allowed urls, it should be stored.
  EXPECT_FALSE(delete_observer_rfh_a.deleted());
  EXPECT_TRUE(rfh_a->IsInBackForwardCache());

  // 4) Now go back to the last stored page, which in our case should be A.
  ASSERT_TRUE(HistoryGoBack(web_contents()));
  EXPECT_EQ(rfh_a, current_frame_host());

  // 5) Check if rfh_b is stored in back-forward cache, since it matches to
  // the list of allowed urls, it should be stored.
  EXPECT_FALSE(delete_observer_rfh_b.deleted());
  EXPECT_TRUE(rfh_b->IsInBackForwardCache());
}

// We don't want to allow websites which doesn't match "allowed_websites" of
// feature params to be stored in back-forward cache.
IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTestWithDomainControlEnabled,
                       DoNotCachePagesWithUnMatchedURLs) {
  DisableCheckingMetricsForAllSites();

  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL(
      "a.disallowed", "/back_forward_cache/disallowed_path.html"));
  GURL url_b(embedded_test_server()->GetURL(
      "b.allowed", "/back_forward_cache/disallowed_path.html"));
  GURL url_c(embedded_test_server()->GetURL(
      "c.disallowed", "/back_forward_cache/disallowed_path.html"));

  // 1) Navigate to A.
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImpl* rfh_a = current_frame_host();
  RenderFrameDeletedObserver delete_observer_rfh_a(rfh_a);

  // 2) Navigate to B.
  EXPECT_TRUE(NavigateToURL(shell(), url_b));
  RenderFrameHostImpl* rfh_b = current_frame_host();
  RenderFrameDeletedObserver delete_observer_rfh_b(rfh_b);

  // 3) Since url of A doesn't match to the the list of allowed urls it should
  // not be stored in back-forward cache.
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  delete_observer_rfh_a.WaitUntilDeleted();

  // 4) Navigate to C.
  EXPECT_TRUE(NavigateToURL(shell(), url_c));

  // 5) Since url of B doesn't match to the the list of allowed urls it should
  // not be stored in back-forward cache.
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  delete_observer_rfh_b.WaitUntilDeleted();

  // 6) Go back to B.
  ASSERT_TRUE(HistoryGoBack(web_contents()));

  // Nothing is recorded when the domain does not match.
  ExpectOutcomeDidNotChange(FROM_HERE);
  ExpectNotRestoredDidNotChange(FROM_HERE);
}

// Test the "blocked_websites" feature params in back-forward cache.
class BackForwardCacheBrowserTestWithBlockedWebsites
    : public BackForwardCacheBrowserTest {
 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    // Sets the blocked websites for testing, additionally adding the params
    // used by BackForwardCacheBrowserTest.
    std::string blocked_websites =
        "https://a.blocked/, "
        "https://b.blocked/";
    EnableFeatureAndSetParams(features::kBackForwardCache, "blocked_websites",
                              blocked_websites);

    BackForwardCacheBrowserTest::SetUpCommandLine(command_line);
  }
};

// Check the disallowed page isn't bfcached when it's navigated from allowed
// page.
IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTestWithBlockedWebsites,
                       NavigateFromAllowedPageToDisallowedPage) {
  // Skip checking the AllSites metrics since BackForwardCacheMetrics stop
  // recording except BackForwardCache.AllSites.* metrics when the target URL is
  // disallowed by allowed_websites or blocked_websites.
  DisableCheckingMetricsForAllSites();

  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL(
      "a.allowed", "/back_forward_cache/allowed_path.html"));
  GURL url_b(embedded_test_server()->GetURL(
      "b.blocked", "/back_forward_cache/disallowed_path.html"));

  // 1) Navigate to A.
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  RenderFrameHostImpl* rfh_a = current_frame_host();
  RenderFrameDeletedObserver delete_observer_rfh_a(rfh_a);

  // 2) Navigate to B.
  EXPECT_TRUE(NavigateToURL(shell(), url_b));
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  RenderFrameHostImpl* rfh_b = current_frame_host();
  RenderFrameDeletedObserver delete_observer_rfh_b(rfh_b);

  // 3) Check if rfh_a is stored in back-forward cache, since it doesn't match
  // to the blocked_websites, and allowed_websites are empty, so it should
  // be stored.
  EXPECT_FALSE(delete_observer_rfh_a.deleted());
  EXPECT_TRUE(rfh_a->IsInBackForwardCache());

  // 4) Now go back to the last stored page, which in our case should be A.
  ASSERT_TRUE(HistoryGoBack(web_contents()));
  EXPECT_EQ(rfh_a, current_frame_host());
  ExpectRestored(FROM_HERE);

  // 5) Check if rfh_b is not stored in back-forward cache, since it matches to
  // the blocked_websites.
  delete_observer_rfh_b.WaitUntilDeleted();
  EXPECT_TRUE(delete_observer_rfh_b.deleted());

  // 6) Go forward to B. B should not restored from the back-forward cache.
  ASSERT_TRUE(HistoryGoForward(web_contents()));

  // Nothing is recorded since B is disallowed.
  ExpectOutcomeDidNotChange(FROM_HERE);
  ExpectNotRestoredDidNotChange(FROM_HERE);
}

// Check the allowed page is bfcached when it's navigated from disallowed
// page.
IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTestWithBlockedWebsites,
                       NavigateFromDisallowedPageToAllowedPage) {
  // Skip checking the AllSites metrics since BackForwardCacheMetrics stop
  // recording except BackForwardCache.AllSites.* metrics when the target URL is
  // disallowed by allowed_websites or blocked_websites.
  DisableCheckingMetricsForAllSites();

  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL(
      "a.blocked", "/back_forward_cache/disallowed_path.html"));
  GURL url_b(embedded_test_server()->GetURL(
      "b.allowed", "/back_forward_cache/allowed_path.html"));

  // 1) Navigate to A.
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  RenderFrameHostImpl* rfh_a = current_frame_host();
  RenderFrameDeletedObserver delete_observer_rfh_a(rfh_a);

  // 2) Navigate to B.
  EXPECT_TRUE(NavigateToURL(shell(), url_b));
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  RenderFrameHostImpl* rfh_b = current_frame_host();
  RenderFrameDeletedObserver delete_observer_rfh_b(rfh_b);

  // 3) Check if rfh_a is not stored in back-forward cache, since it matches to
  // the blocked_websites.
  delete_observer_rfh_a.WaitUntilDeleted();
  EXPECT_TRUE(delete_observer_rfh_a.deleted());

  // 4) Now go back to url_a which is not bfcached.
  ASSERT_TRUE(HistoryGoBack(web_contents()));

  // Nothing is recorded since A is disallowed.
  ExpectOutcomeDidNotChange(FROM_HERE);
  ExpectNotRestoredDidNotChange(FROM_HERE);

  // 5) Check if rfh_b is stored in back-forward cache, since it doesn't match
  // to the blocked_websites, and allowed_websites are empty, so it should
  // be stored.
  EXPECT_FALSE(delete_observer_rfh_b.deleted());
  EXPECT_TRUE(rfh_b->IsInBackForwardCache());

  // 6) Go forward to url_b which is bfcached.
  ASSERT_TRUE(HistoryGoForward(web_contents()));
  ExpectRestored(FROM_HERE);
}

// Test BackForwardCache::IsAllowed() with several allowed_websites URL
// patterns.
class BackForwardCacheBrowserTestForAllowedWebsitesUrlPatterns
    : public BackForwardCacheBrowserTest {
 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    // Sets the allowed websites for testing, additionally adding the params
    // used by BackForwardCacheBrowserTest.
    std::string allowed_websites =
        "https://a.com/,"
        "https://b.com/path,"
        "https://c.com/path/";
    EnableFeatureAndSetParams(features::kBackForwardCache, "allowed_websites",
                              allowed_websites);

    BackForwardCacheBrowserTest::SetUpCommandLine(command_line);
  }
};

// Check if the URLs are allowed when allowed_websites are specified.
IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTestForAllowedWebsitesUrlPatterns,
                       AllowedWebsitesUrlPatterns) {
  BackForwardCacheImpl& bfcache =
      web_contents()->GetController().GetBackForwardCache();

  // Doesn't match with any allowed_websites.
  EXPECT_FALSE(bfcache.IsAllowed(GURL("https://a.org/")));

  // Exact match with https://a.com/.
  EXPECT_TRUE(bfcache.IsAllowed(GURL("https://a.com/")));
  EXPECT_TRUE(bfcache.IsAllowed(GURL("https://a.com")));

  // Match with https://a.com/ since we don't take into account the difference
  // on port number.
  EXPECT_TRUE(bfcache.IsAllowed(GURL("https://a.com:123/")));

  // Match with https://a.com/ since we don't take into account the difference
  // on query.
  EXPECT_TRUE(bfcache.IsAllowed(GURL("https://a.com:123/?x=1")));

  // Match with https://a.com/ since we don't take into account the difference
  // on scheme.
  EXPECT_TRUE(bfcache.IsAllowed(GURL("http://a.com/")));

  // Match with https://a.com/ since we are checking the prefix on path.
  EXPECT_TRUE(bfcache.IsAllowed(GURL("https://a.com/path")));

  // Doesn't match with https://a.com/ since the host doesn't match with a.com.
  EXPECT_FALSE(bfcache.IsAllowed(GURL("https://prefix.a.com/")));

  // Doesn't match with https://b.com/path since the path prefix doesn't match.
  EXPECT_FALSE(bfcache.IsAllowed(GURL("https://b.com/")));

  // Exact match with https://b.com/path.
  EXPECT_TRUE(bfcache.IsAllowed(GURL("https://b.com/path")));

  // Match with https://b.com/path since we are checking the prefix on path.
  EXPECT_TRUE(bfcache.IsAllowed(GURL("https://b.com/path/")));
  EXPECT_TRUE(bfcache.IsAllowed(GURL("https://b.com/path_abc")));
  EXPECT_TRUE(bfcache.IsAllowed(GURL("https://b.com/path_abc?x=1")));

  // Doesn't match with https://c.com/path/ since the path prefix doesn't match.
  EXPECT_FALSE(bfcache.IsAllowed(GURL("https://c.com/path")));
}

// Test BackForwardCache::IsAllowed() with several blocked_websites URL
// patterns.
class BackForwardCacheBrowserTestForBlockedWebsitesUrlPatterns
    : public BackForwardCacheBrowserTest {
 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    // Sets the blocked websites for testing, additionally adding the params
    // used by BackForwardCacheBrowserTest.
    std::string blocked_websites =
        "https://a.com/,"
        "https://b.com/path,"
        "https://c.com/path/";
    EnableFeatureAndSetParams(features::kBackForwardCache, "blocked_websites",
                              blocked_websites);

    BackForwardCacheBrowserTest::SetUpCommandLine(command_line);
  }
};

// Check if the URLs are allowed when blocked_websites are specified.
IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTestForBlockedWebsitesUrlPatterns,
                       BlockedWebsitesUrlPatterns) {
  BackForwardCacheImpl& bfcache =
      web_contents()->GetController().GetBackForwardCache();

  // Doesn't match with any blocked_websites.
  EXPECT_TRUE(bfcache.IsAllowed(GURL("https://a.org/")));

  // Exact match with https://a.com/.
  EXPECT_FALSE(bfcache.IsAllowed(GURL("https://a.com/")));
  EXPECT_FALSE(bfcache.IsAllowed(GURL("https://a.com")));

  // Match with https://a.com/ since we don't take into account the difference
  // on port number.
  EXPECT_FALSE(bfcache.IsAllowed(GURL("https://a.com:123/")));

  // Match with https://a.com/ since we don't take into account the difference
  // on query.
  EXPECT_FALSE(bfcache.IsAllowed(GURL("https://a.com:123/?x=1")));

  // Match with https://a.com/ since we don't take into account the difference
  // on scheme.
  EXPECT_FALSE(bfcache.IsAllowed(GURL("http://a.com/")));

  // Match with https://a.com/ since we are checking the prefix on path.
  EXPECT_FALSE(bfcache.IsAllowed(GURL("https://a.com/path")));

  // Doesn't match with https://a.com/ since the host doesn't match with a.com.
  EXPECT_TRUE(bfcache.IsAllowed(GURL("https://prefix.a.com/")));

  // Doesn't match with https://b.com/path since the path prefix doesn't match.
  EXPECT_TRUE(bfcache.IsAllowed(GURL("https://b.com/")));

  // Exact match with https://b.com/path.
  EXPECT_FALSE(bfcache.IsAllowed(GURL("https://b.com/path")));

  // Match with https://b.com/path since we are checking the prefix on path.
  EXPECT_FALSE(bfcache.IsAllowed(GURL("https://b.com/path/")));
  EXPECT_FALSE(bfcache.IsAllowed(GURL("https://b.com/path_abc")));
  EXPECT_FALSE(bfcache.IsAllowed(GURL("https://b.com/path_abc?x=1")));

  // Doesn't match with https://c.com/path/ since the path prefix doesn't match.
  EXPECT_TRUE(bfcache.IsAllowed(GURL("https://c.com/path")));
}

// Test BackForwardCache::IsAllowed() with several allowed_websites and
// blocked_websites URL patterns.
class BackForwardCacheBrowserTestForWebsitesUrlPatterns
    : public BackForwardCacheBrowserTest {
 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    // Sets the allowed websites for testing, additionally adding the params
    // used by BackForwardCacheBrowserTest.
    std::string allowed_websites = "https://a.com/";
    EnableFeatureAndSetParams(features::kBackForwardCache, "allowed_websites",
                              allowed_websites);

    // Sets the blocked websites for testing, additionally adding the params
    // used by BackForwardCacheBrowserTest.
    std::string blocked_websites = "https://a.com/";
    EnableFeatureAndSetParams(features::kBackForwardCache, "blocked_websites",
                              blocked_websites);

    BackForwardCacheBrowserTest::SetUpCommandLine(command_line);
  }
};

// Check if the URLs are allowed when allowed_websites and blocked_websites are
// specified.
IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTestForWebsitesUrlPatterns,
                       WebsitesUrlPatterns) {
  BackForwardCacheImpl& bfcache =
      web_contents()->GetController().GetBackForwardCache();

  // https://a.com/ is not allowed since blocked_websites will be prioritized
  // when the same website is specified in allowed_websites and
  // blocked_websites.
  EXPECT_FALSE(bfcache.IsAllowed(GURL("https://a.com/")));
  EXPECT_FALSE(bfcache.IsAllowed(GURL("https://a.com")));
}

// Test the "blocked_cgi_params" feature params in back-forward cache.
class BackForwardCacheBrowserTestWithBlockedCgiParams
    : public BackForwardCacheBrowserTest {
 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    // Sets the blocked websites for testing, additionally adding the params
    // used by BackForwardCacheBrowserTest.
    std::string blocked_cgi_params = "ibp=1|tbm=1";
    EnableFeatureAndSetParams(features::kBackForwardCache, "blocked_cgi_params",
                              blocked_cgi_params);

    BackForwardCacheBrowserTest::SetUpCommandLine(command_line);
  }
};

// Check the disallowed page isn't bfcached when it's navigated from allowed
// page.
IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTestWithBlockedCgiParams,
                       NavigateFromAllowedPageToDisallowedPage) {
  // Skip checking the AllSites metrics since BackForwardCacheMetrics stop
  // recording except BackForwardCache.AllSites.* metrics when the target URL is
  // disallowed by allowed_websites or blocked_websites.
  DisableCheckingMetricsForAllSites();

  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_allowed(
      embedded_test_server()->GetURL("a.llowed", "/title1.html?tbm=0"));
  GURL url_not_allowed(
      embedded_test_server()->GetURL("nota.llowed", "/title1.html?tbm=1"));

  // 1) Navigate to url_allowed.
  EXPECT_TRUE(NavigateToURL(shell(), url_allowed));
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  RenderFrameHostImpl* rfh_allowed = current_frame_host();
  RenderFrameDeletedObserver delete_observer_rfh_allowed(rfh_allowed);

  // 2) Navigate to url_not_allowed.
  EXPECT_TRUE(NavigateToURL(shell(), url_not_allowed));
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  RenderFrameHostImpl* rfh_not_allowed = current_frame_host();
  RenderFrameDeletedObserver delete_observer_rfh_not_allowed(rfh_not_allowed);

  // 3) Check that url_allowed is stored in back-forward cache.
  EXPECT_FALSE(delete_observer_rfh_allowed.deleted());
  EXPECT_TRUE(rfh_allowed->IsInBackForwardCache());

  // 4) Now go back to url_allowed.
  ASSERT_TRUE(HistoryGoBack(web_contents()));
  EXPECT_EQ(rfh_allowed, current_frame_host());
  ExpectRestored(FROM_HERE);

  // 5) Check that url_not_allowed is not stored in back-forward cache
  delete_observer_rfh_not_allowed.WaitUntilDeleted();
  EXPECT_TRUE(delete_observer_rfh_not_allowed.deleted());

  // 6) Go forward to url_not_allowed, it should not be restored from the
  // back-forward cache.
  ASSERT_TRUE(HistoryGoForward(web_contents()));

  // Nothing is recorded since it is disallowed.
  ExpectOutcomeDidNotChange(FROM_HERE);
  ExpectNotRestoredDidNotChange(FROM_HERE);
}

// Check the allowed page is bfcached when it's navigated from disallowed
// page.
IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTestWithBlockedCgiParams,
                       NavigateFromDisallowedPageToAllowedPage) {
  // Skip checking the AllSites metrics since BackForwardCacheMetrics stop
  // recording except BackForwardCache.AllSites.* metrics when the target URL is
  // disallowed by allowed_websites or blocked_websites.
  DisableCheckingMetricsForAllSites();

  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_allowed(
      embedded_test_server()->GetURL("a.llowed", "/title1.html?tbm=0"));
  GURL url_not_allowed(
      embedded_test_server()->GetURL("nota.llowed", "/title1.html?tbm=1"));

  // 1) Navigate to url_not_allowed.
  EXPECT_TRUE(NavigateToURL(shell(), url_not_allowed));
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  RenderFrameHostImpl* rfh_not_allowed = current_frame_host();
  RenderFrameDeletedObserver delete_observer_rfh_not_allowed(rfh_not_allowed);

  // 2) Navigate to url_allowed.
  EXPECT_TRUE(NavigateToURL(shell(), url_allowed));
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  RenderFrameHostImpl* rfh_allowed = current_frame_host();
  RenderFrameDeletedObserver delete_observer_rfh_allowed(rfh_allowed);

  // 3) Check that url_not_allowed is not stored in back-forward cache.
  delete_observer_rfh_not_allowed.WaitUntilDeleted();
  EXPECT_TRUE(delete_observer_rfh_not_allowed.deleted());

  // 4) Now go back to url_not_allowed.
  ASSERT_TRUE(HistoryGoBack(web_contents()));

  // Nothing is recorded since it is disallowed.
  ExpectOutcomeDidNotChange(FROM_HERE);
  ExpectNotRestoredDidNotChange(FROM_HERE);

  // 5) Check that url_allowed is stored in back-forward cache
  EXPECT_FALSE(delete_observer_rfh_allowed.deleted());
  EXPECT_TRUE(rfh_allowed->IsInBackForwardCache());

  // 6) Go forward to url_allowed, it should be restored from the
  // back-forward cache.
  ASSERT_TRUE(HistoryGoForward(web_contents()));
  ExpectRestored(FROM_HERE);
}

// Check that if WebPreferences was changed while a page was bfcached, it will
// get up-to-date WebPreferences when it was restored.
IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest, WebPreferences) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title2.html"));

  // 1) Navigate to A.
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImpl* rfh_a = current_frame_host();
  auto browsing_instance_id = rfh_a->GetSiteInstance()->GetBrowsingInstanceId();

  // A should prefer light color scheme (which is the default).
  EXPECT_EQ(
      true,
      EvalJs(web_contents(),
             "window.matchMedia('(prefers-color-scheme: light)').matches"));

  // 2) Navigate to B. A should be stored in the back-forward cache.
  EXPECT_TRUE(NavigateToURL(shell(), url_b));
  RenderFrameHostImpl* rfh_b = current_frame_host();
  EXPECT_NE(browsing_instance_id,
            rfh_b->GetSiteInstance()->GetBrowsingInstanceId());
  EXPECT_TRUE(rfh_a->IsInBackForwardCache());
  EXPECT_NE(rfh_a, rfh_b);

  blink::web_pref::WebPreferences prefs =
      web_contents()->GetOrCreateWebPreferences();
  prefs.preferred_color_scheme = blink::mojom::PreferredColorScheme::kDark;
  web_contents()->SetWebPreferences(prefs);

  // 3) Set WebPreferences to prefer dark color scheme.
  EXPECT_EQ(
      true,
      EvalJs(web_contents(),
             "window.matchMedia('(prefers-color-scheme: dark)').matches"));

  // 4) Go back to A, which should also prefer the dark color scheme now.
  ASSERT_TRUE(HistoryGoBack(web_contents()));
  EXPECT_EQ(rfh_a, current_frame_host());

  EXPECT_EQ(
      true,
      EvalJs(web_contents(),
             "window.matchMedia('(prefers-color-scheme: dark)').matches"));
}

// Check the BackForwardCache is disabled when there is a nested WebContents
// inside a page.
IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest, NestedWebContents) {
  // 1) Navigate to a page.
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url(embedded_test_server()->GetURL("a.com", "/page_with_iframe.html"));
  GURL url_inner(embedded_test_server()->GetURL("a.com", "/title2.html"));

  EXPECT_TRUE(NavigateToURL(shell(), url));

  RenderFrameHostImpl* rfh_a = current_frame_host();
  RenderFrameHostImpl* child = rfh_a->child_at(0)->current_frame_host();
  EXPECT_TRUE(child);

  // Create and attach an inner WebContents.
  auto* inner_contents = CreateAndAttachInnerContents(child);
  EXPECT_TRUE(NavigateToURL(inner_contents, url_inner));
  RenderFrameDeletedObserver deleted(rfh_a);

  // 2) Navigate away.
  shell()->LoadURL(embedded_test_server()->GetURL("b.com", "/title1.html"));
  // The page has an inner WebContents so it should be deleted.
  deleted.WaitUntilDeleted();

  // 3) Go back to the page with an inner WebContents.
  ASSERT_TRUE(HistoryGoBack(web_contents()));
  ExpectNotRestored({NotRestoredReason::kHaveInnerContents}, {}, {}, {}, {},
                    FROM_HERE);
}

IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest, RestoreWhilePendingCommit) {
  net::test_server::ControllableHttpResponse response(embedded_test_server(),
                                                      "/main_document");
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url1(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url2(embedded_test_server()->GetURL("b.com", "/title2.html"));
  GURL url3(embedded_test_server()->GetURL("c.com", "/main_document"));

  // Load a page and navigate away from it, so it is stored in the back-forward
  // cache.
  EXPECT_TRUE(NavigateToURL(shell(), url1));
  RenderFrameHost* rfh1 = current_frame_host();
  EXPECT_TRUE(NavigateToURL(shell(), url2));

  // Try to navigate to a new page, but leave it in a pending state.
  shell()->LoadURL(url3);
  response.WaitForRequest();

  // Navigate back and restore page from the cache, cancelling the previous
  // navigation.
  ASSERT_TRUE(HistoryGoBack(web_contents()));
  EXPECT_EQ(rfh1, current_frame_host());
}

IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest,
                       IsInactiveAndDisallowActivationIsNoopWhenActive) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  // 1) Navigate to A.
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  EXPECT_FALSE(current_frame_host()->IsInactiveAndDisallowActivation(
      DisallowActivationReasonId::kForTesting));

  // 2) Navigate to B.
  EXPECT_TRUE(NavigateToURL(shell(), url_b));

  // 3) Go back to A.
  ASSERT_TRUE(HistoryGoBack(web_contents()));
  ExpectRestored(FROM_HERE);
}

IN_PROC_BROWSER_TEST_F(
    BackForwardCacheBrowserTest,
    IsInactiveAndDisallowActivationDoesEvictForCachedFrames) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  // 1) Navigate to A.
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameDeletedObserver delete_observer_rfh_a(current_frame_host());
  RenderFrameHostImpl* rfh_a = current_frame_host();

  // 2) Navigate to B.
  EXPECT_TRUE(NavigateToURL(shell(), url_b));
  const uint64_t reason = DisallowActivationReasonId::kForTesting;
  EXPECT_TRUE(rfh_a->IsInactiveAndDisallowActivation(reason));

  // 3) Go back to A.
  ASSERT_TRUE(HistoryGoBack(web_contents()));
  ExpectNotRestored({NotRestoredReason::kIgnoreEventAndEvict}, {}, {}, {},
                    {reason}, FROM_HERE);
}

// Test scenarios where the "BackForwardCache" content flag is enabled but
// the command line flag "DisableBackForwardCache" is turned on, resulting in
// the feature being disabled.
class BackForwardCacheDisabledThroughCommandLineBrowserTest
    : public BackForwardCacheBrowserTest {
 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    BackForwardCacheBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kDisableBackForwardCache);
    EnableFeatureAndSetParams(blink::features::kLoadingTasksUnfreezable,
                              "max_buffered_bytes_per_process", "1000");
  }
};

// Ensures that the back-forward cache trial stays inactivated.
IN_PROC_BROWSER_TEST_F(BackForwardCacheDisabledThroughCommandLineBrowserTest,
                       BFCacheDisabled) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  // Ensure that the trial starts inactive.
  EXPECT_FALSE(base::FieldTrialList::IsTrialActive(
      base::FeatureList::GetFieldTrial(features::kBackForwardCache)
          ->trial_name()));

  EXPECT_FALSE(IsBackForwardCacheEnabled());

  // Ensure that we do not activate the trial when querying bfcache status,
  // which is protected by low-memory setting.
  EXPECT_FALSE(base::FieldTrialList::IsTrialActive(
      base::FeatureList::GetFieldTrial(features::kBackForwardCache)
          ->trial_name()));

  // 1) Navigate to A.
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImpl* rfh_a = current_frame_host();
  RenderFrameDeletedObserver delete_observer_rfh_a(rfh_a);

  // 2) Navigate to B.
  EXPECT_TRUE(NavigateToURL(shell(), url_b));

  // 3) A shouldn't be stored in back-forward cache because it's disabled.
  delete_observer_rfh_a.WaitUntilDeleted();

  // Nothing is recorded when back-forward cache is disabled.
  ExpectOutcomeDidNotChange(FROM_HERE);
  ExpectNotRestoredDidNotChange(FROM_HERE);

  // Ensure that the trial still hasn't been activated.
  EXPECT_FALSE(base::FieldTrialList::IsTrialActive(
      base::FeatureList::GetFieldTrial(features::kBackForwardCache)
          ->trial_name()));
}

// Ensures that the back-forward cache trial stays inactivated even when
// renderer code related to back-forward cache runs (in this case, network
// request loading).
IN_PROC_BROWSER_TEST_F(BackForwardCacheDisabledThroughCommandLineBrowserTest,
                       BFCacheDisabled_NetworkRequests) {
  net::test_server::ControllableHttpResponse image_response(
      embedded_test_server(), "/image.png");
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  // Ensure that the trials starts inactive.
  EXPECT_FALSE(base::FieldTrialList::IsTrialActive(
      base::FeatureList::GetFieldTrial(features::kBackForwardCache)
          ->trial_name()));

  EXPECT_FALSE(IsBackForwardCacheEnabled());

  // Ensure that we do not activate the trials for kBackForwardCache and
  // kLoadingTasksUnfreezable when querying bfcache or unfreezable loading tasks
  // status.
  EXPECT_FALSE(base::FieldTrialList::IsTrialActive(
      base::FeatureList::GetFieldTrial(features::kBackForwardCache)
          ->trial_name()));

  // 1) Navigate to A.
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImpl* rfh_a = current_frame_host();
  RenderFrameDeletedObserver delete_observer_rfh_a(rfh_a);

  // Request for an image and send a response to trigger loading code. This is
  // to ensure kLoadingTasksUnfreezable won't trigger bfcache activation.
  EXPECT_TRUE(ExecJs(rfh_a, R"(
      var image = document.createElement("img");
      image.src = "image.png";
      document.body.appendChild(image);
    )"));
  image_response.WaitForRequest();
  image_response.Send(net::HTTP_OK, "image/png");
  image_response.Send("image_body");
  image_response.Done();

  // 2) Navigate to B.
  EXPECT_TRUE(NavigateToURL(shell(), url_b));

  // 3) A shouldn't be stored in back-forward cache because it's disabled.
  delete_observer_rfh_a.WaitUntilDeleted();

  // Nothing is recorded when back-forward cache is disabled.
  ExpectOutcomeDidNotChange(FROM_HERE);
  ExpectNotRestoredDidNotChange(FROM_HERE);

  // Ensure that the trials still haven't been activated.
  EXPECT_FALSE(base::FieldTrialList::IsTrialActive(
      base::FeatureList::GetFieldTrial(features::kBackForwardCache)
          ->trial_name()));
}

IN_PROC_BROWSER_TEST_F(
    BackForwardCacheBrowserTest,
    EvictingDocumentsInRelatedSiteInstancesDoesNotRestartNavigation) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a1(embedded_test_server()->GetURL("a.com", "/title1.html#part1"));
  GURL url_a2(embedded_test_server()->GetURL("a.com", "/title1.html#part2"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  // 1) Navigate to A1.
  EXPECT_TRUE(NavigateToURL(shell(), url_a1));

  // 2) Navigate to A2.
  EXPECT_TRUE(NavigateToURL(shell(), url_a2));

  // 3) Navigate to B.
  EXPECT_TRUE(NavigateToURL(shell(), url_b));

  // 4) Go back to A2, but do not wait for the navigation to commit.
  web_contents()->GetController().GoBack();

  // 5) Go back to A1.
  // This will attempt to evict A2 from the cache because
  // their navigation entries have related site instances, while a navigation
  // to A2 is in flight. Ensure that we do not try to restart it as it should
  // be superseded by a navigation to A1.
  ASSERT_TRUE(HistoryGoBack(web_contents()));
  EXPECT_EQ(url_a1, web_contents()->GetLastCommittedURL());
}

namespace {

class ExecJsInDidFinishNavigation : public WebContentsObserver {
 public:
  explicit ExecJsInDidFinishNavigation(WebContents* web_contents)
      : WebContentsObserver(web_contents) {}

  void DidFinishNavigation(NavigationHandle* navigation_handle) override {
    if (!navigation_handle->IsInMainFrame() ||
        !navigation_handle->HasCommitted() ||
        navigation_handle->IsSameDocument()) {
      return;
    }

    ExecuteScriptAsync(navigation_handle->GetRenderFrameHost(),
                       "var foo = 42;");
  }
};

}  // namespace

// This test checks that the message posted from DidFinishNavigation
// (ExecuteScriptAsync) is received after the message restoring the page from
// the back-forward cache (PageMsg_RestorePageFromBackForwardCache).
IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest,
                       MessageFromDidFinishNavigation) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  // 1) Navigate to A.
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImpl* rfh_a = current_frame_host();
  EXPECT_TRUE(ExecJs(rfh_a, "window.alive = 'I am alive';"));

  // 2) Navigate to B.
  EXPECT_TRUE(NavigateToURL(shell(), url_b));

  ExecJsInDidFinishNavigation observer(shell()->web_contents());

  // 3) Go back to A. Expect the page to be restored from the cache.
  ASSERT_TRUE(HistoryGoBack(web_contents()));
  EXPECT_EQ("I am alive", EvalJs(rfh_a, "window.alive"));

  // Make sure that the javascript execution requested from DidFinishNavigation
  // did not result in eviction. If the document was evicted, the document
  // would be reloaded - check that it didn't happen and the tab is not
  // loading.
  EXPECT_FALSE(web_contents()->IsLoading());

  EXPECT_EQ(rfh_a, current_frame_host());
}

#if BUILDFLAG(IS_ANDROID)
IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest,
                       ChildImportanceTestForBackForwardCachedPagesTest) {
  web_contents()->SetPrimaryMainFrameImportance(
      ChildProcessImportance::MODERATE);

  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  // 1) Navigate to A.
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImpl* rfh_a = current_frame_host();
  RenderFrameDeletedObserver delete_observer_rfh_a(rfh_a);

  // 2) Navigate to B.
  EXPECT_TRUE(NavigateToURL(shell(), url_b));
  ASSERT_FALSE(delete_observer_rfh_a.deleted());

  // 3) Verify the importance of page after entering back-forward cache to be
  // "NORMAL".
  EXPECT_EQ(ChildProcessImportance::NORMAL,
            rfh_a->GetProcess()->GetEffectiveImportance());

  // 4) Go back to A.
  ASSERT_TRUE(HistoryGoBack(web_contents()));

  // 5) Verify the importance was restored correctly after page leaves
  // back-forward cache.
  EXPECT_EQ(ChildProcessImportance::MODERATE,
            rfh_a->GetProcess()->GetEffectiveImportance());
}
#endif

IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest, PageshowMetrics) {
  // TODO(crbug.com/40702446): Do not check for unexpected messages
  // because the input task queue is not currently frozen, causing flakes in
  // this test.
  DoNotFailForUnexpectedMessagesWhileCached();
  ASSERT_TRUE(embedded_test_server()->Start());

  const char kHistogramName[] =
      "BackForwardCache.MainFrameHasPageshowListenersOnRestore";

  const GURL url1(embedded_test_server()->GetURL("a.com", "/title1.html"));
  const GURL url2(embedded_test_server()->GetURL("b.com", "/title1.html"));

  // 1) Navigate to the page.
  EXPECT_TRUE(NavigateToURL(shell(), url1));
  EXPECT_TRUE(ExecJs(current_frame_host(), R"(
    window.foo = 42;
  )"));

  // 2) Navigate away and back.
  EXPECT_TRUE(NavigateToURL(shell(), url2));
  ASSERT_TRUE(HistoryGoBack(web_contents()));

  // As we don't get an explicit ACK when the page is restored (yet), force
  // a round-trip to the renderer to effectively flush the queue.
  EXPECT_EQ(42, EvalJs(current_frame_host(), "window.foo"));

  // Expect the back-forward restore without pageshow to be detected.
  content::FetchHistogramsFromChildProcesses();
  EXPECT_THAT(histogram_tester().GetAllSamples(kHistogramName),
              ElementsAre(base::Bucket(0, 1)));

  EXPECT_TRUE(ExecJs(current_frame_host(), R"(
    window.addEventListener("pageshow", () => {});
  )"));

  // 3) Navigate away and back (again).
  EXPECT_TRUE(NavigateToURL(shell(), url2));
  ASSERT_TRUE(HistoryGoBack(web_contents()));

  // As we don't get an explicit ACK when the page is restored (yet), force
  // a round-trip to the renderer to effectively flush the queue.
  EXPECT_EQ(42, EvalJs(current_frame_host(), "window.foo"));

  // Expect the back-forward restore with pageshow to be detected.
  content::FetchHistogramsFromChildProcesses();
  EXPECT_THAT(histogram_tester().GetAllSamples(kHistogramName),
              ElementsAre(base::Bucket(0, 1), base::Bucket(1, 1)));
}

// Navigate from A(B) to C and check IsActive status for RenderFrameHost A
// and B before and after entering back-forward cache.
IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest, CheckIsActive) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b)"));
  GURL url_c(embedded_test_server()->GetURL("c.com", "/title1.html"));

  // 1) Navigate to A(B).
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImpl* rfh_a = current_frame_host();
  RenderFrameHostImpl* rfh_b = rfh_a->child_at(0)->current_frame_host();

  EXPECT_TRUE(rfh_a->IsActive());
  EXPECT_TRUE(rfh_b->IsActive());

  // 2) Navigate to C.
  EXPECT_TRUE(NavigateToURL(shell(), url_c));
  EXPECT_TRUE(rfh_a->IsInBackForwardCache());
  EXPECT_TRUE(rfh_b->IsInBackForwardCache());

  EXPECT_FALSE(rfh_a->IsActive());
  EXPECT_FALSE(rfh_b->IsActive());
}

// Test that LifecycleStateImpl is updated correctly when page enters and
// restores back from BackForwardCache.
IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest,
                       CheckLifecycleStateTransition) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title2.html"));

  // 1) Navigate to A and check the LifecycleStateImpl of A.
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImpl* rfh_a = current_frame_host();
  EXPECT_EQ(RenderFrameHostImpl::LifecycleStateImpl::kActive,
            rfh_a->lifecycle_state());
  EXPECT_EQ(RenderFrameHost::LifecycleState::kActive,
            rfh_a->GetLifecycleState());
  EXPECT_TRUE(rfh_a->GetPage().IsPrimary());
  EXPECT_TRUE(rfh_a->IsInPrimaryMainFrame());

  // 2) Navigate to B, now A enters BackForwardCache. Check the
  // LifecycleStateImpl of both RenderFrameHost A and B.
  {
    ::testing::NiceMock<MockWebContentsObserver> state_change_observer(
        web_contents());
    EXPECT_CALL(state_change_observer,
                RenderFrameHostStateChanged(
                    rfh_a, RenderFrameHost::LifecycleState::kActive,
                    RenderFrameHost::LifecycleState::kInBackForwardCache));
    // We don't know |rfh_b| yet, so we'll match any frame.
    EXPECT_CALL(state_change_observer,
                RenderFrameHostStateChanged(
                    ::testing::Not(rfh_a),
                    RenderFrameHost::LifecycleState::kPendingCommit,
                    RenderFrameHost::LifecycleState::kActive));

    EXPECT_TRUE(NavigateToURL(shell(), url_b));
  }
  RenderFrameHostImpl* rfh_b = current_frame_host();
  EXPECT_TRUE(rfh_a->IsInBackForwardCache());
  EXPECT_EQ(RenderFrameHostImpl::LifecycleStateImpl::kInBackForwardCache,
            rfh_a->lifecycle_state());
  EXPECT_EQ(RenderFrameHost::LifecycleState::kInBackForwardCache,
            rfh_a->GetLifecycleState());
  EXPECT_FALSE(rfh_a->GetPage().IsPrimary());
  EXPECT_FALSE(rfh_a->IsInPrimaryMainFrame());
  EXPECT_EQ(RenderFrameHostImpl::LifecycleStateImpl::kActive,
            rfh_b->lifecycle_state());
  EXPECT_EQ(RenderFrameHost::LifecycleState::kActive,
            rfh_b->GetLifecycleState());
  EXPECT_TRUE(rfh_b->GetPage().IsPrimary());
  EXPECT_TRUE(rfh_b->IsInPrimaryMainFrame());

  // 3) Go back to A and check again the LifecycleStateImpl of both
  // RenderFrameHost A and B.
  {
    ::testing::NiceMock<MockWebContentsObserver> state_change_observer(
        web_contents());
    EXPECT_CALL(state_change_observer,
                RenderFrameHostStateChanged(
                    rfh_a, RenderFrameHost::LifecycleState::kInBackForwardCache,
                    RenderFrameHost::LifecycleState::kActive));
    EXPECT_CALL(state_change_observer,
                RenderFrameHostStateChanged(
                    rfh_b, RenderFrameHost::LifecycleState::kActive,
                    RenderFrameHost::LifecycleState::kInBackForwardCache));

    ASSERT_TRUE(HistoryGoBack(web_contents()));
  }
  EXPECT_EQ(RenderFrameHostImpl::LifecycleStateImpl::kActive,
            rfh_a->lifecycle_state());
  EXPECT_TRUE(rfh_a->GetPage().IsPrimary());
  EXPECT_TRUE(rfh_a->IsInPrimaryMainFrame());
  EXPECT_TRUE(rfh_b->IsInBackForwardCache());
  EXPECT_EQ(RenderFrameHostImpl::LifecycleStateImpl::kInBackForwardCache,
            rfh_b->lifecycle_state());
  EXPECT_FALSE(rfh_b->GetPage().IsPrimary());
  EXPECT_FALSE(rfh_b->IsInPrimaryMainFrame());
}

IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest,
                       CheckLifecycleStateTransitionWithSubframes) {
  IsolateAllSitesForTesting(base::CommandLine::ForCurrentProcess());
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b)"));
  GURL url_c(embedded_test_server()->GetURL(
      "c.com", "/cross_site_iframe_factory.html?c(d)"));

  // Navigate to A(B) and check the lifecycle states of A and B.
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImpl* rfh_a = current_frame_host();
  RenderFrameHostImpl* rfh_b = rfh_a->child_at(0)->current_frame_host();
  EXPECT_FALSE(rfh_a->IsInBackForwardCache());
  EXPECT_FALSE(rfh_b->IsInBackForwardCache());
  EXPECT_EQ(RenderFrameHostImpl::LifecycleStateImpl::kActive,
            rfh_a->lifecycle_state());
  EXPECT_EQ(RenderFrameHost::LifecycleState::kActive,
            rfh_a->GetLifecycleState());
  EXPECT_EQ(RenderFrameHostImpl::LifecycleStateImpl::kActive,
            rfh_b->lifecycle_state());
  EXPECT_EQ(RenderFrameHost::LifecycleState::kActive,
            rfh_b->GetLifecycleState());

  // Navigate to C(D), now A(B) enters BackForwardCache.
  {
    ::testing::NiceMock<MockWebContentsObserver> state_change_observer(
        web_contents());
    EXPECT_CALL(state_change_observer,
                RenderFrameHostStateChanged(
                    rfh_a, RenderFrameHost::LifecycleState::kActive,
                    RenderFrameHost::LifecycleState::kInBackForwardCache));
    EXPECT_CALL(state_change_observer,
                RenderFrameHostStateChanged(
                    rfh_b, RenderFrameHost::LifecycleState::kActive,
                    RenderFrameHost::LifecycleState::kInBackForwardCache));
    // We don't know |rfh_c| and |rfh_d| yet, so we'll match any frame.
    EXPECT_CALL(state_change_observer,
                RenderFrameHostStateChanged(
                    ::testing::Not(::testing::AnyOf(rfh_a, rfh_b)),
                    RenderFrameHost::LifecycleState::kPendingCommit,
                    RenderFrameHost::LifecycleState::kActive))
        .Times(2);
    // Deletion of frame D's initial RFH.
    EXPECT_CALL(state_change_observer,
                RenderFrameHostStateChanged(
                    ::testing::Not(::testing::AnyOf(rfh_a, rfh_b)),
                    RenderFrameHost::LifecycleState::kActive,
                    RenderFrameHost::LifecycleState::kPendingDeletion));

    EXPECT_TRUE(NavigateToURL(shell(), url_c));
  }
  RenderFrameHostImpl* rfh_c = current_frame_host();
  RenderFrameHostImpl* rfh_d = rfh_c->child_at(0)->current_frame_host();
  EXPECT_TRUE(rfh_a->IsInBackForwardCache());
  EXPECT_TRUE(rfh_b->IsInBackForwardCache());
  EXPECT_FALSE(rfh_c->IsInBackForwardCache());
  EXPECT_FALSE(rfh_d->IsInBackForwardCache());
  EXPECT_EQ(RenderFrameHostImpl::LifecycleStateImpl::kInBackForwardCache,
            rfh_a->lifecycle_state());
  EXPECT_EQ(RenderFrameHost::LifecycleState::kInBackForwardCache,
            rfh_a->GetLifecycleState());
  EXPECT_EQ(RenderFrameHostImpl::LifecycleStateImpl::kInBackForwardCache,
            rfh_b->lifecycle_state());
  EXPECT_EQ(RenderFrameHost::LifecycleState::kInBackForwardCache,
            rfh_b->GetLifecycleState());
  EXPECT_EQ(RenderFrameHostImpl::LifecycleStateImpl::kActive,
            rfh_c->lifecycle_state());
  EXPECT_EQ(RenderFrameHost::LifecycleState::kActive,
            rfh_c->GetLifecycleState());
  EXPECT_EQ(RenderFrameHostImpl::LifecycleStateImpl::kActive,
            rfh_d->lifecycle_state());
  EXPECT_EQ(RenderFrameHost::LifecycleState::kActive,
            rfh_d->GetLifecycleState());

  // Go back to A(B), A(B) is restored and C(D) enters BackForwardCache.
  {
    ::testing::NiceMock<MockWebContentsObserver> state_change_observer(
        web_contents());
    EXPECT_CALL(state_change_observer,
                RenderFrameHostStateChanged(
                    rfh_a, RenderFrameHost::LifecycleState::kInBackForwardCache,
                    RenderFrameHost::LifecycleState::kActive));
    EXPECT_CALL(state_change_observer,
                RenderFrameHostStateChanged(
                    rfh_b, RenderFrameHost::LifecycleState::kInBackForwardCache,
                    RenderFrameHost::LifecycleState::kActive));
    EXPECT_CALL(state_change_observer,
                RenderFrameHostStateChanged(
                    rfh_c, RenderFrameHost::LifecycleState::kActive,
                    RenderFrameHost::LifecycleState::kInBackForwardCache));
    EXPECT_CALL(state_change_observer,
                RenderFrameHostStateChanged(
                    rfh_d, RenderFrameHost::LifecycleState::kActive,
                    RenderFrameHost::LifecycleState::kInBackForwardCache));

    ASSERT_TRUE(HistoryGoBack(web_contents()));
  }
  EXPECT_FALSE(rfh_a->IsInBackForwardCache());
  EXPECT_FALSE(rfh_b->IsInBackForwardCache());
  EXPECT_TRUE(rfh_c->IsInBackForwardCache());
  EXPECT_TRUE(rfh_d->IsInBackForwardCache());
  EXPECT_EQ(RenderFrameHostImpl::LifecycleStateImpl::kActive,
            rfh_a->lifecycle_state());
  EXPECT_EQ(RenderFrameHost::LifecycleState::kActive,
            rfh_a->GetLifecycleState());
  EXPECT_EQ(RenderFrameHostImpl::LifecycleStateImpl::kActive,
            rfh_b->lifecycle_state());
  EXPECT_EQ(RenderFrameHost::LifecycleState::kActive,
            rfh_b->GetLifecycleState());
  EXPECT_EQ(RenderFrameHostImpl::LifecycleStateImpl::kInBackForwardCache,
            rfh_c->lifecycle_state());
  EXPECT_EQ(RenderFrameHost::LifecycleState::kInBackForwardCache,
            rfh_c->GetLifecycleState());
  EXPECT_EQ(RenderFrameHostImpl::LifecycleStateImpl::kInBackForwardCache,
            rfh_d->lifecycle_state());
  EXPECT_EQ(RenderFrameHost::LifecycleState::kInBackForwardCache,
            rfh_d->GetLifecycleState());
}

namespace {

class EchoFakeWithFilter final : public mojom::Echo {
 public:
  explicit EchoFakeWithFilter(mojo::PendingReceiver<mojom::Echo> receiver,
                              std::unique_ptr<mojo::MessageFilter> filter)
      : receiver_(this, std::move(receiver)) {
    receiver_.SetFilter(std::move(filter));
  }
  ~EchoFakeWithFilter() override = default;

  // mojom::Echo implementation
  void EchoString(const std::string& input,
                  EchoStringCallback callback) override {
    std::move(callback).Run(input);
  }

 private:
  mojo::Receiver<mojom::Echo> receiver_;
};

}  // namespace

IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest,
                       MessageReceivedOnAssociatedInterfaceWhileCached) {
  DoNotFailForUnexpectedMessagesWhileCached();
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  // 1) Navigate to A.
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImpl* rfh_a = current_frame_host();
  RenderFrameDeletedObserver delete_observer_rfh_a(rfh_a);
  PageLifecycleStateManagerTestDelegate delegate(
      rfh_a->render_view_host()->GetPageLifecycleStateManager());

  // 2) Navigate to B.
  EXPECT_TRUE(NavigateToURL(shell(), url_b));
  ASSERT_TRUE(delegate.WaitForInBackForwardCacheAck());
  ASSERT_FALSE(delete_observer_rfh_a.deleted());
  EXPECT_TRUE(rfh_a->IsInBackForwardCache());

  mojo::Remote<mojom::Echo> remote;
  EchoFakeWithFilter echo(
      remote.BindNewPipeAndPassReceiver(),
      rfh_a->CreateMessageFilterForAssociatedReceiver(mojom::Echo::Name_));

  base::RunLoop loop;
  remote->EchoString(
      "", base::BindLambdaForTesting([&](const std::string&) { loop.Quit(); }));
  loop.Run();

  ExpectBucketCount(
      "BackForwardCache.UnexpectedRendererToBrowserMessage.InterfaceName",
      base::HistogramBase::Sample(
          static_cast<int32_t>(base::HashMetricName(mojom::Echo::Name_))),
      1);
}

IN_PROC_BROWSER_TEST_F(
    BackForwardCacheBrowserTest,
    MessageReceivedOnAssociatedInterfaceWhileCachedForProcessWithNonCachedPages) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL("/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("/title2.html"));

  // 1) Navigate to A.
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImpl* rfh_a = current_frame_host();
  RenderFrameDeletedObserver delete_observer_rfh_a(rfh_a);
  PageLifecycleStateManagerTestDelegate delegate(
      rfh_a->render_view_host()->GetPageLifecycleStateManager());

  // 2) Navigate to B.
  EXPECT_TRUE(NavigateToURL(shell(), url_b));
  ASSERT_TRUE(delegate.WaitForInBackForwardCacheAck());
  RenderFrameHostImpl* rfh_b = current_frame_host();
  ASSERT_FALSE(delete_observer_rfh_a.deleted());
  EXPECT_TRUE(rfh_a->IsInBackForwardCache());
  // Make sure both pages are on the same process (they are same site so they
  // should).
  ASSERT_EQ(rfh_a->GetProcess(), rfh_b->GetProcess());

  mojo::Remote<mojom::Echo> remote;
  EchoFakeWithFilter echo(
      remote.BindNewPipeAndPassReceiver(),
      rfh_a->CreateMessageFilterForAssociatedReceiver(mojom::Echo::Name_));

  remote->EchoString("", base::NullCallback());
  // Give the killing a chance to run. (We do not expect a kill but need to
  // "wait" for it to not happen)
  base::RunLoop().RunUntilIdle();

  // 3) Go back to A.
  ASSERT_TRUE(HistoryGoBack(web_contents()));

  ExpectRestored(FROM_HERE);
}

IN_PROC_BROWSER_TEST_F(
    HighCacheSizeBackForwardCacheBrowserTest,
    MessageReceivedOnAssociatedInterfaceForProcessWithMultipleCachedPages) {
  DoNotFailForUnexpectedMessagesWhileCached();
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a_1(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_a_2(embedded_test_server()->GetURL("a.com", "/title2.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  // Get url_a_1 and url_a_2 into the cache.
  EXPECT_TRUE(NavigateToURL(shell(), url_a_1));
  RenderFrameHostImpl* rfh_a_1 = current_frame_host();
  RenderFrameDeletedObserver delete_observer_rfh_a_1(rfh_a_1);

  EXPECT_TRUE(NavigateToURL(shell(), url_a_2));
  RenderFrameHostImpl* rfh_a_2 = current_frame_host();
  RenderFrameDeletedObserver delete_observer_rfh_a_2(rfh_a_2);

  EXPECT_TRUE(NavigateToURL(shell(), url_b));
  RenderFrameHostImpl* rfh_b = current_frame_host();
  RenderFrameDeletedObserver delete_observer_rfh_b(rfh_b);

  ASSERT_FALSE(delete_observer_rfh_a_1.deleted());
  ASSERT_FALSE(delete_observer_rfh_a_2.deleted());
  EXPECT_TRUE(rfh_a_1->IsInBackForwardCache());
  EXPECT_TRUE(rfh_a_2->IsInBackForwardCache());
  ASSERT_EQ(rfh_a_1->GetProcess(), rfh_a_2->GetProcess());

  mojo::Remote<mojom::Echo> remote;
  EchoFakeWithFilter echo(
      remote.BindNewPipeAndPassReceiver(),
      rfh_a_1->CreateMessageFilterForAssociatedReceiver(mojom::Echo::Name_));

  base::RunLoop loop;
  remote->EchoString(
      "", base::BindLambdaForTesting([&](const std::string&) { loop.Quit(); }));
  loop.Run();

  ExpectBucketCount(
      "BackForwardCache.UnexpectedRendererToBrowserMessage.InterfaceName",
      base::HistogramBase::Sample(
          static_cast<int32_t>(base::HashMetricName(mojom::Echo::Name_))),
      1);

  EXPECT_FALSE(delete_observer_rfh_b.deleted());
}

IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest,
                       MessageReceivedOnAssociatedInterfaceWhileFreezing) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));
  url::Origin origin_a = url::Origin::Create(url_a);
  url::Origin origin_b = url::Origin::Create(url_b);

  // 1) Navigate to A.
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImpl* rfh_a = current_frame_host();
  RenderFrameDeletedObserver delete_observer_rfh_a(rfh_a);
  PageLifecycleStateManagerTestDelegate delegate(
      rfh_a->render_view_host()->GetPageLifecycleStateManager());

  mojo::Remote<mojom::Echo> remote;
  EchoFakeWithFilter echo(
      remote.BindNewPipeAndPassReceiver(),
      rfh_a->CreateMessageFilterForAssociatedReceiver(mojom::Echo::Name_));

  delegate.OnStoreInBackForwardCacheSent(base::BindLambdaForTesting(
      [&]() { remote->EchoString("", base::NullCallback()); }));

  delegate.OnRestoreFromBackForwardCacheSent(base::BindLambdaForTesting(
      [&]() { remote->EchoString("", base::NullCallback()); }));

  // 2) Navigate to B.
  EXPECT_TRUE(NavigateToURL(shell(), url_b));

  // 3) Go back to A.
  ASSERT_TRUE(HistoryGoBack(web_contents()));

  ExpectRestored(FROM_HERE);
}

// Tests that if a page is already ineligible to be saved in the back-forward
// cache at navigation time, we shouldn't try to proactively swap
// BrowsingInstances.
IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest,
                       ShouldNotSwapBrowsingInstanceWhenPageWillNotBeCached) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_1(embedded_test_server()->GetURL("/title1.html"));
  GURL url_2(embedded_test_server()->GetURL("/title2.html"));
  GURL url_3(embedded_test_server()->GetURL("/title3.html"));

  // 1) Navigate to |url_1| .
  EXPECT_TRUE(NavigateToURL(shell(), url_1));
  RenderFrameHostImpl* rfh_1 = current_frame_host();
  scoped_refptr<SiteInstanceImpl> site_instance_1 =
      static_cast<SiteInstanceImpl*>(rfh_1->GetSiteInstance());

  // 2) Navigate to |url_2|.
  EXPECT_TRUE(NavigateToURL(shell(), url_2));
  RenderFrameHostImpl* rfh_2 = current_frame_host();
  RenderFrameDeletedObserver rfh_2_deleted_observer(rfh_2);
  scoped_refptr<SiteInstanceImpl> site_instance_2 =
      static_cast<SiteInstanceImpl*>(rfh_2->GetSiteInstance());

  // |rfh_1| should get into the back-forward cache.
  EXPECT_TRUE(rfh_1->IsInBackForwardCache());
  // Check that title1.html and title2.html are in different BrowsingInstances.
  EXPECT_FALSE(site_instance_1->IsRelatedSiteInstance(site_instance_2.get()));

  // Disable the BackForwardCache for |rfh_2|.
  DisableBFCacheForRFHForTesting(rfh_2->GetGlobalId());

  // 3) Navigate to |url_3|.
  EXPECT_TRUE(NavigateToURL(shell(), url_3));
  RenderFrameHostImpl* rfh_3 = current_frame_host();
  scoped_refptr<SiteInstanceImpl> site_instance_3 =
      static_cast<SiteInstanceImpl*>(rfh_3->GetSiteInstance());

  // Check that |url_2| and |url_3| are reusing the same SiteInstance (and
  // BrowsingInstance).
  EXPECT_EQ(site_instance_2, site_instance_3);
  if (rfh_2 != rfh_3) {
    // If we aren't reusing the RenderFrameHost then |rfh_2| will eventually
    // get deleted because it's not saved in the back-forward cache.
    rfh_2_deleted_observer.WaitUntilDeleted();
  }
}

// We should try to reuse process on same-site renderer-initiated navigations.
IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest,
                       RendererInitiatedSameSiteNavigationReusesProcess) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_1(embedded_test_server()->GetURL("/title1.html"));
  GURL url_2(embedded_test_server()->GetURL("/title2.html"));

  // Navigate to title1.html.
  EXPECT_TRUE(NavigateToURL(shell(), url_1));
  scoped_refptr<SiteInstanceImpl> site_instance_1 =
      web_contents()->GetPrimaryMainFrame()->GetSiteInstance();
  // Navigate to title2.html. The navigation is document/renderer initiated.
  EXPECT_TRUE(NavigateToURLFromRenderer(shell(), url_2));
  scoped_refptr<SiteInstanceImpl> site_instance_2 =
      web_contents()->GetPrimaryMainFrame()->GetSiteInstance();

  // Check that title1.html and title2.html are in different BrowsingInstances
  // but have the same renderer process.
  EXPECT_FALSE(site_instance_1->IsRelatedSiteInstance(site_instance_2.get()));
  EXPECT_EQ(site_instance_1->GetProcess(), site_instance_2->GetProcess());
}

// We should try to reuse process on same-site browser-initiated navigations.
IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest,
                       BrowserInitiatedSameSiteNavigationReusesProcess) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_1(embedded_test_server()->GetURL("/title1.html"));
  GURL url_2(embedded_test_server()->GetURL("/title2.html"));

  // 1) Navigate to title1.html.
  EXPECT_TRUE(NavigateToURL(shell(), url_1));
  scoped_refptr<SiteInstanceImpl> site_instance_1 =
      web_contents()->GetPrimaryMainFrame()->GetSiteInstance();
  // 2) Navigate to title2.html. The navigation is browser initiated.
  EXPECT_TRUE(NavigateToURL(shell(), url_2));
  scoped_refptr<SiteInstanceImpl> site_instance_2 =
      web_contents()->GetPrimaryMainFrame()->GetSiteInstance();

  // Check that title1.html and title2.html are in different BrowsingInstances
  // but have the same renderer process.
  EXPECT_FALSE(site_instance_1->IsRelatedSiteInstance(site_instance_2.get()));
  EXPECT_EQ(site_instance_1->GetProcess(), site_instance_2->GetProcess());

  // 3) Do a back navigation to title1.html.
  ASSERT_TRUE(HistoryGoBack(web_contents()));
  EXPECT_EQ(web_contents()->GetLastCommittedURL(), url_1);
  scoped_refptr<SiteInstanceImpl> site_instance_1_history_nav =
      web_contents()->GetPrimaryMainFrame()->GetSiteInstance();

  // We will reuse the SiteInstance and renderer process of |site_instance_1|.
  EXPECT_EQ(site_instance_1_history_nav, site_instance_1);
  EXPECT_EQ(site_instance_1_history_nav->GetProcess(),
            site_instance_1->GetProcess());
}

// We should not try to reuse process on cross-site navigations.
IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest,
                       CrossSiteNavigationDoesNotReuseProcess) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL a1_url(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL b_url(embedded_test_server()->GetURL("b.com", "/title1.html"));
  GURL a2_url(embedded_test_server()->GetURL("a.com", "/title2.html"));

  // Navigate to A1.
  EXPECT_TRUE(NavigateToURL(shell(), a1_url));
  scoped_refptr<SiteInstanceImpl> a1_site_instance =
      web_contents()->GetPrimaryMainFrame()->GetSiteInstance();
  // Navigate to B. The navigation is browser initiated.
  EXPECT_TRUE(NavigateToURL(shell(), b_url));
  scoped_refptr<SiteInstanceImpl> b_site_instance =
      web_contents()->GetPrimaryMainFrame()->GetSiteInstance();

  // Check that A1 and B are in different BrowsingInstances and renderer
  // processes.
  EXPECT_FALSE(a1_site_instance->IsRelatedSiteInstance(b_site_instance.get()));
  EXPECT_NE(a1_site_instance->GetProcess(), b_site_instance->GetProcess());

  // Navigate to A2. The navigation is renderer-initiated.
  EXPECT_TRUE(NavigateToURLFromRenderer(shell(), a2_url));
  scoped_refptr<SiteInstanceImpl> a2_site_instance =
      web_contents()->GetPrimaryMainFrame()->GetSiteInstance();

  // Check that B and A2 are in different BrowsingInstances and renderer
  // processes.
  EXPECT_FALSE(b_site_instance->IsRelatedSiteInstance(a2_site_instance.get()));
  EXPECT_NE(b_site_instance->GetProcess(), a2_site_instance->GetProcess());
}

// This observer keeps tracks whether a given RenderViewHost is deleted or not
// to avoid accessing it and causing use-after-free condition.
class RenderViewHostDeletedObserver : public WebContentsObserver {
 public:
  explicit RenderViewHostDeletedObserver(RenderViewHost* rvh)
      : WebContentsObserver(WebContents::FromRenderViewHost(rvh)),
        render_view_host_(rvh),
        deleted_(false) {}

  void RenderViewDeleted(RenderViewHost* render_view_host) override {
    if (render_view_host_ == render_view_host)
      deleted_ = true;
  }

  bool deleted() const { return deleted_; }

 private:
  raw_ptr<RenderViewHost, AcrossTasksDanglingUntriaged> render_view_host_;
  bool deleted_;
};

// Tests that RenderViewHost is deleted on eviction along with
// RenderProcessHost.
IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest,
                       RenderViewHostDeletedOnEviction) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  NavigationControllerImpl& controller = web_contents()->GetController();
  BackForwardCacheImpl& cache = controller.GetBackForwardCache();

  // 1) Navigate to A.
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImpl* rfh_a = current_frame_host();
  RenderFrameDeletedObserver delete_observer_rfh_a(rfh_a);
  RenderViewHostDeletedObserver delete_observer_rvh_a(
      rfh_a->GetRenderViewHost());

  RenderProcessHost* process = rfh_a->GetProcess();
  RenderProcessHostWatcher destruction_observer(
      process, RenderProcessHostWatcher::WATCH_FOR_HOST_DESTRUCTION);
  cache.Flush();

  // 2) Navigate to B. A should be stored in cache, count of entries should
  // be 1.
  EXPECT_TRUE(NavigateToURL(shell(), url_b));
  EXPECT_TRUE(rfh_a->IsInBackForwardCache());
  EXPECT_EQ(1u, cache.GetEntries().size());

  // 3) Initiate eviction of rfh_a from BackForwardCache. Entries should be 0.
  // RenderViewHost, RenderProcessHost and RenderFrameHost should all be
  // deleted.
  EXPECT_TRUE(rfh_a->IsInactiveAndDisallowActivation(
      DisallowActivationReasonId::kForTesting));
  destruction_observer.Wait();
  ASSERT_TRUE(delete_observer_rvh_a.deleted());
  delete_observer_rfh_a.WaitUntilDeleted();
  EXPECT_EQ(0u, cache.GetEntries().size());
}

// Tests that cross-process sub-frame's RenderViewHost is deleted on root
// RenderFrameHost eviction from BackForwardCache along with its
// RenderProcessHost.
IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest,
                       CrossProcessSubFrameRenderViewHostDeletedOnEviction) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b)"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  // 1) Navigate to A.
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImpl* a1 = current_frame_host();
  RenderFrameHostImpl* b1 = a1->child_at(0)->current_frame_host();
  RenderFrameDeletedObserver delete_observer_rfh_b1(b1);

  RenderViewHostDeletedObserver delete_observer_rvh_b1(b1->GetRenderViewHost());

  // 2) Navigate to URL B.
  EXPECT_TRUE(NavigateToURL(shell(), url_b));
  EXPECT_TRUE(a1->IsInBackForwardCache());

  // 3) Initiate eviction of rfh a1 from BackForwardCache. RenderViewHost,
  // RenderProcessHost and RenderFrameHost of sub-frame b1 should all be deleted
  // on eviction.
  EXPECT_TRUE(a1->IsInactiveAndDisallowActivation(
      DisallowActivationReasonId::kForTesting));
  delete_observer_rfh_b1.WaitUntilDeleted();
  ASSERT_TRUE(delete_observer_rvh_b1.deleted());
}

// Tests that same-process sub-frame's RenderViewHost is deleted on root
// RenderFrameHost eviction from BackForwardCache along with its
// RenderProcessHost.
IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest,
                       SameProcessSubFrameRenderViewHostDeletedOnEviction) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(a)"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  // 1) Navigate to A.
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImpl* a1 = current_frame_host();
  RenderFrameHostImpl* a2 = a1->child_at(0)->current_frame_host();
  RenderFrameDeletedObserver delete_observer_rfh_a2(a2);

  RenderViewHostDeletedObserver delete_observer_rvh_a2(a2->GetRenderViewHost());

  RenderProcessHost* process = a2->GetProcess();
  RenderProcessHostWatcher destruction_observer(
      process, RenderProcessHostWatcher::WATCH_FOR_HOST_DESTRUCTION);

  // 2) Navigate to URL B.
  EXPECT_TRUE(NavigateToURL(shell(), url_b));
  EXPECT_TRUE(a1->IsInBackForwardCache());

  // 3) Initiate eviction of rfh a1 from BackForwardCache. RenderViewHost,
  // RenderProcessHost and RenderFrameHost of sub-frame a2 should all be
  // deleted.
  EXPECT_TRUE(a1->IsInactiveAndDisallowActivation(
      DisallowActivationReasonId::kForTesting));
  destruction_observer.Wait();
  ASSERT_TRUE(delete_observer_rvh_a2.deleted());
  delete_observer_rfh_a2.WaitUntilDeleted();
}

IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest,
                       NavigationCancelledAfterJsEvictionWasDisabled) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  // 1) Navigate to A.
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImpl* rfh_a = current_frame_host();
  RenderFrameDeletedObserver delete_observer_rfh_a(rfh_a);

  PageLifecycleStateManagerTestDelegate delegate(
      rfh_a->render_view_host()->GetPageLifecycleStateManager());

  // 2) Navigate to B.
  EXPECT_TRUE(NavigateToURL(shell(), url_b));

  RenderFrameHostImpl* rfh_b = current_frame_host();

  delegate.OnDisableJsEvictionSent(base::BindLambdaForTesting([&]() {
    // Posted because Stop() will destroy the NavigationRequest but
    // DisableJsEviction will be called from inside the navigation which may
    // not be a safe place to destruct a NavigationRequest.
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(&WebContentsImpl::Stop,
                                  base::Unretained(web_contents())));
  }));

  // 3) Do not go back to A (navigation cancelled).
  web_contents()->GetController().GoBack();
  ASSERT_TRUE(WaitForLoadStop(web_contents()));

  EXPECT_EQ(rfh_b, current_frame_host());

  delete_observer_rfh_a.WaitUntilDeleted();

  // 4) Go back to A.
  ASSERT_TRUE(HistoryGoBack(web_contents()));

  ExpectNotRestored({NotRestoredReason::kNavigationCancelledWhileRestoring}, {},
                    {}, {}, {}, FROM_HERE);
}

IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest,
                       SubframeNavigationDoesNotRecordMetrics) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b)"));
  GURL url_c(embedded_test_server()->GetURL("c.com", "/title1.html"));

  // 1) Navigate to A(B).
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImpl* rfh_a = current_frame_host();

  // 2) Navigate from B to C.
  EXPECT_TRUE(NavigateFrameToURL(rfh_a->child_at(0), url_c));
  EXPECT_EQ(url_c,
            rfh_a->child_at(0)->current_frame_host()->GetLastCommittedURL());
  EXPECT_FALSE(rfh_a->IsInBackForwardCache());

  // 4) Go back from C to B.
  ASSERT_TRUE(HistoryGoBack(web_contents()));
  EXPECT_TRUE(
      rfh_a->child_at(0)->current_frame_host()->GetLastCommittedURL().DomainIs(
          "b.com"));
  EXPECT_FALSE(rfh_a->IsInBackForwardCache());

  // The reason why the frame is not cached in a subframe navigation is not
  // recorded.
  ExpectOutcomeDidNotChange(FROM_HERE);
}

IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest,
                       EnsureIsolationInfoForSubresourcesNotEmpty) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  BackForwardCacheImpl& cache =
      web_contents()->GetController().GetBackForwardCache();

  // 1) Navigate to A.
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImpl* rfh_a = current_frame_host();

  cache.Flush();

  // 2) Navigate to B. A should be stored in cache, count of entries should
  // be 1.
  EXPECT_TRUE(NavigateToURL(shell(), url_b));
  RenderFrameHostImpl* rfh_b = current_frame_host();
  EXPECT_TRUE(rfh_a->IsInBackForwardCache());
  EXPECT_EQ(1u, cache.GetEntries().size());

  // 3) GoBack to A. RenderFrameHost of A should be restored and B should be
  // stored in cache, count of entries should be 1. IsolationInfoForSubresources
  // of rfh_a should not be empty.
  ASSERT_TRUE(HistoryGoBack(web_contents()));
  EXPECT_EQ(rfh_a, current_frame_host());
  EXPECT_TRUE(rfh_b->IsInBackForwardCache());
  EXPECT_EQ(1u, cache.GetEntries().size());
  EXPECT_FALSE(rfh_a->GetIsolationInfoForSubresources().IsEmpty());

  // 4) GoForward to B. RenderFrameHost of B should be restored and A should be
  // stored in cache, count of entries should be 1. IsolationInfoForSubresources
  // of rfh_b should not be empty.
  ASSERT_TRUE(HistoryGoForward(web_contents()));
  EXPECT_EQ(rfh_b, current_frame_host());
  EXPECT_TRUE(rfh_a->IsInBackForwardCache());
  EXPECT_EQ(1u, cache.GetEntries().size());
  EXPECT_FALSE(rfh_b->GetIsolationInfoForSubresources().IsEmpty());
}

// Regression test for crbug.com/1183313, but for is_overriding_user_agent.
// Checks that we won't restore an entry from the BackForwardCache if the
// is_overriding_user_agent value used in the entry differs from the one used
// in the restoring navigation.
IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest,
                       DoNotRestoreWhenIsOverridingUserAgentDiffers) {
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));
  GURL url_c(embedded_test_server()->GetURL("c.com", "/title1.html"));
  NavigationControllerImpl& controller = web_contents()->GetController();
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();

  const std::string user_agent_override = "foo";

  // 1) Navigate to A without user agent override.
  {
    FrameNavigateParamsCapturer params_capturer(root);
    EXPECT_TRUE(NavigateToURL(shell(), url_a));
    params_capturer.Wait();
    EXPECT_FALSE(params_capturer.is_overriding_user_agent());
    EXPECT_NE(user_agent_override,
              EvalJs(shell()->web_contents(), "navigator.userAgent"));
  }

  RenderFrameHostImpl* rfh_a = current_frame_host();

  // Enable user agent override for future navigations.
  UserAgentInjector injector(shell()->web_contents(), user_agent_override);

  // 2) Navigate to B with user agent override.
  {
    FrameNavigateParamsCapturer params_capturer(root);
    EXPECT_TRUE(NavigateToURL(shell(), url_b));
    params_capturer.Wait();
    EXPECT_TRUE(params_capturer.is_overriding_user_agent());
    EXPECT_EQ(user_agent_override,
              EvalJs(shell()->web_contents(), "navigator.userAgent"));
  }

  // A should be stored in the back-forward cache.
  EXPECT_TRUE(rfh_a->IsInBackForwardCache());

  RenderFrameHostImpl* rfh_b = current_frame_host();

  // 3) Go back to A. RenderFrameHost of A should not be restored from the
  // back-forward cache, and "is_overriding_user_agent" is set to true
  // correctly.
  {
    RenderFrameDeletedObserver delete_observer(rfh_a);
    FrameNavigateParamsCapturer params_capturer(root);
    controller.GoBack();
    params_capturer.Wait();
    delete_observer.WaitUntilDeleted();
    EXPECT_TRUE(params_capturer.is_overriding_user_agent());
    EXPECT_EQ(user_agent_override,
              EvalJs(shell()->web_contents(), "navigator.userAgent"));
    ExpectNotRestored({NotRestoredReason::kNavigationCancelledWhileRestoring,
                       NotRestoredReason::kUserAgentOverrideDiffers},
                      {}, {}, {}, {}, FROM_HERE);
  }

  // B should be stored in the back-forward cache.
  EXPECT_TRUE(rfh_b->IsInBackForwardCache());

  // 4) Go forward to B. RenderFrameHost of B should be restored from the
  // back-forward cache, and "is_overriding_user_agent" is set to true
  // correctly.
  {
    FrameNavigateParamsCapturer params_capturer(root);
    controller.GoForward();
    params_capturer.Wait();
    EXPECT_TRUE(params_capturer.is_overriding_user_agent());
    EXPECT_EQ(user_agent_override,
              EvalJs(shell()->web_contents(), "navigator.userAgent"));
    EXPECT_EQ(rfh_b, current_frame_host());
    ExpectRestored(FROM_HERE);
  }

  // Stop overriding user agent from now on.
  injector.set_is_overriding_user_agent(false);

  // 5) Go to C, which should not do a user agent override.
  {
    FrameNavigateParamsCapturer params_capturer(root);
    EXPECT_TRUE(NavigateToURL(shell(), url_c));
    params_capturer.Wait();
    EXPECT_FALSE(params_capturer.is_overriding_user_agent());
    EXPECT_NE(user_agent_override,
              EvalJs(shell()->web_contents(), "navigator.userAgent"));
  }

  // B should be stored in the back-forward cache again.
  EXPECT_TRUE(rfh_b->IsInBackForwardCache());

  // 6) Go back to B. RenderFrameHost of B should not be restored from the
  // back-forward cache, and "is_overriding_user_agent" is set to false
  // correctly.
  {
    FrameNavigateParamsCapturer params_capturer(root);
    RenderFrameDeletedObserver delete_observer(rfh_b);
    controller.GoBack();
    params_capturer.Wait();
    delete_observer.WaitUntilDeleted();
    EXPECT_FALSE(params_capturer.is_overriding_user_agent());
    EXPECT_NE(user_agent_override,
              EvalJs(shell()->web_contents(), "navigator.userAgent"));
    ExpectNotRestored({NotRestoredReason::kNavigationCancelledWhileRestoring,
                       NotRestoredReason::kUserAgentOverrideDiffers},
                      {}, {}, {}, {}, FROM_HERE);
  }
}

IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest,
                       RestoreWhenUserAgentOverrideDiffers) {
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));
  NavigationControllerImpl& controller = web_contents()->GetController();
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();

  // Enable user agent override for future navigations.
  const std::string user_agent_override_1 = "foo";
  UserAgentInjector injector(shell()->web_contents(), user_agent_override_1);

  // 1) Start a new navigation to A with user agent override.
  {
    FrameNavigateParamsCapturer params_capturer(root);
    EXPECT_TRUE(NavigateToURL(shell(), url_a));
    params_capturer.Wait();
    EXPECT_TRUE(params_capturer.is_overriding_user_agent());
    EXPECT_EQ(user_agent_override_1,
              EvalJs(shell()->web_contents(), "navigator.userAgent"));
  }

  RenderFrameHostImpl* rfh_a = current_frame_host();

  // 2) Navigate to another page.
  EXPECT_TRUE(NavigateToURL(shell(), url_b));

  // A should be stored in the back-forward cache.
  EXPECT_TRUE(rfh_a->IsInBackForwardCache());

  // Change the user agent override string.
  const std::string user_agent_override_2 = "bar";
  injector.set_user_agent_override(user_agent_override_2);

  // 3) Go back to A, which should restore the page saved in the back-forward
  // cache and use the old user agent.
  // TODO(crbug.com/40758687): This should use the new UA override.
  {
    FrameNavigateParamsCapturer params_capturer(root);
    controller.GoBack();
    params_capturer.Wait();
    EXPECT_TRUE(params_capturer.is_overriding_user_agent());
    EXPECT_EQ(user_agent_override_1,
              EvalJs(shell()->web_contents(), "navigator.userAgent"));
    EXPECT_EQ(rfh_a, current_frame_host());
    ExpectRestored(FROM_HERE);
  }

  // 4) Navigate to another page, which should use the new user agent. Note that
  // we didn't do this in step 2 instead because the UA override change during
  // navigation would trigger a RendererPreferences to the active page (page A).
  {
    FrameNavigateParamsCapturer params_capturer(root);
    EXPECT_TRUE(NavigateToURL(shell(), url_b));
    params_capturer.Wait();
    EXPECT_TRUE(params_capturer.is_overriding_user_agent());
    EXPECT_EQ(user_agent_override_2,
              EvalJs(shell()->web_contents(), "navigator.userAgent"));
  }
}

IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest,
                       WebContentsDestroyedWhileRestoringThePageFromBFCache) {
  ASSERT_TRUE(embedded_test_server()->Start());

  Shell* shell = CreateBrowser();

  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  // 1) Navigate to A.
  EXPECT_TRUE(NavigateToURL(shell, url_a));

  // 2) Navigate to another page.
  EXPECT_TRUE(NavigateToURL(shell, url_b));

  // 3) Start navigating back.
  TestActivationManager activation_manager(shell->web_contents(), url_a);
  shell->web_contents()->GetController().GoBack();
  EXPECT_TRUE(activation_manager.WaitForBeforeChecks());

  ::testing::NiceMock<MockWebContentsObserver> observer(shell->web_contents());
  EXPECT_CALL(observer, DidFinishNavigation(_))
      .WillOnce(::testing::Invoke([](NavigationHandle* handle) {
        EXPECT_FALSE(handle->HasCommitted());
        EXPECT_TRUE(handle->IsServedFromBackForwardCache());
        // This call checks that |rfh_restored_from_back_forward_cache| is not
        // deleted and the virtual |GetRoutingID| does not crash.
        EXPECT_TRUE(NavigationRequest::From(handle)
                        ->GetRenderFrameHostRestoredFromBackForwardCache()
                        ->GetRoutingID());
      }));

  shell->Close();
}

// Test if the delegate doesn't support BFCache that the reason is
// recorded correctly.
IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest,
                       DelegateDoesNotSupportBackForwardCache) {
  // Set the delegate to null to force the default behavior.
  web_contents()->SetDelegate(nullptr);

  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  // BackForwardCache is empty.
  RenderFrameHostImpl* rfh_a = current_frame_host();
  RenderFrameDeletedObserver delete_observer_rfh_a(rfh_a);

  EXPECT_TRUE(NavigateToURL(shell(), url_b));
  // BackForwardCache contains only rfh_a.
  RenderFrameHostImpl* rfh_b = current_frame_host();
  RenderFrameDeletedObserver delete_observer_rfh_b(rfh_b);

  ASSERT_TRUE(HistoryGoToOffset(web_contents(), -1));
  ExpectNotRestored({NotRestoredReason::kBackForwardCacheDisabledForDelegate},
                    {}, {}, {}, {}, FROM_HERE);
}

// Tests what metrics are recorded when the BackForwardCache feature is
// disabled.
class BackForwardCacheDisabledBrowserTest : public ContentBrowserTest {
 protected:
  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    content::SetupCrossSiteRedirector(embedded_test_server());
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  void SetUp() override {
    feature_list_.InitAndDisableFeature(features::kBackForwardCache);
    ContentBrowserTest::SetUp();
  }

  void ExpectHistoryNavigationOutcomeCount(std::string histogram_suffix,
                                           int all_sites_count,
                                           int feature_enabled_count) {
    histogram_tester_.ExpectTotalCount(
        "BackForwardCache.AllSites.HistoryNavigationOutcome" + histogram_suffix,
        all_sites_count);
    histogram_tester_.ExpectTotalCount(
        "BackForwardCache.HistoryNavigationOutcome" + histogram_suffix,
        feature_enabled_count);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
  base::HistogramTester histogram_tester_;
};

IN_PROC_BROWSER_TEST_F(BackForwardCacheDisabledBrowserTest,
                       MetricsOnHistoryNavigation) {
  GURL url_a(embedded_test_server()->GetURL("a.test", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.test", "/title2.html"));

  // 1) Navigate to A.
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImplWrapper rfh_a(
      shell()->web_contents()->GetPrimaryMainFrame());

  // 2) Navigate to B.
  EXPECT_TRUE(NavigateToURL(shell(), url_b));
  if (!rfh_a.IsRenderFrameDeleted()) {
    EXPECT_FALSE(rfh_a->IsInBackForwardCache());
  }

  // 3) Confirm the metrics recorded when going back to A, which should not be
  // restored from BFCache.
  ASSERT_TRUE(HistoryGoBack(shell()->web_contents()));

  ExpectHistoryNavigationOutcomeCount("", /*all_sites_count=*/1,
                                      /*feature_enabled_count=*/0);
  ExpectHistoryNavigationOutcomeCount(".NotRestoredReason",
                                      /*all_sites_count=*/1,
                                      /*feature_enabled_count=*/0);
  ExpectHistoryNavigationOutcomeCount(".BlocklistedFeature",
                                      /*all_sites_count=*/0,
                                      /*feature_enabled_count=*/0);
  ExpectHistoryNavigationOutcomeCount(".DisabledForRenderFrameHostReason2",
                                      /*all_sites_count=*/0,
                                      /*feature_enabled_count=*/0);
  ExpectHistoryNavigationOutcomeCount(".DisallowActivationReason",
                                      /*all_sites_count=*/0,
                                      /*feature_enabled_count=*/0);
  ExpectHistoryNavigationOutcomeCount(".BrowsingInstanceNotSwappedReason",

                                      /*all_sites_count=*/0,
                                      /*feature_enabled_count=*/0);
}

IN_PROC_BROWSER_TEST_F(BackForwardCacheDisabledBrowserTest,
                       MetricsOnHistoryNavigation_SameSite) {
  GURL url_a(embedded_test_server()->GetURL("a.test", "/title1.html"));
  GURL url_a2(embedded_test_server()->GetURL("a.test", "/title2.html"));

  // 1) Navigate to A.
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImplWrapper rfh_a(
      shell()->web_contents()->GetPrimaryMainFrame());

  // 2) Navigate to A2.
  EXPECT_TRUE(NavigateToURL(shell(), url_a2));
  if (!rfh_a.IsRenderFrameDeleted()) {
    EXPECT_FALSE(rfh_a->IsInBackForwardCache());
  }

  // 3) Confirm the metrics recorded when going back to A, which should not be
  // restored from BFCache.
  ASSERT_TRUE(HistoryGoBack(shell()->web_contents()));

  ExpectHistoryNavigationOutcomeCount("", /*all_sites_count=*/1,
                                      /*feature_enabled_count=*/0);
  ExpectHistoryNavigationOutcomeCount(
      ".NotRestoredReason",
      /*all_sites_count=*/ShouldCreateNewHostForAllFrames() ? 2 : 1,
      /*feature_enabled_count=*/0);
  ExpectHistoryNavigationOutcomeCount(".BlocklistedFeature",
                                      /*all_sites_count=*/0,
                                      /*feature_enabled_count=*/0);
  ExpectHistoryNavigationOutcomeCount(".DisabledForRenderFrameHostReason2",
                                      /*all_sites_count=*/0,
                                      /*feature_enabled_count=*/0);
  ExpectHistoryNavigationOutcomeCount(".BrowsingInstanceNotSwappedReason",
                                      /*all_sites_count=*/1,
                                      /*feature_enabled_count=*/0);
}

IN_PROC_BROWSER_TEST_F(BackForwardCacheDisabledBrowserTest,
                       MetricsOnHistoryNavigation_BlocklistedFeature) {
  GURL url_a(embedded_test_server()->GetURL("a.test", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.test", "/title2.html"));

  // 1) Navigate to A.
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImplWrapper rfh_a(
      shell()->web_contents()->GetPrimaryMainFrame());
  rfh_a->UseDummyStickyBackForwardCacheDisablingFeatureForTesting();

  // 2) Navigate to B.
  EXPECT_TRUE(NavigateToURL(shell(), url_b));
  if (!rfh_a.IsRenderFrameDeleted()) {
    EXPECT_FALSE(rfh_a->IsInBackForwardCache());
  }

  // 3) Confirm the metrics recorded when going back to A, which should not be
  // restored from BFCache.
  ASSERT_TRUE(HistoryGoBack(shell()->web_contents()));

  ExpectHistoryNavigationOutcomeCount("", /*all_sites_count=*/1,
                                      /*feature_enabled_count=*/0);
  // The page is not BFCached because the feature is enabled and the blocklisted
  // feature.
  ExpectHistoryNavigationOutcomeCount(".NotRestoredReason",
                                      /*all_sites_count=*/2,
                                      /*feature_enabled_count=*/0);
  ExpectHistoryNavigationOutcomeCount(".BlocklistedFeature",
                                      /*all_sites_count=*/1,
                                      /*feature_enabled_count=*/0);
  ExpectHistoryNavigationOutcomeCount(".DisabledForRenderFrameHostReason2",
                                      /*all_sites_count=*/0,
                                      /*feature_enabled_count=*/0);
  ExpectHistoryNavigationOutcomeCount(".BrowsingInstanceNotSwappedReason",

                                      /*all_sites_count=*/0,
                                      /*feature_enabled_count=*/0);
}

IN_PROC_BROWSER_TEST_F(BackForwardCacheDisabledBrowserTest,
                       MetricsOnHistoryNavigation_DisabledForRFH) {
  GURL url_a(embedded_test_server()->GetURL("a.test", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.test", "/title2.html"));

  // 1) Navigate to A.
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImplWrapper rfh_a(
      shell()->web_contents()->GetPrimaryMainFrame());
  rfh_a->DisableBackForwardCache(RenderFrameHostDisabledForTestingReason());

  // 2) Navigate to B.
  EXPECT_TRUE(NavigateToURL(shell(), url_b));
  if (!rfh_a.IsRenderFrameDeleted()) {
    EXPECT_FALSE(rfh_a->IsInBackForwardCache());
  }

  // 3) Confirm the metrics recorded when going back to A, which should not be
  // restored from BFCache.
  ASSERT_TRUE(HistoryGoBack(shell()->web_contents()));

  ExpectHistoryNavigationOutcomeCount("", /*all_sites_count=*/1,
                                      /*feature_enabled_count=*/0);
  // The page is not BFCached because the feature is enabled and the RFH
  // disabling call.
  ExpectHistoryNavigationOutcomeCount(".NotRestoredReason",
                                      /*all_sites_count=*/2,
                                      /*feature_enabled_count=*/0);
  ExpectHistoryNavigationOutcomeCount(".BlocklistedFeature",
                                      /*all_sites_count=*/0,
                                      /*feature_enabled_count=*/0);
  // Note: The "all sites" version doesn't exist for the
  // `DisabledForRenderFrameHostReason2` histogram, so we expect it to be 0 too.
  ExpectHistoryNavigationOutcomeCount(".DisabledForRenderFrameHostReason2",
                                      /*all_sites_count=*/0,
                                      /*feature_enabled_count=*/0);
  ExpectHistoryNavigationOutcomeCount(".BrowsingInstanceNotSwappedReason",

                                      /*all_sites_count=*/0,
                                      /*feature_enabled_count=*/0);
}

IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest, NoThrottlesOnCacheRestore) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  // 1) Navigate to A.
  ASSERT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImpl* rfh_a = current_frame_host();
  RenderFrameDeletedObserver delete_observer_rfh_a(rfh_a);

  bool did_register_throttles = false;

  // This will track for each navigation whether we attempted to register
  // NavigationThrottles.
  content::ShellContentBrowserClient::Get()
      ->set_create_throttles_for_navigation_callback(base::BindLambdaForTesting(
          [&did_register_throttles](content::NavigationHandle* handle)
              -> std::vector<std::unique_ptr<content::NavigationThrottle>> {
            did_register_throttles = true;
            return std::vector<std::unique_ptr<content::NavigationThrottle>>();
          }));

  // 2) Navigate to B.
  ASSERT_TRUE(NavigateToURL(shell(), url_b));
  RenderFrameHostImpl* rfh_b = current_frame_host();
  RenderFrameDeletedObserver delete_observer_rfh_b(rfh_b);
  ASSERT_FALSE(delete_observer_rfh_a.deleted());
  ASSERT_TRUE(rfh_a->IsInBackForwardCache());
  EXPECT_TRUE(did_register_throttles);
  did_register_throttles = false;

  // 3) Go back to A which is in the BackForward cache and will be restored via
  // an IsPageActivation navigation. Ensure that we did not register
  // NavigationThrottles for this navigation since we already ran their checks
  // when we navigated to A in step 1.
  ASSERT_TRUE(HistoryGoBack(web_contents()));
  EXPECT_FALSE(did_register_throttles);

  ExpectRestored(FROM_HERE);
}

// Tests that a back navigation from a crashed page has the process state
// tracked correctly by WebContentsImpl.
IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest,
                       BackNavigationFromCrashedPage) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));
  url::Origin origin_a = url::Origin::Create(url_a);
  url::Origin origin_b = url::Origin::Create(url_b);

  // 1) Navigate to A.
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImpl* rfh_a = current_frame_host();
  RenderFrameDeletedObserver delete_observer_rfh_a(rfh_a);
  EXPECT_FALSE(web_contents()->IsCrashed());

  // 2) Navigate to B.
  EXPECT_TRUE(NavigateToURL(shell(), url_b));
  RenderFrameHostImpl* rfh_b = current_frame_host();
  RenderFrameDeletedObserver delete_observer_rfh_b(rfh_b);
  EXPECT_FALSE(delete_observer_rfh_a.deleted());
  EXPECT_TRUE(rfh_a->IsInBackForwardCache());
  EXPECT_EQ(rfh_a->GetVisibilityState(), PageVisibilityState::kHidden);
  EXPECT_EQ(origin_a, rfh_a->GetLastCommittedOrigin());
  EXPECT_EQ(origin_b, rfh_b->GetLastCommittedOrigin());
  EXPECT_FALSE(rfh_b->IsInBackForwardCache());
  EXPECT_EQ(rfh_b->GetVisibilityState(), PageVisibilityState::kVisible);
  EXPECT_FALSE(web_contents()->IsCrashed());

  // 3) Crash B.
  CrashTab(web_contents());
  EXPECT_TRUE(web_contents()->IsCrashed());
  EXPECT_TRUE(delete_observer_rfh_b.deleted());

  // 4) Go back to A.
  ASSERT_TRUE(HistoryGoBack(web_contents()));

  EXPECT_FALSE(delete_observer_rfh_a.deleted());
  EXPECT_EQ(origin_a, rfh_a->GetLastCommittedOrigin());
  EXPECT_EQ(rfh_a, current_frame_host());
  EXPECT_FALSE(rfh_a->IsInBackForwardCache());
  EXPECT_EQ(rfh_a->GetVisibilityState(), PageVisibilityState::kVisible);
  EXPECT_FALSE(web_contents()->IsCrashed());

  ExpectRestored(FROM_HERE);
}

// Test that when two back navigations are created to the same history entry one
// after another without waiting for the first one to commit, the second one
// should be committed as a normal back navigation without restoring the BFCache
// entry.
IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest,
                       TwoBackNavigationsToTheSameEntry) {
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  // 1) Navigate to a cacheable page A.
  ASSERT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImplWrapper rfh_a(current_frame_host());

  // 2) Navigate away.
  ASSERT_TRUE(NavigateToURL(shell(), url_b));
  EXPECT_EQ(web_contents()->GetLastCommittedURL(), url_b);

  // Page A should be in BFCache.
  EXPECT_FALSE(rfh_a.IsDestroyed());
  EXPECT_TRUE(rfh_a->IsInBackForwardCache());

  // 3) Navigate back to A, but before the `CommitDeferringCondition` check
  // happens, start another navigation back to the same entry A.
  TestActivationManager activation_manager(web_contents(), url_a);
  web_contents()->GetController().GoBack();
  ASSERT_TRUE(activation_manager.WaitForAfterChecks());
  ASSERT_TRUE(HistoryGoToIndex(web_contents(), 0));
  EXPECT_EQ(web_contents()->GetLastCommittedURL(), url_a);

  // 4) Page A should not be restored from BFCache because the first navigation
  // is cancelled and the second navigation should be a non-BFCache navigation.
  ExpectNotRestored({NotRestoredReason::kNavigationCancelledWhileRestoring}, {},
                    {}, {}, {}, FROM_HERE);
}

// Injects a blank subframe into the current document just before processing
// DidCommitNavigation for a specified URL.
class InjectCreateChildFrame : public DidCommitNavigationInterceptor {
 public:
  InjectCreateChildFrame(WebContents* web_contents, const GURL& url)
      : DidCommitNavigationInterceptor(web_contents), url_(url) {}

  InjectCreateChildFrame(const InjectCreateChildFrame&) = delete;
  InjectCreateChildFrame& operator=(const InjectCreateChildFrame&) = delete;

  bool was_called() { return was_called_; }

 private:
  // DidCommitNavigationInterceptor implementation.
  bool WillProcessDidCommitNavigation(
      RenderFrameHost* render_frame_host,
      NavigationRequest* navigation_request,
      mojom::DidCommitProvisionalLoadParamsPtr*,
      mojom::DidCommitProvisionalLoadInterfaceParamsPtr* interface_params)
      override {
    if (!was_called_ && navigation_request &&
        navigation_request->GetURL() == url_) {
      EXPECT_TRUE(ExecJs(
          web_contents(),
          "document.body.appendChild(document.createElement('iframe'));"));
    }
    was_called_ = true;
    return true;
  }

  bool was_called_ = false;
  GURL url_;
};

// Verify that when A navigates to B, and A creates a subframe just before B
// commits, the subframe does not inherit a proxy in B's process from its
// parent.  Otherwise, if A gets bfcached and later restored, the subframe's
// proxy would be (1) in a different BrowsingInstance than the rest of its
// page, and (2) preserved after the restore, which would cause crashes when
// later using that proxy (for example, when creating more subframes). See
// https://crbug.com/1243541.
IN_PROC_BROWSER_TEST_F(
    BackForwardCacheBrowserTest,
    InjectSubframeDuringPendingCrossBrowsingInstanceNavigation) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title2.html"));

  // 1) Navigate to A.
  ASSERT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImplWrapper rfh_a(current_frame_host());
  EXPECT_EQ(0U, rfh_a->child_count());

  // 2) Navigate to B, and inject a blank subframe just before it commits.
  {
    InjectCreateChildFrame injector(shell()->web_contents(), url_b);

    TestNavigationObserver navigation_observer(shell()->web_contents(), 1);
    shell()->LoadURL(url_b);
    navigation_observer.Wait();
    // We cannot use NavigateToURL which will automatically wait for particular
    // url in the navigation above because running a nested message loop in the
    // injector confuses TestNavigationObserver by changing the order of
    // notifications.
    EXPECT_EQ(url_b, shell()->web_contents()->GetLastCommittedURL());

    EXPECT_TRUE(injector.was_called());
  }

  // `rfh_a` should be in BackForwardCache, and it should have a subframe.
  EXPECT_TRUE(rfh_a->IsInBackForwardCache());
  ASSERT_EQ(1U, rfh_a->child_count());

  // The new subframe should not have any proxies at this point.  In
  // particular, it shouldn't inherit a proxy in b.com from its parent.
  EXPECT_TRUE(rfh_a->child_at(0)
                  ->render_manager()
                  ->GetAllProxyHostsForTesting()
                  .empty());

  RenderFrameHostImplWrapper rfh_b(current_frame_host());

  // 3) Go back.  This should restore `rfh_a` from the cache, and `rfh_b`
  // should go into the cache.
  ASSERT_TRUE(HistoryGoBack(web_contents()));

  EXPECT_EQ(rfh_a.get(), current_frame_host());
  EXPECT_TRUE(rfh_b->IsInBackForwardCache());

  // 4) Add a grandchild frame to `rfh_a`.  This shouldn't crash.
  RenderFrameHostCreatedObserver frame_observer(shell()->web_contents(), 1);
  EXPECT_TRUE(
      ExecJs(rfh_a->child_at(0),
             "document.body.appendChild(document.createElement('iframe'));"));
  frame_observer.Wait();
  EXPECT_EQ(1U, rfh_a->child_at(0)->child_count());

  // Make sure the grandchild is live.
  EXPECT_TRUE(ExecJs(rfh_a->child_at(0)->child_at(0), "true"));
}

class BackForwardCacheBrowserTestWithFlagForScreenReader
    : public BackForwardCacheBrowserTest,
      public ::testing::WithParamInterface<bool> {
 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    if (IsBackForwardCacheEnabledForScreenReader()) {
      EnableFeatureAndSetParams(
          features::kEnableBackForwardCacheForScreenReader, "", "true");
    } else {
      DisableFeature(features::kEnableBackForwardCacheForScreenReader);
    }
    BackForwardCacheBrowserTest::SetUpCommandLine(command_line);
  }

  bool IsBackForwardCacheEnabledForScreenReader() { return GetParam(); }
};

INSTANTIATE_TEST_SUITE_P(All,
                         BackForwardCacheBrowserTestWithFlagForScreenReader,
                         ::testing::Bool());

IN_PROC_BROWSER_TEST_P(BackForwardCacheBrowserTestWithFlagForScreenReader,
                       ScreenReaderOn) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));
  BackForwardCacheDisabledTester tester;

  // Use Screen Reader.
  ScopedAccessibilityModeOverride scoped_accessibility_mode(
      shell()->web_contents(), ui::kAXModeComplete);

  // Navigate to Page A.
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImplWrapper rfh_a(current_frame_host());
  int process_id = current_frame_host()->GetProcess()->GetID();
  int routing_id = current_frame_host()->GetRoutingID();

  // Navigate away to Page B.
  EXPECT_TRUE(NavigateToURL(shell(), url_b));
  if (IsBackForwardCacheEnabledForScreenReader()) {
    EXPECT_TRUE(rfh_a.get());
    EXPECT_TRUE(rfh_a->IsInBackForwardCache());
    // Navigate back.
    ASSERT_TRUE(HistoryGoBack(web_contents()));
    ExpectRestored(FROM_HERE);
  } else {
    EXPECT_TRUE(rfh_a.WaitUntilRenderFrameDeleted());
    // Navigate back.
    ASSERT_TRUE(HistoryGoBack(web_contents()));
    auto reason = BackForwardCacheDisable::DisabledReason(
        BackForwardCacheDisable::DisabledReasonId::kScreenReader);
    ExpectNotRestored({NotRestoredReason::kDisableForRenderFrameHostCalled}, {},
                      {}, {reason}, {}, FROM_HERE);
    EXPECT_TRUE(
        tester.IsDisabledForFrameWithReason(process_id, routing_id, reason));
  }
}

class BackForwardCacheBrowserTestWithFlagForAXEvents
    : public BackForwardCacheBrowserTest,
      public ::testing::WithParamInterface<bool> {
 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    EnableFeatureAndSetParams(features::kEnableBackForwardCacheForScreenReader,
                              "", "true");
    if (ShouldEvictOnAXEvents()) {
      EnableFeatureAndSetParams(features::kEvictOnAXEvents, "", "true");
    } else {
      DisableFeature(features::kEvictOnAXEvents);
    }
    BackForwardCacheBrowserTest::SetUpCommandLine(command_line);
  }

  bool ShouldEvictOnAXEvents() { return GetParam(); }
};

INSTANTIATE_TEST_SUITE_P(All,
                         BackForwardCacheBrowserTestWithFlagForAXEvents,
                         ::testing::Bool());

// Verify that the page will be evicted upon accessibility events if the
// flag to evict on ax events is off, and evicted otherwise.
IN_PROC_BROWSER_TEST_P(BackForwardCacheBrowserTestWithFlagForAXEvents,
                       EvictOnAccessibilityEventsOrNot) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  // 1) Navigate to A.
  EXPECT_TRUE(NavigateToURL(shell()->web_contents(), url_a));
  RenderFrameHostImplWrapper rfh_a(current_frame_host());
  // Use Screen Reader.
  ScopedAccessibilityModeOverride scoped_accessibility_mode(
      shell()->web_contents(), ui::kAXModeComplete);

  // Wait until we receive the kLoadComplete AX event. This means that the
  // kLoadStart event has definitely already passed and any kLoadStart we see
  // from this frame in the future is newly generated.
  AccessibilityNotificationWaiter waiter_complete(
      shell()->web_contents(), ui::kAXModeComplete,
      ax::mojom::Event::kLoadComplete);
  ASSERT_TRUE(waiter_complete.WaitForNotification());

  // 2) Navigate to B.
  EXPECT_TRUE(NavigateToURL(shell()->web_contents(), url_b));
  RenderFrameHostImplWrapper rfh_b(current_frame_host());
  EXPECT_TRUE(rfh_a.get());
  EXPECT_TRUE(rfh_a->IsInBackForwardCache());

  // 3) Set the callback for generated events, and expect that this is never
  // fired.
  ui::BrowserAccessibilityManager* manager =
      rfh_a->GetOrCreateBrowserAccessibilityManager();
  manager->SetGeneratedEventCallbackForTesting(
      base::BindRepeating([](ui::BrowserAccessibilityManager* manager,
                             ui::AXEventGenerator::Event event,
                             ui::AXNodeID event_target_id) { FAIL(); }));
  // Generate an event.
  ui::AXUpdatesAndEvents updates_and_events;
  ui::AXTreeUpdate update;
  update.root_id = 1;
  updates_and_events.updates.emplace_back(update);
  updates_and_events.events.emplace_back(
      /*id=*/0, ax::mojom::Event::kChildrenChanged);
  // If any events are generated and fired, they will be fired synchronously
  // in the same task of |HandleAXEventsForTests()| and and result in a test
  // fail.
  rfh_a->HandleAXEventsForTests(rfh_a->GetAXTreeID(),
                                std::move(updates_and_events),
                                ui::AXLocationAndScrollUpdates());

  // Reset the callback before restoring the page so that we will not fail when
  // events are generated.
  manager->SetGeneratedEventCallbackForTesting(
      ui::GeneratedEventCallbackForTesting());

  // 4) Navigate back.
  ASSERT_TRUE(HistoryGoBack(web_contents()));
  if (ShouldEvictOnAXEvents()) {
    const uint64_t reason = DisallowActivationReasonId::kAXEvent;
    ExpectNotRestored({NotRestoredReason::kIgnoreEventAndEvict}, {}, {}, {},
                      {reason}, FROM_HERE);
  } else {
    AccessibilityNotificationWaiter waiter_start(shell()->web_contents(),
                                                 ui::kAXModeComplete,
                                                 ax::mojom::Event::kLoadStart);
    // Ensure that |rfh_a| is successfully restored from bfcache and that we see
    // LOAD_START event.
    EXPECT_EQ(current_frame_host(), rfh_a.get());
    ExpectRestored(FROM_HERE);

    ASSERT_TRUE(waiter_start.WaitForNotification());
    auto* waiter_start_rfhi = static_cast<RenderFrameHostImpl*>(
        waiter_start.event_browser_accessibility_manager()->delegate());
    EXPECT_EQ(waiter_start_rfhi, rfh_a.get());
  }
}

class BackForwardCacheBrowserTestWithFlagForAXLocationChange
    : public BackForwardCacheBrowserTest,
      public ::testing::WithParamInterface<bool> {
 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    EnableFeatureAndSetParams(features::kEnableBackForwardCacheForScreenReader,
                              "", "true");
    if (ShouldEvictOnAXLocationChange()) {
      DisableFeature(features::kDoNotEvictOnAXLocationChange);
    } else {
      EnableFeatureAndSetParams(features::kDoNotEvictOnAXLocationChange, "",
                                "");
    }
    BackForwardCacheBrowserTest::SetUpCommandLine(command_line);
  }

  bool ShouldEvictOnAXLocationChange() { return GetParam(); }
};

INSTANTIATE_TEST_SUITE_P(All,
                         BackForwardCacheBrowserTestWithFlagForAXLocationChange,
                         ::testing::Bool());

IN_PROC_BROWSER_TEST_P(BackForwardCacheBrowserTestWithFlagForAXLocationChange,
                       EvictOnAXLocationChangeOrNot) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  // 1) Navigate to A.
  EXPECT_TRUE(NavigateToURL(shell()->web_contents(), url_a));
  RenderFrameHostImplWrapper rfh_a(current_frame_host());
  // Use Screen Reader.
  ScopedAccessibilityModeOverride scoped_accessibility_mode(
      shell()->web_contents(), ui::kAXModeComplete);

  // Wait until we receive the kLoadComplete AX event. This means that the
  // kLoadStart event has definitely already passed and any kLoadStart we see
  // from this frame in the future is newly generated.
  AccessibilityNotificationWaiter waiter_complete(
      shell()->web_contents(), ui::kAXModeComplete,
      ax::mojom::Event::kLoadComplete);
  ASSERT_TRUE(waiter_complete.WaitForNotification());

  // 2) Navigate to B.
  EXPECT_TRUE(NavigateToURL(shell()->web_contents(), url_b));
  RenderFrameHostImplWrapper rfh_b(current_frame_host());
  ASSERT_TRUE(rfh_a.get());
  EXPECT_TRUE(rfh_a->IsInBackForwardCache());

  // 3) Set the callback for location change.
  ui::BrowserAccessibilityManager* manager =
      rfh_a->GetOrCreateBrowserAccessibilityManager();
  // This callback will count the number of times location change happens.
  // Note that this callback runs even when the page is in back/forward cache.
  int location_change_counter_for_testing = 0;
  manager->SetLocationChangeCallbackForTesting(base::BindRepeating(
      [](int* location_change_counter_for_testing) {
        // Increment the location change count.
        *location_change_counter_for_testing += 1;
      },
      &location_change_counter_for_testing));

  // Generate a location change event.
  ui::AXLocationAndScrollUpdates changes_1;
  ui::AXRelativeBounds relative_bounds_1;
  relative_bounds_1.bounds =
      gfx::RectF(/*x=*/1, /*y=*/2, /*width=*/3, /*height=*/4);
  changes_1.location_changes.emplace_back(0, relative_bounds_1);
  rfh_a->HandleAXLocationChanges(rfh_a->GetAXTreeID(), std::move(changes_1),
                                 /*reset_token=*/1, {});

  // Generate another location change event.
  ui::AXLocationAndScrollUpdates changes_2;
  ui::AXRelativeBounds relative_bounds_2;
  relative_bounds_2.bounds =
      gfx::RectF(/*x=*/2, /*y=*/3, /*width=*/4, /*height=*/5);
  changes_2.location_changes.emplace_back(0, relative_bounds_2);
  rfh_a->HandleAXLocationChanges(rfh_a->GetAXTreeID(), std::move(changes_2),
                                 /*reset_token=*/1, {});

  // 4) Navigate back.
  ASSERT_TRUE(HistoryGoBack(web_contents()));

  if (ShouldEvictOnAXLocationChange()) {
    const uint64_t reason = DisallowActivationReasonId::kAXLocationChange;
    ExpectNotRestored({NotRestoredReason::kIgnoreEventAndEvict}, {}, {}, {},
                      {reason}, FROM_HERE);
    EXPECT_EQ(0, location_change_counter_for_testing);
  } else {
    AccessibilityNotificationWaiter waiter_start(shell()->web_contents(),
                                                 ui::kAXModeComplete,
                                                 ax::mojom::Event::kLoadStart);
    // Ensure that |rfh_a| is successfully restored from bfcache and that we see
    // LOAD_START event.
    EXPECT_EQ(current_frame_host(), rfh_a.get());
    ExpectRestored(FROM_HERE);

    // Location change should have happened twice.
    EXPECT_EQ(2, location_change_counter_for_testing);

    ASSERT_TRUE(waiter_start.WaitForNotification());
    auto* waiter_start_rfhi = static_cast<RenderFrameHostImpl*>(
        waiter_start.event_browser_accessibility_manager()->delegate());
    EXPECT_EQ(waiter_start_rfhi, rfh_a.get());
  }
}

class BackgroundForegroundProcessLimitBackForwardCacheBrowserTest
    : public BackForwardCacheBrowserTest {
 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    EnableFeatureAndSetParams(features::kBackForwardCache, "cache_size",
                              base::NumberToString(kBackForwardCacheSize));
    EnableFeatureAndSetParams(
        features::kBackForwardCache, "foreground_cache_size",
        base::NumberToString(kForegroundBackForwardCacheSize));
    BackForwardCacheBrowserTest::SetUpCommandLine(command_line);
  }

  void ExpectCached(const RenderFrameHostImplWrapper& rfh,
                    bool cached,
                    bool backgrounded) {
    EXPECT_FALSE(rfh.IsDestroyed());
    EXPECT_EQ(cached, rfh->IsInBackForwardCache());
    EXPECT_EQ(backgrounded, rfh->GetProcess()->GetPriority() ==
                                base::Process::Priority::kBestEffort);
  }
  // The number of pages the BackForwardCache can hold per tab.
  const size_t kBackForwardCacheSize = 4;
  const size_t kForegroundBackForwardCacheSize = 2;
};

// Test that a series of same-site navigations (which use the same process)
// uses the foreground limit.
IN_PROC_BROWSER_TEST_F(
    BackgroundForegroundProcessLimitBackForwardCacheBrowserTest,
    CacheEvictionSameSite) {
  ASSERT_TRUE(embedded_test_server()->Start());

  std::vector<RenderFrameHostImplWrapper> rfhs;

  for (size_t i = 0; i <= kBackForwardCacheSize * 2; ++i) {
    SCOPED_TRACE(i);
    GURL url(embedded_test_server()->GetURL(
        "a.com", base::StringPrintf("/title1.html?i=%zu", i)));
    ASSERT_TRUE(NavigateToURL(shell(), url));
    rfhs.emplace_back(current_frame_host());
    EXPECT_NE(rfhs.back()->GetProcess()->GetPriority(),
              base::Process::Priority::kBestEffort);

    for (size_t j = 0; j <= i; ++j) {
      SCOPED_TRACE(j);
      // The last page is active, the previous |kForegroundBackForwardCacheSize|
      // should be in the cache, any before that should be deleted.
      if (i - j <= kForegroundBackForwardCacheSize) {
        // All of the processes should be in the foreground.
        ExpectCached(rfhs[j], /*cached=*/i != j,
                     /*backgrounded=*/false);
      } else {
        ASSERT_TRUE(rfhs[j].WaitUntilRenderFrameDeleted());
      }
    }
  }

  // Navigate back but not to the initial about:blank.
  for (size_t i = 0; i <= kBackForwardCacheSize * 2 - 1; ++i) {
    SCOPED_TRACE(i);
    ASSERT_TRUE(HistoryGoBack(web_contents()));
    // The first |kBackForwardCacheSize| navigations should be restored from the
    // cache. The rest should not.
    if (i < kForegroundBackForwardCacheSize) {
      ExpectRestored(FROM_HERE);
    } else {
      ExpectNotRestored({NotRestoredReason::kForegroundCacheLimit}, {}, {}, {},
                        {}, FROM_HERE);
    }
  }
}

// Test that a series of cross-site navigations (which use different processes)
// use the background limit.
//
// TODO(crbug.com/40179515): This test is flaky. It has been reenabled with
// improved failure output (https://crrev.com/c/2862346). It's OK to disable it
// again when it fails.
IN_PROC_BROWSER_TEST_F(
    BackgroundForegroundProcessLimitBackForwardCacheBrowserTest,
    CacheEvictionCrossSite) {
  ASSERT_TRUE(embedded_test_server()->Start());

  std::vector<RenderFrameHostImplWrapper> rfhs;

  for (size_t i = 0; i <= kBackForwardCacheSize * 2; ++i) {
    SCOPED_TRACE(i);
    // Note: do NOT use .com domains here because a4.com is on the HSTS preload
    // list, which will cause our test requests to timeout.
    GURL url(embedded_test_server()->GetURL(base::StringPrintf("a%zu.test", i),
                                            "/title1.html"));
    ASSERT_TRUE(NavigateToURL(shell(), url));
    rfhs.emplace_back(current_frame_host());
    EXPECT_NE(rfhs.back()->GetProcess()->GetPriority(),
              base::Process::Priority::kBestEffort);

    for (size_t j = 0; j <= i; ++j) {
      SCOPED_TRACE(j);
      // The last page is active, the previous |kBackgroundBackForwardCacheSize|
      // should be in the cache, any before that should be deleted.
      if (i - j <= kBackForwardCacheSize) {
        EXPECT_FALSE(rfhs[j].IsDestroyed());
        // Pages except the active one should be cached and in the background.
        ExpectCached(rfhs[j], /*cached=*/i != j,
                     /*backgrounded=*/i != j);
      } else {
        ASSERT_TRUE(rfhs[j].WaitUntilRenderFrameDeleted());
      }
    }
  }

  // Navigate back but not to the initial about:blank.
  for (size_t i = 0; i <= kBackForwardCacheSize * 2 - 1; ++i) {
    SCOPED_TRACE(i);
    ASSERT_TRUE(HistoryGoBack(web_contents()));
    // The first |kBackForwardCacheSize| navigations should be restored from the
    // cache. The rest should not.
    if (i < kBackForwardCacheSize) {
      ExpectRestored(FROM_HERE);
    } else {
      ExpectNotRestored({NotRestoredReason::kCacheLimit}, {}, {}, {}, {},
                        FROM_HERE);
    }
  }
}

// Test that the cache responds to processes switching from background to
// foreground. We set things up so that we have
// Cached sites:
//   a0.test
//   a1.test
//   a2.test
//   a3.test
// and the active page is a4.test. Then set the process for a[1-3] to
// foregrounded so that there are 3 entries whose processes are foregrounded.
// BFCache should evict the eldest (a1) leaving a0 because despite being older,
// it is backgrounded. Setting the priority directly is not ideal but there is
// no reliable way to cause the processes to go into the foreground just by
// navigating because proactive browsing instance swap makes it impossible to
// reliably create a new a1.test renderer in the same process as the old
// a1.test.
//
// Note that we do NOT use .com domains because a4.com is on the HSTS preload
// list.  Since our test server doesn't use HTTPS, using a4.com results in the
// test timing out.
IN_PROC_BROWSER_TEST_F(
    BackgroundForegroundProcessLimitBackForwardCacheBrowserTest,
    ChangeToForeground) {
  ASSERT_TRUE(embedded_test_server()->Start());

  std::vector<RenderFrameHostImplWrapper> rfhs;

  // Navigate through a[0-3].com.
  for (size_t i = 0; i < kBackForwardCacheSize; ++i) {
    SCOPED_TRACE(i);
    GURL url(embedded_test_server()->GetURL(base::StringPrintf("a%zu.test", i),
                                            "/title1.html"));
    ASSERT_TRUE(NavigateToURL(shell(), url));
    rfhs.emplace_back(current_frame_host());
    EXPECT_NE(rfhs.back()->GetProcess()->GetPriority(),
              base::Process::Priority::kBestEffort);
  }
  // Check that a0-2 are cached and backgrounded.
  for (size_t i = 0; i < kBackForwardCacheSize - 1; ++i) {
    SCOPED_TRACE(i);
    ExpectCached(rfhs[i], /*cached=*/true, /*backgrounded=*/true);
  }

  // Navigate to a page which causes the processes for a[1-3] to be
  // foregrounded.
  GURL url(embedded_test_server()->GetURL("a4.test", "/title1.html"));
  ASSERT_TRUE(NavigateToURL(shell(), url));

  // Assert that we really have set up the situation we want where the processes
  // are shared and in the foreground.
  RenderFrameHostImpl* rfh = current_frame_host();
  ASSERT_NE(rfh->GetProcess()->GetPriority(),
            base::Process::Priority::kBestEffort);

  rfhs[1]->GetProcess()->OnMediaStreamAdded();
  rfhs[2]->GetProcess()->OnMediaStreamAdded();
  rfhs[3]->GetProcess()->OnMediaStreamAdded();

  // The page should be evicted.
  ASSERT_TRUE(rfhs[1].WaitUntilRenderFrameDeleted());

  // Check that a0 is cached and backgrounded.
  ExpectCached(rfhs[0], /*cached=*/true, /*backgrounded=*/true);
  // Check that a2-3 are cached and foregrounded.
  ExpectCached(rfhs[2], /*cached=*/true, /*backgrounded=*/false);
  ExpectCached(rfhs[3], /*cached=*/true, /*backgrounded=*/false);
}

// Test that the BackForwardCacheTimeToLiveControl feature works and takes
// precedence over the default value
// `kDefaultTimeToLiveInBackForwardCacheInSeconds`.
IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest, TestTimeToLiveParameter) {
  // Inject mock time task runner to be used in the eviction timer, so we can,
  // check for the functionality we are interested before and after the time to
  // live. We don't replace SingleThreadTaskRunner::GetCurrentDefault to ensure
  // that it doesn't affect other unrelated callsites.
  scoped_refptr<base::TestMockTimeTaskRunner> task_runner =
      base::MakeRefCounted<base::TestMockTimeTaskRunner>();

  web_contents()->GetController().GetBackForwardCache().SetTaskRunnerForTesting(
      task_runner);

  base::TimeDelta time_to_live_in_back_forward_cache =
      BackForwardCacheImpl::GetTimeToLiveInBackForwardCache(
          BackForwardCacheImpl::kNotInCCNSContext);
  // This should match the value set via EnableFeatureAndSetParams by
  // parent test class `BackForwardCacheBrowserTest`.
  EXPECT_EQ(time_to_live_in_back_forward_cache, base::Seconds(3600));

  base::TimeDelta delta = base::Milliseconds(1);

  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  // 1) Navigate to A.
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImplWrapper rfh_a(current_frame_host());

  // 2) Navigate to B.
  EXPECT_TRUE(NavigateToURL(shell(), url_b));
  RenderFrameHostImplWrapper rfh_b(current_frame_host());

  // 3) Fast forward to just before eviction is due.
  task_runner->FastForwardBy(time_to_live_in_back_forward_cache - delta);

  // 4) Confirm A is still in BackForwardCache.
  ASSERT_FALSE(rfh_a.IsDestroyed());
  EXPECT_TRUE(rfh_a->IsInBackForwardCache());

  // 5) Fast forward to when eviction is due.
  task_runner->FastForwardBy(delta);

  // 6) Confirm A is evicted.
  ASSERT_TRUE(rfh_a.WaitUntilRenderFrameDeleted());
  EXPECT_EQ(current_frame_host(), rfh_b.get());

  // 7) Go back to A.
  ASSERT_TRUE(HistoryGoBack(web_contents()));
  ExpectNotRestored({BackForwardCacheMetrics::NotRestoredReason::kTimeout}, {},
                    {}, {}, {}, FROM_HERE);
}

// Test that when we navigate away from an error page and back with no error
// that we don't serve the error page from BFCache.
IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest,
                       ErrorDocumentNotCachedWithSecondError) {
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  // Navigate to a.com.
  ASSERT_TRUE(NavigateToURL(web_contents(), url_a));

  // Navigate to b.com and block due to an error.
  NavigateAndBlock(url_b, /*history_offset=*/0);
  RenderFrameHostImplWrapper rfh_b(current_frame_host());

  // Navigate back to a.com.
  ASSERT_TRUE(HistoryGoBack(web_contents()));
  ExpectRestored(FROM_HERE);
  ASSERT_TRUE(rfh_b.WaitUntilRenderFrameDeleted());

  // Navigate forward to b.com again and block with an error again.
  NavigateAndBlock(url_b, /*history_offset=*/1);
  ExpectNotRestored(
      {NotRestoredReason::kHTTPStatusNotOK, NotRestoredReason::kErrorDocument},
      {}, {}, {}, {}, FROM_HERE);
}

// Test that when we navigate away from an error page and back with no error
// that we don't serve the error page from BFCache.
IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest,
                       ErrorDocumentNotCachedWithoutSecondError) {
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  // Navigate to a.com.
  ASSERT_TRUE(NavigateToURL(web_contents(), url_a));

  // Navigate to b.com and block due to an error.
  NavigateAndBlock(url_b, /*history_offset=*/0);
  RenderFrameHostImplWrapper rfh_b(current_frame_host());

  int history_entry_id =
      web_contents()->GetController().GetLastCommittedEntry()->GetUniqueID();

  // Navigate back to a.com.
  ASSERT_TRUE(HistoryGoBack(web_contents()));
  ASSERT_TRUE(rfh_b.WaitUntilRenderFrameDeleted());
  ExpectRestored(FROM_HERE);

  // Navigate forward to b.com again with no error.
  ASSERT_TRUE(HistoryGoForward(web_contents()));

  // Check that we indeed got a new history entry.
  ASSERT_NE(
      history_entry_id,
      web_contents()->GetController().GetLastCommittedEntry()->GetUniqueID());
  // The reasons from the old entry should be copied to the new entry.
  ExpectNotRestored(
      {NotRestoredReason::kHTTPStatusNotOK, NotRestoredReason::kErrorDocument},
      {}, {}, {}, {}, FROM_HERE);
}

class BackForwardCacheBrowserTestWithFencedFrames
    : public BackForwardCacheBrowserTest {
 public:
  BackForwardCacheBrowserTestWithFencedFrames() = default;
  ~BackForwardCacheBrowserTestWithFencedFrames() override = default;

  test::FencedFrameTestHelper& fenced_frame_test_helper() {
    return *fenced_frame_test_helper_;
  }

 private:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    EnableFeatureAndSetParams(blink::features::kFencedFrames, "", "");
    EnableFeatureAndSetParams(features::kPrivacySandboxAdsAPIsOverride, "", "");
    BackForwardCacheBrowserTest::SetUpCommandLine(command_line);

    fenced_frame_test_helper_ = std::make_unique<test::FencedFrameTestHelper>();
  }

  std::unique_ptr<test::FencedFrameTestHelper> fenced_frame_test_helper_;
};

IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTestWithFencedFrames,
                       CachesFencedFramesSimple) {
  CreateHttpsServer();
  ASSERT_TRUE(https_server()->Start());
  GURL url_a(https_server()->GetURL("a.test", "/fenced_frames/title1.html"));
  GURL url_b(https_server()->GetURL("b.test", "/title1.html"));

  // 1) Navigate to A.
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImplWrapper rfh_a(current_frame_host());

  // 2) Create fenced frame and wait for it to load.
  const GURL fenced_frame_url =
      https_server()->GetURL("c.test", "/fenced_frames/title1.html");
  RenderFrameHostImplWrapper fenced_frame(
      fenced_frame_test_helper().CreateFencedFrame(
          web_contents()->GetPrimaryMainFrame(), fenced_frame_url));
  EXPECT_TRUE(WaitForDOMContentLoaded(fenced_frame.get()));

  // 3) Navigate to B.
  EXPECT_TRUE(NavigateToURL(shell(), url_b));
  EXPECT_TRUE(rfh_a->IsInBackForwardCache());
  EXPECT_TRUE(fenced_frame->IsInBackForwardCache());

  // 4) Go back to A.
  ASSERT_TRUE(HistoryGoBack(web_contents()));
  ExpectRestored(FROM_HERE);
  EXPECT_FALSE(fenced_frame->IsInBackForwardCache());
}

// Test that the back/forward cache can store documents containing a fenced
// frame in their contents.
IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTestWithFencedFrames,
                       InnerFrameStorageSupport) {
  CreateHttpsServer();
  ASSERT_TRUE(https_server()->Start());
  GURL url_a(https_server()->GetURL(
      "a.test", "/fenced_frames/basic_fenced_frame_src.html"));
  GURL url_b(https_server()->GetURL("b.test", "/title1.html"));

  // 1. Navigate to A.
  ASSERT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImplWrapper rfh_a(current_frame_host());
  EXPECT_FALSE(rfh_a->IsInBackForwardCache());

  // 2. Retrieve the rfh for the fenced frame
  EXPECT_EQ(1u, rfh_a->frame_tree_node()->child_count());
  RenderFrameHostImplWrapper first_delegate_frame(
      rfh_a->frame_tree_node()->child_at(0)->current_frame_host());
  RenderFrameHostImplWrapper first_fenced_frame(
      FrameTreeNode::GloballyFindByID(
          first_delegate_frame->inner_tree_main_frame_tree_node_id())
          ->current_frame_host());
  EXPECT_FALSE(first_delegate_frame->IsInBackForwardCache());
  ASSERT_TRUE(first_fenced_frame);
  EXPECT_FALSE(first_fenced_frame->IsInBackForwardCache());

  // 3. Add a second fenced frame.
  GURL title_fenced_frame(
      https_server()->GetURL("a.test", "/fenced_frames/title1.html"));
  RenderFrameHostImplWrapper second_fenced_frame(
      fenced_frame_test_helper().CreateFencedFrame(rfh_a.get(),
                                                   title_fenced_frame));
  ASSERT_TRUE(second_fenced_frame);
  EXPECT_EQ(2u, rfh_a->frame_tree_node()->child_count());
  RenderFrameHostImplWrapper second_delegate_frame(
      rfh_a->frame_tree_node()->child_at(1)->current_frame_host());
  EXPECT_TRUE(WaitForDOMContentLoaded(second_fenced_frame.get()));

  // 4. Add a nested fenced frame.
  RenderFrameHostImplWrapper nested_fenced_frame(
      fenced_frame_test_helper().CreateFencedFrame(second_fenced_frame.get(),
                                                   title_fenced_frame));
  ASSERT_TRUE(nested_fenced_frame);
  EXPECT_EQ(1u, second_fenced_frame->frame_tree_node()->child_count());
  RenderFrameHostImplWrapper nested_delegate_frame(
      second_fenced_frame->frame_tree_node()
          ->child_at(0)
          ->current_frame_host());
  EXPECT_TRUE(WaitForDOMContentLoaded(nested_fenced_frame.get()));

  StartRecordingEvents(first_fenced_frame.get());
  StartRecordingEvents(second_fenced_frame.get());
  StartRecordingEvents(nested_fenced_frame.get());

  // 5. Navigate to B.
  ASSERT_TRUE(NavigateToURL(shell(), url_b));
  RenderFrameHostImplWrapper rfh_b(current_frame_host());
  EXPECT_FALSE(rfh_b->IsInBackForwardCache());

  // 6. Confirm A and its inner frames are in BackForwardCache.
  ASSERT_TRUE(rfh_a->IsInBackForwardCache());
  EXPECT_TRUE(first_delegate_frame->IsInBackForwardCache());
  EXPECT_TRUE(first_fenced_frame->IsInBackForwardCache());
  EXPECT_TRUE(second_delegate_frame->IsInBackForwardCache());
  EXPECT_TRUE(second_fenced_frame->IsInBackForwardCache());
  EXPECT_TRUE(nested_delegate_frame->IsInBackForwardCache());
  EXPECT_TRUE(nested_fenced_frame->IsInBackForwardCache());

  // 7. Navigate back restoring A.
  ASSERT_TRUE(HistoryGoBack(web_contents()));
  EXPECT_EQ(current_frame_host(), rfh_a.get());
  EXPECT_FALSE(rfh_a->IsInBackForwardCache());
  EXPECT_FALSE(first_delegate_frame->IsInBackForwardCache());
  EXPECT_FALSE(first_fenced_frame->IsInBackForwardCache());
  EXPECT_FALSE(second_delegate_frame->IsInBackForwardCache());
  EXPECT_FALSE(second_fenced_frame->IsInBackForwardCache());
  EXPECT_FALSE(nested_delegate_frame->IsInBackForwardCache());
  EXPECT_FALSE(nested_fenced_frame->IsInBackForwardCache());

  // visibilitychange events are added twice per each because it is fired for
  // both window and document.
  base::Value matching_events =
      ListValueOf("window.pagehide.persisted", "document.visibilitychange",
                  "window.visibilitychange", "document.freeze",
                  "document.resume", "document.visibilitychange",
                  "window.visibilitychange", "window.pageshow.persisted");

  MatchEventList(first_fenced_frame.get(), matching_events.Clone());
  MatchEventList(second_fenced_frame.get(), matching_events.Clone());
  MatchEventList(nested_fenced_frame.get(), matching_events.Clone());

  // 8. Navigate forward to B, storing A again in BackForwardCache.
  ASSERT_TRUE(HistoryGoForward(web_contents()));
  EXPECT_TRUE(rfh_a->IsInBackForwardCache());
  EXPECT_TRUE(first_delegate_frame->IsInBackForwardCache());
  EXPECT_TRUE(first_fenced_frame->IsInBackForwardCache());
  EXPECT_TRUE(second_delegate_frame->IsInBackForwardCache());
  EXPECT_TRUE(second_fenced_frame->IsInBackForwardCache());
  EXPECT_TRUE(nested_delegate_frame->IsInBackForwardCache());
  EXPECT_TRUE(nested_fenced_frame->IsInBackForwardCache());

  // 9. Navigate back restoring A one more time.
  ASSERT_TRUE(HistoryGoBack(web_contents()));
  EXPECT_EQ(current_frame_host(), rfh_a.get());
  EXPECT_FALSE(rfh_a->IsInBackForwardCache());
  EXPECT_FALSE(first_delegate_frame->IsInBackForwardCache());
  EXPECT_FALSE(first_fenced_frame->IsInBackForwardCache());
  EXPECT_FALSE(second_delegate_frame->IsInBackForwardCache());
  EXPECT_FALSE(second_fenced_frame->IsInBackForwardCache());
  EXPECT_FALSE(nested_delegate_frame->IsInBackForwardCache());
  EXPECT_FALSE(nested_fenced_frame->IsInBackForwardCache());
}

// Test that documents are evicted correctly through the outermost main frame.
IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTestWithFencedFrames,
                       OuterDocumentTimeEviction) {
  CreateHttpsServer();
  ASSERT_TRUE(https_server()->Start());
  // Inject mock time task runner to be used in the eviction timer, so we can,
  // check for the functionality we are interested before and after the time to
  // live. We don't replace SingleThreadTaskRunner::GetCurrentDefault to ensure
  // that it doesn't affect other unrelated callsites.
  scoped_refptr<base::TestMockTimeTaskRunner> task_runner =
      base::MakeRefCounted<base::TestMockTimeTaskRunner>();

  web_contents()->GetController().GetBackForwardCache().SetTaskRunnerForTesting(
      task_runner);

  base::TimeDelta time_to_live_in_back_forward_cache =
      BackForwardCacheImpl::GetTimeToLiveInBackForwardCache(
          BackForwardCacheImpl::kNotInCCNSContext);
  // This should match the value we set in EnableFeatureAndSetParams.
  EXPECT_EQ(time_to_live_in_back_forward_cache, base::Seconds(3600));

  base::TimeDelta delta = base::Milliseconds(1);

  GURL url_a(https_server()->GetURL("a.test", "/title1.html"));
  GURL url_b(https_server()->GetURL("b.test", "/title1.html"));

  // 1. Navigate to A.
  ASSERT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImplWrapper rfh_a(current_frame_host());

  // 2. Add a fenced frame to initial page A.
  GURL fenced_frame_url(
      https_server()->GetURL("a.test", "/fenced_frames/empty.html"));
  RenderFrameHostImplWrapper fenced_frame_rfh(
      fenced_frame_test_helper().CreateFencedFrame(rfh_a.get(),
                                                   fenced_frame_url));
  ASSERT_TRUE(fenced_frame_rfh);
  EXPECT_TRUE(WaitForDOMContentLoaded(fenced_frame_rfh.get()));

  // 3. Navigate to B.
  ASSERT_TRUE(NavigateToURL(shell(), url_b));
  RenderFrameHostImplWrapper rfh_b(current_frame_host());
  EXPECT_FALSE(rfh_b->IsBackForwardCacheEvictionTimeRunningForTesting());

  // 4. Fast forward to just before eviction is due.
  task_runner->FastForwardBy(time_to_live_in_back_forward_cache - delta);

  // 5. Confirm A is still in BackForwardCache.
  ASSERT_TRUE(rfh_a);
  EXPECT_TRUE(rfh_a->IsBackForwardCacheEvictionTimeRunningForTesting());
  EXPECT_TRUE(rfh_a->IsInBackForwardCache());
  EXPECT_FALSE(
      fenced_frame_rfh->IsBackForwardCacheEvictionTimeRunningForTesting());
  EXPECT_TRUE(fenced_frame_rfh->IsInBackForwardCache());

  // 6. Fast forward to when eviction is due.
  task_runner->FastForwardBy(delta);

  // 7. Confirm A is evicted.
  ASSERT_TRUE(fenced_frame_rfh.WaitUntilRenderFrameDeleted());
  ASSERT_TRUE(rfh_a.WaitUntilRenderFrameDeleted());
  EXPECT_EQ(current_frame_host(), rfh_b.get());

  // 8. Go back to A.
  ASSERT_TRUE(HistoryGoBack(web_contents()));
  ExpectNotRestored({BackForwardCacheMetrics::NotRestoredReason::kTimeout}, {},
                    {}, {}, {}, FROM_HERE);
}

// This test checks that the TreeResults generated are correct.
IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTestWithFencedFrames,
                       TreeResults) {
  CreateHttpsServer();
  ASSERT_TRUE(https_server()->Start());
  GURL url_a(https_server()->GetURL("a.test", "/title1.html"));

  // 1. Navigate to A.
  ASSERT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImplWrapper rfh_a(current_frame_host());

  // 2. Add fenced frames.
  GURL fenced_frame_url_a(
      https_server()->GetURL("a.test", "/fenced_frames/title1.html?value=a"));
  GURL fenced_frame_url_b(
      https_server()->GetURL("b.test", "/fenced_frames/title1.html?value=b"));
  GURL fenced_frame_url_c(
      https_server()->GetURL("c.test", "/fenced_frames/title1.html?value=c"));
  RenderFrameHostImplWrapper fenced_frame_a(
      fenced_frame_test_helper().CreateFencedFrame(rfh_a.get(),
                                                   fenced_frame_url_a));
  RenderFrameHostImplWrapper fenced_frame_b(
      fenced_frame_test_helper().CreateFencedFrame(rfh_a.get(),
                                                   fenced_frame_url_b));
  RenderFrameHostImplWrapper fenced_frame_c(
      fenced_frame_test_helper().CreateFencedFrame(fenced_frame_b.get(),
                                                   fenced_frame_url_c));
  EXPECT_TRUE(fenced_frame_a);
  EXPECT_TRUE(fenced_frame_b);
  EXPECT_TRUE(fenced_frame_c);
  EXPECT_TRUE(WaitForDOMContentLoaded(fenced_frame_a.get()));
  EXPECT_TRUE(WaitForDOMContentLoaded(fenced_frame_b.get()));
  EXPECT_TRUE(WaitForDOMContentLoaded(fenced_frame_c.get()));
  fenced_frame_c->UseDummyStickyBackForwardCacheDisablingFeatureForTesting();

  // 3. Generate a tree.
  BackForwardCacheCanStoreDocumentResultWithTree can_store_result =
      web_contents()
          ->GetController()
          .GetBackForwardCache()
          .GetCurrentBackForwardCacheEligibility(rfh_a.get());
  ASSERT_TRUE(can_store_result.tree_reasons);

  // 4. Check that tree results refers only to the fenced frames. We should
  // not see the delegate frames in this list.
  EXPECT_EQ(url_a, can_store_result.tree_reasons->GetUrl());
  EXPECT_EQ(2u, can_store_result.tree_reasons->GetChildren().size());
  EXPECT_THAT(
      can_store_result.tree_reasons->GetDocumentResult(),
      MatchesDocumentResult(NotRestoredReasons(), BlockListedFeatures()));

  // 5. Ensure that each fenced frame is correct. Any frames inside a fenced
  // frame should be always considered cross origin.
  auto& child_a_results = can_store_result.tree_reasons->GetChildren().at(0);
  EXPECT_EQ(fenced_frame_url_a, child_a_results->GetUrl());
  EXPECT_FALSE(child_a_results->IsSameOrigin());
  EXPECT_EQ(0u, child_a_results->GetChildren().size());

  auto& child_b_results = can_store_result.tree_reasons->GetChildren().at(1);
  EXPECT_EQ(fenced_frame_url_b, child_b_results->GetUrl());
  EXPECT_FALSE(child_b_results->IsSameOrigin());
  EXPECT_EQ(1u, child_b_results->GetChildren().size());

  auto& child_c_results = child_b_results->GetChildren().at(0);
  EXPECT_EQ(fenced_frame_url_c, child_c_results->GetUrl());
  EXPECT_FALSE(child_c_results->IsSameOrigin());

  // 6. Check the blocked reasons are set correctly on the fenced frame.
  EXPECT_THAT(child_c_results->GetDocumentResult(),
              MatchesDocumentResult(
                  NotRestoredReasons({NotRestoredReason::kBlocklistedFeatures}),
                  BlockListedFeatures(
                      {blink::scheduler::WebSchedulerTrackedFeature::kDummy})));

  // 7. Ensure that the web exposed reasons do not replicate any of
  // fenced frame results.
  blink::mojom::BackForwardCacheNotRestoredReasonsPtr web_reasons =
      can_store_result.tree_reasons->GetWebExposedNotRestoredReasons();
  EXPECT_TRUE(web_reasons->same_origin_details);
  EXPECT_EQ(2u, web_reasons->same_origin_details->children.size());
  EXPECT_FALSE(
      web_reasons->same_origin_details->children.at(0)->same_origin_details);
  EXPECT_FALSE(
      web_reasons->same_origin_details->children.at(1)->same_origin_details);
}

IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTestWithFencedFrames,
                       EvictionOnInnerFrameTree) {
  DoNotFailForUnexpectedMessagesWhileCached();
  CreateHttpsServer();
  ASSERT_TRUE(https_server()->Start());
  GURL url_a(https_server()->GetURL("a.test", "/fenced_frames/title1.html"));
  GURL url_b(https_server()->GetURL("b.test", "/title1.html"));

  // 1) Navigate to A.
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImplWrapper rfh_a(current_frame_host());

  // 2) Create fenced frame and wait for it to load.
  const GURL fenced_frame_url =
      https_server()->GetURL("c.test", "/fenced_frames/title1.html");
  RenderFrameHostImpl* fenced_frame = static_cast<RenderFrameHostImpl*>(
      fenced_frame_test_helper().CreateFencedFrame(
          web_contents()->GetPrimaryMainFrame(), fenced_frame_url));
  EXPECT_TRUE(WaitForDOMContentLoaded(fenced_frame));
  RenderFrameDeletedObserver delete_observer_fenced_frame(fenced_frame);

  // 3) Navigate to B.
  EXPECT_TRUE(NavigateToURL(shell(), url_b));
  EXPECT_TRUE(rfh_a->IsInBackForwardCache());

  // 4) Execute JS inside inner fenced frame.
  EvictByJavaScript(fenced_frame);

  // FencedFrame is evicted from the BackForwardCache:
  delete_observer_fenced_frame.WaitUntilDeleted();

  // 4) Go back to A.
  ASSERT_TRUE(HistoryGoBack(web_contents()));
  ExpectNotRestored({NotRestoredReason::kJavaScriptExecution}, {}, {}, {}, {},
                    FROM_HERE);
}

}  // namespace content
