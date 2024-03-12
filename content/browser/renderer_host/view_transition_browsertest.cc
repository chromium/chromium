// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "components/viz/host/host_frame_sink_manager.h"
#include "content/browser/compositor/surface_utils.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/shell/browser/shell.h"
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

}  // namespace content
