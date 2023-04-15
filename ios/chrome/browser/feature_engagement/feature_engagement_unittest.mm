// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/test/task_environment.h"
#import "components/feature_engagement/public/event_constants.h"
#import "components/feature_engagement/public/feature_constants.h"
#import "components/feature_engagement/public/tracker.h"
#import "components/feature_engagement/test/scoped_iph_feature_list.h"
#import "components/feature_engagement/test/test_tracker.h"
#import "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// The minimum number of times Chrome must be opened in order for the Reading
// List Badge to be shown.
const int kMinChromeOpensRequiredForReadingList = 5;

// The minimum number of times Chrome must be opened in order for the New Tab
// Tip to be shown.
const int kMinChromeOpensRequiredForNewTabTip = 3;

}  // namespace

// Unittests related to the triggering of In Product Help features. Anything
// that tests what events cause a feature to trigger should be tested there.
class FeatureEngagementTest : public PlatformTest {
 public:
  FeatureEngagementTest();
  ~FeatureEngagementTest() override;

  std::map<std::string, std::string> BadgedReadingListParams() {
    std::map<std::string, std::string> params;
    params["event_1"] =
        "name:chrome_opened;comparator:>=5;window:90;storage:90";
    params["event_trigger"] =
        "name:badged_reading_list_trigger;comparator:==0;window:1095;storage:"
        "1095";
    params["event_used"] =
        "name:viewed_reading_list;comparator:==0;window:90;storage:90";
    params["session_rate"] = "==0";
    params["availability"] = "any";
    return params;
  }

  std::map<std::string, std::string> BadgedTranslateManualTriggerParams() {
    std::map<std::string, std::string> params;
    params["availability"] = "any";
    params["session_rate"] = "==0";
    params["event_used"] = "name:triggered_translate_infobar;comparator:==0;"
                           "window:360;storage:360";
    params["event_trigger"] = "name:badged_translate_manual_trigger_trigger;"
                              "comparator:==0;window:360;"
                              "storage:360";
    return params;
  }

  std::map<std::string, std::string> NewTabTipPromoParams() {
    std::map<std::string, std::string> params;
    params["event_1"] =
        "name:chrome_opened;comparator:>=3;window:90;storage:90";
    params["event_trigger"] =
        "name:new_tab_tip_trigger;comparator:<2;window:1095;storage:"
        "1095";
    params["event_used"] =
        "name:new_tab_opened;comparator:==0;window:90;storage:90";
    params["session_rate"] = "==0";
    params["availability"] = "any";
    return params;
  }

  std::map<std::string, std::string> BottomToolbarTipParams() {
    std::map<std::string, std::string> params;
    params["availability"] = "any";
    params["session_rate"] = "==0";
    params["event_used"] =
        "name:bottom_toolbar_opened;comparator:any;window:90;storage:90";
    params["event_trigger"] =
        "name:bottom_toolbar_trigger;comparator:==0;window:90;storage:90";
    return params;
  }

  std::map<std::string, std::string> LongPressTipParams() {
    std::map<std::string, std::string> params;
    params["availability"] = "any";
    params["session_rate"] = "<=1";
    params["event_used"] =
        "name:long_press_toolbar_opened;comparator:any;window:90;storage:90";
    params["event_trigger"] =
        "name:long_press_toolbar_trigger;comparator:==0;window:90;storage:90";
    params["event_1"] =
        "name:bottom_toolbar_opened;comparator:>=1;window:90;storage:90";
    return params;
  }

  std::map<std::string, std::string> DefaultSiteViewTipParams() {
    std::map<std::string, std::string> params;
    params["availability"] = "any";
    params["session_rate"] = "<3";
    params["event_used"] =
        "name:default_site_view_used;comparator:==0;window:720;storage:720";
    params["event_trigger"] =
        "name:default_site_view_shown;comparator:==0;window:720;storage:720";
    params["event_1"] =
        "name:desktop_version_requested;comparator:>=3;window:60;storage:60";
    return params;
  }

  std::map<std::string, std::string> TabPinnedTipParams() {
    std::map<std::string, std::string> params;
    params["availability"] = "any";
    params["session_rate"] = "any";
    params["event_used"] = "name:popup_menu_tip_used;comparator:==0;window:180;"
                           "storage:180";
    params["event_trigger"] =
        "name:tab_pinned_tip_triggered;comparator:==0;window:1825;"
        "storage:1825";
    return params;
  }

  base::RepeatingCallback<void(bool)> BoolArgumentQuitClosure() {
    return base::IgnoreArgs<bool>(run_loop_.QuitClosure());
  }

 protected:
  base::test::TaskEnvironment task_environment_;

  base::RunLoop run_loop_;
};

FeatureEngagementTest::FeatureEngagementTest() {}

FeatureEngagementTest::~FeatureEngagementTest() {}

// Tests the Badged Reading List promo triggers if the user has opened Chrome
// enough times.
TEST_F(FeatureEngagementTest, TestBadgedReadingListTriggers) {
  feature_engagement::test::ScopedIphFeatureList list;
  list.InitAndEnableFeaturesWithParameters(
      {{feature_engagement::kIPHBadgedReadingListFeature,
        BadgedReadingListParams()}});

  std::unique_ptr<feature_engagement::Tracker> tracker =
      feature_engagement::CreateTestTracker();
  // Make sure tracker is initialized.
  tracker->AddOnInitializedCallback(BoolArgumentQuitClosure());
  run_loop_.Run();

  // Ensure that Chrome has been launched enough times for the Badged Reading
  // List to appear.
  for (int index = 0; index < kMinChromeOpensRequiredForReadingList; index++) {
    tracker->NotifyEvent(feature_engagement::events::kChromeOpened);
  }

  EXPECT_TRUE(tracker->ShouldTriggerHelpUI(
      feature_engagement::kIPHBadgedReadingListFeature));
}

// Tests the Badged Reading List promo does not trigger if the user has opened
// Chrome too few times.
TEST_F(FeatureEngagementTest, TestBadgedReadingListTooFewChromeOpens) {
  feature_engagement::test::ScopedIphFeatureList list;
  list.InitAndEnableFeaturesWithParameters(
      {{feature_engagement::kIPHBadgedReadingListFeature,
        BadgedReadingListParams()}});

  std::unique_ptr<feature_engagement::Tracker> tracker =
      feature_engagement::CreateTestTracker();
  // Make sure tracker is initialized.
  tracker->AddOnInitializedCallback(BoolArgumentQuitClosure());
  run_loop_.Run();

  // Only open Chrome once
  tracker->NotifyEvent(feature_engagement::events::kChromeOpened);

  EXPECT_FALSE(tracker->ShouldTriggerHelpUI(
      feature_engagement::kIPHBadgedReadingListFeature));
}

// Tests the Badged Reading List promo does not trigger a second time after it
// has already triggered once
TEST_F(FeatureEngagementTest, TestBadgedReadingListAlreadyUsed) {
  feature_engagement::test::ScopedIphFeatureList list;
  list.InitAndEnableFeaturesWithParameters(
      {{feature_engagement::kIPHBadgedReadingListFeature,
        BadgedReadingListParams()}});

  std::unique_ptr<feature_engagement::Tracker> tracker =
      feature_engagement::CreateTestTracker();
  // Make sure tracker is initialized.
  tracker->AddOnInitializedCallback(BoolArgumentQuitClosure());
  run_loop_.Run();

  // Ensure that Chrome has been launched enough times for the Badged Reading
  // List to appear.
  for (int index = 0; index < kMinChromeOpensRequiredForReadingList; index++) {
    tracker->NotifyEvent(feature_engagement::events::kChromeOpened);
  }

  EXPECT_TRUE(tracker->ShouldTriggerHelpUI(
      feature_engagement::kIPHBadgedReadingListFeature));

  // Dismiss IPH.
  tracker->Dismissed(feature_engagement::kIPHBadgedReadingListFeature);

  // The IPH should not trigger the second time
  EXPECT_FALSE(tracker->ShouldTriggerHelpUI(
      feature_engagement::kIPHBadgedReadingListFeature));
}

// Verifies that the Badged Manual Translate Trigger feature shows only once
// when the triggering conditions are met.
TEST_F(FeatureEngagementTest, TestBadgedTranslateManualTriggerShouldShowOnce) {
  feature_engagement::test::ScopedIphFeatureList list;
  list.InitAndEnableFeaturesWithParameters(
      {{feature_engagement::kIPHBadgedTranslateManualTriggerFeature,
        BadgedTranslateManualTriggerParams()}});

  std::unique_ptr<feature_engagement::Tracker> tracker =
      feature_engagement::CreateTestTracker();
  // Make sure tracker is initialized.
  tracker->AddOnInitializedCallback(BoolArgumentQuitClosure());
  run_loop_.Run();

  EXPECT_TRUE(tracker->ShouldTriggerHelpUI(
      feature_engagement::kIPHBadgedTranslateManualTriggerFeature));

  // Dismiss IPH.
  tracker->Dismissed(
      feature_engagement::kIPHBadgedTranslateManualTriggerFeature);

  // The IPH should not trigger the second time
  EXPECT_FALSE(tracker->ShouldTriggerHelpUI(
      feature_engagement::kIPHBadgedTranslateManualTriggerFeature));
}

// Verifes that the New Tab Tip Promo is triggered after the proper conditions
// are met.
TEST_F(FeatureEngagementTest, TestNewTabTipPromoShouldShow) {
  feature_engagement::test::ScopedIphFeatureList list;
  list.InitAndEnableFeaturesWithParameters(
      {{feature_engagement::kIPHNewTabTipFeature, NewTabTipPromoParams()}});

  std::unique_ptr<feature_engagement::Tracker> tracker =
      feature_engagement::CreateTestTracker();
  // Make sure tracker is initialized.
  tracker->AddOnInitializedCallback(BoolArgumentQuitClosure());
  run_loop_.Run();

  // Ensure that Chrome has been launched enough times to meet the trigger
  // condition.
  for (int index = 0; index < kMinChromeOpensRequiredForNewTabTip; index++) {
    tracker->NotifyEvent(feature_engagement::events::kChromeOpened);
  }

  EXPECT_TRUE(
      tracker->ShouldTriggerHelpUI(feature_engagement::kIPHNewTabTipFeature));
}

// Verifies that the bottom toolbar tip triggers.
TEST_F(FeatureEngagementTest, TestBottomToolbarTipTriggers) {
  feature_engagement::test::ScopedIphFeatureList list;
  list.InitAndEnableFeaturesWithParameters(
      {{feature_engagement::kIPHBottomToolbarTipFeature,
        BottomToolbarTipParams()}});

  std::unique_ptr<feature_engagement::Tracker> tracker =
      feature_engagement::CreateTestTracker();
  // Make sure tracker is initialized.
  tracker->AddOnInitializedCallback(BoolArgumentQuitClosure());
  run_loop_.Run();

  EXPECT_TRUE(tracker->ShouldTriggerHelpUI(
      feature_engagement::kIPHBottomToolbarTipFeature));
}

// Verifies that the longpress toolbar tip is displayed after the bottom toolbar
// tip is opened
TEST_F(FeatureEngagementTest, TestLongPressTipAppearAfterBottomToolbar) {
  feature_engagement::test::ScopedIphFeatureList list;
  list.InitAndEnableFeaturesWithParameters(
      {{feature_engagement::kIPHLongPressToolbarTipFeature,
        LongPressTipParams()}});

  std::unique_ptr<feature_engagement::Tracker> tracker =
      feature_engagement::CreateTestTracker();
  // Make sure tracker is initialized.
  tracker->AddOnInitializedCallback(BoolArgumentQuitClosure());
  run_loop_.Run();

  // Open the bottom toolbar.
  tracker->NotifyEvent(feature_engagement::events::kBottomToolbarOpened);

  EXPECT_TRUE(tracker->ShouldTriggerHelpUI(
      feature_engagement::kIPHLongPressToolbarTipFeature));
}

// Verifies that the IPH for Request desktop is triggered after 3 requests of
// the desktop version of a website.
TEST_F(FeatureEngagementTest, TestRequestDesktopTip) {
  feature_engagement::test::ScopedIphFeatureList list;
  list.InitAndEnableFeaturesWithParameters(
      {{feature_engagement::kIPHDefaultSiteViewFeature,
        DefaultSiteViewTipParams()}});

  std::unique_ptr<feature_engagement::Tracker> tracker =
      feature_engagement::CreateTestTracker();
  // Make sure tracker is initialized.
  tracker->AddOnInitializedCallback(BoolArgumentQuitClosure());
  run_loop_.Run();

  // Request the desktop version of a website, this should not trigger the tip.
  tracker->NotifyEvent(feature_engagement::events::kDesktopVersionRequested);
  EXPECT_FALSE(tracker->ShouldTriggerHelpUI(
      feature_engagement::kIPHDefaultSiteViewFeature));

  // Second time, still no tip.
  tracker->NotifyEvent(feature_engagement::events::kDesktopVersionRequested);
  EXPECT_FALSE(tracker->ShouldTriggerHelpUI(
      feature_engagement::kIPHDefaultSiteViewFeature));

  // Third time, this should trigger the tip.
  tracker->NotifyEvent(feature_engagement::events::kDesktopVersionRequested);
  EXPECT_TRUE(tracker->ShouldTriggerHelpUI(
      feature_engagement::kIPHDefaultSiteViewFeature));

  tracker->Dismissed(feature_engagement::kIPHDefaultSiteViewFeature);

  // Fourth time, the tip should no longer trigger.
  tracker->NotifyEvent(feature_engagement::events::kDesktopVersionRequested);
  EXPECT_FALSE(tracker->ShouldTriggerHelpUI(
      feature_engagement::kIPHDefaultSiteViewFeature));
}

// Verifies that the IPH for Request desktop is not triggered if the user
// interacted with the default page mode.
TEST_F(FeatureEngagementTest,
       TestRequestDesktopTipAfterChangingDefaultPageMode) {
  feature_engagement::test::ScopedIphFeatureList list;
  list.InitAndEnableFeaturesWithParameters(
      {{feature_engagement::kIPHDefaultSiteViewFeature,
        DefaultSiteViewTipParams()}});

  std::unique_ptr<feature_engagement::Tracker> tracker =
      feature_engagement::CreateTestTracker();
  // Make sure tracker is initialized.
  tracker->AddOnInitializedCallback(BoolArgumentQuitClosure());
  run_loop_.Run();

  // Interact with the default page mode.
  tracker->NotifyEvent(feature_engagement::events::kDefaultSiteViewUsed);

  // Request the desktop version of a website, this should not trigger the tip.
  tracker->NotifyEvent(feature_engagement::events::kDesktopVersionRequested);
  EXPECT_FALSE(tracker->ShouldTriggerHelpUI(
      feature_engagement::kIPHDefaultSiteViewFeature));

  // Second time, still no tip.
  tracker->NotifyEvent(feature_engagement::events::kDesktopVersionRequested);
  EXPECT_FALSE(tracker->ShouldTriggerHelpUI(
      feature_engagement::kIPHDefaultSiteViewFeature));

  // Third time, the tip should still not be shown.
  tracker->NotifyEvent(feature_engagement::events::kDesktopVersionRequested);
  EXPECT_FALSE(tracker->ShouldTriggerHelpUI(
      feature_engagement::kIPHDefaultSiteViewFeature));
}

// Verifies that the IPH for Pinned tab triggers after pinning a tab from
// the overflow menu.
TEST_F(FeatureEngagementTest, TestPinTabFromOverflowMenu) {
  feature_engagement::test::ScopedIphFeatureList list;
  list.InitAndEnableFeaturesWithParameters(
      {{feature_engagement::kIPHTabPinnedFeature, TabPinnedTipParams()}});

  std::unique_ptr<feature_engagement::Tracker> tracker =
      feature_engagement::CreateTestTracker();
  // Make sure tracker is initialized.
  tracker->AddOnInitializedCallback(BoolArgumentQuitClosure());
  run_loop_.Run();

  // Check that the badge is initially displayed.
  EXPECT_TRUE(
      tracker->ShouldTriggerHelpUI(feature_engagement::kIPHTabPinnedFeature));
  tracker->Dismissed(feature_engagement::kIPHTabPinnedFeature);

  // Check that the badge is not displayed a second time.
  EXPECT_FALSE(
      tracker->ShouldTriggerHelpUI(feature_engagement::kIPHTabPinnedFeature));
}
