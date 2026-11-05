// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/level_up/coordinator/level_up_mediator.h"

#import <algorithm>
#import <set>
#import <utility>
#import <vector>

#import "base/functional/bind.h"
#import "base/memory/raw_ptr.h"
#import "base/test/metrics/histogram_tester.h"
#import "base/test/scoped_feature_list.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/level_up/coordinator/level_up_task.h"
#import "ios/chrome/browser/level_up/model/level_up_metrics.h"
#import "ios/chrome/browser/level_up/model/level_up_service.h"
#import "ios/chrome/browser/level_up/model/level_up_service_factory.h"
#import "ios/chrome/browser/level_up/model/task_info.h"
#import "ios/chrome/browser/level_up/model/task_types.h"
#import "ios/chrome/browser/level_up/ui/level_up_consumer.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_manager_ios.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
#import "ios/chrome/browser/sync/model/test_sync_service_utils.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

// Fake consumer representing one window's Level Up UI.
@interface FakeLevelUpConsumer : NSObject <LevelUpConsumer>
// Types of the last recommended tasks received, in display order.
@property(nonatomic, readonly) std::vector<TaskType> recommendedTasks;
// Types of the last recommended tasks received that are marked completed.
@property(nonatomic, readonly) std::set<TaskType> completedRecommendedTasks;
@end

@implementation FakeLevelUpConsumer
@synthesize delegate = _delegate;

#pragma mark - LevelUpConsumer

- (void)setRecommendedTasks:(NSArray<LevelUpTask*>*)recommendedTasks {
  _recommendedTasks.clear();
  _completedRecommendedTasks.clear();
  for (LevelUpTask* task in recommendedTasks) {
    const TaskType type = task.taskInfo->GetTaskType();
    _recommendedTasks.push_back(type);
    if (task.completed) {
      _completedRecommendedTasks.insert(type);
    }
  }
}

@end

namespace {

// The number of recommended tasks shown in the "All tasks" section.
constexpr size_t kRecommendedTasksCount = 4;

// Helper method for UpdatesRecommendedTasksInAllLiveWindows test. It verifies
// that `after` matches `before`, except that `completed_type`, if exisits in
// `before`, is replaced in place by an uncompleted task not shown in `before`.
// Must be called after `completed_type` is marked completed in `service`, and
// when there must exists an unshonw, uncompleted task for replacement.
void ExpectOnlyCompletedTaskReplaced(const LevelUpService& service,
                                     const std::vector<TaskType>& before,
                                     const std::vector<TaskType>& after,
                                     TaskType completed_type) {
  size_t uncompleted_tasks_count = 0;
  for (const auto& [type, info] : service.GetTasks()) {
    if (!service.IsTaskCompleted(type)) {
      ++uncompleted_tasks_count;
    }
  }
  // A new recommended task replacement must exist.
  ASSERT_GE(uncompleted_tasks_count, before.size());
  ASSERT_EQ(before.size(), after.size());
  for (size_t i = 0; i < before.size(); ++i) {
    if (before[i] == completed_type) {
      EXPECT_NE(before[i], after[i]);
      EXPECT_FALSE(std::ranges::contains(before, after[i]));
    } else {
      EXPECT_EQ(before[i], after[i]);
    }
  }
}

class LevelUpMediatorTest : public PlatformTest {
 protected:
  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(kIOSLevelUp);
    PlatformTest::SetUp();

    TestProfileIOS::Builder builder;
    builder.AddTestingFactory(
        AuthenticationServiceFactory::GetInstance(),
        AuthenticationServiceFactory::GetDefaultFactory());
    builder.AddTestingFactory(SyncServiceFactory::GetInstance(),
                              base::BindRepeating(&CreateTestSyncService));
    profile_ = profile_manager_.AddProfileWithBuilder(std::move(builder));

    pref_service_ = profile_->GetPrefs();
    pref_service_->SetBoolean(prefs::kLevelUpOptIn, true);

    service_ = LevelUpServiceFactory::GetForProfile(profile_);

    mediator_ = CreateMediator();
  }

  void TearDown() override {
    for (LevelUpMediator* mediator in mediators_) {
      [mediator disconnect];
    }
    mediators_ = nil;
    mediator_ = nil;
    service_ = nullptr;
    pref_service_ = nullptr;
    profile_ = nullptr;
    PlatformTest::TearDown();
  }

  // Creates a Level Up mediator.
  LevelUpMediator* CreateMediator() {
    LevelUpMediator* mediator = [[LevelUpMediator alloc]
        initWithAuthenticationService:AuthenticationServiceFactory::
                                          GetForProfile(profile_)
                      identityManager:IdentityManagerFactory::GetForProfile(
                                          profile_)
                       levelUpService:service_
                          prefService:pref_service_];
    [mediators_ addObject:mediator];
    return mediator;
  }

  web::WebTaskEnvironment task_environment_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  TestProfileManagerIOS profile_manager_;
  base::test::ScopedFeatureList scoped_feature_list_;
  raw_ptr<TestProfileIOS> profile_ = nullptr;
  raw_ptr<PrefService> pref_service_ = nullptr;
  raw_ptr<LevelUpService> service_ = nullptr;
  LevelUpMediator* mediator_ = nil;
  // All mediators created by `CreateMediator()`.
  NSMutableArray<LevelUpMediator*>* mediators_ = [[NSMutableArray alloc] init];
};

// Tests that toggling progress updates updates the preference and records the
// NotificationsStateChanged histogram.
TEST_F(LevelUpMediatorTest, TogglesProgressUpdatesAndRecordsMetric) {
  base::HistogramTester histogram_tester;
  EXPECT_TRUE(pref_service_->GetBoolean(prefs::kLevelUpUIEnabled));

  EXPECT_FALSE([mediator_ toggleProgressUpdates]);
  EXPECT_FALSE(pref_service_->GetBoolean(prefs::kLevelUpUIEnabled));
  histogram_tester.ExpectBucketCount(kLevelUpNotificationsStateChangedHistogram,
                                     false, 1);

  EXPECT_TRUE([mediator_ toggleProgressUpdates]);
  EXPECT_TRUE(pref_service_->GetBoolean(prefs::kLevelUpUIEnabled));
  histogram_tester.ExpectBucketCount(kLevelUpNotificationsStateChangedHistogram,
                                     true, 1);
  histogram_tester.ExpectTotalCount(kLevelUpNotificationsStateChangedHistogram,
                                    2);
}

// Tests that setting new tasks notifications updates the preference and records
// the NewTaskNotificationsStateChanged histogram.
TEST_F(LevelUpMediatorTest, SetsNewTasksNotificationAndRecordsMetric) {
  base::HistogramTester histogram_tester;

  [mediator_ setNewTasksNotificationEnabled:NO];
  EXPECT_FALSE(
      pref_service_->GetBoolean(prefs::kLevelUpNewTasksNotificationEnabled));
  histogram_tester.ExpectBucketCount(
      kLevelUpNewTaskNotificationsStateChangedHistogram, false, 1);

  [mediator_ setNewTasksNotificationEnabled:YES];
  EXPECT_TRUE(
      pref_service_->GetBoolean(prefs::kLevelUpNewTasksNotificationEnabled));
  histogram_tester.ExpectBucketCount(
      kLevelUpNewTaskNotificationsStateChangedHistogram, true, 1);
  histogram_tester.ExpectTotalCount(
      kLevelUpNewTaskNotificationsStateChangedHistogram, 2);
}

// Tests that turning off Level Up resets tasks, updates the opt-in preference,
// and records the OptInStateChanged histogram.
TEST_F(LevelUpMediatorTest, TurnsOffLevelUpAndRecordsMetric) {
  service_->MarkTaskCompleted(TaskType::kTabGroups);
  ASSERT_TRUE(service_->IsTaskCompleted(TaskType::kTabGroups));

  base::HistogramTester histogram_tester;
  [mediator_ turnOffLevelUp];

  EXPECT_FALSE(pref_service_->GetBoolean(prefs::kLevelUpOptIn));
  EXPECT_FALSE(service_->IsTaskCompleted(TaskType::kTabGroups));
  histogram_tester.ExpectUniqueSample(kLevelUpOptInStateChangedHistogram, false,
                                      1);
}

// Tests that opening Level Up shows a list of distinct, uncompleted
// recommended tasks.
TEST_F(LevelUpMediatorTest, CreatesRecommendedTasksOnOpen) {
  FakeLevelUpConsumer* consumer = [[FakeLevelUpConsumer alloc] init];
  mediator_.consumer = consumer;

  const std::vector<TaskType> types = consumer.recommendedTasks;
  EXPECT_EQ(kRecommendedTasksCount, types.size());
  EXPECT_EQ(types.size(),
            std::set<TaskType>(types.begin(), types.end()).size());
  EXPECT_TRUE(consumer.completedRecommendedTasks.empty());
}

// Tests that in a multi-window scenario, for every task completed, every live
// window replaces only the completed task in its own recommended list.
TEST_F(LevelUpMediatorTest, UpdatesRecommendedTasksInAllLiveWindows) {
  FakeLevelUpConsumer* consumer_a = [[FakeLevelUpConsumer alloc] init];
  mediator_.consumer = consumer_a;
  LevelUpMediator* mediator_b = CreateMediator();
  FakeLevelUpConsumer* consumer_b = [[FakeLevelUpConsumer alloc] init];
  mediator_b.consumer = consumer_b;

  const std::vector<TaskType> initial_types_a = consumer_a.recommendedTasks;
  const std::vector<TaskType> initial_types_b = consumer_b.recommendedTasks;
  ASSERT_EQ(kRecommendedTasksCount, initial_types_b.size());

  // Complete a task shown in window B. Window A replaces it only if it shows
  // it too; A replacement always exists because there are more uncompleted
  // tasks than recommended slots.
  const TaskType completed_type = initial_types_b[1];
  service_->MarkTaskCompleted(completed_type);
  ExpectOnlyCompletedTaskReplaced(*service_, initial_types_a,
                                  consumer_a.recommendedTasks, completed_type);
  // Window B shows `completed_type`, so it must replace only that task in
  // place.
  ExpectOnlyCompletedTaskReplaced(*service_, initial_types_b,
                                  consumer_b.recommendedTasks, completed_type);

  EXPECT_TRUE(consumer_a.completedRecommendedTasks.empty());
  EXPECT_TRUE(consumer_b.completedRecommendedTasks.empty());
}

// Tests that in a multi-window scenario, when a task is completed while no
// uncompleted task is left to replace it, every live window keeps the task in
// place and only marks it completed.
TEST_F(LevelUpMediatorTest, MarksCompletedTaskInAllLiveWindowsWhenNoneLeft) {
  // Complete all but `kRecommendedTasksCount` tasks so that every remaining
  // uncompleted task is already shown and none is left as a replacement.
  std::vector<TaskType> types_to_complete;
  for (const auto& [type, info] : service_->GetTasks()) {
    types_to_complete.push_back(type);
  }
  ASSERT_GT(types_to_complete.size(), kRecommendedTasksCount);
  types_to_complete.resize(types_to_complete.size() - kRecommendedTasksCount);
  for (TaskType type : types_to_complete) {
    service_->MarkTaskCompleted(type);
  }

  FakeLevelUpConsumer* consumer_a = [[FakeLevelUpConsumer alloc] init];
  mediator_.consumer = consumer_a;
  LevelUpMediator* mediator_b = CreateMediator();
  FakeLevelUpConsumer* consumer_b = [[FakeLevelUpConsumer alloc] init];
  mediator_b.consumer = consumer_b;

  const std::vector<TaskType> initial_types_a = consumer_a.recommendedTasks;
  const std::vector<TaskType> initial_types_b = consumer_b.recommendedTasks;
  ASSERT_EQ(kRecommendedTasksCount, initial_types_a.size());

  const TaskType completed_type = initial_types_a[1];
  service_->MarkTaskCompleted(completed_type);

  EXPECT_EQ(initial_types_a, consumer_a.recommendedTasks);
  EXPECT_EQ(initial_types_b, consumer_b.recommendedTasks);
  EXPECT_EQ(std::set<TaskType>{completed_type},
            consumer_a.completedRecommendedTasks);
  EXPECT_EQ(std::set<TaskType>{completed_type},
            consumer_b.completedRecommendedTasks);
}

}  // namespace
