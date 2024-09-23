// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/site_per_process_browsertest.h"

#include "build/build_config.h"
#include "content/browser/renderer_host/cross_process_frame_connector.h"
#include "content/browser/renderer_host/frame_tree.h"
#include "content/browser/renderer_host/render_frame_proxy_host.h"
#include "content/common/features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/fenced_frame_test_util.h"
#include "content/shell/browser/shell.h"
#include "content/test/render_document_feature.h"
#include "content/test/render_widget_host_visibility_observer.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {

// Helper class to wait for the next sad frame to be shown in a specific
// FrameTreeNode.  This can be used to wait for sad frame visibility metrics to
// be logged.
class SadFrameShownObserver {
 public:
  explicit SadFrameShownObserver(FrameTreeNode* ftn) {
    RenderFrameProxyHost* proxy_to_parent =
        ftn->render_manager()->GetProxyToParent();
    proxy_to_parent->cross_process_frame_connector()
        ->set_child_frame_crash_shown_closure_for_testing(
            run_loop_.QuitClosure());
  }

  explicit SadFrameShownObserver(RenderFrameHostImpl* rfhi) {
    RenderFrameProxyHost* proxy_to_parent = rfhi->GetProxyToOuterDelegate();
    proxy_to_parent->cross_process_frame_connector()
        ->set_child_frame_crash_shown_closure_for_testing(
            run_loop_.QuitClosure());
  }

  SadFrameShownObserver(const SadFrameShownObserver&) = delete;
  SadFrameShownObserver& operator=(const SadFrameShownObserver&) = delete;

  void Wait() { run_loop_.Run(); }

 private:
  base::RunLoop run_loop_;
};

}  // namespace

// Disable the feature to mark hidden tabs with sad frames for reload, for use
// in tests where this feature interferes with the behavior being tested.
class SitePerProcessBrowserTestWithoutSadFrameTabReload
    : public SitePerProcessBrowserTest {
 public:
  SitePerProcessBrowserTestWithoutSadFrameTabReload() {
    feature_list_.InitAndDisableFeature(
        features::kReloadHiddenTabsWithCrashedSubframes);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// This test is flaky on all platforms.
// TODO(crbug.com/40749527): Deflake it and enable this test back.
IN_PROC_BROWSER_TEST_P(
    SitePerProcessBrowserTestWithoutSadFrameTabReload,
    DISABLED_ChildFrameCrashMetrics_KilledWhileHiddenThenShown) {
  // Set-up a frame tree that helps verify what the metrics tracks:
  // 1) frames (12 frames are affected if B process gets killed) or
  // 2) widgets (10 b widgets and 1 c widget are affected if B is killed) or
  // 3) crashes (1 crash if B process gets killed)?
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b(b,c),b,b,b,b,b,b,b,b,b)"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));
  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();

  // Hide the web contents (UpdateWebContentsVisibility is called twice to avoid
  // hitting the |!did_first_set_visible_| case).  Make sure all subframes are
  // considered hidden at this point.
  web_contents()->UpdateWebContentsVisibility(Visibility::VISIBLE);
  web_contents()->UpdateWebContentsVisibility(Visibility::HIDDEN);
  for (size_t i = 0U; i < root->child_count(); i++) {
    RenderFrameProxyHost* proxy_to_parent =
        root->child_at(i)->render_manager()->GetProxyToParent();
    CrossProcessFrameConnector* connector =
        proxy_to_parent->cross_process_frame_connector();
    EXPECT_FALSE(connector->IsVisible())
        << " subframe " << i << " with URL " << root->child_at(i)->current_url()
        << " is visible";
  }

  // Kill the subframe.
  base::HistogramTester histograms;
  RenderProcessHost* child_process =
      root->child_at(0)->current_frame_host()->GetProcess();
  RenderProcessHostWatcher crash_observer(
      child_process, RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
  child_process->Shutdown(0);
  crash_observer.Wait();

  // Verify that no child frame metrics got logged (yet - while WebContents are
  // hidden).
  histograms.ExpectTotalCount("Stability.ChildFrameCrash.Visibility", 0);
  histograms.ExpectTotalCount(
      "Stability.ChildFrameCrash.ShownAfterCrashingReason", 0);

  // Show the web contents, wait for each of the subframe FrameTreeNodes to
  // show a sad frame, and verify that the expected metrics got logged.
  std::vector<std::unique_ptr<SadFrameShownObserver>> observers;
  for (size_t i = 0U; i < root->child_count(); i++) {
    observers.push_back(
        std::make_unique<SadFrameShownObserver>(root->child_at(i)));
  }

  web_contents()->UpdateWebContentsVisibility(Visibility::VISIBLE);

  for (size_t i = 0U; i < root->child_count(); i++) {
    SCOPED_TRACE(testing::Message()
                 << " Waiting for sad frame from subframe " << i
                 << " with URL:" << root->child_at(i)->current_url());
    observers[i]->Wait();
  }

  histograms.ExpectUniqueSample(
      "Stability.ChildFrameCrash.Visibility",
      CrossProcessFrameConnector::CrashVisibility::kShownAfterCrashing, 10);
  histograms.ExpectUniqueSample(
      "Stability.ChildFrameCrash.ShownAfterCrashingReason",
      CrossProcessFrameConnector::ShownAfterCrashingReason::kTabWasShown, 10);

  // Hide and show the web contents again and verify that no more metrics got
  // logged.
  web_contents()->UpdateWebContentsVisibility(Visibility::HIDDEN);
  web_contents()->UpdateWebContentsVisibility(Visibility::VISIBLE);
  histograms.ExpectUniqueSample(
      "Stability.ChildFrameCrash.Visibility",
      CrossProcessFrameConnector::CrashVisibility::kShownAfterCrashing, 10);
  histograms.ExpectUniqueSample(
      "Stability.ChildFrameCrash.ShownAfterCrashingReason",
      CrossProcessFrameConnector::ShownAfterCrashingReason::kTabWasShown, 10);
}

IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTestWithoutSadFrameTabReload,
                       ChildFrameCrashMetrics_ScrolledIntoViewAfterTabIsShown) {
  // Start on a page that has a single iframe, which is positioned out of
  // view, and navigate that iframe cross-site.
  GURL main_url(
      embedded_test_server()->GetURL("a.com", "/iframe_out_of_view.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));
  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();
  EXPECT_TRUE(NavigateToURLFromRenderer(
      root->child_at(0),
      embedded_test_server()->GetURL("b.com", "/title1.html")));

  // Hide the web contents (UpdateWebContentsVisibility is called twice to avoid
  // hitting the |!did_first_set_visible_| case).
  web_contents()->UpdateWebContentsVisibility(Visibility::VISIBLE);
  web_contents()->UpdateWebContentsVisibility(Visibility::HIDDEN);

  // Kill the child frame.
  base::HistogramTester histograms;
  RenderProcessHost* child_process =
      root->child_at(0)->current_frame_host()->GetProcess();
  RenderProcessHostWatcher crash_observer(
      child_process, RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
  child_process->Shutdown(0);
  crash_observer.Wait();

  // Verify that no child frame crash metrics got logged yet.
  histograms.ExpectTotalCount("Stability.ChildFrameCrash.Visibility", 0);
  histograms.ExpectTotalCount(
      "Stability.ChildFrameCrash.ShownAfterCrashingReason", 0);

  // Show the web contents.  The crash metrics still shouldn't be logged, since
  // the crashed frame is out of view.
  web_contents()->UpdateWebContentsVisibility(Visibility::VISIBLE);
  histograms.ExpectTotalCount("Stability.ChildFrameCrash.Visibility", 0);
  histograms.ExpectTotalCount(
      "Stability.ChildFrameCrash.ShownAfterCrashingReason", 0);

  // Scroll the subframe into view and wait until the scrolled frame draws
  // itself.
  std::string scrolling_script = R"(
    var frame = document.body.querySelector("iframe");
    frame.scrollIntoView();
  )";
  EXPECT_TRUE(ExecJs(root, scrolling_script));
  // This will ensure that browser has received the
  // FrameHostMsg_UpdateViewportIntersection IPC message from the renderer main
  // thread.
  EXPECT_EQ(true,
            EvalJsAfterLifecycleUpdate(root->current_frame_host(), "", "true"));

  // Verify that the expected metrics got logged.
  histograms.ExpectUniqueSample(
      "Stability.ChildFrameCrash.Visibility",
      CrossProcessFrameConnector::CrashVisibility::kShownAfterCrashing, 1);
  histograms.ExpectUniqueSample(
      "Stability.ChildFrameCrash.ShownAfterCrashingReason",
      CrossProcessFrameConnector::ShownAfterCrashingReason::
          kViewportIntersectionAfterTabWasShown,
      1);

  // Hide and show the web contents again and verify that no more metrics got
  // logged.
  web_contents()->UpdateWebContentsVisibility(Visibility::HIDDEN);
  web_contents()->UpdateWebContentsVisibility(Visibility::VISIBLE);
  histograms.ExpectUniqueSample(
      "Stability.ChildFrameCrash.Visibility",
      CrossProcessFrameConnector::CrashVisibility::kShownAfterCrashing, 1);
  histograms.ExpectUniqueSample(
      "Stability.ChildFrameCrash.ShownAfterCrashingReason",
      CrossProcessFrameConnector::ShownAfterCrashingReason::
          kViewportIntersectionAfterTabWasShown,
      1);
}

class SitePerProcessBrowserTestWithSadFrameTabReload
    : public SitePerProcessBrowserTest {
 public:
  SitePerProcessBrowserTestWithSadFrameTabReload() {
    // Disable the feature to mark hidden tabs with sad frames for reload, since
    // it makes the scenario for which this test collects metrics impossible.
    feature_list_.InitAndEnableFeature(
        features::kReloadHiddenTabsWithCrashedSubframes);
  }

  void CrashProcess(FrameTreeNode* ftn) {
    RenderProcessHost* process = ftn->current_frame_host()->GetProcess();
    RenderProcessHostWatcher crash_observer(
        process, RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
    process->Shutdown(0);
    crash_observer.Wait();
    EXPECT_FALSE(ftn->current_frame_host()->IsRenderFrameLive());
  }

  void CrashRendererProcess(RenderFrameHostImpl* rfhi) {
    RenderProcessHost* process = rfhi->GetProcess();
    RenderProcessHostWatcher crash_observer(
        process, RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
    process->Shutdown(0);
    crash_observer.Wait();
    EXPECT_FALSE(rfhi->IsRenderFrameLive());
  }

  WebContentsImpl* web_contents() {
    return static_cast<WebContentsImpl*>(shell()->web_contents());
  }

  RenderFrameHostImpl* primary_main_frame_host() {
    return web_contents()->GetPrimaryMainFrame();
  }

  content::test::FencedFrameTestHelper& fenced_frame_test_helper() {
    return fenced_frame_helper_;
  }

 private:
  content::test::FencedFrameTestHelper fenced_frame_helper_;
  base::test::ScopedFeatureList feature_list_;
};

// Verify the feature where hidden tabs with a visible crashed subframe are
// marked for reload. This avoids showing crashed subframes if a hidden tab is
// eventually shown. See https://crbug.com/841572.
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTestWithSadFrameTabReload,
                       ReloadHiddenTabWithCrashedSubframeInViewport) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b)"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));
  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();

  // Hide the WebContents (UpdateWebContentsVisibility is called twice to avoid
  // hitting the |!did_first_set_visible_| case).
  web_contents()->UpdateWebContentsVisibility(Visibility::VISIBLE);
  web_contents()->UpdateWebContentsVisibility(Visibility::HIDDEN);
  EXPECT_EQ(Visibility::HIDDEN, web_contents()->GetVisibility());

  // Kill the b.com subframe's process.  This should mark the hidden
  // WebContents for reload.
  {
    base::HistogramTester histograms;
    CrashProcess(root->child_at(0));
    histograms.ExpectUniqueSample(
        "Stability.ChildFrameCrash.TabMarkedForReload", true, 1);
    histograms.ExpectUniqueSample(
        "Stability.ChildFrameCrash.TabMarkedForReload.Visibility",
        blink::mojom::FrameVisibility::kRenderedInViewport, 1);
  }

  // Show the WebContents.  This should trigger a reload of the main frame.
  {
    base::HistogramTester histograms;
    web_contents()->UpdateWebContentsVisibility(Visibility::VISIBLE);
    EXPECT_TRUE(WaitForLoadStop(web_contents()));
    histograms.ExpectUniqueSample(
        "Navigation.LoadIfNecessaryType",
        NavigationControllerImpl::NeedsReloadType::kCrashedSubframe, 1);
  }

  // Both frames should now have live renderer processes.
  EXPECT_TRUE(root->current_frame_host()->IsRenderFrameLive());
  EXPECT_TRUE(root->child_at(0)->current_frame_host()->IsRenderFrameLive());
}

// Verify that when a tab is reloaded because it was previously marked for
// reload due to having a sad frame, we log the sad frame as shown during a tab
// reload, rather than being shown to the user directly, since the sad frame is
// expected to go away shortly. See https://crbug.com/1132938.
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTestWithSadFrameTabReload,
                       CrashedSubframeVisibilityMetricsDuringTabReload) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b,c)"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));
  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();

  // Hide the WebContents (UpdateWebContentsVisibility is called twice to avoid
  // hitting the |!did_first_set_visible_| case).
  RenderWidgetHostVisibilityObserver hide_observer(
      root->child_at(0)->current_frame_host()->GetRenderWidgetHost(),
      false /* became_visible */);
  web_contents()->UpdateWebContentsVisibility(Visibility::VISIBLE);
  web_contents()->UpdateWebContentsVisibility(Visibility::HIDDEN);
  EXPECT_EQ(Visibility::HIDDEN, web_contents()->GetVisibility());
  hide_observer.WaitUntilSatisfied();

  // Kill the b.com subframe's process.  This should mark the hidden
  // WebContents for reload.
  CrashProcess(root->child_at(0));
  auto& controller = static_cast<NavigationControllerImpl&>(
      shell()->web_contents()->GetController());
  EXPECT_TRUE(controller.NeedsReload());
  EXPECT_EQ(1, controller.GetEntryCount());

  // Show the WebContents. This should trigger a reload of the main frame.  Sad
  // frame visibility metrics should indicate that the sad frame is shown while
  // the tab is being reloaded.  Because the tab reload will wipe out the sad
  // frame, this isn't as bad as kShownAfterCrashing.
  {
    base::HistogramTester histograms;
    SadFrameShownObserver sad_frame_observer(root->child_at(0));
    TestNavigationManager manager(web_contents(), main_url);
    web_contents()->UpdateWebContentsVisibility(Visibility::VISIBLE);
    EXPECT_TRUE(manager.WaitForRequestStart());
    sad_frame_observer.Wait();

    histograms.ExpectUniqueSample("Stability.ChildFrameCrash.Visibility",
                                  CrashVisibility::kShownWhileAncestorIsLoading,
                                  1);

    // Ensure no new metrics are logged after the reload completes.
    ASSERT_TRUE(manager.WaitForNavigationFinished());
    EXPECT_TRUE(manager.was_successful());
    EXPECT_FALSE(controller.NeedsReload());
    EXPECT_EQ(1, controller.GetEntryCount());
    histograms.ExpectUniqueSample("Stability.ChildFrameCrash.Visibility",
                                  CrashVisibility::kShownWhileAncestorIsLoading,
                                  1);
  }
}

// Verify that a sad frame shown when its parent frame is loading is logged
// with appropriate metrics, namely as kShownWhileAncestorIsLoading rather than
// kShownAfterCrashing. See https://crbug.com/1132938.
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTestWithSadFrameTabReload,
                       CrashedSubframeVisibilityMetricsDuringParentLoad) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b(c))"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));
  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();
  FrameTreeNode* child = root->child_at(0);
  FrameTreeNode* grandchild = child->child_at(0);

  // Hide the grandchild frame.
  RenderWidgetHostVisibilityObserver hide_observer(
      grandchild->current_frame_host()->GetRenderWidgetHost(),
      false /* became_visible */);
  EXPECT_TRUE(
      ExecJs(child, "document.querySelector('iframe').style.display = 'none'"));
  hide_observer.WaitUntilSatisfied();

  // Kill the c.com grandchild process.
  CrashProcess(grandchild);

  // Start a navigation in the b.com frame, but don't commit.
  GURL url_d(embedded_test_server()->GetURL("d.com", "/title1.html"));
  TestNavigationManager manager(web_contents(), url_d);
  EXPECT_TRUE(ExecJs(child, JsReplace("location.href = $1", url_d)));
  EXPECT_TRUE(manager.WaitForRequestStart());

  // Make the grandchild iframe with the sad frame visible again.  This should
  // get logged as kShownWhileAncestorIsLoading, because its parent is
  // currently loading.
  {
    base::HistogramTester histograms;
    SadFrameShownObserver sad_frame_observer(grandchild);
    EXPECT_TRUE(ExecJs(
        child, "document.querySelector('iframe').style.display = 'block'"));
    sad_frame_observer.Wait();

    histograms.ExpectUniqueSample("Stability.ChildFrameCrash.Visibility",
                                  CrashVisibility::kShownWhileAncestorIsLoading,
                                  1);

    // Ensure no new metrics are logged after the navigation completes.
    ASSERT_TRUE(manager.WaitForNavigationFinished());
    EXPECT_TRUE(manager.was_successful());
    histograms.ExpectUniqueSample("Stability.ChildFrameCrash.Visibility",
                                  CrashVisibility::kShownWhileAncestorIsLoading,
                                  1);
  }
}

// Verify that a sad frame shown when its parent frame is loading is logged
// with appropriate metrics, namely as kShownWhileAncestorIsLoading rather than
// kShownAfterCrashing. See https://crbug.com/1132938.
IN_PROC_BROWSER_TEST_P(
    SitePerProcessBrowserTestWithSadFrameTabReload,
    // TODO(crbug.com/40839850): Re-enable this test
    DISABLED_CrashedFencedframeVisibilityMetricsDuringParentLoad) {
  GURL primary_url(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL child_url(
      embedded_test_server()->GetURL("b.com", "/fenced_frames/title1.html"));
  GURL grandchild_url(
      embedded_test_server()->GetURL("c.com", "/fenced_frames/title1.html"));

  EXPECT_TRUE(NavigateToURL(shell(), primary_url));
  RenderFrameHostImplWrapper primary_rfh(primary_main_frame_host());
  RenderFrameHostImplWrapper child_rfh(
      fenced_frame_test_helper().CreateFencedFrame(primary_rfh.get(),
                                                   child_url));
  // Note that height and width follows the layout function in
  // content/test/data/cross_site_iframe_factory.html.
  EXPECT_TRUE(ExecJs(primary_rfh.get(), R"(
       var ff = document.querySelector('fencedframe');
       // layoutX = gridSizeX * largestChildX + extraXPerLevel
       ff.width = 1 * (110 + 30) + 50;
       // layoutY = gridSizeY * largestChildY + extraYPerLevel
       ff.height = 1 * (110 + 30) + 50
       )"));
  RenderFrameHostImplWrapper grandchild_rfh(
      fenced_frame_test_helper().CreateFencedFrame(child_rfh.get(),
                                                   grandchild_url));
  // Note that height and width follows the layout function in
  // content/test/data/cross_site_iframe_factory.html.
  EXPECT_TRUE(ExecJs(child_rfh.get(), R"(
       var ff = document.querySelector('fencedframe');
       ff.width = 110;
       ff.height = 110;
       )"));

  // Hide the grandchild frame.
  RenderWidgetHostVisibilityObserver hide_observer(
      grandchild_rfh->GetRenderWidgetHost(), false /* became_visible */);
  EXPECT_TRUE(
      ExecJs(child_rfh.get(),
             "document.querySelector('fencedframe').style.display = 'none'"));
  hide_observer.WaitUntilSatisfied();

  // Kill the grandchild process.
  CrashRendererProcess(grandchild_rfh.get());

  // Start a navigation in the child frame, but don't commit.
  GURL url_d(
      embedded_test_server()->GetURL("d.com", "/fenced_frames/title1.html"));
  TestNavigationManager manager(web_contents(), url_d);
  EXPECT_TRUE(ExecJs(child_rfh.get(), JsReplace("location.href = $1", url_d)));
  EXPECT_TRUE(manager.WaitForRequestStart());

  // Make the grandchild fencedframe with the sad frame visible again.
  // This should get logged as kShownWhileAncestorIsLoading, because its parent
  // is currently loading.
  {
    base::HistogramTester histograms;
    SadFrameShownObserver sad_frame_observer(grandchild_rfh.get());
    EXPECT_TRUE(ExecJs(
        child_rfh.get(),
        "document.querySelector('fencedframe').style.display = 'block'"));
    sad_frame_observer.Wait();

    histograms.ExpectUniqueSample("Stability.ChildFrameCrash.Visibility",
                                  CrashVisibility::kShownWhileAncestorIsLoading,
                                  1);

    // Ensure no new metrics are logged after the navigation completes.
    ASSERT_TRUE(manager.WaitForNavigationFinished());
    EXPECT_TRUE(manager.was_successful());
    histograms.ExpectUniqueSample("Stability.ChildFrameCrash.Visibility",
                                  CrashVisibility::kShownWhileAncestorIsLoading,
                                  1);
  }
}

// Verify the feature where hidden tabs with crashed subframes are marked for
// reload. This avoids showing crashed subframes if a hidden tab is eventually
// shown. Similar to the test above, except that the crashed subframe is
// scrolled out of view.
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTestWithSadFrameTabReload,
                       // TODO(crbug.com/40870019): Re-enable this test
                       DISABLED_ReloadHiddenTabWithCrashedSubframeOutOfView) {
  // Set WebContents to VISIBLE to avoid hitting the |!did_first_set_visible_|
  // case when we hide it later.
  web_contents()->UpdateWebContentsVisibility(Visibility::VISIBLE);

  // Navigate to a page with an OOPIF that's scrolled out of view.
  GURL out_of_view_url(
      embedded_test_server()->GetURL("a.com", "/iframe_out_of_view.html"));
  EXPECT_TRUE(NavigateToURL(shell(), out_of_view_url));
  EXPECT_EQ("LOADED", EvalJs(shell(), "notifyWhenLoaded();"));
  NavigateIframeToURL(web_contents(), "test_iframe",
                      embedded_test_server()->GetURL("b.com", "/title1.html"));

  // This will ensure that the layout has completed and visibility of the OOPIF
  // has been updated in the browser process.
  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();
  EXPECT_EQ(true,
            EvalJsAfterLifecycleUpdate(root->current_frame_host(), "", "true"));

  // Verify the OOPIF isn't visible at the moment.
  RenderFrameProxyHost* proxy_to_parent =
      root->child_at(0)->render_manager()->GetProxyToParent();
  CrossProcessFrameConnector* connector =
      proxy_to_parent->cross_process_frame_connector();
  EXPECT_FALSE(connector->IsVisible());
  EXPECT_EQ(blink::mojom::FrameVisibility::kRenderedOutOfViewport,
            connector->visibility());

  // Hide the WebContents and crash the OOPIF.
  {
    base::HistogramTester histograms;
    web_contents()->UpdateWebContentsVisibility(Visibility::HIDDEN);
    CrashProcess(root->child_at(0));
    histograms.ExpectUniqueSample(
        "Stability.ChildFrameCrash.TabMarkedForReload", true, 1);
    histograms.ExpectUniqueSample(
        "Stability.ChildFrameCrash.TabMarkedForReload.Visibility",
        blink::mojom::FrameVisibility::kRenderedOutOfViewport, 1);
  }

  EXPECT_TRUE(root->current_frame_host()->IsRenderFrameLive());
  EXPECT_FALSE(root->child_at(0)->current_frame_host()->IsRenderFrameLive());

  // Show the tab and ensure that it reloads.
  {
    base::HistogramTester histograms;
    web_contents()->UpdateWebContentsVisibility(Visibility::VISIBLE);
    EXPECT_TRUE(WaitForLoadStop(web_contents()));
    histograms.ExpectUniqueSample(
        "Navigation.LoadIfNecessaryType",
        NavigationControllerImpl::NeedsReloadType::kCrashedSubframe, 1);
  }

  EXPECT_TRUE(root->current_frame_host()->IsRenderFrameLive());
  EXPECT_TRUE(root->child_at(0)->current_frame_host()->IsRenderFrameLive());
}

// Verify that hidden tabs with a crashed subframe are not marked for reload
// when the crashed subframe is hidden with "display:none".
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTestWithSadFrameTabReload,
                       DoNotReloadHiddenTabWithHiddenCrashedSubframe) {
  // Set WebContents to VISIBLE to avoid hitting the |!did_first_set_visible_|
  // case when we hide it later.
  web_contents()->UpdateWebContentsVisibility(Visibility::VISIBLE);

  GURL hidden_iframe_url(
      embedded_test_server()->GetURL("a.com", "/page_with_hidden_iframe.html"));
  EXPECT_TRUE(NavigateToURL(shell(), hidden_iframe_url));
  NavigateIframeToURL(web_contents(), "test_iframe",
                      embedded_test_server()->GetURL("b.com", "/title1.html"));

  // Ensure that the parent frame has propagated the OOPIF's hidden visibility
  // to the browser process by forcing requestAnimationFrame and
  // waiting for layout to finish.
  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();
  EXPECT_EQ(true,
            EvalJsAfterLifecycleUpdate(root->current_frame_host(), "", "true"));

  // The OOPIF should be hidden at this point.
  RenderFrameProxyHost* proxy_to_parent =
      root->child_at(0)->render_manager()->GetProxyToParent();
  EXPECT_TRUE(proxy_to_parent->cross_process_frame_connector()->IsHidden());

  // Crashing a hidden OOPIF shouldn't mark the tab for reload.
  web_contents()->UpdateWebContentsVisibility(Visibility::HIDDEN);
  base::HistogramTester histograms;
  CrashProcess(root->child_at(0));
  histograms.ExpectUniqueSample("Stability.ChildFrameCrash.TabMarkedForReload",
                                false, 1);

  // Making the WebContents visible again should keep the sad frame and should
  // not load anything new.
  web_contents()->UpdateWebContentsVisibility(Visibility::VISIBLE);
  EXPECT_TRUE(WaitForLoadStop(web_contents()));
  EXPECT_TRUE(root->current_frame_host()->IsRenderFrameLive());
  EXPECT_FALSE(root->child_at(0)->current_frame_host()->IsRenderFrameLive());
}

// Ensure that the sad frame reload policy doesn't trigger for a visible tab,
// even if it becomes hidden and then visible again.
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTestWithSadFrameTabReload,
                       DoNotReloadVisibleTabWithCrashedSubframe) {
  // Set WebContents to VISIBLE to avoid hitting the |!did_first_set_visible_|
  // case when we hide it later.
  web_contents()->UpdateWebContentsVisibility(Visibility::VISIBLE);
  EXPECT_EQ(Visibility::VISIBLE, web_contents()->GetVisibility());

  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b)"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));
  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();
  base::HistogramTester histograms;
  CrashProcess(root->child_at(0));
  histograms.ExpectUniqueSample("Stability.ChildFrameCrash.TabMarkedForReload",
                                false, 1);

  EXPECT_EQ(Visibility::VISIBLE, web_contents()->GetVisibility());
  web_contents()->UpdateWebContentsVisibility(Visibility::HIDDEN);
  web_contents()->UpdateWebContentsVisibility(Visibility::VISIBLE);
  EXPECT_TRUE(WaitForLoadStop(web_contents()));
  EXPECT_TRUE(root->current_frame_host()->IsRenderFrameLive());
  EXPECT_FALSE(root->child_at(0)->current_frame_host()->IsRenderFrameLive());
  histograms.ExpectUniqueSample("Stability.ChildFrameCrash.TabMarkedForReload",
                                false, 1);
}

IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest,
                       // TODO(crbug.com/40870019): Re-enable this test
                       DISABLED_ChildFrameCrashMetrics_KilledWhileVisible) {
  // Set-up a frame tree that helps verify what the metrics tracks:
  // 1) frames (12 frames are affected if B process gets killed) or
  // 2) crashes (simply 1 crash if B process gets killed)?
  // 3) widgets (10 b widgets and 1 c widget are affected if B is killed,
  //    but a sad frame will appear only in 9 widgets - this excludes
  //    widgets for the b,c(b) part of the frame tree) or
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b(b,c(b)),b,b,b,b,b,b,b,b)"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));
  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();

  std::vector<std::unique_ptr<SadFrameShownObserver>> observers;
  for (size_t i = 0U; i < root->child_count(); i++) {
    // At this point, all b.com subframes should be considered visible.
    RenderFrameProxyHost* proxy_to_parent =
        root->child_at(i)->render_manager()->GetProxyToParent();
    CrossProcessFrameConnector* connector =
        proxy_to_parent->cross_process_frame_connector();
    EXPECT_TRUE(connector->IsVisible())
        << " subframe " << i << " with URL " << root->child_at(i)->current_url()
        << " is hidden";
    observers.push_back(
        std::make_unique<SadFrameShownObserver>(root->child_at(i)));
  }

  // Kill the child frame and wait for each of the subframe FrameTreeNodes to
  // show a sad frame.
  base::HistogramTester histograms;
  RenderProcessHost* child_process =
      root->child_at(0)->current_frame_host()->GetProcess();
  RenderProcessHostWatcher crash_observer(
      child_process, RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
  child_process->Shutdown(0);
  crash_observer.Wait();
  for (size_t i = 0U; i < root->child_count(); i++) {
    SCOPED_TRACE(testing::Message()
                 << " Waiting for sad frame from subframe " << i
                 << " with URL:" << root->child_at(i)->current_url());
    observers[i]->Wait();
  }

  // Verify that the expected metrics got logged.
  histograms.ExpectUniqueSample(
      "Stability.ChildFrameCrash.Visibility",
      CrossProcessFrameConnector::CrashVisibility::kCrashedWhileVisible, 9);

  // Hide and show the web contents and verify that no more metrics got logged.
  web_contents()->UpdateWebContentsVisibility(Visibility::HIDDEN);
  web_contents()->UpdateWebContentsVisibility(Visibility::VISIBLE);
  histograms.ExpectUniqueSample(
      "Stability.ChildFrameCrash.Visibility",
      CrossProcessFrameConnector::CrashVisibility::kCrashedWhileVisible, 9);
}

INSTANTIATE_TEST_SUITE_P(All,
                         SitePerProcessBrowserTestWithoutSadFrameTabReload,
                         testing::ValuesIn(RenderDocumentFeatureLevelValues()));
INSTANTIATE_TEST_SUITE_P(All,
                         SitePerProcessBrowserTestWithSadFrameTabReload,
                         testing::ValuesIn(RenderDocumentFeatureLevelValues()));

}  // namespace content
