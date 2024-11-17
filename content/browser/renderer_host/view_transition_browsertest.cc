// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "cc/test/pixel_comparator.h"
#include "cc/test/pixel_test_utils.h"
#include "components/viz/host/host_frame_sink_manager.h"
#include "content/browser/compositor/surface_utils.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/renderer_host/view_transition_opt_in_state.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/test/back_forward_cache_util.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/shell/browser/shell.h"
#include "content/shell/browser/shell_download_manager_delegate.h"
#include "content/test/content_browser_test_utils_internal.h"
#include "mojo/public/cpp/bindings/sync_call_restrictions.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/default_handlers.h"
#include "services/viz/privileged/mojom/compositing/features.mojom-features.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/features_generated.h"
#include "ui/gfx/geometry/size.h"

namespace content {

class ViewTransitionBrowserTest : public ContentBrowserTest {
 public:
  class TestCondition : public CommitDeferringCondition {
   public:
    TestCondition(NavigationRequest& request, base::RunLoop* run_loop)
        : CommitDeferringCondition(request), run_loop_(run_loop) {}
    ~TestCondition() override = default;

    Result WillCommitNavigation(base::OnceClosure resume) override {
      GetUIThreadTaskRunner()->PostTask(FROM_HERE, run_loop_->QuitClosure());
      return Result::kDefer;
    }

    const char* TraceEventName() const override { return "TestCondition"; }

   private:
    raw_ptr<base::RunLoop> run_loop_;
  };

  ViewTransitionBrowserTest() {
    feature_list_.InitWithFeatures(
        /*enabled_features=*/
        {blink::features::kViewTransitionOnNavigation,
         viz::mojom::EnableVizTestApis},
        /*disabled_features=*/{});
  }

  void SetUpOnMainThread() override {
    ContentBrowserTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
    embedded_test_server()->ServeFilesFromSourceDirectory(
        GetTestDataFilePath());
    net::test_server::RegisterDefaultHandlers(embedded_test_server());
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  void WaitForConditionsDone(NavigationRequest* request) {
    // Inject a condition to know when the VT response has been received but
    // before the NavigationRequest is notified.
    run_loop_ = std::make_unique<base::RunLoop>();
    request->RegisterCommitDeferringConditionForTesting(
        std::make_unique<TestCondition>(*request, run_loop_.get()));
    run_loop_->Run();
  }

  bool HasVTOptIn(RenderFrameHost* rfh) {
    auto* opt_in_state = ViewTransitionOptInState::GetForCurrentDocument(
        static_cast<RenderFrameHostImpl*>(rfh));
    return opt_in_state &&
           opt_in_state->same_origin_opt_in() ==
               blink::mojom::ViewTransitionSameOriginOptIn::kEnabled;
  }

 private:
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<base::RunLoop> run_loop_;
};

IN_PROC_BROWSER_TEST_F(ViewTransitionBrowserTest,
                       NavigationCancelledAfterScreenshot) {
  // Start with a page which has an opt-in for VT.
  GURL test_url(
      embedded_test_server()->GetURL("/view_transitions/basic-vt-opt-in.html"));
  ASSERT_TRUE(NavigateToURL(shell()->web_contents(), test_url));
  WaitForCopyableViewInWebContents(shell()->web_contents());

  TestNavigationManager navigation_manager(shell()->web_contents(), test_url);
  ASSERT_TRUE(
      ExecJs(shell()->web_contents(), "location.href = location.href;"));

  // Wait for response and resume. The navigation should be blocked by the view
  // transition condition.
  ASSERT_TRUE(navigation_manager.WaitForResponse());
  navigation_manager.ResumeNavigation();

  auto* navigation_request =
      NavigationRequest::From(navigation_manager.GetNavigationHandle());
  ASSERT_TRUE(navigation_request);
  ASSERT_TRUE(
      navigation_request->IsCommitDeferringConditionDeferredForTesting());
  ASSERT_FALSE(navigation_request->commit_params().view_transition_state);

  WaitForConditionsDone(navigation_request);
  ASSERT_TRUE(navigation_request->commit_params().view_transition_state);

  mojo::ScopedAllowSyncCallForTesting allow_sync;

  bool has_resources = false;
  GetHostFrameSinkManager()
      ->GetFrameSinkManagerTestApi()
      .HasUnclaimedViewTransitionResources(&has_resources);
  ASSERT_TRUE(has_resources);

  shell()->web_contents()->Stop();
  ASSERT_FALSE(navigation_manager.was_committed());
  GetHostFrameSinkManager()
      ->GetFrameSinkManagerTestApi()
      .HasUnclaimedViewTransitionResources(&has_resources);
  ASSERT_FALSE(has_resources);

  // Ensure the old renderer discards the outgoing transition.
  EXPECT_TRUE(ExecJs(
      shell()->web_contents()->GetPrimaryMainFrame(),
      "(async () => { await document.startViewTransition().ready; })()"));
}

IN_PROC_BROWSER_TEST_F(ViewTransitionBrowserTest,
                       NavigationCancelledBeforeScreenshot) {
  // Start with a page which has an opt-in for VT.
  GURL test_url(
      embedded_test_server()->GetURL("/view_transitions/basic-vt-opt-in.html"));
  ASSERT_TRUE(NavigateToURL(shell()->web_contents(), test_url));
  WaitForCopyableViewInWebContents(shell()->web_contents());

  TestNavigationManager navigation_manager(shell()->web_contents(), test_url);
  ASSERT_TRUE(
      ExecJs(shell()->web_contents(), "location.href = location.href;"));

  // Wait for response and resume. The navigation should be blocked by the view
  // transition condition.
  ASSERT_TRUE(navigation_manager.WaitForResponse());
  navigation_manager.ResumeNavigation();

  auto* navigation_request =
      NavigationRequest::From(navigation_manager.GetNavigationHandle());
  ASSERT_TRUE(navigation_request);
  ASSERT_TRUE(
      navigation_request->IsCommitDeferringConditionDeferredForTesting());
  ASSERT_FALSE(navigation_request->commit_params().view_transition_state);

  // Stop the navigation while the screenshot request is in flight.
  shell()->web_contents()->Stop();
  ASSERT_FALSE(navigation_manager.was_committed());

  // Ensure the old renderer discards the outgoing transition.
  EXPECT_TRUE(ExecJs(
      shell()->web_contents()->GetPrimaryMainFrame(),
      "(async () => { await document.startViewTransition().ready; })()"));
}

IN_PROC_BROWSER_TEST_F(ViewTransitionBrowserTest,
                       OwnershipTransferredToNewRenderer) {
  // Start with a page which has an opt-in for VT.
  GURL test_url(
      embedded_test_server()->GetURL("/view_transitions/basic-vt-opt-in.html"));
  ASSERT_TRUE(NavigateToURL(shell()->web_contents(), test_url));
  WaitForCopyableViewInWebContents(shell()->web_contents());

  TestNavigationManager navigation_manager(shell()->web_contents(), test_url);
  ASSERT_TRUE(
      ExecJs(shell()->web_contents(), "location.href = location.href;"));
  ASSERT_TRUE(navigation_manager.WaitForNavigationFinished());
  ASSERT_TRUE(static_cast<RenderWidgetHostViewBase*>(
                  shell()->web_contents()->GetRenderWidgetHostView())
                  ->HasViewTransitionResourcesForTesting());
}

// Ensure a browser-initiated navigation (i.e. typing URL into omnibox) does
// not trigger a view transitions.
IN_PROC_BROWSER_TEST_F(ViewTransitionBrowserTest,
                       NoOpOnBrowserInitiatedNavigations) {
  // Start with a page which has an opt-in for VT.
  GURL test_url(
      embedded_test_server()->GetURL("/view_transitions/basic-vt-opt-in.html"));
  ASSERT_TRUE(NavigateToURL(shell()->web_contents(), test_url));

  GURL test_url_next(embedded_test_server()->GetURL(
      "/view_transitions/basic-vt-opt-in.html?next"));
  ASSERT_TRUE(NavigateToURL(shell()->web_contents(), test_url_next));
  WaitForCopyableViewInWebContents(shell()->web_contents());

  EXPECT_EQ(false, EvalJs(shell()->web_contents(), "had_incoming_transition"));
}

class ViewTransitionDownloadBrowserTest : public ViewTransitionBrowserTest {
 public:
  void SetUpOnMainThread() override {
    ViewTransitionBrowserTest::SetUpOnMainThread();

    // Set up a test download directory, in order to prevent prompting for
    // handling downloads.
    ASSERT_TRUE(downloads_directory_.CreateUniqueTempDir());
    ShellDownloadManagerDelegate* delegate =
        static_cast<ShellDownloadManagerDelegate*>(
            shell()
                ->web_contents()
                ->GetBrowserContext()
                ->GetDownloadManagerDelegate());
    delegate->SetDownloadBehaviorForTesting(downloads_directory_.GetPath());
  }

 private:
  base::ScopedTempDir downloads_directory_;
};

IN_PROC_BROWSER_TEST_F(ViewTransitionDownloadBrowserTest,
                       NavigationToDownloadLink) {
  GURL test_url(
      embedded_test_server()->GetURL("/view_transitions/basic-vt-opt-in.html"));
  ASSERT_TRUE(NavigateToURL(shell()->web_contents(), test_url));
  WaitForCopyableViewInWebContents(shell()->web_contents());

  GURL download_url(embedded_test_server()->GetURL("/download-test1.lib"));
  TestNavigationManager navigation_manager(shell()->web_contents(),
                                           download_url);
  ASSERT_TRUE(ExecJs(shell()->web_contents(),
                     JsReplace("location.href = $1", download_url)));

  // Wait for response and resume. The navigation should not be blocked by the
  // view transition condition.
  ASSERT_TRUE(navigation_manager.WaitForRequestStart());

  ASSERT_TRUE(HasVTOptIn(shell()->web_contents()->GetPrimaryMainFrame()));
  auto* navigation_request =
      NavigationRequest::From(navigation_manager.GetNavigationHandle());
  ASSERT_EQ(
      shell()->web_contents()->GetPrimaryMainFrame()->GetLastCommittedOrigin(),
      navigation_request->GetTentativeOriginAtRequestTime());

  ASSERT_TRUE(navigation_manager.WaitForNavigationFinished());
  ASSERT_FALSE(navigation_manager.was_committed());
}

class ViewTransitionBrowserTestTraverse
    : public ViewTransitionBrowserTest,
      public testing::WithParamInterface<bool> {
 public:
  bool BFCacheEnabled() const { return GetParam(); }

  bool NavigateBack(GURL back_url, WebContents* contents = nullptr) {
    if (!contents) {
      contents = shell()->web_contents();
    }
    // We need to trigger the navigation *after* executing the script below so
    // the event handlers the script relies on are set before they're dispatched
    // by the navigation.
    //
    // We pass this as a callback to EvalJs so the navigation is initiated
    // before we wait for the script result since it relies on events dispatched
    // during the navigation.
    auto trigger_navigation = base::BindOnce(
        &ViewTransitionBrowserTestTraverse::TriggerBackNavigation,
        base::Unretained(this), back_url, contents);

    auto result =
        EvalJs(contents,
               JsReplace(
                   R"(
    (async () => {
      let navigateFired = false;
      navigation.onnavigate = (event) => {
        navigateFired = (event.navigationType === "traverse");
      };
      let pageswapfired = new Promise((resolve) => {
        onpageswap = (e) => {
          if (!navigateFired || e.viewTransition == null) {
            resolve(null);
            return;
          }
          activation = e.activation;
          resolve(activation);
        };
      });
      let result = await pageswapfired;
      return result != null;
    })();
  )"),
               EXECUTE_SCRIPT_DEFAULT_OPTIONS, ISOLATED_WORLD_ID_GLOBAL,
               std::move(trigger_navigation));
    return result.ExtractBool();
  }

  void TriggerBackNavigation(GURL back_url, WebContents* web_contents) {
    if (BFCacheEnabled()) {
      TestActivationManager manager(web_contents, back_url);
      web_contents->GetController().GoBack();
      manager.WaitForNavigationFinished();
    } else {
      TestNavigationManager manager(web_contents, back_url);
      web_contents->GetController().GoBack();
      ASSERT_TRUE(manager.WaitForNavigationFinished());
    }
  }
};

IN_PROC_BROWSER_TEST_P(ViewTransitionBrowserTestTraverse,
                       NavigateEventFiresBeforeCapture) {
  if (!BFCacheEnabled()) {
    DisableBackForwardCacheForTesting(
        shell()->web_contents(),
        BackForwardCache::DisableForTestingReason::TEST_REQUIRES_NO_CACHING);
  } else if (!base::FeatureList::IsEnabled(features::kBackForwardCache)) {
    GTEST_SKIP();
  }

  GURL test_url(
      embedded_test_server()->GetURL("/view_transitions/basic-vt-opt-in.html"));
  ASSERT_TRUE(NavigateToURL(shell()->web_contents(), test_url));
  GURL second_url(embedded_test_server()->GetURL(
      "/view_transitions/basic-vt-opt-in.html?new"));
  ASSERT_TRUE(NavigateToURL(shell()->web_contents(), second_url));
  WaitForCopyableViewInWebContents(shell()->web_contents());
  auto& nav_controller = static_cast<NavigationControllerImpl&>(
      shell()->web_contents()->GetController());
  ASSERT_TRUE(nav_controller.CanGoBack());
  ASSERT_TRUE(NavigateBack(test_url));
}

// A session restore (e.g. "Duplicate Tab", "Undo Close Tab") uses RESTORE
// navigation types when traversing the session history. Ensure these
// navigations trigger a view transition.
IN_PROC_BROWSER_TEST_P(ViewTransitionBrowserTestTraverse,
                       TransitionOnSessionRestoreTraversal) {
  // A restored session will never have its session history in BFCache so
  // there's no need to run a BFCache version of the test.
  if (BFCacheEnabled()) {
    GTEST_SKIP();
  }

  // Start with a page which has an opt-in for VT.
  GURL url_a(
      embedded_test_server()->GetURL("/view_transitions/basic-vt-opt-in.html"));
  ASSERT_TRUE(NavigateToURL(shell()->web_contents(), url_a));

  // Navigate to another page with an opt-in. (There's no transition due to
  // being browser-initiated)
  GURL url_b(embedded_test_server()->GetURL(
      "/view_transitions/basic-vt-opt-in.html?next"));
  ASSERT_TRUE(NavigateToURL(shell()->web_contents(), url_b));

  // Clone the tab and load the page. Note: the cloned web contents must be put
  // into a window to generate BeginFrames which are required since a view
  // transition will not trigger unless a frame has been generated and the page
  // revealed.
  std::unique_ptr<WebContents> new_tab = shell()->web_contents()->Clone();
  WebContentsImpl* new_tab_impl = static_cast<WebContentsImpl*>(new_tab.get());
  shell()->AddNewContents(nullptr, std::move(new_tab), url_b,
                          WindowOpenDisposition::NEW_FOREGROUND_TAB,
                          blink::mojom::WindowFeatures(), false, nullptr);
  NavigationController& new_controller = new_tab_impl->GetController();

  {
    TestNavigationObserver clone_observer(new_tab_impl);
    new_controller.LoadIfNecessary();
    clone_observer.Wait();
  }

  // Ensure the page has been revealed before navigating back so that a
  // transition will be triggered.
  WaitForCopyableViewInWebContents(new_tab_impl);

  // TODO(crbug.com/331226127) Intentionally ignore the return value as the
  // navigation API (erroneously?) doesn't fire events for restored traversals.
  NavigateBack(url_a, new_tab_impl);

  // Ensure a frame has been generated so that the reveal event would have been
  // fired.
  WaitForCopyableViewInWebContents(new_tab_impl);

  EXPECT_EQ(true, EvalJs(new_tab_impl, "had_incoming_transition"));
}

INSTANTIATE_TEST_SUITE_P(P,
                         ViewTransitionBrowserTestTraverse,
                         ::testing::Bool());

class ViewTransitionCaptureTest
    : public ContentBrowserTest,
      public ::testing::WithParamInterface<std::string> {
 public:
  ViewTransitionCaptureTest() { EnablePixelOutput(); }

  void SetUpOnMainThread() override {
    ContentBrowserTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
    embedded_test_server()->ServeFilesFromSourceDirectory(
        GetTestDataFilePath());
    net::test_server::RegisterDefaultHandlers(embedded_test_server());
    ASSERT_TRUE(embedded_test_server()->Start());
  }

 protected:
  SkBitmap TakeScreenshot() {
    WaitForCopyableViewInWebContents(shell()->web_contents());
    base::test::TestFuture<const SkBitmap&> future_bitmap;
    shell()->web_contents()->GetRenderWidgetHostView()->CopyFromSurface(
        gfx::Rect(), gfx::Size(), future_bitmap.GetCallback());
    return future_bitmap.Take();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_P(ViewTransitionCaptureTest,
                       ViewTransitionNoArtifactDuringCapture) {
  GURL test_url(embedded_test_server()->GetURL(GetParam()));
  auto* web_contents = shell()->web_contents();
  web_contents->Resize({0, 0, 20, 20});
  ASSERT_TRUE(NavigateToURL(web_contents, test_url));
  ASSERT_EQ(EvalJs(web_contents, JsReplace(R"(
            new Promise(resolve => {
              requestAnimationFrame(() => resolve("ok"));
            }))")),
            "ok");
  SkBitmap before_bitmap = TakeScreenshot();

  // Sanity to see that we've captured something.
  ASSERT_NE(before_bitmap.getColor(5, 5), 0u);
  // This starts a view transition with a "hanging" promise that never resolves.
  // When the view-transition callback is called, we resolve the external
  // promise that signals us that it's time to capture.
  ASSERT_EQ(EvalJs(web_contents, JsReplace(R"(
              new Promise(ready_to_capture => {
                document.startViewTransition(() => new Promise(() => {
                    ready_to_capture('ok');
                }));
              }))")),
            "ok");
  auto after_bitmap = TakeScreenshot();
  ASSERT_EQ(before_bitmap.width(), after_bitmap.width());
  ASSERT_EQ(before_bitmap.height(), after_bitmap.height());
  EXPECT_TRUE(cc::MatchesBitmap(before_bitmap, after_bitmap,
                                cc::ExactPixelComparator()));
}

INSTANTIATE_TEST_SUITE_P(
    P,
    ViewTransitionCaptureTest,
    testing::Values("/view_transitions/parent-child.html",
                    "/view_transitions/parent-child-opacity.html"));

}  // namespace content
