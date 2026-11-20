// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actor/util/actor_dialog_utils.h"

#import "base/test/metrics/histogram_tester.h"
#import "base/test/scoped_feature_list.h"
#import "ios/chrome/browser/intelligence/actor/model/actor_tab_helper.h"
#import "ios/chrome/browser/intelligence/actor/public/actor_control_state.h"
#import "ios/chrome/browser/intelligence/features/features.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace {

class ActorDialogUtilsTest : public PlatformTest {
 protected:
  ActorTabHelper* AttachActorTabHelper(actor::ActorControlState control_state) {
    ActorTabHelper::CreateForWebState(&web_state_);
    ActorTabHelper* tab_helper = ActorTabHelper::FromWebState(&web_state_);
    tab_helper->SetControlState(control_state);
    return tab_helper;
  }

  base::test::ScopedFeatureList feature_list_;
  web::FakeWebState web_state_;
  base::HistogramTester histogram_tester_;
};

// Tests that a null `WebState` never suppresses dialogs.
TEST_F(ActorDialogUtilsTest, NullWebStateDoesNotSuppress) {
  EXPECT_FALSE(ShouldSuppressDialogDuringActuation(
      nullptr, ActorSuppressedDialogType::kPermissions));
}

// Tests that a `WebState` with no `ActorTabHelper` never suppresses dialogs.
// Most tabs are in this state, since the helper is only attached when actor
// is enabled.
TEST_F(ActorDialogUtilsTest, MissingTabHelperDoesNotSuppress) {
  EXPECT_FALSE(ShouldSuppressDialogDuringActuation(
      &web_state_, ActorSuppressedDialogType::kPermissions));
}

// Tests that an inactive tab does not suppress dialogs, so that permission
// prompts keep working normally outside of actuation.
TEST_F(ActorDialogUtilsTest, InactiveTabDoesNotSuppress) {
  AttachActorTabHelper(actor::ActorControlState::kInactive);

  EXPECT_FALSE(ShouldSuppressDialogDuringActuation(
      &web_state_, ActorSuppressedDialogType::kPermissions));
}

// Tests that suppression stays off during actuation when the top-level feature
// is explicitly disabled.
TEST_F(ActorDialogUtilsTest, DisabledFeatureDoesNotSuppress) {
  feature_list_.InitAndDisableFeature(kActorSuppressDialogs);
  AttachActorTabHelper(actor::ActorControlState::kActorControlled);

  EXPECT_FALSE(ShouldSuppressDialogDuringActuation(
      &web_state_, ActorSuppressedDialogType::kPermissions));
}

// Tests that suppression stays off during actuation when the permissions
// category parameter is disabled.
TEST_F(ActorDialogUtilsTest, DisabledParamDoesNotSuppress) {
  feature_list_.InitAndEnableFeatureWithParameters(
      kActorSuppressDialogs,
      {{kActorSuppressDialogsPermissionsParam, "false"}});
  AttachActorTabHelper(actor::ActorControlState::kActorControlled);

  EXPECT_FALSE(ShouldSuppressDialogDuringActuation(
      &web_state_, ActorSuppressedDialogType::kPermissions));
}

// Tests that an actuating tab suppresses by default without needing explicit
// feature enablement.
TEST_F(ActorDialogUtilsTest, ActuatingTabSuppresses) {
  AttachActorTabHelper(actor::ActorControlState::kActorControlled);

  EXPECT_TRUE(ShouldSuppressDialogDuringActuation(
      &web_state_, ActorSuppressedDialogType::kPermissions));
}

// Tests that suppression stops as soon as the task finishes actuating.
TEST_F(ActorDialogUtilsTest, StoppingActuationRestoresPrompts) {
  ActorTabHelper* tab_helper =
      AttachActorTabHelper(actor::ActorControlState::kActorControlled);
  ASSERT_TRUE(ShouldSuppressDialogDuringActuation(
      &web_state_, ActorSuppressedDialogType::kPermissions));

  tab_helper->SetControlState(actor::ActorControlState::kInactive);

  EXPECT_FALSE(ShouldSuppressDialogDuringActuation(
      &web_state_, ActorSuppressedDialogType::kPermissions));
}

// Tests that each suppressed dialog bucket is recorded in the
// `IOS.Actor.Dialog.Suppressed` histogram.
TEST_F(ActorDialogUtilsTest, RecordsSuppressedDialogs) {
  RecordDialogSuppressedDuringActuation(
      IOSDialogDuringActuation::kCameraPermission);
  RecordDialogSuppressedDuringActuation(
      IOSDialogDuringActuation::kMicrophonePermission);
  RecordDialogSuppressedDuringActuation(
      IOSDialogDuringActuation::kCameraAndMicrophonePermission);
  RecordDialogSuppressedDuringActuation(
      IOSDialogDuringActuation::kGeolocationPermission);

  histogram_tester_.ExpectBucketCount(
      kActorDialogSuppressedHistogram,
      IOSDialogDuringActuation::kCameraPermission, 1);
  histogram_tester_.ExpectBucketCount(
      kActorDialogSuppressedHistogram,
      IOSDialogDuringActuation::kMicrophonePermission, 1);
  histogram_tester_.ExpectBucketCount(
      kActorDialogSuppressedHistogram,
      IOSDialogDuringActuation::kCameraAndMicrophonePermission, 1);
  histogram_tester_.ExpectBucketCount(
      kActorDialogSuppressedHistogram,
      IOSDialogDuringActuation::kGeolocationPermission, 1);
  histogram_tester_.ExpectTotalCount(kActorDialogSuppressedHistogram, 4);
}

}  // namespace
