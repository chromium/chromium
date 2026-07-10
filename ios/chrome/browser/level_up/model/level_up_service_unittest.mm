// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/level_up/model/level_up_service.h"

#import "base/memory/raw_ptr.h"
#import "base/test/scoped_feature_list.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/level_up/model/level_up_service_factory.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace {

class LevelUpServiceTest : public PlatformTest {
 protected:
  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(kIOSLevelUp);
    PlatformTest::SetUp();

    TestProfileIOS::Builder builder;
    profile_ = std::move(builder).Build();
    service_ = LevelUpServiceFactory::GetForProfile(profile_.get());
  }

  web::WebTaskEnvironment task_environment_;
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<TestProfileIOS> profile_;
  raw_ptr<LevelUpService> service_;
};

// Tests the initial default state of the service.
TEST_F(LevelUpServiceTest, TestDefaultState) {
  EXPECT_FALSE(service_->IsUIEnabled());
  EXPECT_EQ(service_->GetCurrentLevel(), 1);
  EXPECT_EQ(service_->GetTasksRemainingForNextLevel(), 3);

  // Verify all tasks are initially uncompleted.
  const auto& tasks = service_->GetTasks();
  for (const auto& [type, info] : tasks) {
    EXPECT_FALSE(service_->IsTaskCompleted(type));
  }
}

// Tests the milestone progression and remaining tasks calculations.
TEST_F(LevelUpServiceTest, TestMilestoneProgression) {
  // Complete 1 task.
  service_->MarkTaskCompleted(TaskType::kTabGroups);
  EXPECT_TRUE(service_->IsTaskCompleted(TaskType::kTabGroups));
  EXPECT_EQ(service_->GetCurrentLevel(), 1);
  EXPECT_EQ(service_->GetTasksRemainingForNextLevel(), 2);

  // Complete 2nd task.
  service_->MarkTaskCompleted(TaskType::kAutofill);
  EXPECT_EQ(service_->GetCurrentLevel(), 1);
  EXPECT_EQ(service_->GetTasksRemainingForNextLevel(), 1);

  // Complete 3rd task -> Should reach Level 2!
  service_->MarkTaskCompleted(TaskType::kPinTabs);
  EXPECT_EQ(service_->GetCurrentLevel(), 2);
  // Reaching Level 3 requires 8 total tasks. 8 - 3 = 5 remaining.
  EXPECT_EQ(service_->GetTasksRemainingForNextLevel(), 5);

  // Complete 4 more tasks (total 7).
  service_->MarkTaskCompleted(TaskType::kGemini);
  service_->MarkTaskCompleted(TaskType::kPaymentMethods);
  service_->MarkTaskCompleted(TaskType::kQuickDelete);
  service_->MarkTaskCompleted(TaskType::kSafeBrowsing);
  EXPECT_EQ(service_->GetCurrentLevel(), 2);
  EXPECT_EQ(service_->GetTasksRemainingForNextLevel(), 1);

  // Complete 8th task -> Should reach Level 3!
  service_->MarkTaskCompleted(TaskType::kIncognito);
  EXPECT_EQ(service_->GetCurrentLevel(), 3);
  // Reaching Level 4 requires all 12 tasks. 12 - 8 = 4 remaining.
  EXPECT_EQ(service_->GetTasksRemainingForNextLevel(), 4);

  // Complete remaining 4 tasks (total 12).
  service_->MarkTaskCompleted(TaskType::kPasswordCheckup);
  service_->MarkTaskCompleted(TaskType::kLensSearch);
  service_->MarkTaskCompleted(TaskType::kAISearch);
  service_->MarkTaskCompleted(TaskType::kCameraSearch);
  EXPECT_EQ(service_->GetCurrentLevel(), 4);
  EXPECT_EQ(service_->GetTasksRemainingForNextLevel(), 0);
}

// Tests that the level is monotonic and never decreases.
TEST_F(LevelUpServiceTest, TestLevelMonotonicity) {
  // Complete 3 tasks to reach Level 2.
  service_->MarkTaskCompleted(TaskType::kTabGroups);
  service_->MarkTaskCompleted(TaskType::kAutofill);
  service_->MarkTaskCompleted(TaskType::kPinTabs);
  EXPECT_EQ(service_->GetCurrentLevel(), 2);

  // Manually force the highest level preference to Level 3 (simulating a sync
  // from another device).
  PrefService* prefs = profile_->GetPrefs();
  prefs->SetInteger(prefs::kLevelUpHighestLevel, 3);

  // Re-create the service (or trigger LoadPrefs) to simulate startup.
  auto new_service = std::make_unique<LevelUpService>(prefs);

  // The level should be 3 (the highest level from preferences), even though
  // only 3 tasks are completed (which would normally be Level 2).
  EXPECT_EQ(new_service->GetCurrentLevel(), 3);

  // Completing another task (4 total) should still keep us at Level 3.
  new_service->MarkTaskCompleted(TaskType::kGemini);
  EXPECT_EQ(new_service->GetCurrentLevel(), 3);
}

TEST_F(LevelUpServiceTest, TestTaskCompletionResetsFreshness) {
  PrefService* prefs = profile_->GetPrefs();
  prefs->SetInteger(
      prefs::kIosMagicStackSegmentationLevelUpImpressionsSinceFreshness, 5);
  EXPECT_EQ(
      5,
      prefs->GetInteger(
          prefs::kIosMagicStackSegmentationLevelUpImpressionsSinceFreshness));

  service_->MarkTaskCompleted(TaskType::kTabGroups);
  EXPECT_EQ(
      0,
      prefs->GetInteger(
          prefs::kIosMagicStackSegmentationLevelUpImpressionsSinceFreshness));
}

}  // namespace
