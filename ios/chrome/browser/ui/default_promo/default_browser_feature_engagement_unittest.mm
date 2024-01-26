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

// Basic test for the generic default browser promo.
TEST_F(DefaultBrowserFeatureEngagementTest, DefaultBrowserBasicTest) {
  std::unique_ptr<feature_engagement::Tracker> tracker =
      feature_engagement::CreateTestTracker();
  base::Time initial_time = test_clock_.Now();
  tracker->SetClockForTesting(test_clock_, initial_time);

  // Make sure tracker is initialized.
  tracker->AddOnInitializedCallback(BoolArgumentQuitClosure());
  run_loop_.Run();

  // Promo shouldn't trigger because the preconditions are not satistfied.
  EXPECT_FALSE(tracker->ShouldTriggerHelpUI(
      feature_engagement::kIPHiOSPromoDefaultBrowserFeature));

  // Promos can be displayed only after Chrome opened 7 times.
  for (int i = 0; i < 7; i++) {
    tracker->NotifyEvent(feature_engagement::events::kChromeOpened);
  }

  // The promo should trigger because all the preconditions are now satisfied.
  EXPECT_TRUE(tracker->ShouldTriggerHelpUI(
      feature_engagement::kIPHiOSPromoDefaultBrowserFeature));
  tracker->Dismissed(feature_engagement::kIPHiOSPromoDefaultBrowserFeature);

  // It shouldn't trigger the second time.
  EXPECT_FALSE(tracker->ShouldTriggerHelpUI(
      feature_engagement::kIPHiOSPromoDefaultBrowserFeature));
}

// Basic test for the All Tabs default browser promo.
TEST_F(DefaultBrowserFeatureEngagementTest, AllTabsPromoBasicTest) {
  feature_engagement::test::ScopedIphFeatureList list;
  list.InitAndEnableFeatures({feature_engagement::kIPHiOSPromoAllTabsFeature});
  std::unique_ptr<feature_engagement::Tracker> tracker =
      feature_engagement::CreateTestTracker();
  base::Time initial_time = test_clock_.Now();
  tracker->SetClockForTesting(test_clock_, initial_time);

  // Make sure tracker is initialized.
  tracker->AddOnInitializedCallback(BoolArgumentQuitClosure());
  run_loop_.Run();

  // Promo shouldn't trigger because the preconditions are not satistfied.
  EXPECT_FALSE(tracker->ShouldTriggerHelpUI(
      feature_engagement::kIPHiOSPromoAllTabsFeature));

  // Make sure the preconditions are satisfied.
  tracker->NotifyEvent("all_tabs_promo_conditions_met");

  // Promos can be displayed only after Chrome opened 7 times.
  for (int i = 0; i < 7; i++) {
    tracker->NotifyEvent(feature_engagement::events::kChromeOpened);
  }

  // The promo should trigger because all the preconditions are now satisfied.
  EXPECT_TRUE(tracker->ShouldTriggerHelpUI(
      feature_engagement::kIPHiOSPromoAllTabsFeature));
  tracker->Dismissed(feature_engagement::kIPHiOSPromoAllTabsFeature);

  // It shouldn't trigger the second time.
  EXPECT_FALSE(tracker->ShouldTriggerHelpUI(
      feature_engagement::kIPHiOSPromoAllTabsFeature));

  // After a month is still shouldn't trigger
  test_clock_.Advance(base::Days(30));
  EXPECT_FALSE(tracker->ShouldTriggerHelpUI(
      feature_engagement::kIPHiOSPromoAllTabsFeature));
}

// Basic test for the Made for iOS default browser promo.
TEST_F(DefaultBrowserFeatureEngagementTest, MadeForIOSPromoBasicTest) {
  feature_engagement::test::ScopedIphFeatureList list;
  list.InitAndEnableFeatures(
      {feature_engagement::kIPHiOSPromoMadeForIOSFeature});
  std::unique_ptr<feature_engagement::Tracker> tracker =
      feature_engagement::CreateTestTracker();
  base::Time initial_time = test_clock_.Now();
  tracker->SetClockForTesting(test_clock_, initial_time);

  // Make sure tracker is initialized.
  tracker->AddOnInitializedCallback(BoolArgumentQuitClosure());
  run_loop_.Run();

  // Promo shouldn't trigger because the preconditions are not satistfied.
  EXPECT_FALSE(tracker->ShouldTriggerHelpUI(
      feature_engagement::kIPHiOSPromoMadeForIOSFeature));

  // Make sure the preconditions are satisfied.
  tracker->NotifyEvent("made_for_ios_promo_conditions_met");

  // Promos can be displayed only after Chrome opened 7 times.
  for (int i = 0; i < 7; i++) {
    tracker->NotifyEvent(feature_engagement::events::kChromeOpened);
  }

  // The promo should trigger because all the preconditions are now satisfied.
  EXPECT_TRUE(tracker->ShouldTriggerHelpUI(
      feature_engagement::kIPHiOSPromoMadeForIOSFeature));
  tracker->Dismissed(feature_engagement::kIPHiOSPromoMadeForIOSFeature);

  // It shouldn't trigger the second time.
  EXPECT_FALSE(tracker->ShouldTriggerHelpUI(
      feature_engagement::kIPHiOSPromoMadeForIOSFeature));

  // After a month is still shouldn't trigger
  test_clock_.Advance(base::Days(30));
  EXPECT_FALSE(tracker->ShouldTriggerHelpUI(
      feature_engagement::kIPHiOSPromoMadeForIOSFeature));
}

// Basic test for the Stay Safe default browser promo.
TEST_F(DefaultBrowserFeatureEngagementTest, StaySafePromoBasicTest) {
  feature_engagement::test::ScopedIphFeatureList list;
  list.InitAndEnableFeatures({feature_engagement::kIPHiOSPromoStaySafeFeature});
  std::unique_ptr<feature_engagement::Tracker> tracker =
      feature_engagement::CreateTestTracker();
  base::Time initial_time = test_clock_.Now();
  tracker->SetClockForTesting(test_clock_, initial_time);

  // Make sure tracker is initialized.
  tracker->AddOnInitializedCallback(BoolArgumentQuitClosure());
  run_loop_.Run();

  // Promo shouldn't trigger because the preconditions are not satistfied.
  EXPECT_FALSE(tracker->ShouldTriggerHelpUI(
      feature_engagement::kIPHiOSPromoStaySafeFeature));

  // Make sure the preconditions are satisfied.
  tracker->NotifyEvent("stay_safe_promo_conditions_met");

  // Promos can be displayed only after Chrome opened 7 times.
  for (int i = 0; i < 7; i++) {
    tracker->NotifyEvent(feature_engagement::events::kChromeOpened);
  }

  // The promo should trigger because all the preconditions are now satisfied.
  EXPECT_TRUE(tracker->ShouldTriggerHelpUI(
      feature_engagement::kIPHiOSPromoStaySafeFeature));
  tracker->Dismissed(feature_engagement::kIPHiOSPromoStaySafeFeature);

  // It shouldn't trigger the second time.
  EXPECT_FALSE(tracker->ShouldTriggerHelpUI(
      feature_engagement::kIPHiOSPromoStaySafeFeature));

  // After a month is still shouldn't trigger
  test_clock_.Advance(base::Days(30));
  EXPECT_FALSE(tracker->ShouldTriggerHelpUI(
      feature_engagement::kIPHiOSPromoStaySafeFeature));
}

// Test for default browser group configuration.
TEST_F(DefaultBrowserFeatureEngagementTest, DefaultBrowserGroupTest) {
  feature_engagement::test::ScopedIphFeatureList list;
  list.InitAndEnableFeatures({feature_engagement::kIPHiOSPromoStaySafeFeature});
  std::unique_ptr<feature_engagement::Tracker> tracker =
      feature_engagement::CreateTestTracker();
  base::Time initial_time = test_clock_.Now();
  tracker->SetClockForTesting(test_clock_, initial_time);

  // Make sure tracker is initialized.
  tracker->AddOnInitializedCallback(BoolArgumentQuitClosure());
  run_loop_.Run();

  // Promo shouldn't trigger because the preconditions are not satistfied.
  EXPECT_FALSE(tracker->ShouldTriggerHelpUI(
      feature_engagement::kIPHiOSPromoDefaultBrowserFeature));

  // Make sure the preconditions are satisfied for the Stay Safe promo.
  tracker->NotifyEvent("stay_safe_promo_conditions_met");

  // Promos can be displayed only after Chrome opened 7 times.
  for (int i = 0; i < 7; i++) {
    tracker->NotifyEvent(feature_engagement::events::kChromeOpened);
  }

  // Mark one of the group promos as displayed.
  tracker->NotifyEvent("default_browser_promos_group_trigger");

  // The promo cannot be triggered because after one default browser promo we
  // need to wait 14 days.
  EXPECT_FALSE(tracker->ShouldTriggerHelpUI(
      feature_engagement::kIPHiOSPromoStaySafeFeature));

  // After 14 days it should trigger another promo.
  test_clock_.Advance(base::Days(14));
  EXPECT_TRUE(tracker->ShouldTriggerHelpUI(
      feature_engagement::kIPHiOSPromoStaySafeFeature));
  tracker->Dismissed(feature_engagement::kIPHiOSPromoStaySafeFeature);
}
