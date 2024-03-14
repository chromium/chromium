// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "components/viz/host/host_frame_sink_manager.h"
#include "content/browser/compositor/surface_utils.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/public/test/back_forward_cache_util.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/shell/browser/shell.h"
#include "content/test/content_browser_test_utils_internal.h"
#include "mojo/public/cpp/bindings/sync_call_restrictions.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/default_handlers.h"
#include "third_party/blink/public/common/features.h"

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

   private:
    raw_ptr<base::RunLoop> run_loop_;
  };

  ViewTransitionBrowserTest() {
    feature_list_.InitWithFeatures(
        /*enabled_features=*/{blink::features::kViewTransitionOnNavigation},
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

  ASSERT_TRUE(
      GetHostFrameSinkManager()->HasUnclaimedViewTransitionResourcesForTest());

  shell()->web_contents()->Stop();
  ASSERT_FALSE(navigation_manager.was_committed());
  ASSERT_FALSE(
      GetHostFrameSinkManager()->HasUnclaimedViewTransitionResourcesForTest());
}

IN_PROC_BROWSER_TEST_F(ViewTransitionBrowserTest,
                       OwnershipTransferredToNewRenderer) {
  // Start with a page which has an opt-in for VT.
  GURL test_url(
      embedded_test_server()->GetURL("/view_transitions/basic-vt-opt-in.html"));
  ASSERT_TRUE(NavigateToURL(shell()->web_contents(), test_url));

  TestNavigationManager navigation_manager(shell()->web_contents(), test_url);
  ASSERT_TRUE(
      ExecJs(shell()->web_contents(), "location.href = location.href;"));
  ASSERT_TRUE(navigation_manager.WaitForNavigationFinished());
  ASSERT_TRUE(static_cast<RenderWidgetHostViewBase*>(
                  shell()->web_contents()->GetRenderWidgetHostView())
                  ->HasViewTransitionResourcesForTesting());
}

class ViewTransitionBrowserTestTraverse
    : public ViewTransitionBrowserTest,
      public testing::WithParamInterface<bool> {
 public:
  bool BFCacheEnabled() const { return GetParam(); }

  bool NavigateBack(GURL back_url) {
    // We need to trigger the navigation *after* executing the script below so
    // the event handlers the script relies on are set before they're dispatched
    // by the navigation.
    //
    // We pass this as a callback to EvalJs so the navigation is initiated
    // before we wait for the script result since it relies on events dispatched
    // during the navigation.
    auto trigger_navigation = base::BindOnce(
        &ViewTransitionBrowserTestTraverse::TriggerBackNavigation,
        base::Unretained(this), back_url);

    auto result =
        EvalJs(shell()->web_contents(),
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

  void TriggerBackNavigation(GURL back_url) {
    if (BFCacheEnabled()) {
      TestActivationManager manager(shell()->web_contents(), back_url);
      shell()->web_contents()->GetController().GoBack();
      manager.WaitForNavigationFinished();
    } else {
      TestNavigationManager manager(shell()->web_contents(), back_url);
      shell()->web_contents()->GetController().GoBack();
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

INSTANTIATE_TEST_SUITE_P(P,
                         ViewTransitionBrowserTestTraverse,
                         ::testing::Bool());

}  // namespace content
