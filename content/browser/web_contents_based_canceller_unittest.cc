// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/web_contents_based_canceller.h"

#include <optional>

#include "base/test/test_future.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/test/web_contents_tester.h"
#include "content/test/test_render_view_host.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {

constexpr gfx::Size kTestMinWindowSize(400, 300);

class TestWindowBoundsDelegate : public WebContentsDelegate {
 public:
  explicit TestWindowBoundsDelegate(const gfx::Rect& window_bounds)
      : window_bounds_(window_bounds) {}

  std::optional<gfx::Rect> GetWindowBoundsInScreen() override {
    return window_bounds_;
  }

 private:
  gfx::Rect window_bounds_;
};

}  // namespace

class WebContentsBasedCancellerTest
    : public RenderViewHostImplTestHarness,
      public testing::WithParamInterface<
          WebContentsBasedCanceller::CancelCondition> {
 public:
  void SetUp() override {
    RenderViewHostImplTestHarness::SetUp();
    main_rfh_impl()->GetView()->SetBounds(gfx::Rect(0, 0, 800, 600));
  }

  RenderFrameHostImpl* main_rfh_impl() {
    return static_cast<RenderFrameHostImpl*>(main_rfh());
  }

 protected:
  std::unique_ptr<WebContentsBasedCanceller> CreateWebContentsBasedCanceller() {
    return WebContentsBasedCanceller::Create(main_rfh_impl(), GetParam(),
                                             kTestMinWindowSize);
  }
};

INSTANTIATE_TEST_SUITE_P(
    All,
    WebContentsBasedCancellerTest,
    testing::Values(WebContentsBasedCanceller::CancelCondition::kActiveState,
                    WebContentsBasedCanceller::CancelCondition::kVisibility,
                    WebContentsBasedCanceller::CancelCondition::kWindowSize),
    [](const testing::TestParamInfo<WebContentsBasedCancellerTest::ParamType>&
           info) {
      switch (info.param) {
        case WebContentsBasedCanceller::CancelCondition::kActiveState:
          return "ActiveState";
        case WebContentsBasedCanceller::CancelCondition::kVisibility:
          return "Visibility";
        case WebContentsBasedCanceller::CancelCondition::kWindowSize:
          return "WindowSize";
      }
    });

TEST_P(WebContentsBasedCancellerTest, CreateActiveVisible) {
  EXPECT_NE(nullptr, CreateWebContentsBasedCanceller());
}

TEST_P(WebContentsBasedCancellerTest, CreateInactive) {
  main_rfh_impl()->SetLifecycleState(
      RenderFrameHostLifecycleStateImpl::kInBackForwardCache);
  switch (GetParam()) {
    case WebContentsBasedCanceller::CancelCondition::kActiveState:
    case WebContentsBasedCanceller::CancelCondition::kVisibility:
      EXPECT_EQ(nullptr, CreateWebContentsBasedCanceller());
      break;
    case WebContentsBasedCanceller::CancelCondition::kWindowSize:
      EXPECT_NE(nullptr, CreateWebContentsBasedCanceller());
      break;
  }
}

TEST_P(WebContentsBasedCancellerTest, CreateHidden) {
  web_contents()->WasHidden();
  switch (GetParam()) {
    case WebContentsBasedCanceller::CancelCondition::kActiveState:
    case WebContentsBasedCanceller::CancelCondition::kWindowSize:
      EXPECT_NE(nullptr, CreateWebContentsBasedCanceller());
      break;
    case WebContentsBasedCanceller::CancelCondition::kVisibility:
      EXPECT_EQ(nullptr, CreateWebContentsBasedCanceller());
      break;
  }
}

TEST_P(WebContentsBasedCancellerTest, CreateOccluded) {
  web_contents()->WasOccluded();
  switch (GetParam()) {
    case WebContentsBasedCanceller::CancelCondition::kActiveState:
    case WebContentsBasedCanceller::CancelCondition::kWindowSize:
      EXPECT_NE(nullptr, CreateWebContentsBasedCanceller());
      break;
    case WebContentsBasedCanceller::CancelCondition::kVisibility:
      EXPECT_EQ(nullptr, CreateWebContentsBasedCanceller())
          << "Dialog was allowed for an OCCLUDED WebContents.";
      break;
  }
}

TEST_P(WebContentsBasedCancellerTest, BecomeInactive) {
  auto ac = CreateWebContentsBasedCanceller();
  base::test::TestFuture<void> future;
  ac->SetCancelCallback(future.GetCallback());
  main_rfh_impl()->SetLifecycleState(
      RenderFrameHostLifecycleStateImpl::kInBackForwardCache);
  switch (GetParam()) {
    case WebContentsBasedCanceller::CancelCondition::kActiveState:
    case WebContentsBasedCanceller::CancelCondition::kVisibility:
      EXPECT_TRUE(future.Wait());
      break;
    case WebContentsBasedCanceller::CancelCondition::kWindowSize:
      EXPECT_FALSE(future.IsReady());
      break;
  }
}

TEST_P(WebContentsBasedCancellerTest, BecomeHidden) {
  auto ac = CreateWebContentsBasedCanceller();
  base::test::TestFuture<void> future;
  ac->SetCancelCallback(future.GetCallback());
  web_contents()->WasHidden();
  switch (GetParam()) {
    case WebContentsBasedCanceller::CancelCondition::kActiveState:
    case WebContentsBasedCanceller::CancelCondition::kWindowSize:
      EXPECT_FALSE(future.IsReady());
      break;
    case WebContentsBasedCanceller::CancelCondition::kVisibility:
#if BUILDFLAG(IS_ANDROID)
      // Android sends HIDDEN when picking a file. We should not cancel in this
      // case.
      // TODO(crbug.com/457495639): Figure out how to handle Android.
      EXPECT_FALSE(future.IsReady());
#else
      EXPECT_TRUE(future.IsReady());
#endif
      break;
  }
}

TEST_P(WebContentsBasedCancellerTest, BecomeOccluded) {
  auto ac = CreateWebContentsBasedCanceller();
  ASSERT_NE(nullptr, ac);
  base::test::TestFuture<void> future;
  ac->SetCancelCallback(future.GetCallback());
  web_contents()->WasOccluded();
  switch (GetParam()) {
    case WebContentsBasedCanceller::CancelCondition::kActiveState:
    case WebContentsBasedCanceller::CancelCondition::kWindowSize:
      EXPECT_FALSE(future.IsReady());
      break;
    case WebContentsBasedCanceller::CancelCondition::kVisibility:
#if BUILDFLAG(IS_ANDROID)
      // Android sends HIDDEN when picking a file. We should not cancel in this
      // case.
      // TODO(crbug.com/457495639): Figure out how to handle Android.
      EXPECT_FALSE(future.IsReady());
#else
      EXPECT_TRUE(future.IsReady())
          << "Dialog was NOT cancelled when WebContents became OCCLUDED. "
          << "The file picker remains visible over a foreign foreground "
          << "window, enabling origin spoofing.";
#endif
      break;
  }
}

TEST_P(WebContentsBasedCancellerTest, InactiveBeforeSettingCallback) {
  auto ac = CreateWebContentsBasedCanceller();
  main_rfh_impl()->SetLifecycleState(
      RenderFrameHostLifecycleStateImpl::kInBackForwardCache);
  base::test::TestFuture<void> future;
  ac->SetCancelCallback(future.GetCallback());
  switch (GetParam()) {
    case WebContentsBasedCanceller::CancelCondition::kActiveState:
    case WebContentsBasedCanceller::CancelCondition::kVisibility:
      EXPECT_TRUE(future.IsReady());
      break;
    case WebContentsBasedCanceller::CancelCondition::kWindowSize:
      EXPECT_FALSE(future.IsReady());
      break;
  }
}

TEST_P(WebContentsBasedCancellerTest, HiddenBeforeSettingCallback) {
  auto ac = CreateWebContentsBasedCanceller();
  web_contents()->WasHidden();
  base::test::TestFuture<void> future;
  ac->SetCancelCallback(future.GetCallback());
  switch (GetParam()) {
    case WebContentsBasedCanceller::CancelCondition::kActiveState:
    case WebContentsBasedCanceller::CancelCondition::kWindowSize:
      EXPECT_FALSE(future.IsReady());
      break;
    case WebContentsBasedCanceller::CancelCondition::kVisibility:
      EXPECT_TRUE(future.IsReady());
      break;
  }
}

TEST_P(WebContentsBasedCancellerTest, OccludedBeforeSettingCallback) {
  auto ac = CreateWebContentsBasedCanceller();
  ASSERT_NE(nullptr, ac);
  web_contents()->WasOccluded();
  base::test::TestFuture<void> future;
  ac->SetCancelCallback(future.GetCallback());
  switch (GetParam()) {
    case WebContentsBasedCanceller::CancelCondition::kActiveState:
    case WebContentsBasedCanceller::CancelCondition::kWindowSize:
      EXPECT_FALSE(future.IsReady());
      break;
    case WebContentsBasedCanceller::CancelCondition::kVisibility:
      EXPECT_TRUE(future.IsReady());
      break;
  }
}

TEST_P(WebContentsBasedCancellerTest, CreateSmall) {
  main_rfh_impl()->GetView()->SetBounds(gfx::Rect(0, 0, 100, 100));
  switch (GetParam()) {
    case WebContentsBasedCanceller::CancelCondition::kActiveState:
      EXPECT_NE(nullptr, CreateWebContentsBasedCanceller());
      break;
    case WebContentsBasedCanceller::CancelCondition::kVisibility:
    case WebContentsBasedCanceller::CancelCondition::kWindowSize:
#if BUILDFLAG(IS_ANDROID)
      EXPECT_NE(nullptr, CreateWebContentsBasedCanceller());
#else
      EXPECT_EQ(nullptr, CreateWebContentsBasedCanceller());
#endif
      break;
  }
}

TEST_P(WebContentsBasedCancellerTest, BecomeSmall) {
  auto ac = CreateWebContentsBasedCanceller();
  ASSERT_NE(nullptr, ac);
  base::test::TestFuture<void> future;
  ac->SetCancelCallback(future.GetCallback());
  main_rfh_impl()->GetView()->SetBounds(gfx::Rect(0, 0, 100, 100));
  static_cast<WebContentsObserver*>(ac.get())->PrimaryMainFrameWasResized(true);
  switch (GetParam()) {
    case WebContentsBasedCanceller::CancelCondition::kActiveState:
      EXPECT_FALSE(future.IsReady());
      break;
    case WebContentsBasedCanceller::CancelCondition::kVisibility:
    case WebContentsBasedCanceller::CancelCondition::kWindowSize:
#if BUILDFLAG(IS_ANDROID)
      EXPECT_FALSE(future.IsReady());
#else
      EXPECT_TRUE(future.IsReady());
#endif
      break;
  }
}

TEST_P(WebContentsBasedCancellerTest, SmallBeforeSettingCallback) {
  auto ac = CreateWebContentsBasedCanceller();
  ASSERT_NE(nullptr, ac);
  main_rfh_impl()->GetView()->SetBounds(gfx::Rect(0, 0, 100, 100));
  base::test::TestFuture<void> future;
  ac->SetCancelCallback(future.GetCallback());
  switch (GetParam()) {
    case WebContentsBasedCanceller::CancelCondition::kActiveState:
      EXPECT_FALSE(future.IsReady());
      break;
    case WebContentsBasedCanceller::CancelCondition::kVisibility:
    case WebContentsBasedCanceller::CancelCondition::kWindowSize:
#if BUILDFLAG(IS_ANDROID)
      EXPECT_FALSE(future.IsReady());
#else
      EXPECT_TRUE(future.IsReady());
#endif
      break;
  }
}

#if !BUILDFLAG(IS_ANDROID)
TEST_F(WebContentsBasedCancellerTest, HeightOnlyViolation) {
  main_rfh_impl()->GetView()->SetBounds(gfx::Rect(0, 0, 500, 200));
  EXPECT_TRUE(WebContentsBasedCanceller::IsWindowTooSmall(web_contents(),
                                                          kTestMinWindowSize));
}

TEST_F(WebContentsBasedCancellerTest, WebUIExemptFromSizeCheck) {
  WebContentsTester::For(web_contents())
      ->NavigateAndCommit(GURL("chrome://test"));
  main_rfh_impl()->GetView()->SetBounds(gfx::Rect(0, 0, 100, 100));
  EXPECT_FALSE(WebContentsBasedCanceller::IsWindowTooSmall(web_contents(),
                                                           kTestMinWindowSize));
}

TEST_F(WebContentsBasedCancellerTest,
       NarrowedWebContentsInNormalTopLevelWindowAllowed) {
  TestWindowBoundsDelegate delegate(gfx::Rect(0, 0, 800, 600));
  web_contents()->SetDelegate(&delegate);
  main_rfh_impl()->GetView()->SetBounds(gfx::Rect(0, 0, 200, 600));
  EXPECT_FALSE(WebContentsBasedCanceller::IsWindowTooSmall(web_contents(),
                                                           kTestMinWindowSize));
  web_contents()->SetDelegate(nullptr);
}

TEST_F(WebContentsBasedCancellerTest, EmptyMinWindowSizeAllowsSmallWindow) {
  main_rfh_impl()->GetView()->SetBounds(gfx::Rect(0, 0, 100, 100));
  EXPECT_FALSE(
      WebContentsBasedCanceller::IsWindowTooSmall(web_contents(), gfx::Size()));
}

TEST_F(WebContentsBasedCancellerTest, ZeroSizeWindowIsTooSmall) {
  main_rfh_impl()->GetView()->SetBounds(gfx::Rect());
  EXPECT_TRUE(WebContentsBasedCanceller::IsWindowTooSmall(web_contents(),
                                                          kTestMinWindowSize));
}
#endif  // !BUILDFLAG(IS_ANDROID)

// Tests that destroying does not call the callback.
TEST_P(WebContentsBasedCancellerTest, Destroy) {
  auto ac = CreateWebContentsBasedCanceller();
  base::test::TestFuture<void> future;
  ac->SetCancelCallback(future.GetCallback());
  ac.reset();
  main_rfh_impl()->SetLifecycleState(
      RenderFrameHostLifecycleStateImpl::kInBackForwardCache);
  EXPECT_FALSE(future.IsReady());
}

}  // namespace content
