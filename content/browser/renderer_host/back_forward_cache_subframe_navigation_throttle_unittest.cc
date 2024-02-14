// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/back_forward_cache_subframe_navigation_throttle.h"

#include "base/test/bind.h"
#include "content/public/test/test_navigation_throttle.h"
#include "content/test/navigation_simulator_impl.h"
#include "content/test/test_render_view_host.h"
#include "content/test/test_web_contents.h"

namespace content {

class BackForwardCacheSubframeNavigationThrottleTestWithoutUrlLoaderTest
    : public RenderViewHostImplTestHarness,
      public WebContentsObserver {
 protected:
  // Return `NavigationRequest` for no-url-loader navigation. this
  // NavigationRequest hasn't reached "pending commit" stage because we defer
  // the navigation by `TestNavigationThrottle` on `WillCommitWithoutUrlLoader`.
  // This `TestNavigationThrottle` is registered on `DidStartNavigation`.
  NavigationRequest* CreatePausedNavigationRequest(TestRenderFrameHost* rfh) {
    auto navigation =
        NavigationSimulator::CreateRendererInitiated(GURL("about:blank"), rfh);
    navigation->Start();
    return rfh->frame_tree_node()->navigation_request();
  }

  // Return `BackForwardCacheSubframeNavigationThrottle` for subframe
  // navigation.
  std::unique_ptr<BackForwardCacheSubframeNavigationThrottle>
  GetNavigationThrottle() {
    // Create navigation request which hasn't reached commit and get
    // `BackForwardCacheSubframeNavigationThrottle` for this navigation.
    NavigationRequest* request = CreatePausedNavigationRequest(subframe_rfh_);
    return BackForwardCacheSubframeNavigationThrottle::MaybeCreateThrottleFor(
        request);
  }

  // Set lifecycle state of `subframe_rfh` to `kInBackForwardCache`.
  void SetLifecycleStateToInBFCache() {
    // Set the lifecycle state to `kInBackForwardCache`.
    static_cast<RenderFrameHostImpl*>(subframe_rfh_)
        ->SetLifecycleState(
            RenderFrameHostImpl::LifecycleStateImpl::kInBackForwardCache);
    ASSERT_TRUE(static_cast<RenderFrameHostImpl*>(subframe_rfh_)
                    ->IsInLifecycleState(
                        RenderFrameHost::LifecycleState::kInBackForwardCache));
  }

  // Confirm if navigation is resumed when `RenderFrameHostStateChanged` is
  // called depending on `should_resume_be_called`.
  void ConfirmIfResumeIsCalled(NavigationThrottle* throttle,
                               RenderFrameHost::LifecycleState old_state,
                               RenderFrameHost::LifecycleState new_state,
                               bool should_resume_be_called) {
    bool resume_called = false;
    throttle->set_resume_callback_for_testing(base::BindLambdaForTesting(
        [&resume_called]() { resume_called = true; }));
    contents()->RenderFrameHostStateChanged(subframe_rfh_, old_state,
                                            new_state);
    if (should_resume_be_called) {
      EXPECT_TRUE(resume_called);
    } else {
      EXPECT_FALSE(resume_called);
    }
  }

  TestRenderFrameHost* subframe_rfh() { return subframe_rfh_; }

 private:
  void SetUp() override {
    RenderViewHostImplTestHarness::SetUp();
    Observe(contents());
    RenderFrameHostTester::For(main_test_rfh())
        ->InitializeRenderFrameIfNeeded();
    subframe_rfh_ = main_test_rfh()->AppendChild("Child");
  }

  // Defer subframe navigations on `WillCommitWithoutUrlLoader` by
  // `TestNavigationThrottle` so that NavigationRequest created in each test
  // wouldn't commit immediately.
  void DidStartNavigation(NavigationHandle* handle) override {
    auto throttle = std::make_unique<TestNavigationThrottle>(handle);
    throttle->SetResponse(
        TestNavigationThrottle::WILL_COMMIT_WITHOUT_URL_LOADER,
        TestNavigationThrottle::SYNCHRONOUS, NavigationThrottle::DEFER);
    handle->RegisterThrottleForTesting(
        std::unique_ptr<TestNavigationThrottle>(std::move(throttle)));
  }

  raw_ptr<TestRenderFrameHost, DanglingUntriaged> subframe_rfh_;
};

TEST_F(BackForwardCacheSubframeNavigationThrottleTestWithoutUrlLoaderTest,
       CreateThrottleForSubframe) {
  NavigationRequest* request = CreatePausedNavigationRequest(subframe_rfh());
  // Confirm that we can create throttle for subframes.
  EXPECT_NE(nullptr,
            BackForwardCacheSubframeNavigationThrottle::MaybeCreateThrottleFor(
                request));
}

TEST_F(BackForwardCacheSubframeNavigationThrottleTestWithoutUrlLoaderTest,
       DoesNotCreateThrottleForMainFrame) {
  NavigationRequest* request = CreatePausedNavigationRequest(main_test_rfh());
  // Confirm that we never create throttle for main frames.
  EXPECT_EQ(nullptr,
            BackForwardCacheSubframeNavigationThrottle::MaybeCreateThrottleFor(
                request));
}

TEST_F(BackForwardCacheSubframeNavigationThrottleTestWithoutUrlLoaderTest,
       DeferIfInBackForwardCache) {
  // Create NavigationThrottle for subframe navigation and set lifecycle of
  // subframe to `kInBackForwardCache`.
  std::unique_ptr<NavigationThrottle> throttle = GetNavigationThrottle();
  SetLifecycleStateToInBFCache();

  // Confirm this navigation can be deferred.
  EXPECT_EQ(NavigationThrottle::DEFER,
            throttle->WillCommitWithoutUrlLoader().action());
}

TEST_F(BackForwardCacheSubframeNavigationThrottleTestWithoutUrlLoaderTest,
       DoesNotDeferIfNotInBackForwardCache) {
  // Create NavigationThrottle for subframe navigation.
  std::unique_ptr<NavigationThrottle> throttle = GetNavigationThrottle();

  // Confirm this navigation cannot be deferred.
  EXPECT_EQ(NavigationThrottle::PROCEED,
            throttle->WillCommitWithoutUrlLoader().action());
}

TEST_F(BackForwardCacheSubframeNavigationThrottleTestWithoutUrlLoaderTest,
       ResumeNavigationWhenNavigatedBackIfSubframeNavigationWasDeferred) {
  // Create NavigationThrottle for subframe navigation and set lifecycle of
  // subframe to `kInBackForwardCache`.
  std::unique_ptr<NavigationThrottle> throttle = GetNavigationThrottle();
  SetLifecycleStateToInBFCache();

  // Confirm this navigation can be deferred.
  EXPECT_EQ(NavigationThrottle::DEFER,
            throttle->WillCommitWithoutUrlLoader().action());

  // Confirm the navigation is resumed when `subframe_rfh_` is restored from
  // bfcache.
  ConfirmIfResumeIsCalled(throttle.get(),
                          RenderFrameHost::LifecycleState::kInBackForwardCache,
                          RenderFrameHost::LifecycleState::kActive,
                          /*should_resume_be_called=*/true);
}

TEST_F(
    BackForwardCacheSubframeNavigationThrottleTestWithoutUrlLoaderTest,
    DoesNotResumeNavigationWhenNavigatedBackIfSubframeNavigationWasNotDeferred) {
  // Create NavigationThrottle for subframe navigation and set lifecycle of
  // subframe to `kInBackForwardCache`.
  std::unique_ptr<NavigationThrottle> throttle = GetNavigationThrottle();
  SetLifecycleStateToInBFCache();

  // Confirm the navigation is not resumed when `subframe_rfh_` is restored from
  // bfcache but subframe navigation was not deferred.
  ConfirmIfResumeIsCalled(throttle.get(),
                          RenderFrameHost::LifecycleState::kInBackForwardCache,
                          RenderFrameHost::LifecycleState::kActive,
                          /*should_resume_be_called=*/false);
}
}  // namespace content
