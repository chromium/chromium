// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/test/simple_test_clock.h"
#import "base/test/task_environment.h"
#import "components/feature_engagement/public/event_constants.h"
#import "components/feature_engagement/public/feature_constants.h"
#import "components/feature_engagement/public/tracker.h"
#import "components/feature_engagement/test/scoped_iph_feature_list.h"
#import "components/feature_engagement/test/test_tracker.h"
#import "testing/platform_test.h"

// Unittests related to the triggering of default browser promos via feature
// engagement.
class DefaultBrowserFeatureEngagementTest : public PlatformTest {
 public:
  DefaultBrowserFeatureEngagementTest() {
    test_clock_.SetNow(base::Time::Now());
  }
  ~DefaultBrowserFeatureEngagementTest() override {}

  base::RepeatingCallback<void(bool)> BoolArgumentQuitClosure() {
    return base::IgnoreArgs<bool>(run_loop_.QuitClosure());
  }

 protected:
  base::test::TaskEnvironment task_environment_;

  base::SimpleTestClock test_clock_;

  base::RunLoop run_loop_;
};

// Tests that the Remind Me Later promo only triggers after the corresponding
// event has been fired.
TEST_F(DefaultBrowserFeatureEngagementTest, TestRemindMeLater) {
  feature_engagement::test::ScopedIphFeatureList list;
  list.InitAndEnableFeatures(
      {feature_engagement::kIPHiOSPromoDefaultBrowserReminderFeature});

  std::unique_ptr<feature_engagement::Tracker> tracker =
      feature_engagement::CreateTestTracker();
  base::Time initial_time = test_clock_.Now();
  tracker->SetClockForTesting(test_clock_, initial_time);

  // Make sure tracker is initialized.
  tracker->AddOnInitializedCallback(BoolArgumentQuitClosure());
  run_loop_.Run();

  // At first, the user has not chosen to be reminded later, so it should not
  // trigger.
  EXPECT_FALSE(tracker->ShouldTriggerHelpUI(
      feature_engagement::kIPHiOSPromoDefaultBrowserReminderFeature));

  // The user taps remind me later.
  tracker->NotifyEvent(
      feature_engagement::events::kDefaultBrowserPromoRemindMeLater);

  // The reminder should still not trigger, as not enough time has passed since
  // the user saw it first.
  EXPECT_FALSE(tracker->ShouldTriggerHelpUI(
      feature_engagement::kIPHiOSPromoDefaultBrowserReminderFeature));

  // After a day, the reminder should trigger.
  test_clock_.Advance(base::Days(1));
  EXPECT_TRUE(tracker->ShouldTriggerHelpUI(
      feature_engagement::kIPHiOSPromoDefaultBrowserReminderFeature));
  tracker->Dismissed(
      feature_engagement::kIPHiOSPromoDefaultBrowserReminderFeature);

  // After another day, the reminder should not trigger again
  test_clock_.Advance(base::Days(1));
  EXPECT_FALSE(tracker->ShouldTriggerHelpUI(
      feature_engagement::kIPHiOSPromoDefaultBrowserReminderFeature));
}
