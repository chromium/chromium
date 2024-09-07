// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/page_impl.h"

#include "base/command_line.h"
#include "base/memory/weak_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "content/browser/renderer_host/frame_tree.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/renderer_host/navigation_request.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/common/content_navigation_policy.h"
#include "content/public/browser/page.h"
#include "content/public/browser/page_user_data.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "content/public/test/back_forward_cache_util.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/mock_web_contents_observer.h"
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

namespace {

int next_id = 0;

// Example class which inherits the PageUserData, all the data is
// associated to the lifetime of the page.
class Data : public PageUserData<Data> {
 public:
  ~Data() override;

  base::WeakPtr<Data> GetWeakPtr() { return weak_ptr_factory_.GetWeakPtr(); }

  int unique_id() { return unique_id_; }

 private:
  explicit Data(Page& page) : PageUserData(page) { unique_id_ = ++next_id; }

  friend class content::PageUserData<Data>;

  int unique_id_;

  base::WeakPtrFactory<Data> weak_ptr_factory_{this};

  PAGE_USER_DATA_KEY_DECL();
};

PAGE_USER_DATA_KEY_IMPL(Data);

Data::~Data() {
  // Both Page and RenderFrameHost should be non-null and valid before Data
  // deletion, as they will be destroyed after PageUserData destruction.
  EXPECT_TRUE(&page());
  EXPECT_TRUE(&(page().GetMainDocument()));
}

}  // namespace

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
    return web_contents()->GetPrimaryFrameTree().root()->current_frame_host();
  }

  PageImpl& page() { return primary_main_frame_host()->GetPage(); }

  Data* CreateOrGetDataForPage(Page& page) {
    Data* data = Data::GetOrCreateForPage(page);
    EXPECT_TRUE(data);
    return data;
  }

  void EnsureEqualPageUserData(Data* data_a, Data* data_b) {
    EXPECT_EQ(data_a->unique_id(), data_b->unique_id());
  }
};

class PageImplPrerenderBrowserTest : public PageImplTest {
 public:
  PageImplPrerenderBrowserTest()
      : prerender_helper_(
            base::BindRepeating(&PageImplPrerenderBrowserTest::GetWebContents,
                                base::Unretained(this))) {}

  void SetUp() override {
    prerender_helper_.RegisterServerRequestMonitor(embedded_test_server());
    PageImplTest::SetUp();
  }

  content::test::PrerenderTestHelper& prerender_test_helper() {
    return prerender_helper_;
  }

  content::WebContents* GetWebContents() { return web_contents(); }

 protected:
  test::PrerenderTestHelper prerender_helper_;
};

// Test that Page and PageUserData objects are same for main RenderFrameHosts
// and subframes which belong to the same Page.
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

  // 3) Check that PageUserData objects for both pages a and b have same
  // unique_id's.
  EnsureEqualPageUserData(CreateOrGetDataForPage(page_a),
                          CreateOrGetDataForPage(page_b));
}

// Test that Page and PageUserData objects are accessible inside
// RenderFrameDeleted callback.
IN_PROC_BROWSER_TEST_F(PageImplTest, RenderFrameHostDeleted) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  // 1) Navigate to A.
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImpl* rfh_a = primary_main_frame_host();
  PageImpl& page_a = rfh_a->GetPage();
  base::WeakPtr<Data> data = CreateOrGetDataForPage(page_a)->GetWeakPtr();
  RenderFrameDeletedObserver delete_rfh_a(rfh_a);

  // 2) PageUserData associated with page_a should be valid when
  // RenderFrameDeleted callback is invoked.
  testing::NiceMock<MockWebContentsObserver> observer(shell()->web_contents());
  EXPECT_CALL(observer, RenderFrameDeleted(testing::_))
      .WillOnce(
          testing::Invoke([data, rfh_a](RenderFrameHost* render_frame_host) {
            // Both PageUserData and Page objects should be accessible before
            // RenderFrameHost deletion.
            EXPECT_EQ(rfh_a, render_frame_host);
            DCHECK(&render_frame_host->GetPage());
            EXPECT_TRUE(data);
          }));

  // Test needs rfh_a to be deleted after navigating but it doesn't happen with
  // BackForwardCache as it is stored in cache.
  DisableBackForwardCacheForTesting(web_contents(),
                                    BackForwardCache::TEST_REQUIRES_NO_CACHING);

  // 3) Navigate to B, deleting rfh_a.
  EXPECT_TRUE(NavigateToURL(shell(), url_b));
  delete_rfh_a.WaitUntilDeleted();
}

// Test basic functionality of PageUserData.
IN_PROC_BROWSER_TEST_F(PageImplTest, GetCreateAndDeleteUserDataForPage) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));

  // 1) Navigate to A.
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  PageImpl& page_a = page();

  // 2) Get the Data associated with this Page. It should be null
  // before creation.
  Data* data = Data::GetForPage(page_a);
  EXPECT_FALSE(data);

  // 3) Create Data and check that GetForPage shouldn't return null
  // now.
  Data::CreateForPage(page_a);
  base::WeakPtr<Data> created_data = Data::GetForPage(page_a)->GetWeakPtr();
  EXPECT_TRUE(created_data);

  // 4) Delete Data and check that GetForPage should return null.
  Data::DeleteForPage(page_a);
  EXPECT_FALSE(created_data);
  EXPECT_FALSE(Data::GetForPage(page_a));
}

// Test GetOrCreateForPage API of PageUserData.
IN_PROC_BROWSER_TEST_F(PageImplTest, GetOrCreateForPage) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));

  // 1) Navigate to A.
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  PageImpl& page_a = page();

  // 2) Get the Data associated with this RenderFrameHost. It should be null
  // before creation.
  Data* data = Data::GetForPage(page_a);
  EXPECT_FALSE(data);

  // 3) |GetOrCreateForPage| should create Data.
  base::WeakPtr<Data> created_data =
      Data::GetOrCreateForPage(page_a)->GetWeakPtr();
  EXPECT_TRUE(created_data);

  // 4) Another call to |GetOrCreateForPage| should not create the
  // new data and the previous data created in 3) should be preserved.
  Data* new_created_data = Data::GetOrCreateForPage(page_a);
  EXPECT_TRUE(created_data);
  EnsureEqualPageUserData(created_data.get(), new_created_data);
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
  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();

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

// Test that Page and PageUserData object remains the same for pending page
// before and after commit.
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
  base::WeakPtr<Data> data_a = CreateOrGetDataForPage(page_a)->GetWeakPtr();

  // 2) Start navigation to B, but don't commit yet.
  TestNavigationManager manager(shell()->web_contents(), url_b);
  shell()->LoadURL(url_b);
  manager.WaitForSpeculativeRenderFrameHostCreation();

  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();
  RenderFrameHostImpl* pending_rfh =
      root->render_manager()->speculative_frame_host();
  NavigationRequest* navigation_request = root->navigation_request();
  EXPECT_EQ(navigation_request->GetAssociatedRFHType(),
            NavigationRequest::AssociatedRenderFrameHostType::SPECULATIVE);
  EXPECT_TRUE(pending_rfh);

  // 3) While there is a speculative RenderFrameHost in the root FrameTreeNode,
  // get the Page associated with this RenderFrameHost and PageUserData
  // associated with this Page.
  PageImpl& pending_rfh_page = pending_rfh->GetPage();
  EXPECT_NE(&pending_rfh_page, &page_a);
  EXPECT_TRUE(page_a.IsPrimary());
  EXPECT_FALSE(pending_rfh_page.IsPrimary());
  base::WeakPtr<Data> data_before_commit =
      CreateOrGetDataForPage(pending_rfh_page)->GetWeakPtr();
  EXPECT_NE(data_before_commit.get()->unique_id(), data_a.get()->unique_id());

  // 4) Let the navigation finish and make sure it has succeeded.
  ASSERT_TRUE(manager.WaitForNavigationFinished());
  EXPECT_EQ(url_b,
            web_contents()->GetPrimaryMainFrame()->GetLastCommittedURL());

  RenderFrameHostImpl* rfh_b = primary_main_frame_host();
  EXPECT_EQ(pending_rfh, rfh_b);
  PageImpl& rfh_b_page = rfh_b->GetPage();

  // 5) Check |pending_rfh_page| and |rfh_b_page| point to the same object.
  EXPECT_EQ(&pending_rfh_page, &rfh_b_page);
  EXPECT_TRUE(rfh_b_page.IsPrimary());
}

// Test that WebContentsObserver::PrimaryPageChanged is invoked on primary page
// changes after page related data is updated e.g. LastCommittedURL.
IN_PROC_BROWSER_TEST_F(PageImplTest, PrimaryPageChangedOnCrossSiteNavigation) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b, c)"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  // 1) Navigate to A.
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  Page* invoked_page;
  GURL last_committed_url;
  int http_status_code;

  // 2) Invoke MockWebContentsObserver to check the values inside
  // PrimaryPageChanged(Page&) match the ones inside
  // DidFinishNavigation(NavigationHandle*).
  WebContents* contents = web_contents();
  testing::NiceMock<MockWebContentsObserver> web_contents_observer(contents);
  testing::InSequence s;

  {
    // 3) Stores the values of page invoked on PrimaryPageChanged and
    // LastCommittedUrl, HttpStatusCode to match with ones inside
    // DidFinishNavigation and page after navigation.
    EXPECT_CALL(web_contents_observer, PrimaryPageChanged(testing::_))
        .WillOnce(testing::Invoke([&invoked_page, &last_committed_url,
                                   &http_status_code, url_b, this](Page& page) {
          invoked_page = &page;
          last_committed_url = page.GetMainDocument().GetLastCommittedURL();
          http_status_code = web_contents()
                                 ->GetController()
                                 .GetVisibleEntry()
                                 ->GetHttpStatusCode();
          EXPECT_EQ(last_committed_url, url_b);
          EXPECT_TRUE(page.IsPrimary());
          EXPECT_EQ(&web_contents()->GetPrimaryPage(), &page);
        }));

    EXPECT_CALL(web_contents_observer, DidFinishNavigation(testing::_))
        .WillOnce(testing::Invoke([&last_committed_url, &http_status_code](
                                      NavigationHandle* navigation_handle) {
          EXPECT_EQ(navigation_handle->GetURL(), last_committed_url);
          EXPECT_EQ(http_status_code, navigation_handle->GetWebContents()
                                          ->GetController()
                                          .GetVisibleEntry()
                                          ->GetHttpStatusCode());
        }));
  }

  // 4) Navigate to B. PrimaryPageChanged and DidFinishNavigation should be
  // triggered for new `page_b` and should match `invoked_page`.
  EXPECT_TRUE(NavigateToURL(shell(), url_b));
  RenderFrameHostImpl* rfh_b = primary_main_frame_host();
  PageImpl& page_b = rfh_b->GetPage();
  EXPECT_EQ(&page_b, invoked_page);
}

// Test that a new Page object is created for a same-site same-RFH navigation.
IN_PROC_BROWSER_TEST_F(PageImplTest, SameSiteSameRenderFrameHostNavigation) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a1(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_a2(embedded_test_server()->GetURL("a.com", "/title2.html"));

  // 1) Navigate to A1.
  EXPECT_TRUE(NavigateToURL(shell(), url_a1));
  RenderFrameHostImplWrapper main_rfh_a1(primary_main_frame_host());
  base::WeakPtr<Page> page_a1 = main_rfh_a1->GetPage().GetWeakPtr();
  testing::NiceMock<MockWebContentsObserver> page_changed_observer(
      web_contents());
  base::WeakPtr<Data> data = CreateOrGetDataForPage(*page_a1)->GetWeakPtr();

  // 2) Navigate to A2. This will result in invoking PrimaryPageChanged
  // callback.
  EXPECT_CALL(page_changed_observer, PrimaryPageChanged(testing::_)).Times(1);
  EXPECT_TRUE(NavigateToURL(shell(), url_a2));
  RenderFrameHostImplWrapper main_rfh_a2(primary_main_frame_host());
  EXPECT_EQ(CanSameSiteMainFrameNavigationsChangeRenderFrameHosts(),
            main_rfh_a1.get() != main_rfh_a2.get());
  PageImpl& page_a2 = main_rfh_a2.get()->GetPage();

  if (IsBackForwardCacheEnabled()) {
    // 3a) With back/forward cache enabled, both Page objects should be in
    // existence at the same time.
    EXPECT_TRUE(page_a1);
    EXPECT_NE(page_a1.get(), &page_a2);
    // And the user data associated with the page (now in bfcache) should also
    // still be alive.
    EXPECT_TRUE(data);
  } else {
    // 3b) Otherwise, check that the old Page object was destroyed. There is no
    // other way to validate that the old Page and new Page are different:
    // comparing pointer values is not a stable test, since the new Page could
    // be reallocated at the same address.
    EXPECT_FALSE(page_a1);
    // Similarly, expect any PageUserData from the old Page to be gone.
    EXPECT_FALSE(data);
  }
}

// Test that a new Page object is created when RenderFrame is recreated after
// crash.
IN_PROC_BROWSER_TEST_F(PageImplTest, NewPageObjectCreatedOnFrameCrash) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));

  // 1) Navigate to A.
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImpl* rfh_a = primary_main_frame_host();
  base::WeakPtr<Page> page_a = rfh_a->GetPage().GetWeakPtr();
  testing::NiceMock<MockWebContentsObserver> page_changed_observer(
      web_contents());
  base::WeakPtr<Data> data = CreateOrGetDataForPage(*page_a)->GetWeakPtr();

  // 2) Make the renderer crash this should not reset the Page or delete the
  // PageUserData.
  RenderProcessHost* renderer_process = rfh_a->GetProcess();
  RenderProcessHostWatcher crash_observer(
      renderer_process, RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
  renderer_process->Shutdown(0);
  crash_observer.Wait();
  EXPECT_TRUE(&(rfh_a->GetPage()));
  EXPECT_TRUE(data);

  // 3) Re-initialize RenderFrame, this should result in invoking
  // PrimaryPageChanged callback.
  EXPECT_CALL(page_changed_observer, PrimaryPageChanged(testing::_)).Times(1);
  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();
  root->render_manager()->InitializeMainRenderFrameForImmediateUse();

  // 4) Check that the old Page object was destroyed. There is no other way to
  // validate that the old Page and new Page are different: comparing pointer
  // values is not a stable test, since the new Page could be reallocated at the
  // same address.
  EXPECT_FALSE(page_a);
  // Similarly, expect any PageUserData from the old Page to be gone.
  EXPECT_FALSE(data);
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
  base::WeakPtr<Page> page_a1 = rfh_a1->GetPage().GetWeakPtr();
  testing::NiceMock<MockWebContentsObserver> page_changed_observer(
      web_contents());
  base::WeakPtr<Data> data = CreateOrGetDataForPage(*page_a1)->GetWeakPtr();

  // 2) Crash the renderer hosting current RFH. This should not reset the Page
  // or delete the PageUserData.
  RenderProcessHost* renderer_process = rfh_a1->GetProcess();
  RenderProcessHostWatcher crash_observer(
      renderer_process, RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
  renderer_process->Shutdown(0);
  crash_observer.Wait();
  EXPECT_TRUE(&(web_contents()->GetPrimaryMainFrame()->GetPage()));
  EXPECT_TRUE(data);

  // 3) Navigate same-site to A2. This will result in invoking
  // PrimaryPageChanged callback after new Page creation.
  EXPECT_CALL(page_changed_observer, PrimaryPageChanged(testing::_)).Times(1);
  EXPECT_TRUE(NavigateToURL(shell(), url_a2));

  // 4) Check that the old Page object was destroyed. There is no other way to
  // validate that the old Page and new Page are different: comparing pointer
  // values is not a stable test, since the new Page could be reallocated at the
  // same address.
  EXPECT_FALSE(page_a1);
  // Similarly, expect any PageUserData from the old Page to be gone.
  EXPECT_FALSE(data);
}

// Test PageImpl with BackForwardCache feature enabled.
class PageImplWithBackForwardCacheTest : public PageImplTest {
 public:
  PageImplWithBackForwardCacheTest() {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        GetDefaultEnabledBackForwardCacheFeaturesForTesting(
            /*ignore_outstanding_network_request=*/false),
        GetDefaultDisabledBackForwardCacheFeaturesForTesting());
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
  testing::NiceMock<MockWebContentsObserver> page_changed_observer(
      web_contents());

  // 2) Get the PageImpl object associated with A and B RenderFrameHost.
  PageImpl& page_a = rfh_a->GetPage();
  PageImpl& page_b = rfh_b->GetPage();
  Data* data = CreateOrGetDataForPage(page_a);

  // 3) Navigate to C. PrimaryPageChanged should be triggered as A(B) is stored
  // in BackForwardCache.
  EXPECT_CALL(page_changed_observer, PrimaryPageChanged(testing::_)).Times(1);
  EXPECT_TRUE(NavigateToURL(shell(), url_c));
  EXPECT_TRUE(rfh_a->IsInBackForwardCache());
  EXPECT_TRUE(rfh_b->IsInBackForwardCache());
  EXPECT_FALSE(page_a.IsPrimary());
  EXPECT_FALSE(page_b.IsPrimary());

  // 4) PageImpl associated with document should point to the same object.
  // PageUserData should not be deleted on navigating away with
  // BackForwardCache.
  EXPECT_EQ(&page_a, &(rfh_a->GetPage()));
  EXPECT_EQ(&page_b, &(rfh_b->GetPage()));
  EXPECT_TRUE(data);

  // 5) Go back to A(B) and the Page object before and after restore should
  // point to the same object. PrimaryPageChanged should still be triggered when
  // primary page changes to the existing page restored from the
  // BackForwardCache point to the same object and PageUserData should not be
  // deleted.
  EXPECT_CALL(page_changed_observer, PrimaryPageChanged(testing::_)).Times(1);
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  EXPECT_EQ(&page_a, &(rfh_a->GetPage()));
  EXPECT_EQ(&page_b, &(rfh_b->GetPage()));
  EXPECT_TRUE(page_a.IsPrimary());
  EXPECT_TRUE(page_b.IsPrimary());
  EXPECT_TRUE(data);
}

// Tests that PageImpl object is correct for IsPrimary.
IN_PROC_BROWSER_TEST_F(PageImplPrerenderBrowserTest, IsPrimary) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // Navigate to a site.
  GURL url_a = embedded_test_server()->GetURL("/empty.html");
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImpl* rfh_a = primary_main_frame_host();
  EXPECT_TRUE(rfh_a->GetPage().IsPrimary());
  testing::NiceMock<MockWebContentsObserver> page_changed_observer(
      web_contents());

  // Prerender to another site.
  GURL prerender_url = embedded_test_server()->GetURL("/title2.html");
  prerender_helper_.AddPrerender(prerender_url);
  FrameTreeNodeId host_id =
      prerender_test_helper().GetHostForUrl(prerender_url);
  content::test::PrerenderHostObserver host_observer(*web_contents(), host_id);
  content::RenderFrameHost* prerender_frame =
      prerender_test_helper().GetPrerenderedMainFrameHost(host_id);
  Page& prerender_page = prerender_frame->GetPage();
  EXPECT_FALSE(prerender_page.IsPrimary());

  // Navigate to the prerendered site. PrimaryPageChanged should only be
  // triggered on activation.
  EXPECT_CALL(page_changed_observer, PrimaryPageChanged(testing::_)).Times(1);
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
