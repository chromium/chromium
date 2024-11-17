// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/display_cutout/safe_area_insets_host_impl.h"

#include <optional>

#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/common/features.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_renderer_host.h"
#include "content/test/test_web_contents.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace content {

class TestSafeAreaInsetsHostImpl : public SafeAreaInsetsHostImpl {
 public:
  explicit TestSafeAreaInsetsHostImpl(WebContentsImpl* web_contents_impl)
      : SafeAreaInsetsHostImpl(web_contents_impl) {}

  void ResetSafeAreaTracking() {
    latest_safe_area_insets_ = std::nullopt;
    previous_safe_area_insets_ = std::nullopt;
    latest_rfh_ = std::nullopt;
    previous_rfh_ = std::nullopt;
  }

  void ResetSendSafeAreaToFrameCallCount() {
    send_safe_area_to_frame_call_count_ = 0;
  }

  void SetHasSentNonZeroInsets(bool has_sent_non_zero_insets) {
    has_sent_non_zero_insets_ = has_sent_non_zero_insets;
  }

  // Override to allow test access.
  void ViewportFitChangedForFrame(RenderFrameHost* rfh,
                                  blink::mojom::ViewportFit value) override {
    SafeAreaInsetsHostImpl::ViewportFitChangedForFrame(rfh, value);
  }

  using SafeAreaInsetsHostImpl::GetValueOrDefault;
  using SafeAreaInsetsHostImpl::SetViewportFitValue;

  bool did_send_safe_area() { return latest_safe_area_insets_.has_value(); }
  gfx::Insets latest_safe_area_insets() {
    return latest_safe_area_insets_.value();
  }
  gfx::Insets previous_safe_area_insets() {
    return previous_safe_area_insets_.value();
  }
  RenderFrameHost* latest_rfh() { return latest_rfh_.value(); }
  RenderFrameHost* previous_rfh() { return previous_rfh_.value(); }
  int send_safe_area_to_frame_call_count() {
    return send_safe_area_to_frame_call_count_;
  }

  RenderFrameHost* active_rfh() { return ActiveRenderFrameHost(); }

 protected:
  // Send the safe area insets to a `RenderFrameHost`.
  void SendSafeAreaToFrame(RenderFrameHost* rfh, gfx::Insets insets) override {
    previous_safe_area_insets_ = latest_safe_area_insets_;
    latest_safe_area_insets_ = insets;
    previous_rfh_ = latest_rfh_;
    latest_rfh_ = rfh;
    send_safe_area_to_frame_call_count_++;
    SafeAreaInsetsHostImpl::SendSafeAreaToFrame(rfh, insets);
  }

 private:
  std::optional<gfx::Insets> latest_safe_area_insets_;
  std::optional<gfx::Insets> previous_safe_area_insets_;
  std::optional<RenderFrameHost*> latest_rfh_;
  std::optional<RenderFrameHost*> previous_rfh_;
  int send_safe_area_to_frame_call_count_ = 0;
};

class SafeAreaInsetsHostImplTest : public RenderViewHostTestHarness {
 protected:
  void SetUp() override {
    RenderViewHostTestHarness::SetUp();
    SetContents(CreateTestWebContents());

    std::unique_ptr<TestSafeAreaInsetsHostImpl>
        test_safe_area_insets_host_impl =
            absl::make_unique<TestSafeAreaInsetsHostImpl>(test_web_contents());
    test_safe_area_insets_host_ = raw_ptr<TestSafeAreaInsetsHostImpl>(
        test_safe_area_insets_host_impl.get());
    test_web_contents()->SetSafeAreaInsetsHost(
        std::move(test_safe_area_insets_host_impl));
  }

  TestWebContents* test_web_contents() const {
    return static_cast<TestWebContents*>(web_contents());
  }

  TestSafeAreaInsetsHostImpl* test_safe_area_insets_host() const {
    return test_safe_area_insets_host_;
  }

  void ResetSafeAreaTracking() {
    test_safe_area_insets_host()->ResetSafeAreaTracking();
  }

  void ResetSendSafeAreaToFrameCallCount() {
    test_safe_area_insets_host()->ResetSendSafeAreaToFrameCallCount();
  }

  void SetHasSentNonZeroInsets(bool has_sent_non_zero_insets) {
    test_safe_area_insets_host()->SetHasSentNonZeroInsets(
        has_sent_non_zero_insets);
  }

  blink::mojom::ViewportFit GetValueOrDefault() {
    return test_safe_area_insets_host()->GetValueOrDefault(main_rfh());
  }

  void SetViewportFitValue(blink::mojom::ViewportFit value) {
    return test_safe_area_insets_host()->SetViewportFitValue(main_rfh(), value);
  }

  void NavigateToCover() {
    FocusWebContentsOnMainFrame();
    NavigateAndCommit(GURL("https://www.viewportFitCover.com"));
    test_safe_area_insets_host()->ViewportFitChangedForFrame(
        main_rfh(), blink::mojom::ViewportFit::kCover);
    // Simulate window insets changing, e.g. java's
    // DisplayCutoutController#onSafeAreaChanged notified from InsetObserver.
    test_web_contents()->SetDisplayCutoutSafeArea(gfx::Insets(42));
  }

  void NavigateToAuto() {
    FocusWebContentsOnMainFrame();
    NavigateAndCommit(GURL("https://www.viewportFitAuto.com"));
    test_safe_area_insets_host()->ViewportFitChangedForFrame(
        main_rfh(), blink::mojom::ViewportFit::kAuto);
    test_web_contents()->SetDisplayCutoutSafeArea(gfx::Insets(0));
  }

  void ExpectAuto() {
    EXPECT_EQ(blink::mojom::ViewportFit::kAuto, GetValueOrDefault())
        << "Not in Auto? Expected the main_rfh() to have kAuto in UserData, "
           "but it is not!";
  }

  void ExpectCover() {
    EXPECT_EQ(blink::mojom::ViewportFit::kCover, GetValueOrDefault())
        << "Not in Cover? Expected the main_rfh() to have kCover in UserData, "
           "but it is not!";
  }

 private:
  raw_ptr<TestSafeAreaInsetsHostImpl> test_safe_area_insets_host_;
};

TEST_F(SafeAreaInsetsHostImplTest, SetDisplayCutoutSafeAreaRedundantInsets) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kDrawCutoutEdgeToEdge},
      /*disabled_features=*/{});

  ResetSafeAreaTracking();
  FocusWebContentsOnMainFrame();
  NavigateAndCommit(GURL("https://www.viewportFitCover.com"));
  test_safe_area_insets_host()->ViewportFitChangedForFrame(
      main_rfh(), blink::mojom::ViewportFit::kCover);

  ResetSendSafeAreaToFrameCallCount();
  // Simulate window insets changing, e.g. java's
  // DisplayCutoutController#onSafeAreaChanged notified from InsetObserver.
  test_web_contents()->SetDisplayCutoutSafeArea(gfx::Insets::TLBR(42, 0, 0, 0));
  EXPECT_EQ(1,
            test_safe_area_insets_host()->send_safe_area_to_frame_call_count())
      << "New insets should have been sent to the frame.";

  ResetSendSafeAreaToFrameCallCount();
  test_web_contents()->SetDisplayCutoutSafeArea(
      gfx::Insets::TLBR(42, 0, 60, 0));
  test_web_contents()->SetDisplayCutoutSafeArea(
      gfx::Insets::TLBR(42, 0, 60, 0));
  EXPECT_EQ(1,
            test_safe_area_insets_host()->send_safe_area_to_frame_call_count())
      << "Only one new set of insets should have been sent to the frame.";

  ResetSendSafeAreaToFrameCallCount();
  test_web_contents()->SetDisplayCutoutSafeArea(gfx::Insets::TLBR(42, 0, 0, 0));
  test_web_contents()->SetDisplayCutoutSafeArea(gfx::Insets::TLBR(42, 0, 0, 0));
  test_web_contents()->SetDisplayCutoutSafeArea(gfx::Insets::TLBR(42, 0, 0, 0));
  EXPECT_EQ(1,
            test_safe_area_insets_host()->send_safe_area_to_frame_call_count())
      << "Only one new set of insets should have been sent to the frame.";
}

TEST_F(SafeAreaInsetsHostImplTest, AutoToCover) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kDrawCutoutEdgeToEdge},
      /*disabled_features=*/{});

  // Redundant zero insets are not sent if no non-zero insets have been sent
  // previously. Navigate to cover to send non-zero insets such that no future
  // inset updates will be skipped.
  NavigateToCover();

  ResetSafeAreaTracking();
  NavigateToAuto();
  ExpectAuto();
  EXPECT_TRUE(test_safe_area_insets_host()->did_send_safe_area())
      << "The Insets should always be sent when they change.";
  EXPECT_EQ(0, test_safe_area_insets_host()->latest_safe_area_insets().top())
      << "No Display Cutout, so the top inset should have been zero";

  ResetSafeAreaTracking();
  NavigateToCover();
  ExpectCover();
  EXPECT_TRUE(test_safe_area_insets_host()->did_send_safe_area())
      << "The Insets should always be sent when they change.";
  EXPECT_EQ(42, test_safe_area_insets_host()->latest_safe_area_insets().top())
      << "The Display Cutout should have caused a non-zero top inset";
}

TEST_F(SafeAreaInsetsHostImplTest, CoverToAuto) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kDrawCutoutEdgeToEdge},
      /*disabled_features=*/{});

  ResetSafeAreaTracking();
  NavigateToCover();
  ExpectCover();
  EXPECT_TRUE(test_safe_area_insets_host()->did_send_safe_area())
      << "The Insets should always be sent when they change.";
  EXPECT_EQ(42, test_safe_area_insets_host()->latest_safe_area_insets().top())
      << "The Display Cutout should have caused a non-zero top inset";

  ResetSafeAreaTracking();
  NavigateToAuto();
  ExpectAuto();
  EXPECT_TRUE(test_safe_area_insets_host()->did_send_safe_area())
      << "The Insets should always be sent when they change.";
  EXPECT_EQ(0, test_safe_area_insets_host()->latest_safe_area_insets().top())
      << "No Display Cutout, so the top inset should have been zero";
}

TEST_F(SafeAreaInsetsHostImplTest, SetViewportFitValue) {
  SetViewportFitValue(blink::mojom::ViewportFit::kAuto);
  EXPECT_EQ(blink::mojom::ViewportFit::kAuto, GetValueOrDefault());
  SetViewportFitValue(blink::mojom::ViewportFit::kCover);
  EXPECT_EQ(blink::mojom::ViewportFit::kCover, GetValueOrDefault());
  // Setting to a default value (kAuto) after a non default value may be
  // optimized to not store anything, but we do not test that directly.
  SetViewportFitValue(blink::mojom::ViewportFit::kAuto);
  EXPECT_EQ(blink::mojom::ViewportFit::kAuto, GetValueOrDefault());
}

TEST_F(SafeAreaInsetsHostImplTest, GetValueOrDefault_ExpiredRfh) {
  base::WeakPtr<RenderFrameHostImpl> null_rfh;
  EXPECT_EQ(blink::mojom::ViewportFit::kAuto,
            test_safe_area_insets_host()->GetValueOrDefault(null_rfh.get()))
      << "Passing in a null pointer should return kAuto instead of crashing.";
}

TEST_F(SafeAreaInsetsHostImplTest, NavigateNoViewportFitChange) {
  // Redundant zero insets are not sent if no non-zero insets have been sent
  // previously. Navigate to cover to send non-zero insets such that no future
  // inset updates will be skipped.
  NavigateToCover();

  ResetSafeAreaTracking();
  ResetSendSafeAreaToFrameCallCount();

  NavigateAndCommit(GURL("https://www.test-site-a.com"));
  EXPECT_EQ(1,
            test_safe_area_insets_host()->send_safe_area_to_frame_call_count())
      << "Navigating to a new url without a change in viewport-fit should only "
         "trigger one update to safe-area-insets.";
  ResetSendSafeAreaToFrameCallCount();

  NavigateAndCommit(GURL("https://www.test-site-b.com"));
  EXPECT_EQ(1,
            test_safe_area_insets_host()->send_safe_area_to_frame_call_count())
      << "Navigating to a new url without a change in viewport-fit should only "
         "trigger one update to safe-area-insets.";
  ResetSendSafeAreaToFrameCallCount();

  NavigateAndCommit(GURL("https://www.test-site-c.com"));
  EXPECT_EQ(1,
            test_safe_area_insets_host()->send_safe_area_to_frame_call_count())
      << "Navigating to a new url without a change in viewport-fit should only "
         "trigger one update to safe-area-insets.";
}

TEST_F(SafeAreaInsetsHostImplTest, NavigateOnlyZeroInsets) {
  SetHasSentNonZeroInsets(false);
  ResetSafeAreaTracking();
  ResetSendSafeAreaToFrameCallCount();

  NavigateAndCommit(GURL("https://www.test-site-a.com"));
  NavigateAndCommit(GURL("https://www.test-site-b.com"));
  NavigateAndCommit(GURL("https://www.test-site-c.com"));
  test_web_contents()->SetDisplayCutoutSafeArea(gfx::Insets(0));

  EXPECT_EQ(0,
            test_safe_area_insets_host()->send_safe_area_to_frame_call_count())
      << "If no non-zero insets have been sent, updates sending zero insets "
         "should be skipped.";
}

TEST_F(SafeAreaInsetsHostImplTest, ActiveFrameInFullscreen) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kDrawCutoutEdgeToEdge},
      /*disabled_features=*/{});

  ResetSafeAreaTracking();
  NavigateToCover();
  ExpectCover();

  auto* subframe = static_cast<RenderFrameHostImpl*>(
      RenderFrameHostTester::For(main_rfh())->AppendChild("subframe"));
  test_safe_area_insets_host()->DidAcquireFullscreen(subframe);

  EXPECT_EQ(main_rfh(), test_safe_area_insets_host()->previous_rfh())
      << "The main frame should have been previously updated.";
  EXPECT_EQ(gfx::Insets(),
            test_safe_area_insets_host()->previous_safe_area_insets())
      << "The main frame should have had its insets cleared.";
  EXPECT_EQ(subframe, test_safe_area_insets_host()->active_rfh())
      << "The fullscreen subframe should be the new active frame.";
  EXPECT_EQ(subframe, test_safe_area_insets_host()->latest_rfh())
      << "The fullscreen subframe should be the latest frame to have been "
         "updated.";
  EXPECT_EQ(gfx::Insets(42),
            test_safe_area_insets_host()->latest_safe_area_insets())
      << "The Display Cutout should have caused a non-zero top inset";

  // Exit fullscreen from sub frame.
  ResetSafeAreaTracking();
  test_safe_area_insets_host()->DidExitFullscreen();

  EXPECT_EQ(subframe, test_safe_area_insets_host()->previous_rfh())
      << "The fullscreen subframe should have been previously updated.";
  EXPECT_EQ(gfx::Insets(),
            test_safe_area_insets_host()->previous_safe_area_insets())
      << "The fullscreen subframe should have had its insets cleared.";
  EXPECT_EQ(main_rfh(), test_safe_area_insets_host()->active_rfh())
      << "The main frame should be the new active frame.";
  EXPECT_EQ(main_rfh(), test_safe_area_insets_host()->latest_rfh())
      << "The main frame should be the latest frame to have been updated.";
  EXPECT_EQ(gfx::Insets(42),
            test_safe_area_insets_host()->latest_safe_area_insets())
      << "The Display Cutout should have caused a non-zero top inset";
}

}  // namespace content
