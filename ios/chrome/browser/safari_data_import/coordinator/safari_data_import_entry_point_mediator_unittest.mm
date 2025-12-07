// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/safari_data_import/coordinator/safari_data_import_entry_point_mediator.h"

#import <memory>

#import "base/functional/callback_helpers.h"
#import "base/run_loop.h"
#import "components/feature_engagement/public/event_constants.h"
#import "components/feature_engagement/public/feature_list.h"
#import "components/feature_engagement/public/tracker.h"
#import "components/feature_engagement/test/scoped_iph_feature_list.h"
#import "components/feature_engagement/test/test_tracker.h"
#import "ios/chrome/app/profile/profile_state.h"
#import "ios/chrome/browser/feature_engagement/model/tracker_factory.h"
#import "ios/chrome/browser/promos_manager/model/constants.h"
#import "ios/chrome/browser/promos_manager/model/mock_promos_manager.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/platform_test.h"

// Test fixture for SafariDataImportEntryPointMediator.
class SafariDataImportEntryPointMediatorTest : public PlatformTest {
 public:
  SafariDataImportEntryPointMediatorTest() : PlatformTest() {
    feature_engagement::test::ScopedIphFeatureList list;
    list.InitAndEnableFeatures(
        {feature_engagement::kIPHiOSSafariImportFeature});

    ProfileState* profile_state = [[ProfileState alloc] initWithAppState:nil];
    scene_state_ = [[SceneState alloc] initWithAppState:nil];
    scene_state_.profileState = profile_state;
    promos_manager_ = std::make_unique<MockPromosManager>();
    tracker_ = feature_engagement::CreateTestTracker();
    tracker_->AddOnInitializedCallback(BoolArgumentQuitClosure());
    run_loop_.Run();

    mediator_ = [[SafariDataImportEntryPointMediator alloc]
         initWithUIBlockerTarget:scene_state_
                   promosManager:promos_manager_.get()
        featureEngagementTracker:tracker_.get()];

    // Set a consistent start time for the mock clock to avoid midnight issues.
    base::Time next_day = base::Time::Now() + base::Days(1);
    base::Time::Exploded exploded;
    next_day.LocalExplode(&exploded);
    exploded.hour = 0;
    base::Time mock_start_time;
    EXPECT_TRUE(base::Time::FromLocalExploded(exploded, &mock_start_time));
    task_environment_.FastForwardBy(mock_start_time - base::Time::Now());
  }

  ~SafariDataImportEntryPointMediatorTest() override {
    [mediator_ disconnect];
  }

  base::RepeatingCallback<void(bool)> BoolArgumentQuitClosure() {
    return base::IgnoreArgs<bool>(run_loop_.QuitClosure());
  }

 protected:
  web::WebTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  SceneState* scene_state_;
  std::unique_ptr<MockPromosManager> promos_manager_;
  std::unique_ptr<feature_engagement::Tracker> tracker_;
  SafariDataImportEntryPointMediator* mediator_;
  base::RunLoop run_loop_;
};

// Tests that the Safari import reminder is registered on request.
TEST_F(SafariDataImportEntryPointMediatorTest,
       TestRegisterSafariImportReminder) {
  EXPECT_CALL(*promos_manager_.get(),
              RegisterPromoForSingleDisplay(
                  promos_manager::Promo::SafariImportRemindMeLater));
  [mediator_ registerReminder];
  // Tests that reminder is displayed two days later.
  task_environment_.FastForwardBy(base::Days(1) + base::Hours(1));
  EXPECT_FALSE(tracker_->ShouldTriggerHelpUI(
      feature_engagement::kIPHiOSSafariImportFeature));
  task_environment_.FastForwardBy(base::Days(1));
  EXPECT_TRUE(tracker_->ShouldTriggerHelpUI(
      feature_engagement::kIPHiOSSafariImportFeature));
  tracker_->Dismissed(feature_engagement::kIPHiOSSafariImportFeature);
}

// Tests that the reminder would not be displayed once the mediator marks Safari
// import as used or dismissed.
TEST_F(SafariDataImportEntryPointMediatorTest,
       TestNoReminderAfterUsedOrDismissed) {
  [mediator_ notifyUsedOrDismissed];
  // Register reminder and test.
  tracker_->NotifyEvent(
      feature_engagement::events::kIOSSafariImportRemindMeLater);
  task_environment_.FastForwardBy(base::Days(2) + base::Hours(1));
  EXPECT_FALSE(tracker_->ShouldTriggerHelpUI(
      feature_engagement::kIPHiOSSafariImportFeature));
}
