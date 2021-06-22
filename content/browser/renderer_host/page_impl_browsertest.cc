// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/page_impl.h"

#include "base/command_line.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "content/browser/renderer_host/frame_tree.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/renderer_host/navigation_request.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/common/content_navigation_policy.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "content/public/test/back_forward_cache_util.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/prerender_test_util.h"
#include "content/public/test/render_frame_host_test_support.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "content/public/test/url_loader_interceptor.h"
#include "content/shell/browser/shell.h"
#include "content/test/content_browser_test_utils_internal.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

namespace content {

class PageImplTest : public ContentBrowserTest {
 public:
  ~PageImplTest() override = default;

 protected:
  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    ContentBrowserTest::SetUpOnMainThread();
  }

  WebContentsImpl* web_contents() const {
    return static_cast<WebContentsImpl*>(shell()->web_contents());
  }

  RenderFrameHostImpl* primary_main_frame_host() {
    return web_contents()->GetFrameTree()->root()->current_frame_host();
  }
};

class PageImplPrerenderBrowserTest : public PageImplTest {
 public:
  PageImplPrerenderBrowserTest()
      : prerender_helper_(
            base::BindRepeating(&PageImplPrerenderBrowserTest::GetWebContents,
                                base::Unretained(this))) {}

  void SetUpOnMainThread() override {
    prerender_helper_.SetUpOnMainThread(embedded_test_server());
    PageImplTest::SetUpOnMainThread();
  }

  content::test::PrerenderTestHelper& prerender_test_helper() {
    return prerender_helper_;
  }

  content::WebContents* GetWebContents() { return web_contents(); }

 protected:
  test::PrerenderTestHelper prerender_helper_;
};

// Test that Page objects are same for main RenderFrameHosts and subframes which
// belong to the same Page.
IN_PROC_BROWSER_TEST_F(PageImplTest, AllFramesBelongToTheSamePage) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b)"));

  // 1) Navigate to a(b).
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImpl* rfh_a = primary_main_frame_host();
  RenderFrameHostImpl* rfh_b = rfh_a->child_at(0)->current_frame_host();

  // 2) Check Page for RenderFrameHosts a and b, they both should point to same
  // Page object.
  PageImpl& page_a = rfh_a->GetPage();
  PageImpl& page_b = rfh_b->GetPage();
  EXPECT_EQ(&page_a, &page_b);
  EXPECT_TRUE(page_a.IsPrimary());
}

// Test that the Page object doesn't change for new subframe RFHs after
// subframes does a cross-site or same-site navigation.
//
// 1) Navigate to A(A1, B).
// 2) Navigate cross site A1 to C.
// 3) Navigate same site B to B2.
IN_PROC_BROWSER_TEST_F(PageImplTest, PageObjectAfterSubframeNavigation) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // 1) Navigate to A1(A2, B).
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(a,b)"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));
  RenderFrameHostImpl* rfh_a = primary_main_frame_host();
  RenderFrameHostImpl* rfh_a2 = rfh_a->child_at(0)->current_frame_host();
  RenderFrameHostImpl* rfh_b = rfh_a->child_at(1)->current_frame_host();

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = web_contents()->GetFrameTree()->root();

  // 2) Check that Page for A1, A2, B point to same object.
  PageImpl& page_a = rfh_a->GetPage();
  PageImpl& page_a2 = rfh_a2->GetPage();
  PageImpl& page_b = rfh_b->GetPage();
  EXPECT_EQ(&page_a, &page_a2);
  EXPECT_EQ(&page_a2, &page_b);

  // 3) Navigate subframe cross-site from A2 -> C.
  GURL url_c = embedded_test_server()->GetURL("c.com", "/title1.html");
  EXPECT_TRUE(NavigateToURLFromRenderer(root->child_at(0), url_c));

  // 4) Page object of new subframe C should be same as page_a.
  RenderFrameHostImpl* rfh_c = rfh_a->child_at(0)->current_frame_host();
  PageImpl& page_c = rfh_c->GetPage();
  EXPECT_EQ(&page_c, &page_a);

  // 5) Navigate subframe same-site from B -> B2.
  GURL url_b = embedded_test_server()->GetURL("b.com", "/title2.html");
  EXPECT_TRUE(NavigateToURLFromRenderer(root->child_at(1), url_b));

  // 6) Page object of new subframe B2 should be same as page_a.
  RenderFrameHostImpl* rfh_b2 = rfh_a->child_at(1)->current_frame_host();
  PageImpl& page_b2 = rfh_b2->GetPage();
  EXPECT_EQ(&page_b2, &page_a);
}

// Test that Page object remains the same for pending page before and after
// commit.
IN_PROC_BROWSER_TEST_F(PageImplTest, PageObjectBeforeAndAfterCommit) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title2.html"));

  // Isolate "b.com" so we are guaranteed to get a different process
  // for navigations to this origin on Android. Doing this ensures that a
  // speculative RenderFrameHost is used.
  IsolateOriginsForTesting(embedded_test_server(), shell()->web_contents(),
                           {"b.com"});

  // 1) Navigate to A.
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImpl* rfh_a = primary_main_frame_host();
  PageImpl& page_a = rfh_a->GetPage();

  // 2) Start navigation to B, but don't commit yet.
  TestNavigationManager manager(shell()->web_contents(), url_b);
  shell()->LoadURL(url_b);
  EXPECT_TRUE(manager.WaitForRequestStart());

  FrameTreeNode* root = web_contents()->GetFrameTree()->root();
  RenderFrameHostImpl* pending_rfh =
      root->render_manager()->speculative_frame_host();
  NavigationRequest* navigation_request = root->navigation_request();
  EXPECT_EQ(navigation_request->associated_site_instance_type(),
            NavigationRequest::AssociatedSiteInstanceType::SPECULATIVE);
  EXPECT_TRUE(pending_rfh);

  // 3) While there is a speculative RenderFrameHost in the root FrameTreeNode,
  // get the Page associated with this RenderFrameHost.
  PageImpl& pending_rfh_page = pending_rfh->GetPage();
  EXPECT_NE(&pending_rfh_page, &page_a);
  EXPECT_TRUE(page_a.IsPrimary());
  EXPECT_FALSE(pending_rfh_page.IsPrimary());

  // 4) Let the navigation finish and make sure it has succeeded.
  manager.WaitForNavigationFinished();
  EXPECT_EQ(url_b, web_contents()->GetMainFrame()->GetLastCommittedURL());

  RenderFrameHostImpl* rfh_b = primary_main_frame_host();
  EXPECT_EQ(pending_rfh, rfh_b);
  PageImpl& rfh_b_page = rfh_b->GetPage();

  // 5) Check |pending_rfh_page| and |rfh_b_page| point to the same object.
  EXPECT_EQ(&pending_rfh_page, &rfh_b_page);
  EXPECT_TRUE(rfh_b_page.IsPrimary());
}

// Test that a new Page object is created for a same-site same-RFH navigation.
// https://crbug.com/1219373 fails with BFCache field trial testing config.
#if defined(OS_ANDROID)
#define MAYBE_SameSiteSameRenderFrameHostNavigation \
  DISABLED_SameSiteSameRenderFrameHostNavigation
#else
#define MAYBE_SameSiteSameRenderFrameHostNavigation \
  SameSiteSameRenderFrameHostNavigation
#endif
IN_PROC_BROWSER_TEST_F(PageImplTest,
                       MAYBE_SameSiteSameRenderFrameHostNavigation) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a1(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_a2(embedded_test_server()->GetURL("a.com", "/title2.html"));

  // 1) Navigate to A1.
  EXPECT_TRUE(NavigateToURL(shell(), url_a1));
  RenderFrameHostImpl* main_rfh_a1 = primary_main_frame_host();
  PageImpl& page_a1 = main_rfh_a1->GetPage();

  // 2) Navigate to A2, both A1 and A2 should reuse RenderFrameHost.
  EXPECT_TRUE(NavigateToURL(shell(), url_a2));
  RenderFrameHostImpl* main_rfh_a2 = primary_main_frame_host();
  EXPECT_EQ(main_rfh_a1, main_rfh_a2);
  PageImpl& page_a2 = main_rfh_a1->GetPage();

  // 3) New Page object should be created.
  EXPECT_NE(&page_a1, &page_a2);
}

// Test that a new Page object is created when RenderFrame is recreated after
// crash.
IN_PROC_BROWSER_TEST_F(PageImplTest, NewPageObjectCreatedOnFrameCrash) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));

  // 1) Navigate to A.
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImpl* rfh_a = primary_main_frame_host();
  PageImpl& page_a = rfh_a->GetPage();

  // 2) Make the renderer crash.
  RenderProcessHost* renderer_process = rfh_a->GetProcess();
  RenderProcessHostWatcher crash_observer(
      renderer_process, RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
  renderer_process->Shutdown(0);
  crash_observer.Wait();
  EXPECT_TRUE(&(rfh_a->GetPage()));

  // 3) Re-initialize RenderFrame.
  FrameTreeNode* root = web_contents()->GetFrameTree()->root();
  root->render_manager()->InitializeMainRenderFrameForImmediateUse();
  RenderFrameHostImpl* new_rfh_a = primary_main_frame_host();

  // 4) Check that new Page object was created after new RenderFrame creation.
  PageImpl& new_page_a = new_rfh_a->GetPage();
  EXPECT_NE(&page_a, &new_page_a);
}

// Test that a new Page object is created when we do a same-site navigation
// after renderer crashes.
IN_PROC_BROWSER_TEST_F(PageImplTest, SameSiteNavigationAfterFrameCrash) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a1(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_a2(embedded_test_server()->GetURL("a.com", "/title2.html"));

  // 1) Navigate to A1.
  EXPECT_TRUE(NavigateToURL(shell(), url_a1));
  RenderFrameHostImpl* rfh_a1 = primary_main_frame_host();
  PageImpl& page_a1 = rfh_a1->GetPage();

  // 2) Crash the renderer hosting current RFH.
  RenderProcessHost* renderer_process = rfh_a1->GetProcess();
  RenderProcessHostWatcher crash_observer(
      renderer_process, RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
  renderer_process->Shutdown(0);
  crash_observer.Wait();
  EXPECT_TRUE(&(web_contents()->GetMainFrame()->GetPage()));

  // 3) Navigate same-site to A2.
  EXPECT_TRUE(NavigateToURL(shell(), url_a2));
  RenderFrameHostImpl* rfh_a2 = primary_main_frame_host();
  PageImpl& page_a2 = rfh_a2->GetPage();

  // 4) Check that new Page object was created after same-site navigation which
  // resulted in new RenderFrame creation.
  EXPECT_NE(&page_a1, &page_a2);
}

// Test PageImpl with BackForwardCache feature enabled.
class PageImplWithBackForwardCacheTest : public PageImplTest {
 public:
  PageImplWithBackForwardCacheTest() {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {{features::kBackForwardCache,
          // Set a very long TTL before expiration (longer than the test
          // timeout) so tests that are expecting deletion don't pass when
          // they shouldn't.
          {{"TimeToLiveInBackForwardCacheInSeconds", "3600"}}}},
        // Allow BackForwardCache for all devices regardless of their memory.
        {features::kBackForwardCacheMemoryControls});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Tests that PageImpl object is not cleared on storing and restoring a Page
// from back-forward cache.
IN_PROC_BROWSER_TEST_F(PageImplWithBackForwardCacheTest,
                       BackForwardCacheNavigation) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b)"));
  GURL url_c(embedded_test_server()->GetURL("c.com", "/title1.html"));

  // 1) Navigate to A(B).
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImpl* rfh_a = primary_main_frame_host();
  RenderFrameHostImpl* rfh_b = rfh_a->child_at(0)->current_frame_host();

  // 2) Get the PageImpl object associated with A and B RenderFrameHost.
  PageImpl& page_a = rfh_a->GetPage();
  PageImpl& page_b = rfh_b->GetPage();

  // 3) Navigate to C. A(B) should be stored in back-forward cache.
  EXPECT_TRUE(NavigateToURL(shell(), url_c));
  EXPECT_TRUE(rfh_a->IsInBackForwardCache());
  EXPECT_TRUE(rfh_b->IsInBackForwardCache());
  EXPECT_FALSE(page_a.IsPrimary());
  EXPECT_FALSE(page_b.IsPrimary());

  // 4) PageImpl associated with document should point to the same object on
  // navigating away with BackForwardCache.
  EXPECT_EQ(&page_a, &(rfh_a->GetPage()));
  EXPECT_EQ(&page_b, &(rfh_b->GetPage()));

  // 5) Go back to A(B) and the Page object before and after restore should
  // point to the same object.
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  EXPECT_EQ(&page_a, &(rfh_a->GetPage()));
  EXPECT_EQ(&page_b, &(rfh_b->GetPage()));
  EXPECT_TRUE(page_a.IsPrimary());
  EXPECT_TRUE(page_b.IsPrimary());
}

// Tests that PageImpl object is correct for IsPrimary.
IN_PROC_BROWSER_TEST_F(PageImplPrerenderBrowserTest, IsPrimary) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // Navigate to a site.
  GURL url_a = embedded_test_server()->GetURL("/empty.html");
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImpl* rfh_a = primary_main_frame_host();
  EXPECT_TRUE(rfh_a->GetPage().IsPrimary());

  // Prerender to another site.
  GURL prerender_url = embedded_test_server()->GetURL("/title2.html");
  prerender_helper_.AddPrerender(prerender_url);
  int host_id = prerender_test_helper().GetHostForUrl(prerender_url);
  content::test::PrerenderHostObserver host_observer(*web_contents(), host_id);
  content::RenderFrameHost* prerender_frame =
      prerender_test_helper().GetPrerenderedMainFrameHost(host_id);
  Page& prerender_page = prerender_frame->GetPage();
  EXPECT_FALSE(prerender_page.IsPrimary());

  // Navigate to the prerendered site.
  prerender_helper_.NavigatePrimaryPage(prerender_url);
  EXPECT_TRUE(host_observer.was_activated());
  EXPECT_EQ(&prerender_page, &(primary_main_frame_host()->GetPage()));
  EXPECT_TRUE(prerender_page.IsPrimary());
}

// Tests that IsPrimary returns false when pending deletion.
IN_PROC_BROWSER_TEST_F(PageImplPrerenderBrowserTest, IsPrimaryPendingDeletion) {
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url_a(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b)"));
  GURL url_d(embedded_test_server()->GetURL("d.com", "/title1.html"));

  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImpl* rfh_a = primary_main_frame_host();
  RenderFrameHostImpl* rfh_b = rfh_a->child_at(0)->current_frame_host();
  EXPECT_TRUE(rfh_a->GetPage().IsPrimary());
  EXPECT_TRUE(rfh_b->GetPage().IsPrimary());
  LeaveInPendingDeletionState(rfh_a);
  LeaveInPendingDeletionState(rfh_b);

  EXPECT_TRUE(NavigateToURL(shell(), url_d));

  EXPECT_FALSE(rfh_a->GetPage().IsPrimary());
  EXPECT_FALSE(rfh_b->GetPage().IsPrimary());
}

}  // namespace content
