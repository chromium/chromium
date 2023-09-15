// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "content/browser/process_lock.h"
#include "content/browser/renderer_host/navigation_controller_impl.h"
#include "content/browser/renderer_host/navigation_request.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/browser/site_info.h"
#include "content/browser/site_instance_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/test/back_forward_cache_util.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/navigation_handle_observer.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_navigation_throttle.h"
#include "content/public/test/test_navigation_throttle_inserter.h"
#include "content/public/test/url_loader_interceptor.h"
#include "content/shell/browser/shell.h"
#include "content/test/content_browser_test_utils_internal.h"
#include "net/dns/mock_host_resolver.h"
#include "ui/base/page_transition_types.h"
#include "url/url_constants.h"

namespace content {

using EarlyRenderFrameHostSwapType =
    NavigationRequest::EarlyRenderFrameHostSwapType;

namespace {

// A customized navigation observer to check whether a navigation used the early
// RenderFrameHost swap or not.
class EarlySwapNavigationObserver : public TestNavigationObserver {
 public:
  using TestNavigationObserver::TestNavigationObserver;
  ~EarlySwapNavigationObserver() override = default;

  EarlyRenderFrameHostSwapType early_swap_type() { return early_swap_type_; }

  // WebContentsObserver overrides:
  void OnDidFinishNavigation(NavigationHandle* handle) override {
    early_swap_type_ =
        NavigationRequest::From(handle)->early_render_frame_host_swap_type();
    TestNavigationObserver::OnDidFinishNavigation(handle);
  }

 private:
  EarlyRenderFrameHostSwapType early_swap_type_ =
      EarlyRenderFrameHostSwapType::kNone;
};

}  // namespace

// Base class for tests that enable the experimental early RenderFrameHost swap
// for back/forward navigations, to support navigation transitions.
class EarlySwapNavigationBrowserTestBase : public ContentBrowserTest {
 public:
  EarlySwapNavigationBrowserTestBase() {
    scoped_feature_list_.InitAndEnableFeature(
        features::kEarlyDocumentSwapForBackForwardTransitions);
  }

  EarlySwapNavigationBrowserTestBase(
      const EarlySwapNavigationBrowserTestBase&) = delete;
  EarlySwapNavigationBrowserTestBase& operator=(
      const EarlySwapNavigationBrowserTestBase&) = delete;

 protected:
  void SetUpOnMainThread() override {
    ContentBrowserTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
    SetupCrossSiteRedirector(embedded_test_server());
    AddRedirectOnSecondNavigationHandler(embedded_test_server());
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    ContentBrowserTest::SetUpCommandLine(command_line);

    // These tests rely on cross-site navigations swapping RenderFrameHosts and
    // processes. Ensure that this happens on Android.
    IsolateAllSitesForTesting(command_line);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// By default, most tests in this file should run without back-forward cache, as
// in that case the early swap is not needed for navigation transitions. Some
// tests do want to enable the back-forward cache to cover the no-early-swap
// behavior in that case. Provide test classes for both of these cases.
class EarlySwapNavigationBrowserTest
    : public EarlySwapNavigationBrowserTestBase {
 public:
  EarlySwapNavigationBrowserTest() {
    feature_list_.InitAndDisableFeature(features::kBackForwardCache);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};
class EarlySwapAndBackForwardCacheBrowserTest
    : public EarlySwapNavigationBrowserTestBase {
 public:
  EarlySwapAndBackForwardCacheBrowserTest() {
    feature_list_.InitWithFeaturesAndParameters(
        GetDefaultEnabledBackForwardCacheFeaturesForTesting(
            /*ignore_outstanding_network_request=*/false),
        GetDefaultDisabledBackForwardCacheFeaturesForTesting());
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Check that a simple browser-initiated, cross-process back navigation invokes
// the early swap.
IN_PROC_BROWSER_TEST_F(EarlySwapNavigationBrowserTest, BackNavigation) {
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();

  // Navigate to a.com and then to b.com.
  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  ASSERT_TRUE(NavigateToURL(shell(), url_a));
  NavigationEntry* entry_a =
      shell()->web_contents()->GetController().GetLastCommittedEntry();
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));
  ASSERT_TRUE(NavigateToURL(shell(), url_b));
  EXPECT_EQ(
      shell()->web_contents()->GetPrimaryMainFrame()->GetLastCommittedURL(),
      url_b);
  NavigationEntry* entry_b =
      shell()->web_contents()->GetController().GetLastCommittedEntry();

  {
    base::HistogramTester histograms;

    // Start a back navigation to a.com, but don't wait for commit.
    TestNavigationManager back_navigation(shell()->web_contents(), url_a);
    shell()->web_contents()->GetController().GoBack();

    // There should be a speculative RFH at this point.
    RenderFrameHostImplWrapper navigating_rfh(
        root->render_manager()->speculative_frame_host());
    ASSERT_TRUE(navigating_rfh.get());

    // The last committed entry should be b.com, and the pending entry should be
    // for a.com.
    EXPECT_EQ(entry_b,
              shell()->web_contents()->GetController().GetLastCommittedEntry());
    EXPECT_EQ(entry_a,
              shell()->web_contents()->GetController().GetPendingEntry());

    // The last committed URL is still b.com.
    EXPECT_EQ(
        shell()->web_contents()->GetPrimaryMainFrame()->GetLastCommittedURL(),
        url_b);

    // Wait for request start.  This will run through WillStartRequest() in all
    // non-test throttles and then pause at WillStartRequest in the
    // TestNavigationManager's navigation throttle.
    EXPECT_TRUE(back_navigation.WaitForRequestStart());

    // The early swap shouldn't have happened yet, so we should still have the
    // same speculative RFH, and the last committed URL should still be b.com.
    EXPECT_EQ(navigating_rfh.get(),
              root->render_manager()->speculative_frame_host());
    EXPECT_EQ(
        shell()->web_contents()->GetPrimaryMainFrame()->GetLastCommittedURL(),
        url_b);
    EXPECT_EQ(entry_b,
              shell()->web_contents()->GetController().GetLastCommittedEntry());
    EXPECT_EQ(entry_a,
              shell()->web_contents()->GetController().GetPendingEntry());

    // Resume the navigation.  This will synchronously run
    // NavigationRequest::OnStartChecksComplete(), which should trigger the
    // early RFH swap.
    back_navigation.ResumeNavigation();

    // The speculative RFH should've been swapped with the current RFH now.
    EXPECT_FALSE(root->render_manager()->speculative_frame_host());
    EXPECT_EQ(root->current_frame_host(), navigating_rfh.get());
    EXPECT_EQ(shell()->web_contents()->GetPrimaryMainFrame(),
              navigating_rfh.get());

    // The back navigation should still be ongoing.  Its NavigationRequest
    // should indicate that an early swap was performed.
    EXPECT_TRUE(root->navigation_request());
    EXPECT_EQ(url_a, root->navigation_request()->GetURL());
    EXPECT_EQ(EarlyRenderFrameHostSwapType::kNavigationTransition,
              root->navigation_request()->early_render_frame_host_swap_type());

    // The last committed URL is now empty, as we've swapped in a
    // RenderFrameHost that hasn't committed anything yet.  However, the new
    // RenderFrameHost should be in an a.com SiteInstance and in a process
    // already locked to a.com.
    EXPECT_EQ(
        shell()->web_contents()->GetPrimaryMainFrame()->GetLastCommittedURL(),
        GURL::EmptyGURL());
    EXPECT_EQ(navigating_rfh->GetSiteInstance()->GetSiteInfo().site_url(),
              GURL("http://a.com"));
    EXPECT_TRUE(
        navigating_rfh->GetProcess()->GetProcessLock().is_locked_to_site());
    EXPECT_EQ(navigating_rfh->GetProcess()->GetProcessLock().lock_url(),
              GURL("http://a.com"));

    // The last committed entry should still be b.com, and the pending entry
    // should be for a.com.  These should not be affected by the early swap.
    EXPECT_EQ(entry_b,
              shell()->web_contents()->GetController().GetLastCommittedEntry());
    EXPECT_EQ(entry_a,
              shell()->web_contents()->GetController().GetPendingEntry());

    // Wait for the navigation to commit.  It should commit in the RFH that was
    // swapped in early.
    EXPECT_TRUE(back_navigation.WaitForNavigationFinished());
    EXPECT_FALSE(root->render_manager()->speculative_frame_host());
    EXPECT_EQ(root->current_frame_host(), navigating_rfh.get());
    EXPECT_EQ(
        shell()->web_contents()->GetPrimaryMainFrame()->GetLastCommittedURL(),
        url_a);
    EXPECT_EQ(entry_a,
              shell()->web_contents()->GetController().GetLastCommittedEntry());

    // Check that the early swap's characteristics are correct in metrics.
    histograms.ExpectUniqueSample(
        "Navigation.EarlyRenderFrameHostSwapType",
        EarlyRenderFrameHostSwapType::kNavigationTransition, 1);
    histograms.ExpectUniqueSample(
        "Navigation.EarlyRenderFrameHostSwap.HasCommitted", 1, 1);
    histograms.ExpectUniqueSample(
        "Navigation.EarlyRenderFrameHostSwap.IsInOutermostMainFrame", 1, 1);
  }
}

// Check that a renderer-initiated, cross-process back navigation does *not*
// invoke the early swap.  Only browser-initiated navigations are eligible for
// navigation transitions.
IN_PROC_BROWSER_TEST_F(EarlySwapNavigationBrowserTest,
                       NoSwapForRendererInitiatedNavigation) {
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();

  // Navigate to a.com and then to b.com.
  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  ASSERT_TRUE(NavigateToURL(shell(), url_a));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));
  ASSERT_TRUE(NavigateToURL(shell(), url_b));
  EXPECT_EQ(root->current_frame_host()->GetLastCommittedURL(), url_b);

  {
    base::HistogramTester histograms;

    // Start a renderer-initiated back navigation to a.com.
    TestNavigationManager back_navigation(shell()->web_contents(), url_a);
    EXPECT_TRUE(ExecJs(root, "history.back()"));

    // There should be a speculative RFH at this point.
    RenderFrameHostImplWrapper navigating_rfh(
        root->render_manager()->speculative_frame_host());
    ASSERT_TRUE(navigating_rfh.get());

    // Wait for request start and then resume the navigation.  Ensure that no
    // early swap is invoked while processing ResumeNavigation().
    EXPECT_TRUE(back_navigation.WaitForRequestStart());
    back_navigation.ResumeNavigation();
    EXPECT_EQ(navigating_rfh.get(),
              root->render_manager()->speculative_frame_host());
    EXPECT_EQ(
        shell()->web_contents()->GetPrimaryMainFrame()->GetLastCommittedURL(),
        url_b);

    // The back navigation should still be ongoing.
    EXPECT_TRUE(root->navigation_request());
    EXPECT_EQ(url_a, root->navigation_request()->GetURL());
    EXPECT_EQ(EarlyRenderFrameHostSwapType::kNone,
              root->navigation_request()->early_render_frame_host_swap_type());

    // Wait for the navigation to commit.
    EXPECT_TRUE(back_navigation.WaitForNavigationFinished());
    EXPECT_FALSE(root->render_manager()->speculative_frame_host());
    EXPECT_EQ(root->current_frame_host(), navigating_rfh.get());
    EXPECT_EQ(
        shell()->web_contents()->GetPrimaryMainFrame()->GetLastCommittedURL(),
        url_a);

    // Check that the metrics reflect that there was no early swap.
    histograms.ExpectUniqueSample("Navigation.EarlyRenderFrameHostSwapType",
                                  EarlyRenderFrameHostSwapType::kNone, 1);
    histograms.ExpectTotalCount(
        "Navigation.EarlyRenderFrameHostSwap.HasCommitted", 0);
    histograms.ExpectTotalCount(
        "Navigation.EarlyRenderFrameHostSwap.IsInOutermostMainFrame", 0);
  }
}

// Check that a cross-process back navigation in a subframe does *not*
// invoke the early swap.  Only main frame navigations are eligible for
// navigation transitions.
IN_PROC_BROWSER_TEST_F(EarlySwapNavigationBrowserTest,
                       NoSwapForBackNavigationInSubframe) {
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();

  // Navigate to a.com with a same-site subframe.
  GURL main_url(
      embedded_test_server()->GetURL("a.com", "/page_with_iframe.html"));
  ASSERT_TRUE(NavigateToURL(shell(), main_url));
  GURL subframe_url(embedded_test_server()->GetURL("a.com", "/title1.html"));
  EXPECT_EQ(root->child_at(0)->current_frame_host()->GetLastCommittedURL(),
            subframe_url);

  // Navigate the subframe to c.com.
  GURL new_subframe_url(
      embedded_test_server()->GetURL("c.com", "/title1.html"));
  EXPECT_TRUE(NavigateFrameToURL(root->child_at(0), new_subframe_url));
  EXPECT_EQ(root->child_at(0)->current_frame_host()->GetLastCommittedURL(),
            new_subframe_url);

  // Do a browser-initiated back navigation in the subframe and ensure there was
  // no early RFH swap.
  EarlySwapNavigationObserver back_navigation(shell()->web_contents());
  shell()->web_contents()->GetController().GoBack();
  back_navigation.Wait();
  EXPECT_TRUE(back_navigation.last_navigation_succeeded());
  EXPECT_EQ(root->child_at(0)->current_frame_host()->GetLastCommittedURL(),
            subframe_url);
  EXPECT_EQ(EarlyRenderFrameHostSwapType::kNone,
            back_navigation.early_swap_type());
}

// Check that reloads do not invoke the early swap.  Only back/forward history
// navigations are eligible for navigation transitions.
IN_PROC_BROWSER_TEST_F(EarlySwapNavigationBrowserTest, NoSwapForReload) {
  GURL main_url(embedded_test_server()->GetURL("a.com", "/title1.html"));
  ASSERT_TRUE(NavigateToURL(shell(), main_url));

  // Do a reload and ensure there was no early RFH swap.
  EarlySwapNavigationObserver reload_observer(shell()->web_contents());
  shell()->Reload();
  reload_observer.Wait();
  EXPECT_TRUE(reload_observer.last_navigation_succeeded());
  EXPECT_EQ(EarlyRenderFrameHostSwapType::kNone,
            reload_observer.early_swap_type());
}

// Check that going back or forward by more than one entry invokes the early
// swap.  It's not yet clear whether this case will require navigation
// transitions, but it is supported for now.
IN_PROC_BROWSER_TEST_F(EarlySwapNavigationBrowserTest,
                       SwapForBackForwardByMultipleEntries) {
  // Set up the following session history: [A, B, C, D*]
  // where * is the current entry.
  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  ASSERT_TRUE(NavigateToURL(shell(), url_a));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));
  ASSERT_TRUE(NavigateToURL(shell(), url_b));
  GURL url_c(embedded_test_server()->GetURL("c.com", "/title1.html"));
  ASSERT_TRUE(NavigateToURL(shell(), url_c));
  GURL url_d(embedded_test_server()->GetURL("d.com", "/title1.html"));
  ASSERT_TRUE(NavigateToURL(shell(), url_d));

  // Go back from D to A: [A*, B, C, D]
  EarlySwapNavigationObserver back_observer(shell()->web_contents());
  shell()->web_contents()->GetController().GoToOffset(-3);
  back_observer.Wait();
  EXPECT_EQ(EarlyRenderFrameHostSwapType::kNavigationTransition,
            back_observer.early_swap_type());

  // Go forward from A to B: [A, B*, C, D]
  EarlySwapNavigationObserver forward_observer(shell()->web_contents());
  shell()->web_contents()->GetController().GoForward();
  forward_observer.Wait();
  EXPECT_EQ(EarlyRenderFrameHostSwapType::kNavigationTransition,
            forward_observer.early_swap_type());

  // Go forward from B to D: [A, B, C, D*]
  EarlySwapNavigationObserver forward_observer2(shell()->web_contents());
  shell()->web_contents()->GetController().GoToOffset(2);
  forward_observer2.Wait();
  EXPECT_EQ(EarlyRenderFrameHostSwapType::kNavigationTransition,
            forward_observer2.early_swap_type());
}

// Check that when a WillStartRequest throttle cancels a back navigation, there
// is no early swap.
IN_PROC_BROWSER_TEST_F(EarlySwapNavigationBrowserTest,
                       NoSwapWhenWillStartRequestFails) {
  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  ASSERT_TRUE(NavigateToURL(shell(), url_a));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));
  ASSERT_TRUE(NavigateToURL(shell(), url_b));

  // Block all subsequent navigation attempts in WillStartRequest.
  TestNavigationThrottleInserter throttle_inserter(
      shell()->web_contents(),
      base::BindLambdaForTesting(
          [&](NavigationHandle* handle) -> std::unique_ptr<NavigationThrottle> {
            auto throttle = std::make_unique<TestNavigationThrottle>(handle);
            throttle->SetResponse(TestNavigationThrottle::WILL_START_REQUEST,
                                  TestNavigationThrottle::SYNCHRONOUS,
                                  NavigationThrottle::BLOCK_REQUEST);
            return throttle;
          }));

  // Try navigating back to A. This should fail with an error page, and there
  // should be no early swap.
  EarlySwapNavigationObserver back_observer(shell()->web_contents());
  shell()->web_contents()->GetController().GoBack();
  back_observer.Wait();
  EXPECT_FALSE(back_observer.last_navigation_succeeded());
  EXPECT_EQ(net::ERR_BLOCKED_BY_CLIENT, back_observer.last_net_error_code());
  EXPECT_TRUE(
      static_cast<SiteInstanceImpl*>(
          shell()->web_contents()->GetPrimaryMainFrame()->GetSiteInstance())
          ->GetSiteInfo()
          .is_error_page());
  EXPECT_EQ(EarlyRenderFrameHostSwapType::kNone,
            back_observer.early_swap_type());
}

// Check that when the back navigation fails because the response comes back
// with an error, the early swap still happens, prior to committing an error
// page. This is because the early swap logic is invoked prior to getting the
// response.
IN_PROC_BROWSER_TEST_F(EarlySwapNavigationBrowserTest,
                       EarlySwapWhenResponseFails) {
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();

  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  ASSERT_TRUE(NavigateToURL(shell(), url_a));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));
  ASSERT_TRUE(NavigateToURL(shell(), url_b));

  std::unique_ptr<URLLoaderInterceptor> interceptor =
      URLLoaderInterceptor::SetupRequestFailForURL(url_a,
                                                   net::ERR_CONNECTION_REFUSED);

  // Try navigating back to A. This should fail with an error page, and there
  // should have been an early swap to a RFH that was ultimately discarded,
  // since we eventually needed to commit an error page in a different RFH.
  EarlySwapNavigationObserver back_observer(shell()->web_contents());
  TestNavigationManager back_manager(shell()->web_contents(), url_a);
  shell()->web_contents()->GetController().GoBack();

  RenderFrameHostImplWrapper navigating_rfh(
      root->render_manager()->speculative_frame_host());
  ASSERT_TRUE(navigating_rfh.get());

  // Wait for request start and then resume the navigation.  Ensure that the
  // early swap has happened.
  EXPECT_TRUE(back_manager.WaitForRequestStart());
  back_manager.ResumeNavigation();
  EXPECT_FALSE(root->render_manager()->speculative_frame_host());
  EXPECT_EQ(navigating_rfh.get(), root->render_manager()->current_frame_host());

  // Check that the NavigationRequest is still around and has recorded the early
  // swap type.
  EXPECT_TRUE(root->navigation_request());
  EXPECT_EQ(url_a, root->navigation_request()->GetURL());
  EXPECT_EQ(EarlyRenderFrameHostSwapType::kNavigationTransition,
            root->navigation_request()->early_render_frame_host_swap_type());

  // Wait for the navigation to finish.  The RFH should be swapped again to an
  // error page RFH.
  back_observer.Wait();
  EXPECT_FALSE(back_observer.last_navigation_succeeded());
  EXPECT_TRUE(
      static_cast<SiteInstanceImpl*>(
          shell()->web_contents()->GetPrimaryMainFrame()->GetSiteInstance())
          ->GetSiteInfo()
          .is_error_page());
  EXPECT_NE(navigating_rfh.get(),
            shell()->web_contents()->GetPrimaryMainFrame());
  EXPECT_EQ(EarlyRenderFrameHostSwapType::kNavigationTransition,
            back_observer.early_swap_type());
}

// Check that when the back navigation is aborted due to receiving a 204
// response, the early swap still happens.  Currently, this leaves an
// about:blank page from the swapped-in RFH as the primary page.
//
// TODO(https://crbug.com/1480129): This case should be converted to show a new
// error page.
IN_PROC_BROWSER_TEST_F(EarlySwapNavigationBrowserTest,
                       EarlySwapWith204Response) {
  // Navigate to a URL that will load a normal 200 response initially, and
  // will redirect to a 204 response when it's requested the second time.
  GURL nocontent_url(embedded_test_server()->GetURL("a.com", "/nocontent"));
  GURL url_a(embedded_test_server()->GetURL(
      "a.com", "/redirect-on-second-navigation?" + nocontent_url.spec()));
  ASSERT_TRUE(NavigateToURL(shell(), url_a));

  // Go to B and then navigate back to A, which should now redirect to a 204
  // page.
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));
  ASSERT_TRUE(NavigateToURL(shell(), url_b));
  EarlySwapNavigationObserver back_observer(shell()->web_contents());
  NavigationHandleObserver back_handle_observer(shell()->web_contents(), url_a);
  shell()->web_contents()->GetController().GoBack();
  back_observer.Wait();

  // The early swap should have happened, because it's invoked prior to
  // receiving the response and realizing that the navigation should be aborted.
  EXPECT_EQ(EarlyRenderFrameHostSwapType::kNavigationTransition,
            back_observer.early_swap_type());

  // The navigation itself didn't commit and isn't considered successful, though
  // there's also no error page.
  EXPECT_FALSE(back_observer.last_navigation_succeeded());
  EXPECT_FALSE(back_handle_observer.has_committed());
  EXPECT_TRUE(back_handle_observer.was_redirected());
  EXPECT_FALSE(
      static_cast<SiteInstanceImpl*>(
          shell()->web_contents()->GetPrimaryMainFrame()->GetSiteInstance())
          ->GetSiteInfo()
          .is_error_page());

  // The current page should be the empty early-swapped RFH in a.com's
  // SiteInstance.
  EXPECT_EQ(
      static_cast<SiteInstanceImpl*>(
          shell()->web_contents()->GetPrimaryMainFrame()->GetSiteInstance())
          ->GetSiteInfo()
          .site_url(),
      GURL("http://a.com"));
  EXPECT_EQ(
      shell()->web_contents()->GetPrimaryMainFrame()->GetLastCommittedURL(),
      GURL::EmptyGURL());
}

// Exercise a case where a back navigation performs an early RFH swap, but that
// RFH is subsequently replaced due to a cross-site redirect.
IN_PROC_BROWSER_TEST_F(EarlySwapNavigationBrowserTest,
                       EarlySwapFollowedByAnotherSwap) {
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();

  // Navigate to an a.com URL that will load a normal 200 response initially,
  // and will redirect to c.com when it's requested the second time.
  GURL url_c(embedded_test_server()->GetURL("c.com", "/title1.html"));
  GURL url_a(embedded_test_server()->GetURL(
      "a.com", "/redirect-on-second-navigation?" + url_c.spec()));
  ASSERT_TRUE(NavigateToURL(shell(), url_a));

  // Go to B and then go back to A, which will now redirect to C.
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));
  ASSERT_TRUE(NavigateToURL(shell(), url_b));

  EarlySwapNavigationObserver back_observer(shell()->web_contents());
  TestNavigationManager back_manager(shell()->web_contents(), url_a);
  shell()->web_contents()->GetController().GoBack();

  RenderFrameHostImplWrapper early_swap_rfh(
      root->render_manager()->speculative_frame_host());
  ASSERT_TRUE(early_swap_rfh.get());

  // Wait for request start and then resume the navigation, which should trigger
  // the early swap.
  EXPECT_TRUE(back_manager.WaitForRequestStart());
  back_manager.ResumeNavigation();
  EXPECT_FALSE(root->render_manager()->speculative_frame_host());
  EXPECT_EQ(early_swap_rfh.get(), root->render_manager()->current_frame_host());

  // Wait for the navigation to finish.  It should've committed in a different
  // RFH that the one that was swapped in early.
  EXPECT_TRUE(back_manager.WaitForNavigationFinished());
  EXPECT_TRUE(back_observer.last_navigation_succeeded());
  EXPECT_EQ(EarlyRenderFrameHostSwapType::kNavigationTransition,
            back_observer.early_swap_type());
  EXPECT_NE(early_swap_rfh.get(),
            shell()->web_contents()->GetPrimaryMainFrame());
  EXPECT_EQ(
      shell()->web_contents()->GetPrimaryMainFrame()->GetLastCommittedURL(),
      url_c);
}

// Check that back navigations that are served from back-forward cache do not
// invoke the early swap.  The early swap is not needed in that case, as there
// will be no delay between showing a navigation transition and activating a
// cached RFH.
IN_PROC_BROWSER_TEST_F(EarlySwapAndBackForwardCacheBrowserTest,
                       NoSwapForBackForwardCache) {
  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  ASSERT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImplWrapper rfh_a(
      shell()->web_contents()->GetPrimaryMainFrame());

  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));
  ASSERT_TRUE(NavigateToURL(shell(), url_b));
  EXPECT_TRUE(rfh_a->IsInBackForwardCache());

  // Go back and ensure there was no early RFH swap.
  EarlySwapNavigationObserver back_observer(shell()->web_contents());
  shell()->web_contents()->GetController().GoBack();
  back_observer.Wait();
  EXPECT_TRUE(back_observer.last_navigation_succeeded());
  EXPECT_EQ(EarlyRenderFrameHostSwapType::kNone,
            back_observer.early_swap_type());
  EXPECT_EQ(rfh_a.get(), shell()->web_contents()->GetPrimaryMainFrame());
}

}  // namespace content
