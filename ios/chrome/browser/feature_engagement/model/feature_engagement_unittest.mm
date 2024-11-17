// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/test/scoped_mock_clock_override.h"
#import "base/test/task_environment.h"
#import "components/feature_engagement/public/event_constants.h"
#import "components/feature_engagement/public/feature_constants.h"
#import "components/feature_engagement/public/tracker.h"
#import "components/feature_engagement/test/scoped_iph_feature_list.h"
#import "components/feature_engagement/test/test_tracker.h"
#import "testing/platform_test.h"

namespace {

// The minimum number of times Chrome must be opened in order for the Reading
// List Badge to be shown.
const int kMinChromeOpensRequiredForReadingList = 5;

// The minimum number of times url must be opened in order for the History IPH
// to be displayed.
const int kMinURLOpensRequiredForHistoryOnOverflowMenuIPH = 2;

// The maximum number of times the Password Manager widget promo can be
// triggered.
const int kMaxPasswordManagerWidgetPromoIPH = 3;

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

  std::map<std::string, std::string> IPHiOSNewTabToolbarItemParams() {
    std::map<std::string, std::string> params;
    params["event_1"] =
        "name:open_url_from_omnibox;comparator:>=2;window:7;storage:7";
    params["event_trigger"] = "name:iph_new_tab_toolbar_item_trigger;"
                              "comparator:==0;window:7;storage:"
                              "7";
    params["event_used"] =
        "name:new_tab_toolbar_item_used;comparator:==0;window:365;storage:365";
    params["session_rate"] = "==0";
    params["availability"] = "any";
    return params;
  }

  std::map<std::string, std::string> IPHiOSPullToRefreshFeatureParams() {
    std::map<std::string, std::string> params;
    params["availability"] = "any";
    params["session_rate"] = "==0";
    params["event_used"] = "name:pull_to_refresh_feature_used;comparator:==0;"
                           "window:61;storage:61";
    params["event_trigger"] = "name:iph_pull_to_refresh_trigger;comparator:==0;"
                              "window:7;storage:7";
    // Default variation.
    params["event_1"] = "name:iph_pull_to_refresh_trigger;comparator:<1;"
                        "window:61;storage:61";
    params["event_2"] = "name:multi_gesture_refresh_used;"
                        "comparator:>=2;window:7;storage:"
                        "7";
    return params;
  }

  std::map<std::string, std::string>
  IPHiOSTabGridSwipeRightForIncognitoParams() {
    std::map<std::string, std::string> params;
    params["availability"] = "any";
    params["session_rate"] = "==0";
    params["event_used"] = "name:swipe_right_for_incognito_used;comparator:==0;"
                           "window:61;storage:61";
    params["event_trigger"] =
        "name:swipe_left_for_incognito_trigger;comparator:==0;"
        "window:7;storage:7";
    // Variation that allows >1 maximum impressions.
    params["event_1"] = "name:swipe_left_for_incognito_trigger;comparator:<2;"
                        "window:61;storage:61";
    params["event_2"] = "name:incognito_page_control_tapped;"
                        "comparator:>=2;window:7;storage:"
                        "7";
    return params;
  }

  std::map<std::string, std::string> IPHiOSHistoryOnOverflowMenuParams() {
    std::map<std::string, std::string> params;
    params["event_1"] =
        "name:open_url_from_omnibox;comparator:>=2;window:7;storage:30";
    params["event_trigger"] = "name:history_on_overflow_menu_trigger;"
                              "comparator:==0;window:7;storage:"
                              "7";
    params["event_used"] = "name:history_on_overflow_menu_used;comparator:==0;"
                           "window:365;storage:365";
    params["session_rate"] = "==0";
    params["availability"] = "any";
    return params;
  }

  std::map<std::string, std::string> IPHiOSSwipeBackForwardFeatureParams() {
    std::map<std::string, std::string> params;
    params["availability"] = "any";
    params["session_rate"] = "==0";
    params["event_used"] = "name:swiped_back_forward_used;comparator:==0;"
                           "window:61;storage:61";
    // Variation that allows one impression per month.
    params["event_trigger"] = "name:swipe_back_forward_trigger;comparator:==0;"
                              "window:30;storage:30";
    params["event_1"] = "name:swipe_back_forward_trigger;comparator:<2;"
                        "window:61;storage:61";
    params["event_2"] = "name:back_forward_button_tapped;"
                        "comparator:>=2;window:30;storage:"
                        "30";
    return params;
  }

  std::map<std::string, std::string>
  IPHiOSSwipeToolbarToChangeTabFeatureParams() {
    std::map<std::string, std::string> params;
    params["availability"] = "any";
    params["session_rate"] = "==0";
    params["event_used"] =
        "name:swipe_toolbar_to_change_tab_used;comparator:==0;"
        "window:61;storage:61";
    params["event_trigger"] =
        "name:swipe_toolbar_to_change_tab_trigger;comparator:==0;"
        "window:61;storage:61";
    params["event_1"] =
        "name:tab_grid_adjacent_tab_tapped;comparator:>=2;window:60;storage:61";
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

// Verifies that the pull down to refresh IPH is not triggered
// due to being used already.
TEST_F(FeatureEngagementTest, TestPullDownToRefreshIPHShouldNotShowAfterUsed) {
  feature_engagement::test::ScopedIphFeatureList list;
  list.InitAndEnableFeaturesWithParameters(
      {{feature_engagement::kIPHiOSPullToRefreshFeature,
        IPHiOSPullToRefreshFeatureParams()}});

  std::unique_ptr<feature_engagement::Tracker> tracker =
      feature_engagement::CreateTestTracker();
  // Make sure tracker is initialized.
  tracker->AddOnInitializedCallback(BoolArgumentQuitClosure());
  run_loop_.Run();
  // Ensure that the pull-to-refresh has been used to prevent triggering.
  tracker->NotifyEvent(feature_engagement::events::kIOSPullToRefreshUsed);
  // Make sure other prerequisites are met.
  tracker->NotifyEvent(feature_engagement::events::kIOSMultiGestureRefreshUsed);
  tracker->NotifyEvent(feature_engagement::events::kIOSMultiGestureRefreshUsed);
  EXPECT_FALSE(tracker->ShouldTriggerHelpUI(
      feature_engagement::kIPHiOSPullToRefreshFeature));
}

// Verifies that the pull down to refresh IPH only happens after the user uses a
// multi-gesture tap to refresh twice.
TEST_F(FeatureEngagementTest,
       TestTabGridPullDownToRefreshShouldNotShowBeforeTwoMutiGestureRefreshes) {
  feature_engagement::test::ScopedIphFeatureList list;
  list.InitAndEnableFeaturesWithParameters(
      {{feature_engagement::kIPHiOSPullToRefreshFeature,
        IPHiOSPullToRefreshFeatureParams()}});

  std::unique_ptr<feature_engagement::Tracker> tracker =
      feature_engagement::CreateTestTracker();
  // Make sure tracker is initialized.
  tracker->AddOnInitializedCallback(BoolArgumentQuitClosure());
  run_loop_.Run();

  // Perform the mult-gesture refresh twice. The IPH should only show after the
  // second refresh.
  tracker->NotifyEvent(feature_engagement::events::kIOSMultiGestureRefreshUsed);
  EXPECT_FALSE(tracker->ShouldTriggerHelpUI(
      feature_engagement::kIPHiOSPullToRefreshFeature));
  tracker->NotifyEvent(feature_engagement::events::kIOSMultiGestureRefreshUsed);
  EXPECT_TRUE(tracker->ShouldTriggerHelpUI(
      feature_engagement::kIPHiOSPullToRefreshFeature));
}

// Verifies that the pull down to refresh IPH would NOT be triggered more than
// allowed by frequency configurations.
TEST_F(
    FeatureEngagementTest,
    TestTabGridPullDownToRefreshIPHShouldBeTriggeredWithinFrequencyConstraints) {
  base::ScopedMockClockOverride scoped_clock;
  scoped_clock.Advance(base::Time::UnixEpoch() - base::Time());
  feature_engagement::test::ScopedIphFeatureList list;
  list.InitAndEnableFeaturesWithParameters(
      {{feature_engagement::kIPHiOSPullToRefreshFeature,
        IPHiOSPullToRefreshFeatureParams()}});

  std::unique_ptr<feature_engagement::Tracker> tracker =
      feature_engagement::CreateTestTracker();
  // Make sure tracker is initialized.
  tracker->AddOnInitializedCallback(BoolArgumentQuitClosure());
  run_loop_.Run();

  // Assume that the IPH is already triggered.
  tracker->NotifyEvent("iph_pull_to_refresh_trigger");
  // Tap button within the event window.
  scoped_clock.Advance(base::Days(59));
  tracker->NotifyEvent(feature_engagement::events::kIOSMultiGestureRefreshUsed);
  tracker->NotifyEvent(feature_engagement::events::kIOSMultiGestureRefreshUsed);
  EXPECT_FALSE(tracker->ShouldTriggerHelpUI(
      feature_engagement::kIPHiOSPullToRefreshFeature));
}

// Verifies that the swipe for incognito IPH on the tab grid is not triggered
// due to being used already.
TEST_F(FeatureEngagementTest,
       TestTabGridSwipeToIncognitoIPHShouldNotShowAfterUsed) {
  feature_engagement::test::ScopedIphFeatureList list;
  list.InitAndEnableFeaturesWithParameters(
      {{feature_engagement::kIPHiOSTabGridSwipeRightForIncognito,
        IPHiOSTabGridSwipeRightForIncognitoParams()}});

  std::unique_ptr<feature_engagement::Tracker> tracker =
      feature_engagement::CreateTestTracker();
  // Make sure tracker is initialized.
  tracker->AddOnInitializedCallback(BoolArgumentQuitClosure());
  run_loop_.Run();
  // Ensure that the swipe for incognito has been used to prevent triggering.
  tracker->NotifyEvent(
      feature_engagement::events::kIOSSwipeRightForIncognitoUsed);
  // Make sure other prerequisites are met.
  tracker->NotifyEvent(
      feature_engagement::events::kIOSIncognitoPageControlTapped);
  tracker->NotifyEvent(
      feature_engagement::events::kIOSIncognitoPageControlTapped);
  EXPECT_FALSE(tracker->ShouldTriggerHelpUI(
      feature_engagement::kIPHiOSTabGridSwipeRightForIncognito));
}

// Verifies that the swipe for incognito IPH only happens after the user taps to
// go to incognito twice.
TEST_F(FeatureEngagementTest,
       TestTabGridSwipeToIncognitoIPHShouldNotShowBeforeTwoTapsToIncognito) {
  feature_engagement::test::ScopedIphFeatureList list;
  list.InitAndEnableFeaturesWithParameters(
      {{feature_engagement::kIPHiOSTabGridSwipeRightForIncognito,
        IPHiOSTabGridSwipeRightForIncognitoParams()}});

  std::unique_ptr<feature_engagement::Tracker> tracker =
      feature_engagement::CreateTestTracker();
  // Make sure tracker is initialized.
  tracker->AddOnInitializedCallback(BoolArgumentQuitClosure());
  run_loop_.Run();

  // Tap the page control to go to incognito twice. The IPH should only show
  // after the second tap.
  tracker->NotifyEvent(
      feature_engagement::events::kIOSIncognitoPageControlTapped);
  EXPECT_FALSE(tracker->ShouldTriggerHelpUI(
      feature_engagement::kIPHiOSTabGridSwipeRightForIncognito));
  tracker->NotifyEvent(
      feature_engagement::events::kIOSIncognitoPageControlTapped);
  EXPECT_TRUE(tracker->ShouldTriggerHelpUI(
      feature_engagement::kIPHiOSTabGridSwipeRightForIncognito));
}

// Verifies that the swipe for incognito IPH would NOT be triggered more than
// allowed by frequency configurations.
TEST_F(
    FeatureEngagementTest,
    TestTabGridSwipeToIncognitoIPHShouldBeTriggeredWithinFrequencyConstraints) {
  base::ScopedMockClockOverride scoped_clock;
  scoped_clock.Advance(base::Time::UnixEpoch() - base::Time());
  feature_engagement::test::ScopedIphFeatureList list;
  list.InitAndEnableFeaturesWithParameters(
      {{feature_engagement::kIPHiOSTabGridSwipeRightForIncognito,
        IPHiOSTabGridSwipeRightForIncognitoParams()}});

  std::unique_ptr<feature_engagement::Tracker> tracker =
      feature_engagement::CreateTestTracker();
  // Make sure tracker is initialized.
  tracker->AddOnInitializedCallback(BoolArgumentQuitClosure());
  run_loop_.Run();

  // Assume that the IPH is already triggered.
  tracker->NotifyEvent("swipe_left_for_incognito_trigger");
  // Tap button to go to incognito immediately; the IPH should NOT be triggered.
  tracker->NotifyEvent(
      feature_engagement::events::kIOSIncognitoPageControlTapped);
  tracker->NotifyEvent(
      feature_engagement::events::kIOSIncognitoPageControlTapped);
  EXPECT_FALSE(tracker->ShouldTriggerHelpUI(
      feature_engagement::kIPHiOSTabGridSwipeRightForIncognito));
  // Do so 7 days later; the IPH should be triggered since it has been 7 days
  // after last trigger.
  scoped_clock.Advance(base::Days(7));
  tracker->NotifyEvent(
      feature_engagement::events::kIOSIncognitoPageControlTapped);
  tracker->NotifyEvent(
      feature_engagement::events::kIOSIncognitoPageControlTapped);
  EXPECT_TRUE(tracker->ShouldTriggerHelpUI(
      feature_engagement::kIPHiOSTabGridSwipeRightForIncognito));
  tracker->Dismissed(feature_engagement::kIPHiOSTabGridSwipeRightForIncognito);
  // Do so 7 days later. The IPH should NOT be triggered since it has already
  // been triggered twice within the last 61 days.
  scoped_clock.Advance(base::Days(7));
  tracker->NotifyEvent(
      feature_engagement::events::kIOSIncognitoPageControlTapped);
  tracker->NotifyEvent(
      feature_engagement::events::kIOSIncognitoPageControlTapped);
  EXPECT_FALSE(tracker->ShouldTriggerHelpUI(
      feature_engagement::kIPHiOSTabGridSwipeRightForIncognito));
}

// Verifies that the swipe back/forward IPH is not triggered
// due to being used already.
TEST_F(FeatureEngagementTest, TestSwipeBackForwardIPHShouldNotShowAfterUsed) {
  feature_engagement::test::ScopedIphFeatureList list;
  list.InitAndEnableFeaturesWithParameters(
      {{feature_engagement::kIPHiOSSwipeBackForwardFeature,
        IPHiOSSwipeBackForwardFeatureParams()}});

  std::unique_ptr<feature_engagement::Tracker> tracker =
      feature_engagement::CreateTestTracker();
  // Make sure tracker is initialized.
  tracker->AddOnInitializedCallback(BoolArgumentQuitClosure());
  run_loop_.Run();
  // Ensure that the swipe back/forward gesture has been used to prevent
  // triggering.
  tracker->NotifyEvent(feature_engagement::events::kIOSSwipeBackForwardUsed);
  // Make sure other prerequisites are met.
  tracker->NotifyEvent(feature_engagement::events::kIOSBackForwardButtonTapped);
  tracker->NotifyEvent(feature_engagement::events::kIOSBackForwardButtonTapped);
  EXPECT_FALSE(tracker->ShouldTriggerHelpUI(
      feature_engagement::kIPHiOSSwipeBackForwardFeature));
}

// Verifies that the swipe back/forward IPH only happens after the user taps to
// go back or forward twice.
TEST_F(FeatureEngagementTest,
       TestSwipeBackForwardIPHShouldNotShowBeforeTwoTapsToIncognito) {
  feature_engagement::test::ScopedIphFeatureList list;
  list.InitAndEnableFeaturesWithParameters(
      {{feature_engagement::kIPHiOSSwipeBackForwardFeature,
        IPHiOSSwipeBackForwardFeatureParams()}});

  std::unique_ptr<feature_engagement::Tracker> tracker =
      feature_engagement::CreateTestTracker();
  // Make sure tracker is initialized.
  tracker->AddOnInitializedCallback(BoolArgumentQuitClosure());
  run_loop_.Run();

  // Tap the back/forward button to navigate pages twice. The IPH should only
  // show after the second tap.
  tracker->NotifyEvent(feature_engagement::events::kIOSBackForwardButtonTapped);
  EXPECT_FALSE(tracker->ShouldTriggerHelpUI(
      feature_engagement::kIPHiOSSwipeBackForwardFeature));
  tracker->NotifyEvent(feature_engagement::events::kIOSBackForwardButtonTapped);
  EXPECT_TRUE(tracker->ShouldTriggerHelpUI(
      feature_engagement::kIPHiOSSwipeBackForwardFeature));
}

// Verifies that the swipe back/forward IPH would NOT be triggered more than
// allowed by frequency configurations.
TEST_F(FeatureEngagementTest,
       TestSwipeBackForwardIPHShouldBeTriggeredWithinFrequencyConstraints) {
  base::ScopedMockClockOverride scoped_clock;
  scoped_clock.Advance(base::Time::UnixEpoch() - base::Time());
  feature_engagement::test::ScopedIphFeatureList list;
  list.InitAndEnableFeaturesWithParameters(
      {{feature_engagement::kIPHiOSSwipeBackForwardFeature,
        IPHiOSSwipeBackForwardFeatureParams()}});

  std::unique_ptr<feature_engagement::Tracker> tracker =
      feature_engagement::CreateTestTracker();
  // Make sure tracker is initialized.
  tracker->AddOnInitializedCallback(BoolArgumentQuitClosure());
  run_loop_.Run();

  // Assume that the IPH is already triggered.
  tracker->NotifyEvent("swipe_back_forward_trigger");
  // Tap button to go to incognito immediately; the IPH should NOT be triggered.
  tracker->NotifyEvent(feature_engagement::events::kIOSBackForwardButtonTapped);
  tracker->NotifyEvent(feature_engagement::events::kIOSBackForwardButtonTapped);
  EXPECT_FALSE(tracker->ShouldTriggerHelpUI(
      feature_engagement::kIPHiOSSwipeBackForwardFeature));
  // Do so 7 days later; the IPH should NOT be triggered.
  scoped_clock.Advance(base::Days(7));
  tracker->NotifyEvent(feature_engagement::events::kIOSBackForwardButtonTapped);
  tracker->NotifyEvent(feature_engagement::events::kIOSBackForwardButtonTapped);
  EXPECT_FALSE(tracker->ShouldTriggerHelpUI(
      feature_engagement::kIPHiOSSwipeBackForwardFeature));
  // Check back 30 days after initial trigger.
  scoped_clock.Advance(base::Days(23));
  EXPECT_TRUE(tracker->ShouldTriggerHelpUI(
      feature_engagement::kIPHiOSSwipeBackForwardFeature));
}

// Verifies that the swipe on toolbar IPH is not triggered after the user has
// swiped on the toolbar to switch tab.
TEST_F(FeatureEngagementTest,
       TestSwipeToolbarToChangeTabIPHShouldNotShowAfterUsed) {
  feature_engagement::test::ScopedIphFeatureList list;
  list.InitAndEnableFeaturesWithParameters(
      {{feature_engagement::kIPHiOSSwipeToolbarToChangeTabFeature,
        IPHiOSSwipeToolbarToChangeTabFeatureParams()}});

  std::unique_ptr<feature_engagement::Tracker> tracker =
      feature_engagement::CreateTestTracker();
  // Make sure tracker is initialized.
  tracker->AddOnInitializedCallback(BoolArgumentQuitClosure());
  run_loop_.Run();
  // Ensure that the swipe back/forward gesture has been used to prevent
  // triggering.
  tracker->NotifyEvent(
      feature_engagement::events::kIOSSwipeToolbarToChangeTabUsed);
  // Make sure other prerequisites are met.
  tracker->NotifyEvent(
      feature_engagement::events::kIOSTabGridAdjacentTabTapped);
  tracker->NotifyEvent(
      feature_engagement::events::kIOSTabGridAdjacentTabTapped);
  EXPECT_FALSE(tracker->ShouldTriggerHelpUI(
      feature_engagement::kIPHiOSSwipeToolbarToChangeTabFeature));
}

// Verifies that the History IPH is triggered after the proper conditions
// are met.
TEST_F(FeatureEngagementTest, TestHistoryOnOverflowMenuIPHShouldShow) {
  feature_engagement::test::ScopedIphFeatureList list;
  list.InitAndEnableFeaturesWithParameters(
      {{feature_engagement::kIPHiOSHistoryOnOverflowMenuFeature,
        IPHiOSHistoryOnOverflowMenuParams()}});

  std::unique_ptr<feature_engagement::Tracker> tracker =
      feature_engagement::CreateTestTracker();
  // Make sure tracker is initialized.
  tracker->AddOnInitializedCallback(BoolArgumentQuitClosure());
  run_loop_.Run();

  // Ensure that url event is met.
  for (int index = 0; index < kMinURLOpensRequiredForHistoryOnOverflowMenuIPH;
       index++) {
    tracker->NotifyEvent(feature_engagement::events::kOpenUrlFromOmnibox);
  }

  EXPECT_TRUE(tracker->ShouldTriggerHelpUI(
      feature_engagement::kIPHiOSHistoryOnOverflowMenuFeature));
}

// Verifies that the History IPH is not triggered due to being used already.
TEST_F(FeatureEngagementTest,
       TestHistoryOnOverflowMenuIPHShouldNotShowDueToUsage) {
  feature_engagement::test::ScopedIphFeatureList list;
  list.InitAndEnableFeaturesWithParameters(
      {{feature_engagement::kIPHiOSHistoryOnOverflowMenuFeature,
        IPHiOSHistoryOnOverflowMenuParams()}});

  std::unique_ptr<feature_engagement::Tracker> tracker =
      feature_engagement::CreateTestTracker();
  // Make sure tracker is initialized.
  tracker->AddOnInitializedCallback(BoolArgumentQuitClosure());
  run_loop_.Run();

  // Ensure that the history has been used to prevent triggering the IPH.
  tracker->NotifyEvent(feature_engagement::events::kHistoryOnOverflowMenuUsed);

  // Ensure that the url event condition is met.
  for (int index = 0; index < kMinURLOpensRequiredForHistoryOnOverflowMenuIPH;
       index++) {
    tracker->NotifyEvent(feature_engagement::events::kOpenUrlFromOmnibox);
  }

  EXPECT_FALSE(tracker->ShouldTriggerHelpUI(
      feature_engagement::kIPHiOSHistoryOnOverflowMenuFeature));
}

// Verifies that the History IPH is not triggered due to not enough urls have
// been opened.
TEST_F(FeatureEngagementTest,
       TestHistoryOnOverflowMenuIPHShouldNotShowDueToNotEnoughUrls) {
  feature_engagement::test::ScopedIphFeatureList list;
  list.InitAndEnableFeaturesWithParameters(
      {{feature_engagement::kIPHiOSHistoryOnOverflowMenuFeature,
        IPHiOSHistoryOnOverflowMenuParams()}});

  std::unique_ptr<feature_engagement::Tracker> tracker =
      feature_engagement::CreateTestTracker();
  // Make sure tracker is initialized.
  tracker->AddOnInitializedCallback(BoolArgumentQuitClosure());
  run_loop_.Run();

  // Ensure that < `kMinURLOpensRequiredForHistoryOnOverflowMenuIPH` urls have
  // been opened.
  tracker->NotifyEvent(feature_engagement::events::kOpenUrlFromOmnibox);

  EXPECT_FALSE(tracker->ShouldTriggerHelpUI(
      feature_engagement::kIPHiOSHistoryOnOverflowMenuFeature));
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

// Verifies that the Password Manager widget promo IPH can't be triggered again
// after being triggered three times.
TEST_F(FeatureEngagementTest, TestPasswordManagerPromoIPHReachedTriggerLimit) {
  feature_engagement::test::ScopedIphFeatureList list;
  list.InitWithExistingFeatures(
      {feature_engagement::kIPHiOSPromoPasswordManagerWidgetFeature});

  std::unique_ptr<feature_engagement::Tracker> tracker =
      feature_engagement::CreateTestTracker();
  // Make sure tracker is initialized.
  tracker->AddOnInitializedCallback(BoolArgumentQuitClosure());
  run_loop_.Run();

  // Ensure that the Password Manager widget promo has been triggered three
  // times.
  for (int index = 0; index < kMaxPasswordManagerWidgetPromoIPH; index++) {
    tracker->NotifyEvent(
        feature_engagement::events::kPasswordManagerWidgetPromoTriggered);
  }

  EXPECT_FALSE(tracker->ShouldTriggerHelpUI(
      feature_engagement::kIPHiOSPromoPasswordManagerWidgetFeature));
}

// Verifies that the Password Manager widget promo IPH is not triggered again
// after the widget was used at least once.
TEST_F(FeatureEngagementTest, TestPasswordManagerPromoIPHReachedUsedLimit) {
  feature_engagement::test::ScopedIphFeatureList list;
  list.InitWithExistingFeatures(
      {feature_engagement::kIPHiOSPromoPasswordManagerWidgetFeature});

  std::unique_ptr<feature_engagement::Tracker> tracker =
      feature_engagement::CreateTestTracker();
  // Make sure tracker is initialized.
  tracker->AddOnInitializedCallback(BoolArgumentQuitClosure());
  run_loop_.Run();

  EXPECT_TRUE(tracker->ShouldTriggerHelpUI(
      feature_engagement::kIPHiOSPromoPasswordManagerWidgetFeature));
  tracker->Dismissed(
      feature_engagement::kIPHiOSPromoPasswordManagerWidgetFeature);

  // Interact with the Password Manager widget.
  tracker->NotifyEvent(
      feature_engagement::events::kPasswordManagerWidgetPromoUsed);

  EXPECT_FALSE(tracker->ShouldTriggerHelpUI(
      feature_engagement::kIPHiOSPromoPasswordManagerWidgetFeature));
}

// Verifies that the Password Manager widget promo IPH is not triggered again
// after the promo was closed by the user.
TEST_F(FeatureEngagementTest, TestPasswordManagerPromoIPHWasClosed) {
  feature_engagement::test::ScopedIphFeatureList list;
  list.InitWithExistingFeatures(
      {feature_engagement::kIPHiOSPromoPasswordManagerWidgetFeature});

  std::unique_ptr<feature_engagement::Tracker> tracker =
      feature_engagement::CreateTestTracker();
  // Make sure tracker is initialized.
  tracker->AddOnInitializedCallback(BoolArgumentQuitClosure());
  run_loop_.Run();

  EXPECT_TRUE(tracker->ShouldTriggerHelpUI(
      feature_engagement::kIPHiOSPromoPasswordManagerWidgetFeature));
  tracker->Dismissed(
      feature_engagement::kIPHiOSPromoPasswordManagerWidgetFeature);

  // Close the Password Manager widget promo.
  tracker->NotifyEvent(
      feature_engagement::events::kPasswordManagerWidgetPromoClosed);

  EXPECT_FALSE(tracker->ShouldTriggerHelpUI(
      feature_engagement::kIPHiOSPromoPasswordManagerWidgetFeature));
}

// Verifies that the Overflow Menu Customization promo IPH is only triggered
// after the user has chosen items that are offscreen by default at least twice.
TEST_F(FeatureEngagementTest, TestOverflowMenuCustomizationPromoShows) {
  feature_engagement::test::ScopedIphFeatureList list;
  list.InitWithExistingFeatures(
      {feature_engagement::kIPHiOSOverflowMenuCustomizationFeature});

  std::unique_ptr<feature_engagement::Tracker> tracker =
      feature_engagement::CreateTestTracker();
  // Make sure tracker is initialized.
  tracker->AddOnInitializedCallback(BoolArgumentQuitClosure());
  run_loop_.Run();

  EXPECT_FALSE(tracker->ShouldTriggerHelpUI(
      feature_engagement::kIPHiOSOverflowMenuCustomizationFeature));

  tracker->NotifyEvent(
      feature_engagement::events::kIOSOverflowMenuOffscreenItemUsed);
  tracker->NotifyEvent(
      feature_engagement::events::kIOSOverflowMenuOffscreenItemUsed);

  EXPECT_TRUE(tracker->ShouldTriggerHelpUI(
      feature_engagement::kIPHiOSOverflowMenuCustomizationFeature));
  tracker->Dismissed(
      feature_engagement::kIPHiOSOverflowMenuCustomizationFeature);
}

// Verifies that the Overflow Menu Customization promo IPH is not triggered
// after the user has used the customization screen.
TEST_F(FeatureEngagementTest,
       TestOverflowMenuCustomizationPromoDoesntShowIfCustomizationUsed) {
  feature_engagement::test::ScopedIphFeatureList list;
  list.InitWithExistingFeatures(
      {feature_engagement::kIPHiOSOverflowMenuCustomizationFeature});

  std::unique_ptr<feature_engagement::Tracker> tracker =
      feature_engagement::CreateTestTracker();
  // Make sure tracker is initialized.
  tracker->AddOnInitializedCallback(BoolArgumentQuitClosure());
  run_loop_.Run();

  EXPECT_FALSE(tracker->ShouldTriggerHelpUI(
      feature_engagement::kIPHiOSOverflowMenuCustomizationFeature));

  // Using offscreen items twice should trigger the promo...
  tracker->NotifyEvent(
      feature_engagement::events::kIOSOverflowMenuOffscreenItemUsed);
  tracker->NotifyEvent(
      feature_engagement::events::kIOSOverflowMenuOffscreenItemUsed);

  EXPECT_TRUE(tracker->WouldTriggerHelpUI(
      feature_engagement::kIPHiOSOverflowMenuCustomizationFeature));

  // Unless the user has also used customization already.
  tracker->NotifyEvent(
      feature_engagement::events::kIOSOverflowMenuCustomizationUsed);

  EXPECT_FALSE(tracker->ShouldTriggerHelpUI(
      feature_engagement::kIPHiOSOverflowMenuCustomizationFeature));
}

// Verifies that the Enhanced Safe Browsing Inline Promo IPH is triggered when
// notified and not triggered when the promo is closed by the user.
TEST_F(FeatureEngagementTest, TestEnhancedSafeBrowsingInlinePromoIsShown) {
  feature_engagement::test::ScopedIphFeatureList list;
  list.InitAndEnableFeatures(
      {feature_engagement::kIPHiOSInlineEnhancedSafeBrowsingPromoFeature});

  std::unique_ptr<feature_engagement::Tracker> tracker =
      feature_engagement::CreateTestTracker();
  // Make sure tracker is initialized.
  tracker->AddOnInitializedCallback(BoolArgumentQuitClosure());
  run_loop_.Run();

  EXPECT_FALSE(tracker->ShouldTriggerHelpUI(
      feature_engagement::kIPHiOSInlineEnhancedSafeBrowsingPromoFeature));

  // Executing one of the pre-determined events should trigger the promo...
  tracker->NotifyEvent(
      feature_engagement::events::kEnhancedSafeBrowsingPromoCriterionMet);

  EXPECT_TRUE(tracker->WouldTriggerHelpUI(
      feature_engagement::kIPHiOSInlineEnhancedSafeBrowsingPromoFeature));

  // The promo should not be displayed if the user taps the promo's 'x' button.
  tracker->NotifyEvent(
      feature_engagement::events::kInlineEnhancedSafeBrowsingPromoClosed);

  EXPECT_FALSE(tracker->ShouldTriggerHelpUI(
      feature_engagement::kIPHiOSInlineEnhancedSafeBrowsingPromoFeature));
}
