// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/web_contents_based_canceller.h"

#include "base/test/test_future.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/test/web_contents_tester.h"
#include "content/test/test_render_view_host.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

class WebContentsBasedCancellerTest
    : public RenderViewHostImplTestHarness,
      public testing::WithParamInterface<
          WebContentsBasedCanceller::CancelCondition> {
 public:
  RenderFrameHostImpl* main_rfh_impl() {
    return static_cast<RenderFrameHostImpl*>(main_rfh());
  }

 protected:
  std::unique_ptr<WebContentsBasedCanceller> CreateWebContentsBasedCanceller() {
    return WebContentsBasedCanceller::Create(main_rfh_impl(), GetParam());
  }
};

INSTANTIATE_TEST_SUITE_P(
    All,
    WebContentsBasedCancellerTest,
    testing::Values(WebContentsBasedCanceller::CancelCondition::kActiveState,
                    WebContentsBasedCanceller::CancelCondition::kVisibility),
    [](const testing::TestParamInfo<WebContentsBasedCancellerTest::ParamType>&
           info) {
      switch (info.param) {
        case WebContentsBasedCanceller::CancelCondition::kActiveState:
          return "ActiveState";
        case WebContentsBasedCanceller::CancelCondition::kVisibility:
          return "Visibility";
      }
    });

TEST_P(WebContentsBasedCancellerTest, CreateActiveVisible) {
  EXPECT_NE(nullptr, CreateWebContentsBasedCanceller());
}

TEST_P(WebContentsBasedCancellerTest, CreateInactive) {
  main_rfh_impl()->SetLifecycleState(
      RenderFrameHostImpl::LifecycleStateImpl::kInBackForwardCache);
  EXPECT_EQ(nullptr, CreateWebContentsBasedCanceller());
}

TEST_P(WebContentsBasedCancellerTest, CreateHidden) {
  web_contents()->WasHidden();
  switch (GetParam()) {
    case WebContentsBasedCanceller::CancelCondition::kActiveState:
      EXPECT_NE(nullptr, CreateWebContentsBasedCanceller());
      break;
    case WebContentsBasedCanceller::CancelCondition::kVisibility:
      EXPECT_EQ(nullptr, CreateWebContentsBasedCanceller());
      break;
  }
}

TEST_P(WebContentsBasedCancellerTest, BecomeInactive) {
  auto ac = CreateWebContentsBasedCanceller();
  base::test::TestFuture<void> future;
  ac->SetCancelCallback(future.GetCallback());
  main_rfh_impl()->SetLifecycleState(
      RenderFrameHostImpl::LifecycleStateImpl::kInBackForwardCache);
  EXPECT_TRUE(future.Wait());
}

TEST_P(WebContentsBasedCancellerTest, BecomeHidden) {
  auto ac = CreateWebContentsBasedCanceller();
  base::test::TestFuture<void> future;
  ac->SetCancelCallback(future.GetCallback());
  web_contents()->WasHidden();
  switch (GetParam()) {
    case WebContentsBasedCanceller::CancelCondition::kActiveState:
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

TEST_P(WebContentsBasedCancellerTest, InactiveBeforeSettingCallback) {
  auto ac = CreateWebContentsBasedCanceller();
  main_rfh_impl()->SetLifecycleState(
      RenderFrameHostImpl::LifecycleStateImpl::kInBackForwardCache);
  base::test::TestFuture<void> future;
  ac->SetCancelCallback(future.GetCallback());
  EXPECT_TRUE(future.IsReady());
}

TEST_P(WebContentsBasedCancellerTest, HiddenBeforeSettingCallback) {
  auto ac = CreateWebContentsBasedCanceller();
  web_contents()->WasHidden();
  base::test::TestFuture<void> future;
  ac->SetCancelCallback(future.GetCallback());
  switch (GetParam()) {
    case WebContentsBasedCanceller::CancelCondition::kActiveState:
      EXPECT_FALSE(future.IsReady());
      break;
    case WebContentsBasedCanceller::CancelCondition::kVisibility:
      EXPECT_TRUE(future.IsReady());
      break;
  }
}

// Tests that destroying does not call the callback.
TEST_P(WebContentsBasedCancellerTest, Destroy) {
  auto ac = CreateWebContentsBasedCanceller();
  base::test::TestFuture<void> future;
  ac->SetCancelCallback(future.GetCallback());
  ac.reset();
  main_rfh_impl()->SetLifecycleState(
      RenderFrameHostImpl::LifecycleStateImpl::kInBackForwardCache);
  EXPECT_FALSE(future.IsReady());
}

}  // namespace content
