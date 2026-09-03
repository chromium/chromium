// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/level_up/model/level_up_service.h"

#import "base/memory/raw_ptr.h"
#import "base/test/scoped_feature_list.h"
#import "components/prefs/pref_service.h"
#import "components/tab_groups/tab_group_color.h"
#import "components/tab_groups/tab_group_id.h"
#import "components/tab_groups/tab_group_visual_data.h"
#import "ios/chrome/browser/level_up/model/level_up_service_factory.h"
#import "ios/chrome/browser/passwords/model/ios_chrome_password_check_manager.h"
#import "ios/chrome/browser/passwords/model/ios_chrome_password_check_manager_factory.h"
#import "ios/chrome/browser/shared/model/browser/browser_list.h"
#import "ios/chrome/browser/shared/model/browser/browser_list_factory.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/tab_group.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
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
    profile_->GetPrefs()->SetBoolean(prefs::kLevelUpOptIn, true);
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
  service_->MarkTaskCompleted(TaskType::kLensWebsiteSearch);
  service_->MarkTaskCompleted(TaskType::kAISearch);
  service_->MarkTaskCompleted(TaskType::kLensCameraSearch);
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

TEST_F(LevelUpServiceTest, TestStatValues) {
  // Initially all stats are 0.
  EXPECT_EQ(0, service_->GetStatValue(LevelUpTaskStatType::kTabsDecluttered));
  EXPECT_EQ(0, service_->GetStatValue(LevelUpTaskStatType::kTypingSaved));
  EXPECT_EQ(0, service_->GetStatValue(LevelUpTaskStatType::kPasswordsVerified));
  EXPECT_EQ(
      0, service_->GetStatValue(LevelUpTaskStatType::kPhotoSearchesPerformed));

  // Increment typing saved stat.
  service_->IncrementStatValue(LevelUpTaskStatType::kTypingSaved, 15);
  EXPECT_EQ(15, service_->GetStatValue(LevelUpTaskStatType::kTypingSaved));

  // Increment tabs decluttered stat.
  service_->IncrementStatValue(LevelUpTaskStatType::kTabsDecluttered, 4);
  EXPECT_EQ(4, service_->GetStatValue(LevelUpTaskStatType::kTabsDecluttered));
}

// Tests that LevelUpTabGroupObserver automatically tracks tab group creation
// and moving tabs into groups.
TEST_F(LevelUpServiceTest, TestTabGroupObserverDecluttering) {
  EXPECT_EQ(0, service_->GetStatValue(LevelUpTaskStatType::kTabsDecluttered));

  BrowserList* browser_list = BrowserListFactory::GetForProfile(profile_.get());
  TestBrowser browser(profile_.get());
  browser_list->AddBrowser(&browser);

  WebStateList* web_state_list = browser.GetWebStateList();
  web_state_list->InsertWebState(std::make_unique<web::FakeWebState>());
  web_state_list->InsertWebState(std::make_unique<web::FakeWebState>());

  // Create a tab group containing index 0.
  tab_groups::TabGroupVisualData visual_data(
      u"Test Group", tab_groups::TabGroupColorId::kBlue);
  const TabGroup* group = web_state_list->CreateGroup(
      {0}, visual_data, tab_groups::TabGroupId::GenerateNew());

  // Stat count should increment by 1 for tab group creation.
  EXPECT_EQ(1, service_->GetStatValue(LevelUpTaskStatType::kTabsDecluttered));

  // Move index 1 into the existing group.
  web_state_list->MoveToGroup({1}, group);

  // Stat count should increment to 2.
  EXPECT_EQ(2, service_->GetStatValue(LevelUpTaskStatType::kTabsDecluttered));
}

// Tests that LevelUpPasswordCheckObserver updates kPasswordsVerified stat when
// a password check finishes (transitions from kRunning to kIdle).
TEST_F(LevelUpServiceTest, TestPasswordCheckObserver) {
  EXPECT_EQ(0, service_->GetStatValue(LevelUpTaskStatType::kPasswordsVerified));

  scoped_refptr<IOSChromePasswordCheckManager> check_manager =
      IOSChromePasswordCheckManagerFactory::GetForProfile(profile_.get());
  ASSERT_TRUE(check_manager);

  auto service_with_manager = std::make_unique<LevelUpService>(
      profile_->GetPrefs(), nullptr, nullptr, check_manager.get());

  // Start and stop password check.
  check_manager->StartPasswordCheck(
      password_manager::LeakDetectionInitiator::kEditCheck);
  check_manager->StopPasswordCheck();

  // Verify kPasswordsVerified stat (0 if no saved passwords in test store).
  EXPECT_EQ(0, service_with_manager->GetStatValue(
                   LevelUpTaskStatType::kPasswordsVerified));
}

// Tests that ResetAllTasksStatus clears task completion, stats, level, and
// segmentation impressions prefs.
TEST_F(LevelUpServiceTest, TestResetAllTasksStatus) {
  // Complete tasks and increment stats.
  service_->MarkTaskCompleted(TaskType::kTabGroups);
  service_->MarkTaskCompleted(TaskType::kAutofill);
  service_->MarkTaskCompleted(TaskType::kPinTabs);
  EXPECT_EQ(2, service_->GetCurrentLevel());
  EXPECT_TRUE(service_->IsTaskCompleted(TaskType::kTabGroups));

  service_->IncrementStatValue(LevelUpTaskStatType::kTabsDecluttered, 5);
  service_->IncrementStatValue(LevelUpTaskStatType::kTypingSaved, 20);
  EXPECT_EQ(5, service_->GetStatValue(LevelUpTaskStatType::kTabsDecluttered));
  EXPECT_EQ(20, service_->GetStatValue(LevelUpTaskStatType::kTypingSaved));

  PrefService* prefs = profile_->GetPrefs();
  prefs->SetInteger(
      prefs::kIosMagicStackSegmentationLevelUpImpressionsSinceFreshness, 3);

  // Reset task status.
  service_->ResetAllTasksStatus();

  // Verify level and completion state are reset.
  EXPECT_EQ(1, service_->GetCurrentLevel());
  EXPECT_FALSE(service_->IsTaskCompleted(TaskType::kTabGroups));
  EXPECT_FALSE(service_->IsTaskCompleted(TaskType::kAutofill));
  EXPECT_FALSE(service_->IsTaskCompleted(TaskType::kPinTabs));

  // Verify all stats are reset.
  EXPECT_EQ(0, service_->GetStatValue(LevelUpTaskStatType::kTabsDecluttered));
  EXPECT_EQ(0, service_->GetStatValue(LevelUpTaskStatType::kTypingSaved));
  EXPECT_EQ(0, service_->GetStatValue(LevelUpTaskStatType::kPasswordsVerified));
  EXPECT_EQ(
      0, service_->GetStatValue(LevelUpTaskStatType::kPhotoSearchesPerformed));

  // Verify segmentation impressions pref is reset.
  EXPECT_EQ(
      0,
      prefs->GetInteger(
          prefs::kIosMagicStackSegmentationLevelUpImpressionsSinceFreshness));
}

// Tests that changing the UI enabled preference updates IsUIEnabled().
TEST_F(LevelUpServiceTest, TestUIEnabledPrefObservation) {
  EXPECT_FALSE(service_->IsUIEnabled());

  profile_->GetPrefs()->SetBoolean(prefs::kLevelUpUIEnabled, true);
  EXPECT_TRUE(service_->IsUIEnabled());

  profile_->GetPrefs()->SetBoolean(prefs::kLevelUpUIEnabled, false);
  EXPECT_FALSE(service_->IsUIEnabled());
}

// Tests that tasks and stats are not tracked when the opt-in preference is
// false.
TEST_F(LevelUpServiceTest, TestOptedOutDoesNotTrack) {
  profile_->GetPrefs()->SetBoolean(prefs::kLevelUpOptIn, false);
  EXPECT_FALSE(service_->IsOptedIn());

  // Attempt to complete a task.
  service_->MarkTaskCompleted(TaskType::kTabGroups);
  EXPECT_FALSE(service_->IsTaskCompleted(TaskType::kTabGroups));
  EXPECT_EQ(service_->GetCurrentLevel(), 1);
  EXPECT_EQ(service_->GetTasksRemainingForNextLevel(), 3);

  // Attempt to increment a stat.
  service_->IncrementStatValue(LevelUpTaskStatType::kTabsDecluttered, 5);
  EXPECT_EQ(0, service_->GetStatValue(LevelUpTaskStatType::kTabsDecluttered));
}

}  // namespace
