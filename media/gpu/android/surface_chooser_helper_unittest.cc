// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/android/surface_chooser_helper.h"

#include <stdint.h>

#include <memory>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/test/simple_test_tick_clock.h"
#include "media/gpu/android/mock_android_video_surface_chooser.h"
#include "media/gpu/android/mock_promotion_hint_aggregator.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::AtLeast;

namespace media {

// Unit tests for PromotionHintAggregatorImplTest
class SurfaceChooserHelperTest : public testing::Test {
 public:
  ~SurfaceChooserHelperTest() override {}

  void SetUp() override {
    // Create a default helper.
    ReplaceHelper(false, false);
  }

  void TearDown() override {}

  void ReplaceHelper(bool is_overlay_required,
                     bool promote_secure_only,
                     bool always_use_texture_owner = false) {
    // Advance the clock so that time 0 isn't recent.
    tick_clock_.Advance(base::Seconds(10000));

    std::unique_ptr<MockAndroidVideoSurfaceChooser> chooser =
        std::make_unique<MockAndroidVideoSurfaceChooser>();
    chooser_ = chooser.get();
    std::unique_ptr<MockPromotionHintAggregator> aggregator =
        std::make_unique<MockPromotionHintAggregator>();
    aggregator_ = aggregator.get();
    helper_ = std::make_unique<SurfaceChooserHelper>(
        std::move(chooser), is_overlay_required, promote_secure_only,
        always_use_texture_owner, std::move(aggregator), &tick_clock_);
  }

  // Convenience function.
  void UpdateChooserState() {
    EXPECT_CALL(*chooser_, MockUpdateState());
    helper_->UpdateChooserState(std::optional<AndroidOverlayFactoryCB>());
  }

  base::SimpleTestTickClock tick_clock_;

  raw_ptr<MockPromotionHintAggregator> aggregator_ = nullptr;

  raw_ptr<MockAndroidVideoSurfaceChooser> chooser_ = nullptr;

  std::unique_ptr<SurfaceChooserHelper> helper_;
};

TEST_F(SurfaceChooserHelperTest, SetIsFullscreen) {
  // Entering fullscreen should expect relayout.
  helper_->SetIsFullscreen(true);
  UpdateChooserState();
  ASSERT_TRUE(chooser_->current_state_.is_fullscreen);
  ASSERT_TRUE(chooser_->current_state_.is_expecting_relayout);

  // Exiting fullscreen should not reset the expecting layout flag.
  helper_->SetIsFullscreen(false);
  UpdateChooserState();
  ASSERT_FALSE(chooser_->current_state_.is_fullscreen);
  // We don't really care if it sets expecting_relayout, clears it, or not.
}

TEST_F(SurfaceChooserHelperTest, SetVideoRotation) {
  // VideoRotation should be forwarded to the chooser.
  helper_->SetVideoRotation(VIDEO_ROTATION_90);
  UpdateChooserState();
  ASSERT_EQ(chooser_->current_state_.video_rotation, VIDEO_ROTATION_90);
}

TEST_F(SurfaceChooserHelperTest, SetIsPersistentVideo) {
  helper_->SetIsPersistentVideo(true);
  UpdateChooserState();
  ASSERT_TRUE(chooser_->current_state_.is_persistent_video);

  helper_->SetIsPersistentVideo(false);
  UpdateChooserState();
  ASSERT_FALSE(chooser_->current_state_.is_persistent_video);
}

TEST_F(SurfaceChooserHelperTest, SetIsOverlayRequired) {
  // The default helper was created without |is_required|, so verify that.
  UpdateChooserState();
  ASSERT_FALSE(chooser_->current_state_.is_required);

  ReplaceHelper(true, false);
  UpdateChooserState();
  ASSERT_TRUE(chooser_->current_state_.is_required);
}

TEST_F(SurfaceChooserHelperTest, SetInsecureSurface) {
  helper_->SetSecureSurfaceMode(
      SurfaceChooserHelper::SecureSurfaceMode::kInsecure);
  UpdateChooserState();
  ASSERT_FALSE(chooser_->current_state_.is_secure);
  ASSERT_FALSE(chooser_->current_state_.is_required);
}

TEST_F(SurfaceChooserHelperTest, SetRequestedSecureSurface) {
  helper_->SetSecureSurfaceMode(
      SurfaceChooserHelper::SecureSurfaceMode::kRequested);
  UpdateChooserState();
  ASSERT_TRUE(chooser_->current_state_.is_secure);
  ASSERT_FALSE(chooser_->current_state_.is_required);
}

TEST_F(SurfaceChooserHelperTest, SetRequiredSecureSurface) {
  helper_->SetSecureSurfaceMode(
      SurfaceChooserHelper::SecureSurfaceMode::kRequired);
  UpdateChooserState();
  ASSERT_TRUE(chooser_->current_state_.is_secure);
  ASSERT_TRUE(chooser_->current_state_.is_required);

  // Also check that removing kRequired puts |is_required| back, since that has
  // special processing for "always required".
  helper_->SetSecureSurfaceMode(
      SurfaceChooserHelper::SecureSurfaceMode::kInsecure);
  UpdateChooserState();
  ASSERT_FALSE(chooser_->current_state_.is_required);
}

TEST_F(SurfaceChooserHelperTest, StillRequiredAfterClearingSecure) {
  // Verify that setting then clearing kRequired doesn't make |is_required|
  // false if overlays were required during construction.
  ReplaceHelper(true, false);
  helper_->SetSecureSurfaceMode(
      SurfaceChooserHelper::SecureSurfaceMode::kRequired);
  UpdateChooserState();
  ASSERT_TRUE(chooser_->current_state_.is_required);

  helper_->SetSecureSurfaceMode(
      SurfaceChooserHelper::SecureSurfaceMode::kInsecure);
  UpdateChooserState();
  // Should still be true.
  ASSERT_TRUE(chooser_->current_state_.is_required);
}

TEST_F(SurfaceChooserHelperTest, SetPromoteSecureOnly) {
  UpdateChooserState();
  ASSERT_FALSE(chooser_->current_state_.promote_secure_only);

  ReplaceHelper(false, true);
  UpdateChooserState();
  ASSERT_TRUE(chooser_->current_state_.promote_secure_only);
}

TEST_F(SurfaceChooserHelperTest, SetAlwaysUseTextureOwner) {
  UpdateChooserState();
  ASSERT_FALSE(chooser_->current_state_.always_use_texture_owner);

  ReplaceHelper(false, true, true);
  UpdateChooserState();
  ASSERT_TRUE(chooser_->current_state_.always_use_texture_owner);
}

TEST_F(SurfaceChooserHelperTest, PromotionHintsForwardsHint) {
  // Make sure that NotifyPromotionHint relays the hint to the aggregator.
  PromotionHintAggregator::Hint hint(gfx::Rect(1, 2, 3, 4), false);
  EXPECT_CALL(*aggregator_, NotifyPromotionHint(hint));
  helper_->NotifyPromotionHintAndUpdateChooser(hint, false);
}

TEST_F(SurfaceChooserHelperTest, PromotionHintsRelayPosition) {
  // Make sure that the overlay position is sent to the chooser.
  gfx::Rect rect(0, 1, 2, 3);

  // Send an unpromotable hint and verify that the state reflects it.  We set
  // it to be promotable so that it notifies the chooser.
  helper_->NotifyPromotionHintAndUpdateChooser(
      PromotionHintAggregator::Hint(rect, true), false);
  ASSERT_EQ(chooser_->current_state_.initial_position, rect);
}

TEST_F(SurfaceChooserHelperTest, PromotionHintsRelayPromotable) {
  // Make sure that the promotability state is forwarded to the chooser.
  EXPECT_CALL(*chooser_, MockUpdateState()).Times(AtLeast(1));
  PromotionHintAggregator::Hint hint(gfx::Rect(), false);

  // Send a hint while the aggregator says it's unpromotable, and verify that
  // the state reflects it.
  helper_->NotifyPromotionHintAndUpdateChooser(hint, false);
  ASSERT_FALSE(chooser_->current_state_.is_compositor_promotable);

  // Send a promotable hint and check the state.  Note that the hint has nothing
  // to do with it; it's the aggregator's state.
  aggregator_->SetIsSafeToPromote(true);
  helper_->NotifyPromotionHintAndUpdateChooser(hint, false);
  ASSERT_TRUE(chooser_->current_state_.is_compositor_promotable);
}

TEST_F(SurfaceChooserHelperTest, PromotionHintsClearRelayoutFlag) {
  // Set fullscreen to enable relayout.
  helper_->SetIsFullscreen(true);
  UpdateChooserState();
  ASSERT_TRUE(chooser_->current_state_.is_expecting_relayout);

  // Send a bunch of hints.
  EXPECT_CALL(*chooser_, MockUpdateState()).Times(AtLeast(1));
  for (int i = 0; i < 15; i++) {
    PromotionHintAggregator::Hint hint(gfx::Rect(), false);
    helper_->NotifyPromotionHintAndUpdateChooser(hint, false);
  }

  // It should no longer be expecting fs.
  ASSERT_FALSE(chooser_->current_state_.is_expecting_relayout);
}

TEST_F(SurfaceChooserHelperTest, PromotionHintsUpdateChooserStatePeriodically) {
  // Verify that, if enough time passes, we'll get chooser updates if we want
  // and overlay but don't have one.
  PromotionHintAggregator::Hint hint(gfx::Rect(), false);

  // Sending the first hint should update the chooser, since we're becoming
  // safe to promote.
  aggregator_->SetIsSafeToPromote(true);
  EXPECT_CALL(*chooser_, MockUpdateState()).Times(1);
  helper_->NotifyPromotionHintAndUpdateChooser(hint, false);

  // Sending an additional hint should not, whether or not we're using an
  // overlay currently.
  EXPECT_CALL(*chooser_, MockUpdateState()).Times(0);
  helper_->NotifyPromotionHintAndUpdateChooser(hint, true);
  EXPECT_CALL(*chooser_, MockUpdateState()).Times(0);
  helper_->NotifyPromotionHintAndUpdateChooser(hint, false);

  // Advancing the time and using an overlay should not send a hint.
  tick_clock_.Advance(base::Seconds(10));
  EXPECT_CALL(*chooser_, MockUpdateState()).Times(0);
  helper_->NotifyPromotionHintAndUpdateChooser(hint, true);

  // If we're not using an overlay, then it should update the chooser to see
  // if it's willing to try for one now.
  EXPECT_CALL(*chooser_, MockUpdateState()).Times(1);
  helper_->NotifyPromotionHintAndUpdateChooser(hint, false);
}

TEST_F(SurfaceChooserHelperTest, FrameInformationIsCorrectForL1) {
  // Verify L1 cases.
  helper_->SetSecureSurfaceMode(
      SurfaceChooserHelper::SecureSurfaceMode::kRequired);

  ASSERT_EQ(SurfaceChooserHelper::FrameInformation::OVERLAY_L1,
            helper_->ComputeFrameInformation(true));
  // We don't check the "not using overlay" case; it's unclear what we should be
  // doing in this case anyway.
}

TEST_F(SurfaceChooserHelperTest, FrameInformationIsCorrectForL3) {
  // Verify L3 cases.
  helper_->SetSecureSurfaceMode(
      SurfaceChooserHelper::SecureSurfaceMode::kRequested);

  ASSERT_EQ(SurfaceChooserHelper::FrameInformation::OVERLAY_L3,
            helper_->ComputeFrameInformation(true));
  ASSERT_EQ(SurfaceChooserHelper::FrameInformation::NON_OVERLAY_L3,
            helper_->ComputeFrameInformation(false));
}

TEST_F(SurfaceChooserHelperTest, FrameInformationIsCorrectForInsecure) {
  // Verify insecure cases.
  helper_->SetSecureSurfaceMode(
      SurfaceChooserHelper::SecureSurfaceMode::kInsecure);

  // Not using an overlay should be NON_OVERLAY_INSECURE
  ASSERT_EQ(SurfaceChooserHelper::FrameInformation::NON_OVERLAY_INSECURE,
            helper_->ComputeFrameInformation(false));

  // Fullscreen state should affect the result, so that we can tell the
  // difference between player-element-fs and div-fs (custom controls).
  helper_->SetIsFullscreen(true);
  ASSERT_EQ(SurfaceChooserHelper::FrameInformation::
                OVERLAY_INSECURE_PLAYER_ELEMENT_FULLSCREEN,
            helper_->ComputeFrameInformation(true));
  helper_->SetIsFullscreen(false);
  ASSERT_EQ(SurfaceChooserHelper::FrameInformation::
                OVERLAY_INSECURE_NON_PLAYER_ELEMENT_FULLSCREEN,
            helper_->ComputeFrameInformation(true));
}

}  // namespace media
