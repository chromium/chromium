// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/display_cutout/display_cutout_host_impl.h"

#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/common/features.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/test_renderer_host.h"
#include "content/test/test_web_contents.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace content {

namespace {

class TestDisplayCutoutHostImpl : public DisplayCutoutHostImpl {
 public:
  explicit TestDisplayCutoutHostImpl(WebContentsImpl* web_contents_impl)
      : DisplayCutoutHostImpl(web_contents_impl) {}

  void ResetSafeArea() {
    did_send_safe_area_ = false;
    safe_area_insets_ = gfx::Insets(0);
  }

  bool did_send_safe_area() { return did_send_safe_area_; }
  gfx::Insets safe_area_insets() { return safe_area_insets_; }

 protected:
  // Send the safe area insets to a |RenderFrameHost|.
  void SendSafeAreaToFrame(RenderFrameHost* rfh, gfx::Insets insets) override {
    did_send_safe_area_ = true;
    safe_area_insets_ = insets;
    DisplayCutoutHostImpl::SendSafeAreaToFrame(rfh, insets);
  }

 private:
  bool did_send_safe_area_ = false;
  gfx::Insets safe_area_insets_;
};

}  // namespace

class DisplayCutoutHostImplTest : public RenderViewHostTestHarness {
 protected:
  void SetUp() override {
    RenderViewHostTestHarness::SetUp();
    SetContents(CreateTestWebContents());

    std::unique_ptr<TestDisplayCutoutHostImpl> test_display_cutout_host_impl =
        absl::make_unique<TestDisplayCutoutHostImpl>(test_web_contents());
    test_display_cutout_host_ = test_display_cutout_host_impl.get();
    test_web_contents()->SetSafeAreaInsetsHost(
        std::move(test_display_cutout_host_impl));
  }

  TestWebContents* test_web_contents() const {
    return static_cast<TestWebContents*>(web_contents());
  }

  TestDisplayCutoutHostImpl* test_display_cutout_host() const {
    return test_display_cutout_host_;
  }

  void ResetSafeArea() { test_display_cutout_host()->ResetSafeArea(); }

  void NavigateToCover() {
    FocusWebContentsOnMainFrame();
    // Simulate window insets changing, e.g. java's
    // DisplayCutoutController#onSafeAreaChanged notified from InsetObserver.
    test_web_contents()->SetDisplayCutoutSafeArea(gfx::Insets(42));
    NavigateAndCommit(GURL("www.viewportFitCover.com"));
    test_web_contents()->NotifyViewportFitChanged(
        blink::mojom::ViewportFit::kCover);
  }

  void NavigateToAuto() {
    FocusWebContentsOnMainFrame();
    test_web_contents()->SetDisplayCutoutSafeArea(gfx::Insets(0));
    NavigateAndCommit(GURL("www.viewportFitAuto.com"));
    test_web_contents()->NotifyViewportFitChanged(
        blink::mojom::ViewportFit::kAuto);
  }

 private:
  raw_ptr<TestDisplayCutoutHostImpl> test_display_cutout_host_;
};

TEST_F(DisplayCutoutHostImplTest, AutoToCover) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kDrawCutoutEdgeToEdge},
      /*disabled_features=*/{});

  ResetSafeArea();
  NavigateToAuto();
  EXPECT_TRUE(test_display_cutout_host()->did_send_safe_area());
  EXPECT_EQ(0, test_display_cutout_host()->safe_area_insets().top())
      << "No Display Cutout, so the top inset should have been zero";

  ResetSafeArea();
  NavigateToCover();
  EXPECT_TRUE(test_display_cutout_host()->did_send_safe_area());
  EXPECT_NE(0, test_display_cutout_host()->safe_area_insets().top())
      << "The Display Cutout should have caused a non-zero top inset";
}

TEST_F(DisplayCutoutHostImplTest, CoverToAuto) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kDrawCutoutEdgeToEdge},
      /*disabled_features=*/{});

  ResetSafeArea();
  NavigateToCover();
  EXPECT_TRUE(test_display_cutout_host()->did_send_safe_area());
  EXPECT_NE(0, test_display_cutout_host()->safe_area_insets().top())
      << "The Display Cutout should have caused a non-zero top inset";

  ResetSafeArea();
  NavigateToAuto();
  EXPECT_TRUE(test_display_cutout_host()->did_send_safe_area());
  EXPECT_EQ(0, test_display_cutout_host()->safe_area_insets().top())
      << "No Display Cutout, so the top inset should have been zero";
}

}  // namespace content
