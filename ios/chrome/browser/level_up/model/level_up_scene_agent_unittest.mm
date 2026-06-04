// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/level_up/model/level_up_scene_agent.h"

#import "base/memory/raw_ptr.h"
#import "base/metrics/user_metrics.h"
#import "base/test/scoped_feature_list.h"
#import "ios/chrome/app/application_delegate/app_state.h"
#import "ios/chrome/app/application_delegate/fake_startup_information.h"
#import "ios/chrome/app/profile/profile_state.h"
#import "ios/chrome/browser/level_up/model/level_up_service.h"
#import "ios/chrome/browser/level_up/model/level_up_service_factory.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace {

class LevelUpSceneAgentTest : public PlatformTest {
 protected:
  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(kIOSLevelUp);
    PlatformTest::SetUp();

    TestProfileIOS::Builder builder;
    profile_ = std::move(builder).Build();

    fake_startup_information_ = [[FakeStartupInformation alloc] init];
    app_state_ =
        [[AppState alloc] initWithStartupInformation:fake_startup_information_];

    profile_state_ = [[ProfileState alloc] initWithAppState:app_state_];
    profile_state_.profile = profile_.get();

    scene_state_ = [[SceneState alloc] initWithAppState:app_state_];
    scene_state_.profileState = profile_state_;

    agent_ = [[LevelUpSceneAgent alloc] init];
    [scene_state_ addAgent:agent_];

    service_ = LevelUpServiceFactory::GetForProfile(profile_.get());
  }

  void TearDown() override {
    [agent_ stopListening];
    agent_ = nil;
    scene_state_ = nil;
    PlatformTest::TearDown();
  }

  web::WebTaskEnvironment task_environment_;
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<TestProfileIOS> profile_;
  FakeStartupInformation* fake_startup_information_;
  AppState* app_state_;
  ProfileState* profile_state_;
  SceneState* scene_state_;
  LevelUpSceneAgent* agent_;
  raw_ptr<LevelUpService> service_;
};

TEST_F(LevelUpSceneAgentTest, TestActionTriggersCompletion) {
  // Ensure tasks are not completed initially.
  EXPECT_FALSE(service_->IsTaskCompleted(TaskType::kTabGroups));
  EXPECT_FALSE(service_->IsTaskCompleted(TaskType::kPasswordCheckup));

  // Simulate the scene becoming active to start listening.
  scene_state_.activationLevel = SceneActivationLevelForegroundActive;

  // Record the action that should trigger kTabGroups completion.
  base::RecordAction(
      base::UserMetricsAction("MobileTabGroupUserCreatedNewGroup"));

  // Verify that the task is now completed.
  EXPECT_TRUE(service_->IsTaskCompleted(TaskType::kTabGroups));
  EXPECT_FALSE(service_->IsTaskCompleted(TaskType::kPasswordCheckup));

  // Record the action that should trigger kPasswordCheckup completion.
  base::RecordAction(
      base::UserMetricsAction("MobilePasswordCheckupSettingsClose"));

  // Verify that password checkup is completed as well.
  EXPECT_TRUE(service_->IsTaskCompleted(TaskType::kPasswordCheckup));
}

}  // namespace
