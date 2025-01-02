// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/capture/web_contents_auto_scaler.h"

#include <memory>

#include "base/test/scoped_feature_list.h"
#include "media/base/media_switches.h"
#include "media/capture/video/video_capture_feedback.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/size.h"

namespace content {
namespace {

// Standardized screen resolutions to test common scenarios.
constexpr gfx::Size kSize720p{1280, 720};
constexpr gfx::Size kSize1080p{1920, 1080};

class FakeDelegate : public WebContentsAutoScaler::Delegate {
 public:
  FakeDelegate() = default;
  ~FakeDelegate() override = default;

  void SetCaptureScaleOverride(float scale) override {
    scale_override_ = scale;
  }
  float GetCaptureScaleOverride() const override { return scale_override_; }

 private:
  float scale_override_ = 1.0f;
};

class WebContentsAutoScalerTest : public ::testing::Test {
 protected:
  WebContentsAutoScalerTest() = default;
  ~WebContentsAutoScalerTest() override = default;

  void CreateAutoScaler(const gfx::Size& capture_size) {
    scaler_ = std::make_unique<WebContentsAutoScaler>(delegate_, capture_size);
  }

  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<WebContentsAutoScaler> scaler_;
  FakeDelegate delegate_;
};

TEST_F(WebContentsAutoScalerTest, SetsScaleOverride) {
  CreateAutoScaler(kSize1080p);

  // Capture starts at 1080p size, there should be no scale override.
  EXPECT_EQ(delegate_.GetCaptureScaleOverride(), 1.0f);

  // Adjust the captured content size to a smaller size. This should activate a
  // scale override correlative to the difference between the two resolutions.
  scaler_->SetCapturedContentSize(kSize720p);
  EXPECT_DOUBLE_EQ(delegate_.GetCaptureScaleOverride(), 1.5);

  // Scaling should go up to a maximum of 2.0.
  scaler_->SetCapturedContentSize(gfx::Size(960, 540));
  EXPECT_DOUBLE_EQ(delegate_.GetCaptureScaleOverride(), 2.0f);

  // The tracker should assume that we are now already scaled by the override
  // value, and so shouldn't change the override if we start getting frames that
  // are large enough.
  scaler_->SetCapturedContentSize(gfx::Size(1920, 1080));
  EXPECT_DOUBLE_EQ(delegate_.GetCaptureScaleOverride(), 2.0f);

  // If a frame ends up being larger than the capture_size, the scale
  // should get adjusted downwards so that the post-scaling size matches
  // the capture size. This assumes a current scale override of 2.0f.
  scaler_->SetCapturedContentSize(gfx::Size(2560, 1440));
  EXPECT_DOUBLE_EQ(delegate_.GetCaptureScaleOverride(), 1.5f);

  // The scaled size should now match the capture size with a scale
  // override of 1.5.
  scaler_->SetCapturedContentSize(gfx::Size(1920, 1080));
  EXPECT_DOUBLE_EQ(delegate_.GetCaptureScaleOverride(), 1.5f);

  // The scaling calculation is based on fitting a scaled copy of the
  // source rectangle within the capture region, preserving aspect ratio.
  // If the content size changes in a way that doesn't affect the scale
  // factor (i.e. letterboxing or pillarboxing), the scale override remains
  // unchanged.
  scaler_->SetCapturedContentSize(gfx::Size(1080, 1080));
  EXPECT_DOUBLE_EQ(delegate_.GetCaptureScaleOverride(), 1.5f);
  scaler_->SetCapturedContentSize(gfx::Size(1920, 540));
  EXPECT_DOUBLE_EQ(delegate_.GetCaptureScaleOverride(), 1.5f);
}

TEST_F(WebContentsAutoScalerTest, SettingScaleFactorMaintainsStableCapture) {
  CreateAutoScaler(kSize1080p);

  // Adjust the captured content size to a smaller size. This should activate a
  // scale override correlative to the difference between the two resolutions.
  scaler_->SetCapturedContentSize(kSize720p);
  EXPECT_DOUBLE_EQ(delegate_.GetCaptureScaleOverride(), 1.5);

  // It should now be scaled to the capture_size, meaning 1080P. The
  // scale override factor should be unaffected.
  scaler_->SetCapturedContentSize(kSize1080p);
  EXPECT_DOUBLE_EQ(delegate_.GetCaptureScaleOverride(), 1.5);
}

TEST_F(WebContentsAutoScalerTest, HighDpiIsRoundedIfBetweenBounds) {
  CreateAutoScaler(kSize1080p);

  // Both factors should be 1.4f, which should be between bounds and rounded
  // up.
  scaler_->SetCapturedContentSize(gfx::Size{1370, 771});
  EXPECT_DOUBLE_EQ(delegate_.GetCaptureScaleOverride(), 1.5);
}

TEST_F(WebContentsAutoScalerTest, HighDpiIsRoundedIfBetweenDifferentBounds) {
  CreateAutoScaler(kSize1080p);

  // Both factors should be 1.6f, which should be between bounds and rounded
  // down.
  scaler_->SetCapturedContentSize(gfx::Size{1200, 675});
  EXPECT_DOUBLE_EQ(delegate_.GetCaptureScaleOverride(), 1.5f);
}

TEST_F(WebContentsAutoScalerTest, HighDpiIsRoundedToMinimum) {
  CreateAutoScaler(kSize1080p);

  // Both factors should be 1.3f, which should be between bounds and rounded
  // down.
  scaler_->SetCapturedContentSize(gfx::Size{1477, 831});
  EXPECT_DOUBLE_EQ(delegate_.GetCaptureScaleOverride(), 1.25f);
}

TEST_F(WebContentsAutoScalerTest, HighDpiIsRoundedToMaximum) {
  CreateAutoScaler(kSize1080p);

  // Both factors should be well over the maximum of 2.0f.
  scaler_->SetCapturedContentSize(gfx::Size{320, 240});
  EXPECT_DOUBLE_EQ(delegate_.GetCaptureScaleOverride(), 2.0f);
}

TEST_F(WebContentsAutoScalerTest, HighDpiScalingIsStable) {
  CreateAutoScaler(kSize1080p);

  // Both factors should be 1.25f, which should be exactly a scaling factor.
  static constexpr gfx::Size kContentSize(1536, 864);
  scaler_->SetCapturedContentSize(kContentSize);
  EXPECT_DOUBLE_EQ(delegate_.GetCaptureScaleOverride(), 1.25f);

  // Now that its applied, it should stay the same.
  static const gfx::Size kScaledContentSize =
      gfx::ScaleToRoundedSize(kContentSize, 1.25f);
  scaler_->SetCapturedContentSize(kScaledContentSize);
  EXPECT_DOUBLE_EQ(delegate_.GetCaptureScaleOverride(), 1.25f);

  // If it varies slightly that shouldn't result in any changes.
  static const gfx::Size kScaledLargerContentSize =
      gfx::ScaleToRoundedSize(kContentSize, 1.27f);
  scaler_->SetCapturedContentSize(kScaledLargerContentSize);
  EXPECT_DOUBLE_EQ(delegate_.GetCaptureScaleOverride(), 1.25f);

  static const gfx::Size kScaledSmallerContentSize =
      gfx::ScaleToRoundedSize(kContentSize, 1.23f);
  scaler_->SetCapturedContentSize(kScaledSmallerContentSize);
  EXPECT_DOUBLE_EQ(delegate_.GetCaptureScaleOverride(), 1.25f);
}

TEST_F(WebContentsAutoScalerTest, HighDpiAdjustsForResourceUtilization) {
  CreateAutoScaler(kSize1080p);

  // Both factors should be 2.0f, which should be exactly a scaling factor.
  scaler_->SetCapturedContentSize(gfx::Size(960, 540));
  EXPECT_DOUBLE_EQ(delegate_.GetCaptureScaleOverride(), 2.0f);

  // Start with default feedback, which should be ignored.
  media::VideoCaptureFeedback feedback;
  scaler_->OnUtilizationReport(feedback);
  EXPECT_DOUBLE_EQ(delegate_.GetCaptureScaleOverride(), 2.0f);

  // As the feedback continues to be poor, the scale override should lower.
  feedback.resource_utilization = 0.9f;
  scaler_->OnUtilizationReport(feedback);
  EXPECT_DOUBLE_EQ(delegate_.GetCaptureScaleOverride(), 1.75f);
  scaler_->OnUtilizationReport(feedback);
  EXPECT_DOUBLE_EQ(delegate_.GetCaptureScaleOverride(), 1.5f);
  scaler_->OnUtilizationReport(feedback);
  EXPECT_DOUBLE_EQ(delegate_.GetCaptureScaleOverride(), 1.25f);
  scaler_->OnUtilizationReport(feedback);
  EXPECT_DOUBLE_EQ(delegate_.GetCaptureScaleOverride(), 1.0f);

  // If things get significantly better, it should go back up.
  feedback.resource_utilization = 0.49f;
  scaler_->OnUtilizationReport(feedback);
  EXPECT_DOUBLE_EQ(delegate_.GetCaptureScaleOverride(), 1.25f);
  scaler_->OnUtilizationReport(feedback);
  EXPECT_DOUBLE_EQ(delegate_.GetCaptureScaleOverride(), 1.5f);
  scaler_->OnUtilizationReport(feedback);
  EXPECT_DOUBLE_EQ(delegate_.GetCaptureScaleOverride(), 1.75f);
  scaler_->OnUtilizationReport(feedback);
  EXPECT_DOUBLE_EQ(delegate_.GetCaptureScaleOverride(), 2.0f);
}

TEST_F(WebContentsAutoScalerTest, HighDpiAdjustsForMaxPixelRate) {
  CreateAutoScaler(kSize1080p);

  scaler_->SetCapturedContentSize(kSize720p);
  EXPECT_DOUBLE_EQ(delegate_.GetCaptureScaleOverride(), 1.5f);

  // Test using too many pixels.
  media::VideoCaptureFeedback feedback;
  feedback.max_pixels = kSize720p.width() * kSize720p.height() - 1;

  // We should lower the maximum, which should eventually lower the override.
  // First, max is now 1.75f.
  scaler_->OnUtilizationReport(feedback);
  EXPECT_DOUBLE_EQ(delegate_.GetCaptureScaleOverride(), 1.5f);

  // Now max is 1.5f.
  scaler_->OnUtilizationReport(feedback);
  EXPECT_DOUBLE_EQ(delegate_.GetCaptureScaleOverride(), 1.5f);

  // Now max is 1.25f.
  scaler_->OnUtilizationReport(feedback);
  EXPECT_DOUBLE_EQ(delegate_.GetCaptureScaleOverride(), 1.25f);

  // Now max is 1.0f.
  scaler_->OnUtilizationReport(feedback);
  EXPECT_DOUBLE_EQ(delegate_.GetCaptureScaleOverride(), 1.0f);
  scaler_->OnUtilizationReport(feedback);
  EXPECT_DOUBLE_EQ(delegate_.GetCaptureScaleOverride(), 1.0f);

  // Things should only change if it gets significantly better.
  feedback.max_pixels = kSize720p.width() * kSize720p.height() + 1;
  scaler_->OnUtilizationReport(feedback);
  EXPECT_DOUBLE_EQ(delegate_.GetCaptureScaleOverride(), 1.0f);

  feedback.max_pixels = kSize720p.width() * kSize720p.height() * 1.33f;
  scaler_->OnUtilizationReport(feedback);
  EXPECT_DOUBLE_EQ(delegate_.GetCaptureScaleOverride(), 1.25f);
  scaler_->OnUtilizationReport(feedback);
  EXPECT_DOUBLE_EQ(delegate_.GetCaptureScaleOverride(), 1.5f);
}

}  // namespace
}  // namespace content
