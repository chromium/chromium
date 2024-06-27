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

  void ResetSafeAreaTracking() { safe_area_insets_ = std::nullopt; }

  // Override to allow test access.
  void ViewportFitChangedForFrame(RenderFrameHost* rfh,
                                  blink::mojom::ViewportFit value) override {
    SafeAreaInsetsHostImpl::ViewportFitChangedForFrame(rfh, value);
  }

  using SafeAreaInsetsHostImpl::GetValueOrDefault;
  using SafeAreaInsetsHostImpl::SetViewportFitValue;

  bool did_send_safe_area() { return safe_area_insets_.has_value(); }
  gfx::Insets safe_area_insets() { return safe_area_insets_.value(); }

  RenderFrameHost* active_rfh() { return ActiveRenderFrameHost(); }

 protected:
  // Send the safe area insets to a `RenderFrameHost`.
  void SendSafeAreaToFrame(RenderFrameHost* rfh, gfx::Insets insets) override {
    safe_area_insets_ = insets;
    SafeAreaInsetsHostImpl::SendSafeAreaToFrame(rfh, insets);
  }

 private:
  std::optional<gfx::Insets> safe_area_insets_;
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

TEST_F(SafeAreaInsetsHostImplTest, AutoToCover) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kDrawCutoutEdgeToEdge},
      /*disabled_features=*/{});

  ResetSafeAreaTracking();
  NavigateToAuto();
  ExpectAuto();
  EXPECT_TRUE(test_safe_area_insets_host()->did_send_safe_area())
      << "The Insets should always be sent when they change.";
  EXPECT_EQ(0, test_safe_area_insets_host()->safe_area_insets().top())
      << "No Display Cutout, so the top inset should have been zero";

  ResetSafeAreaTracking();
  NavigateToCover();
  ExpectCover();
  EXPECT_TRUE(test_safe_area_insets_host()->did_send_safe_area())
      << "The Insets should always be sent when they change.";
  EXPECT_EQ(42, test_safe_area_insets_host()->safe_area_insets().top())
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
  EXPECT_EQ(42, test_safe_area_insets_host()->safe_area_insets().top())
      << "The Display Cutout should have caused a non-zero top inset";

  ResetSafeAreaTracking();
  NavigateToAuto();
  ExpectAuto();
  EXPECT_TRUE(test_safe_area_insets_host()->did_send_safe_area())
      << "The Insets should always be sent when they change.";
  EXPECT_EQ(0, test_safe_area_insets_host()->safe_area_insets().top())
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

  EXPECT_EQ(subframe, test_safe_area_insets_host()->active_rfh());
  EXPECT_EQ(42, test_safe_area_insets_host()->safe_area_insets().top())
      << "The Display Cutout should have caused a non-zero top inset";

  // Exit fullscreen from sub frame.
  ResetSafeAreaTracking();
  test_safe_area_insets_host()->DidExitFullscreen();

  EXPECT_EQ(main_rfh(), test_safe_area_insets_host()->active_rfh());
  EXPECT_EQ(42, test_safe_area_insets_host()->safe_area_insets().top())
      << "The Display Cutout should have caused a non-zero top inset";
}

}  // namespace content
