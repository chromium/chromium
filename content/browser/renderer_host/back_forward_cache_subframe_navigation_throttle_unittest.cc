// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/back_forward_cache_subframe_navigation_throttle.h"

#include "base/test/bind.h"
#include "content/public/test/test_navigation_throttle.h"
#include "content/test/navigation_simulator_impl.h"
#include "content/test/test_render_view_host.h"
#include "content/test/test_web_contents.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {
namespace {

using testing::IsNull;
using testing::NotNull;

}  // namespace

class BackForwardCacheSubframeNavigationThrottleTestBase
    : public RenderViewHostImplTestHarness,
      public WebContentsObserver {
 protected:
  void SetUp() override {
    RenderViewHostImplTestHarness::SetUp();
    RenderFrameHostTester::For(main_test_rfh())
        ->InitializeRenderFrameIfNeeded();
    subframe_rfh_ = main_test_rfh()->AppendChild("Child");
  }

  // Returns a paused `NavigationRequest` for subframe navigation.
  // The returned request hasn't reached "pending commit" stage.
  // To ensure this behavior, subclass that tests navigation without URLLoader
  // must override `WebContentsObserver::DidStartNavigation` properly.
  NavigationRequest* CreatePausedSubframeNavigationRequest(
      TestRenderFrameHost* rfh,
      const GURL& url) {
    auto navigation = NavigationSimulator::CreateRendererInitiated(url, rfh);
    navigation->Start();
    return rfh->frame_tree_node()->navigation_request();
  }

  // Subclass should override this method to achieve desired behavior. See
  // `CreatePausedSubframeNavigationRequest`.
  void DidStartNavigation(NavigationHandle* handle) override {
    NOTIMPLEMENTED();
  }

  // Returns `BackForwardCacheSubframeNavigationThrottle` for a paused subframe
  // navigation.
  std::unique_ptr<BackForwardCacheSubframeNavigationThrottle>
  GetNavigationThrottleFromPausedSubframeNavigation(const GURL& url) {
    // Create navigation request which hasn't reached commit and get
    // `BackForwardCacheSubframeNavigationThrottle` for this navigation.
    NavigationRequest* request =
        CreatePausedSubframeNavigationRequest(subframe_rfh_, url);
    return BackForwardCacheSubframeNavigationThrottle::MaybeCreateThrottleFor(
        request);
  }

  // Sets lifecycle state of `subframe_rfh` to `kInBackForwardCache`.
  void SetLifecycleStateToInBFCache() {
    // Set the lifecycle state to `kInBackForwardCache`.
    static_cast<RenderFrameHostImpl*>(subframe_rfh_)
        ->SetLifecycleState(
            RenderFrameHostImpl::LifecycleStateImpl::kInBackForwardCache);
    ASSERT_TRUE(static_cast<RenderFrameHostImpl*>(subframe_rfh_)
                    ->IsInLifecycleState(
                        RenderFrameHost::LifecycleState::kInBackForwardCache));
  }

  // Confirms if navigation is resumed when `RenderFrameHostStateChanged` is
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

  virtual GURL GetNavigationTargetURL() = 0;
  TestRenderFrameHost* subframe_rfh() { return subframe_rfh_; }

 private:
  raw_ptr<TestRenderFrameHost, DanglingUntriaged> subframe_rfh_;
};

// A test suite to cover BFCaching a page with subframe navigation without
// URLLoader.
// See also "1. No-URL loader navigations" from the doc below for more details
// https://docs.google.com/document/d/1XLOQuHjCVmBfXAJhgrASkVyrHAK_DGmcYPyrTnOrSPM/edit?resourcekey=0-uNz75Ux7INdCLhj2FWVILg&tab=t.0#heading=h.uagir4pgy4kp
class BackForwardCacheSubframeNavigationThrottleWithoutUrlLoaderTest
    : public BackForwardCacheSubframeNavigationThrottleTestBase {
 protected:
  void SetUp() override {
    BackForwardCacheSubframeNavigationThrottleTestBase::SetUp();
    Observe(contents());
  }

  // Defers subframe navigations on `WillCommitWithoutUrlLoader` by
  // registering a `TestNavigationThrottle`, so that NavigationRequest created
  // in each test from `CreatePausedSubframeNavigationRequest` wouldn't commit
  // immediately.
  void DidStartNavigation(NavigationHandle* handle) override {
    auto throttle = std::make_unique<TestNavigationThrottle>(handle);
    throttle->SetResponse(
        TestNavigationThrottle::WILL_COMMIT_WITHOUT_URL_LOADER,
        TestNavigationThrottle::SYNCHRONOUS, NavigationThrottle::DEFER);
    handle->RegisterThrottleForTesting(
        std::unique_ptr<TestNavigationThrottle>(std::move(throttle)));
  }

  GURL GetNavigationTargetURL() override {
    return GURL(kSubframeNavigateWithoutURLLoaderURL);
  }

 private:
  static constexpr char kSubframeNavigateWithoutURLLoaderURL[] = "about:blank";
};

TEST_F(BackForwardCacheSubframeNavigationThrottleWithoutUrlLoaderTest,
       CreateThrottleForSubframe) {
  NavigationRequest* request = CreatePausedSubframeNavigationRequest(
      subframe_rfh(), GetNavigationTargetURL());

  // Confirm that we can create throttle for subframes.
  EXPECT_THAT(
      BackForwardCacheSubframeNavigationThrottle::MaybeCreateThrottleFor(
          request),
      NotNull());
}

TEST_F(BackForwardCacheSubframeNavigationThrottleWithoutUrlLoaderTest,
       DoesNotCreateThrottleForMainFrame) {
  NavigationRequest* request = CreatePausedSubframeNavigationRequest(
      main_test_rfh(), GetNavigationTargetURL());

  // Confirm that we never create throttle for main frames.
  EXPECT_THAT(
      BackForwardCacheSubframeNavigationThrottle::MaybeCreateThrottleFor(
          request),
      IsNull());
}

TEST_F(BackForwardCacheSubframeNavigationThrottleWithoutUrlLoaderTest,
       DeferIfInBackForwardCache) {
  // Create NavigationThrottle for subframe navigation without URL loader and
  // set lifecycle of subframe to `kInBackForwardCache`.
  std::unique_ptr<NavigationThrottle> throttle =
      GetNavigationThrottleFromPausedSubframeNavigation(
          GetNavigationTargetURL());
  SetLifecycleStateToInBFCache();

  // Confirm this navigation can be deferred.
  EXPECT_EQ(NavigationThrottle::DEFER,
            throttle->WillCommitWithoutUrlLoader().action());
}

TEST_F(BackForwardCacheSubframeNavigationThrottleWithoutUrlLoaderTest,
       DoesNotDeferIfNotInBackForwardCache) {
  // Create NavigationThrottle for subframe navigation without URL loader.
  std::unique_ptr<NavigationThrottle> throttle =
      GetNavigationThrottleFromPausedSubframeNavigation(
          GetNavigationTargetURL());

  // Confirm this navigation cannot be deferred.
  EXPECT_EQ(NavigationThrottle::PROCEED,
            throttle->WillCommitWithoutUrlLoader().action());
}

TEST_F(BackForwardCacheSubframeNavigationThrottleWithoutUrlLoaderTest,
       ResumeNavigationWhenNavigatedBackIfSubframeNavigationWasDeferred) {
  // Create NavigationThrottle for subframe navigation without URL loader and
  // set lifecycle of subframe to `kInBackForwardCache`.
  std::unique_ptr<NavigationThrottle> throttle =
      GetNavigationThrottleFromPausedSubframeNavigation(
          GetNavigationTargetURL());
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
    BackForwardCacheSubframeNavigationThrottleWithoutUrlLoaderTest,
    DoesNotResumeNavigationWhenNavigatedBackIfSubframeNavigationWasNotDeferred) {
  // Create NavigationThrottle for subframe navigation without URL loader and
  // set lifecycle of subframe to `kInBackForwardCache`.
  std::unique_ptr<NavigationThrottle> throttle =
      GetNavigationThrottleFromPausedSubframeNavigation(
          GetNavigationTargetURL());
  SetLifecycleStateToInBFCache();

  // Confirm the navigation is not resumed when `subframe_rfh_` is restored
  // from bfcache, because the subframe navigation never got deferred in the
  // first place (since it hasn't called WillStartRequest yet)
  ConfirmIfResumeIsCalled(throttle.get(),
                          RenderFrameHost::LifecycleState::kInBackForwardCache,
                          RenderFrameHost::LifecycleState::kActive,
                          /*should_resume_be_called=*/false);
}

// A test suite to cover BFCaching a page with subframe navigation involving
// URLLoader.
// See also "2. URL loader navigations" from the doc below for more details
// https://docs.google.com/document/d/1XLOQuHjCVmBfXAJhgrASkVyrHAK_DGmcYPyrTnOrSPM/edit?resourcekey=0-uNz75Ux7INdCLhj2FWVILg&tab=t.0#heading=h.uagir4pgy4kp
class BackForwardCacheSubframeNavigationThrottleTest
    : public BackForwardCacheSubframeNavigationThrottleTestBase {
 protected:
  void DidStartNavigation(NavigationHandle* handle) override {}

  GURL GetNavigationTargetURL() override {
    return GURL(kSubframeNavigateWithURLLoaderURL);
  }

 private:
  static constexpr char kSubframeNavigateWithURLLoaderURL[] =
      "http://example.com";
};

// Tests a subframe navigation with URL loader will be deferred by
// `BackForwardCacheSubframeNavigationThrottle::WillStartRequest()` if the
// subframe's lifecycle state is `kInBackForwardCache`.
TEST_F(BackForwardCacheSubframeNavigationThrottleTest,
       DeferInWillStartRequestIfInBFCache) {
  // Create NavigationThrottle for subframe navigation with URL loader and set
  // lifecycle of subframe to `kInBackForwardCache`.
  std::unique_ptr<NavigationThrottle> throttle =
      GetNavigationThrottleFromPausedSubframeNavigation(
          GetNavigationTargetURL());
  SetLifecycleStateToInBFCache();

  // Verify this navigation is deferred.
  EXPECT_EQ(NavigationThrottle::DEFER, throttle->WillStartRequest().action());
}

// Tests a subframe navigation with URL loader will not be deferred by
// `BackForwardCacheSubframeNavigationThrottle::WillStartRequest()` if the
// subframe's lifecycle state is not `kInBackForwardCache`.
TEST_F(BackForwardCacheSubframeNavigationThrottleTest,
       DoesNotDeferInWillStartRequestIfNotInBFCache) {
  // Create NavigationThrottle for subframe navigation with URL loader and set
  // lifecycle of subframe to `kInBackForwardCache`.
  std::unique_ptr<NavigationThrottle> throttle =
      GetNavigationThrottleFromPausedSubframeNavigation(
          GetNavigationTargetURL());

  // Verify this navigation is not deferred.
  EXPECT_EQ(NavigationThrottle::PROCEED, throttle->WillStartRequest().action());
}

// Tests a subframe navigation with URL loader will not be deferred by
// `BackForwardCacheSubframeNavigationThrottle::WillProcessResponse()` if the
// subframe's lifecycle state is not `kInBackForwardCache`.
// Note that it is not allowed to call WillProcessResponse() along with the
// `kInBackForwardCache` state.
TEST_F(BackForwardCacheSubframeNavigationThrottleTest,
       DoesNotDeferInWillProcessResponseIfNotInBFCache) {
  // Create NavigationThrottle for subframe navigation with URL loader.
  std::unique_ptr<NavigationThrottle> throttle =
      GetNavigationThrottleFromPausedSubframeNavigation(
          GetNavigationTargetURL());

  // Verify this navigation is not deferred.
  EXPECT_EQ(NavigationThrottle::PROCEED,
            throttle->WillProcessResponse().action());
}

// Tests a subframe navigation with URL loader will not be deferred by
// `BackForwardCacheSubframeNavigationThrottle::WillRedirectRequest()` if the
// subframe's lifecycle state is not `kInBackForwardCache`.
// Note that it is not allowed to call WillRedirectRequest() along with the
// `kInBackForwardCache` state.
TEST_F(BackForwardCacheSubframeNavigationThrottleTest,
       DoesNotDeferInWillRedirectRequestIfNotInBFCache) {
  // Create a NavigationThrottle for subframe navigation with URL loader.
  std::unique_ptr<NavigationThrottle> throttle =
      GetNavigationThrottleFromPausedSubframeNavigation(
          GetNavigationTargetURL());

  // Verify this navigation is not deferred.
  EXPECT_EQ(NavigationThrottle::PROCEED,
            throttle->WillRedirectRequest().action());
}

// Tests a subframe navigation with URL loader will not be deferred by
// `BackForwardCacheSubframeNavigationThrottle::WillFailRequest()` if the
// subframe's lifecycle state is not `kInBackForwardCache`.
// Note that it is not allowed to call WillFailRequest() along with the
// `kInBackForwardCache` state.
TEST_F(BackForwardCacheSubframeNavigationThrottleTest,
       DoesNotDeferInWillFailRequestIfNotInBFCache) {
  // Create NavigationThrottle for subframe navigation with URL loader.
  std::unique_ptr<NavigationThrottle> throttle =
      GetNavigationThrottleFromPausedSubframeNavigation(
          GetNavigationTargetURL());

  // Verify this navigation is not deferred.
  EXPECT_EQ(NavigationThrottle::PROCEED, throttle->WillFailRequest().action());
}

// A test suite to cover back navigation to a BFCached page with subframe
// navigation involving URLLoader.
class BackForwardCacheSubframeNavigationThrottleAndNavigateBackTest
    : public BackForwardCacheSubframeNavigationThrottleTest,
      public testing::WithParamInterface<bool> {
 protected:
  bool IsSubframeNavigationDeferred() const { return GetParam(); }
};

INSTANTIATE_TEST_SUITE_P(
    All,
    BackForwardCacheSubframeNavigationThrottleAndNavigateBackTest,
    ::testing::Values(false, true),
    [](const testing::TestParamInfo<
        BackForwardCacheSubframeNavigationThrottleAndNavigateBackTest::
            ParamType>& info) {
      return info.param ? "Deferred" : "NotDeferred";
    });

TEST_P(BackForwardCacheSubframeNavigationThrottleAndNavigateBackTest,
       NavigateBackToBFCachedPage) {
  // Create NavigationThrottle for subframe navigation with URL loader and set
  // lifecycle of subframe to `kInBackForwardCache`.
  std::unique_ptr<NavigationThrottle> throttle =
      GetNavigationThrottleFromPausedSubframeNavigation(
          GetNavigationTargetURL());
  SetLifecycleStateToInBFCache();

  // Defer the subframe navigation as specified by the test.
  if (IsSubframeNavigationDeferred()) {
    EXPECT_EQ(NavigationThrottle::DEFER, throttle->WillStartRequest().action());
  }

  // Confirm the navigation is resumed when `subframe_rfh_` is restored from
  // bfcache if it was deferred.
  ConfirmIfResumeIsCalled(
      throttle.get(), RenderFrameHost::LifecycleState::kInBackForwardCache,
      RenderFrameHost::LifecycleState::kActive,
      /*should_resume_be_called=*/IsSubframeNavigationDeferred());
}

}  // namespace content
