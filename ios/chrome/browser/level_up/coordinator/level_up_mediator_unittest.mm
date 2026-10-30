// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/level_up/coordinator/level_up_mediator.h"

#import <utility>

#import "base/functional/bind.h"
#import "base/memory/raw_ptr.h"
#import "base/test/metrics/histogram_tester.h"
#import "base/test/scoped_feature_list.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/level_up/model/level_up_metrics.h"
#import "ios/chrome/browser/level_up/model/level_up_service.h"
#import "ios/chrome/browser/level_up/model/level_up_service_factory.h"
#import "ios/chrome/browser/level_up/model/task_types.h"
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

namespace {

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

    mediator_ = [[LevelUpMediator alloc]
        initWithAuthenticationService:AuthenticationServiceFactory::
                                          GetForProfile(profile_)
                      identityManager:IdentityManagerFactory::GetForProfile(
                                          profile_)
                       levelUpService:service_
                          prefService:pref_service_];
  }

  void TearDown() override {
    [mediator_ disconnect];
    mediator_ = nil;
    service_ = nullptr;
    pref_service_ = nullptr;
    profile_ = nullptr;
    PlatformTest::TearDown();
  }

  web::WebTaskEnvironment task_environment_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  TestProfileManagerIOS profile_manager_;
  base::test::ScopedFeatureList scoped_feature_list_;
  raw_ptr<TestProfileIOS> profile_ = nullptr;
  raw_ptr<PrefService> pref_service_ = nullptr;
  raw_ptr<LevelUpService> service_ = nullptr;
  LevelUpMediator* mediator_ = nil;
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

}  // namespace
