// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/level_up/model/level_up_scene_agent.h"

#import "base/memory/raw_ptr.h"
#import "base/metrics/user_metrics.h"
#import "base/test/scoped_feature_list.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/app/application_delegate/app_state.h"
#import "ios/chrome/app/application_delegate/fake_startup_information.h"
#import "ios/chrome/app/profile/profile_state.h"
#import "ios/chrome/browser/level_up/model/level_up_service.h"
#import "ios/chrome/browser/level_up/model/level_up_service_factory.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/coordinator/scene/test/fake_scene_state.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider_interface.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/snackbar_commands.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

namespace {

class LevelUpSceneAgentTest : public PlatformTest {
 protected:
  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(kIOSLevelUp);
    PlatformTest::SetUp();

    TestProfileIOS::Builder builder;
    profile_ = std::move(builder).Build();
    profile_->GetPrefs()->SetBoolean(prefs::kLevelUpOptIn, true);

    fake_startup_information_ = [[FakeStartupInformation alloc] init];
    app_state_ =
        [[AppState alloc] initWithStartupInformation:fake_startup_information_];

    profile_state_ = [[ProfileState alloc] initWithAppState:app_state_];
    profile_state_.profile = profile_.get();

    scene_state_ = [[FakeSceneState alloc] initWithProfile:profile_.get()];
    scene_state_.profileState = profile_state_;

    Browser* browser =
        scene_state_.browserProviderInterface.mainBrowserProvider.browser;
    mock_snackbar_handler_ = OCMProtocolMock(@protocol(SnackbarCommands));
    CommandDispatcher* dispatcher = browser->GetCommandDispatcher();
    [dispatcher startDispatchingToTarget:mock_snackbar_handler_
                             forProtocol:@protocol(SnackbarCommands)];

    agent_ = [[LevelUpSceneAgent alloc] init];
    [scene_state_ addAgent:agent_];

    service_ = LevelUpServiceFactory::GetForProfile(profile_.get());
  }

  void TearDown() override {
    [agent_ stopListening];
    agent_ = nil;
    [scene_state_ shutdown];
    scene_state_ = nil;
    PlatformTest::TearDown();
  }

  web::WebTaskEnvironment task_environment_;
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<TestProfileIOS> profile_;
  FakeStartupInformation* fake_startup_information_;
  AppState* app_state_;
  ProfileState* profile_state_;
  FakeSceneState* scene_state_;
  id<SnackbarCommands> mock_snackbar_handler_;
  LevelUpSceneAgent* agent_;
  raw_ptr<LevelUpService> service_;
};

// Tests that completing a task marks it completed and does not show the
// snackbar when progress updates are disabled.
TEST_F(LevelUpSceneAgentTest,
       TestActionTriggersCompletionWithoutSnackbarWhenProgressUpdatesDisabled) {
  // Progress updates are disabled by default.
  EXPECT_FALSE(service_->IsUIEnabled());
  EXPECT_FALSE(service_->IsTaskCompleted(TaskType::kTabGroups));

  // Simulate the scene becoming active to start listening.
  scene_state_.activationLevel = SceneActivationLevelForegroundActive;

  // Snackbar should not be shown.
  OCMReject([mock_snackbar_handler_ showSnackbarMessage:[OCMArg any]]);

  // Record the action that should trigger kTabGroups completion.
  base::RecordAction(
      base::UserMetricsAction("MobileTabGroupUserCreatedNewGroup"));

  // Verify that the task is completed and snackbar was not shown.
  EXPECT_TRUE(service_->IsTaskCompleted(TaskType::kTabGroups));
  EXPECT_OCMOCK_VERIFY((id)mock_snackbar_handler_);
}

// Tests that completing a task marks it completed and shows the snackbar when
// progress updates are enabled.
TEST_F(LevelUpSceneAgentTest,
       TestActionTriggersCompletionWithSnackbarWhenProgressUpdatesEnabled) {
  service_->SetUIEnabled(true);
  EXPECT_TRUE(service_->IsUIEnabled());
  EXPECT_FALSE(service_->IsTaskCompleted(TaskType::kTabGroups));

  // Simulate the scene becoming active to start listening.
  scene_state_.activationLevel = SceneActivationLevelForegroundActive;

  // Snackbar should be shown.
  OCMExpect([mock_snackbar_handler_ showSnackbarMessage:[OCMArg any]]);

  // Record the action that should trigger kTabGroups completion.
  base::RecordAction(
      base::UserMetricsAction("MobileTabGroupUserCreatedNewGroup"));

  // Verify that the task is completed and snackbar was shown.
  EXPECT_TRUE(service_->IsTaskCompleted(TaskType::kTabGroups));
  EXPECT_OCMOCK_VERIFY((id)mock_snackbar_handler_);
}

// Tests that recording user actions triggers stat increments.
TEST_F(LevelUpSceneAgentTest, TestActionTriggersStatIncrement) {
  EXPECT_EQ(
      0, service_->GetStatValue(LevelUpTaskStatType::kPhotoSearchesPerformed));

  // Simulate the scene becoming active to start listening.
  scene_state_.activationLevel = SceneActivationLevelForegroundActive;

  // Record action that should trigger photo search stat increment.
  base::RecordAction(
      base::UserMetricsAction("Mobile.LensOverlay.CameraSearch.Performed"));

  // Verify that the photo search stat is incremented.
  EXPECT_EQ(
      1, service_->GetStatValue(LevelUpTaskStatType::kPhotoSearchesPerformed));
}

// Tests that user actions do not track tasks or stats when opted out.
TEST_F(LevelUpSceneAgentTest, TestActionDoesNotTriggerTrackingWhenOptedOut) {
  profile_->GetPrefs()->SetBoolean(prefs::kLevelUpOptIn, false);
  EXPECT_FALSE(service_->IsOptedIn());

  // Simulate the scene becoming active to start listening.
  scene_state_.activationLevel = SceneActivationLevelForegroundActive;

  // Snackbar should not be shown.
  OCMReject([mock_snackbar_handler_ showSnackbarMessage:[OCMArg any]]);

  // Record task action.
  base::RecordAction(
      base::UserMetricsAction("MobileTabGroupUserCreatedNewGroup"));

  // Verify task is not completed.
  EXPECT_FALSE(service_->IsTaskCompleted(TaskType::kTabGroups));

  // Record stat action.
  base::RecordAction(
      base::UserMetricsAction("Mobile.LensOverlay.CameraSearch.Performed"));

  // Verify stat is not incremented.
  EXPECT_EQ(
      0, service_->GetStatValue(LevelUpTaskStatType::kPhotoSearchesPerformed));

  EXPECT_OCMOCK_VERIFY((id)mock_snackbar_handler_);
}

}  // namespace
