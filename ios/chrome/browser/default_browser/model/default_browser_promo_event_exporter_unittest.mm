// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/default_browser/model/default_browser_promo_event_exporter.h"

#import "base/run_loop.h"
#import "base/test/ios/wait_util.h"
#import "components/feature_engagement/public/event_constants.h"
#import "components/feature_engagement/public/tracker.h"
#import "components/feature_engagement/test/test_tracker.h"
#import "ios/chrome/browser/default_browser/model/default_browser_interest_signals.h"
#import "ios/chrome/browser/default_browser/model/utils.h"
#import "ios/chrome/browser/default_browser/model/utils_test_support.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"

namespace {
constexpr base::TimeDelta kMoreThan3Day = base::Days(3) + base::Minutes(1);
constexpr base::TimeDelta kMoreThan30Days = base::Days(30) + base::Minutes(1);
}  // namespace

class DefaultBrowserEventExporterTest : public PlatformTest {
 public:
  DefaultBrowserEventExporterTest() {}
  ~DefaultBrowserEventExporterTest() override {}
  base::RepeatingCallback<void(bool)> BoolArgumentQuitClosure() {
    return base::IgnoreArgs<bool>(run_loop_.QuitClosure());
  }

 protected:
  void SetUp() override {
    PlatformTest::SetUp();
    ClearDefaultBrowserPromoData();
  }
  void TearDown() override {
    ClearDefaultBrowserPromoData();
    PlatformTest::TearDown();
  }

  void RequestExportEventsAndVerifyCallback() {
    __block bool callback_called = false;
    feature_engagement::TrackerEventExporter::ExportEventsCallback callback =
        base::BindOnce(
            ^(const std::vector<
                feature_engagement::TrackerEventExporter::EventData> events) {
              export_events_ = events;
              callback_called = true;
            });

    DefaultBrowserEventExporter* exporter = new DefaultBrowserEventExporter();
    exporter->ExportEvents(std::move(callback));

    EXPECT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
        base::test::ios::kWaitForActionTimeout, ^bool() {
          base::RunLoop().RunUntilIdle();
          return callback_called;
        }));
  }

  int GetExportEventsCount() { return export_events_.size(); }

  // Initializes the feature engagement tracker  with the default browser
  // exporter and set basic common conditions for default browser promos.
  void InitTrackerAndSetBasicConditions() {
    // Initialize tracker with the default browser exporter.
    tracker_ = feature_engagement::CreateTestTracker(
        std::make_unique<DefaultBrowserEventExporter>());

    // Make sure tracker is initialized.
    tracker_->AddOnInitializedCallback(BoolArgumentQuitClosure());
    run_loop_.Run();

    // Promos can be displayed only after Chrome opened 7 times.
    SatisfyChromeOpenCondition();
  }

  void SatisfyChromeOpenCondition() {
    // Promos can be displayed only after Chrome opened 7 times.
    for (int i = 0; i < 7; i++) {
      tracker_->NotifyEvent(feature_engagement::events::kChromeOpened);
    }
  }

  web::WebTaskEnvironment task_environment_;
  base::RunLoop run_loop_;
  std::unique_ptr<feature_engagement::Tracker> tracker_;
  std::vector<feature_engagement::TrackerEventExporter::EventData>
      export_events_;
};

TEST_F(DefaultBrowserEventExporterTest, TestFRETimestampMigration) {
  // No events to export.
  RequestExportEventsAndVerifyCallback();
  EXPECT_EQ(GetExportEventsCount(), 0);

  // When there is a FRE event, it should be exported.
  ClearDefaultBrowserPromoData();
  LogUserInteractionWithFirstRunPromo();

  RequestExportEventsAndVerifyCallback();
  EXPECT_EQ(GetExportEventsCount(), 1);

  // Second time there shouldn't be any events to export.
  RequestExportEventsAndVerifyCallback();
  EXPECT_EQ(GetExportEventsCount(), 0);

  // When FRE happened after migration, there shouldn't be any events to export.
  ClearDefaultBrowserPromoData();
  default_browser::NotifyDefaultBrowserFREPromoShown(nullptr);

  RequestExportEventsAndVerifyCallback();
  EXPECT_EQ(GetExportEventsCount(), 0);
}

TEST_F(DefaultBrowserEventExporterTest, TestPromoInterestEventsMigration) {
  // No events to export.
  RequestExportEventsAndVerifyCallback();
  EXPECT_EQ(GetExportEventsCount(), 0);

  // Check when there is only 1 event.
  ClearDefaultBrowserPromoData();
  LogLikelyInterestedDefaultBrowserUserActivity(DefaultPromoTypeGeneral);

  RequestExportEventsAndVerifyCallback();
  EXPECT_EQ(GetExportEventsCount(), 1);

  // Check that exporting second time will not have any events.
  RequestExportEventsAndVerifyCallback();
  EXPECT_EQ(GetExportEventsCount(), 0);

  // Check when there are 2 events for the same promo type
  ClearDefaultBrowserPromoData();
  LogLikelyInterestedDefaultBrowserUserActivity(DefaultPromoTypeGeneral);
  LogLikelyInterestedDefaultBrowserUserActivity(DefaultPromoTypeGeneral);

  RequestExportEventsAndVerifyCallback();
  EXPECT_EQ(GetExportEventsCount(), 2);

  // Check when there are events for all 4 promo types.
  ClearDefaultBrowserPromoData();
  LogLikelyInterestedDefaultBrowserUserActivity(DefaultPromoTypeGeneral);
  LogLikelyInterestedDefaultBrowserUserActivity(DefaultPromoTypeAllTabs);
  LogLikelyInterestedDefaultBrowserUserActivity(DefaultPromoTypeMadeForIOS);
  LogLikelyInterestedDefaultBrowserUserActivity(DefaultPromoTypeStaySafe);

  RequestExportEventsAndVerifyCallback();
  EXPECT_EQ(GetExportEventsCount(), 4);
}

TEST_F(DefaultBrowserEventExporterTest, TestPromoImpressionsMigration) {
  // No events to export.
  RequestExportEventsAndVerifyCallback();
  EXPECT_EQ(GetExportEventsCount(), 0);

  // Check when there is 1 generic promo, it should create 1 event to export.
  ClearDefaultBrowserPromoData();
  LogUserInteractionWithFullscreenPromo();

  RequestExportEventsAndVerifyCallback();
  EXPECT_EQ(GetExportEventsCount(), 1);

  // Check that exporting second time will not have any events.
  RequestExportEventsAndVerifyCallback();
  EXPECT_EQ(GetExportEventsCount(), 0);

  // Check when there is 1 tailored promo it should create 4 events to export.
  ClearDefaultBrowserPromoData();
  LogUserInteractionWithTailoredFullscreenPromo();

  RequestExportEventsAndVerifyCallback();
  EXPECT_EQ(GetExportEventsCount(), 4);

  // Check when there is 1 generic and 1 tailored promo, should create 5 events
  // to export.
  ClearDefaultBrowserPromoData();
  LogUserInteractionWithFullscreenPromo();
  LogUserInteractionWithTailoredFullscreenPromo();

  RequestExportEventsAndVerifyCallback();
  EXPECT_EQ(GetExportEventsCount(), 5);
}

// Checks that none of the promos triggers when there are no events exported.
TEST_F(DefaultBrowserEventExporterTest, TestMigrationNoEvents) {
  // Initialize tracker.
  InitTrackerAndSetBasicConditions();

  // None of the promos should trigger.
  EXPECT_FALSE(tracker_->WouldTriggerHelpUI(
      feature_engagement::kIPHiOSPromoGenericDefaultBrowserFeature));
  EXPECT_FALSE(tracker_->WouldTriggerHelpUI(
      feature_engagement::kIPHiOSPromoAllTabsFeature));
  EXPECT_FALSE(tracker_->WouldTriggerHelpUI(
      feature_engagement::kIPHiOSPromoMadeForIOSFeature));
  EXPECT_FALSE(tracker_->WouldTriggerHelpUI(
      feature_engagement::kIPHiOSPromoStaySafeFeature));
}

// Checks that none of the promos triggers even when conditions are met when
// cooldown from FRE is not satisfied.
TEST_F(DefaultBrowserEventExporterTest, TestMigrationFRECooldown) {
  // Write to user defaults before creating the tracker.
  LogUserInteractionWithFirstRunPromo();
  LogLikelyInterestedDefaultBrowserUserActivity(DefaultPromoTypeGeneral);
  LogLikelyInterestedDefaultBrowserUserActivity(DefaultPromoTypeAllTabs);
  LogLikelyInterestedDefaultBrowserUserActivity(DefaultPromoTypeMadeForIOS);
  LogLikelyInterestedDefaultBrowserUserActivity(DefaultPromoTypeStaySafe);

  // Initialize tracker.
  InitTrackerAndSetBasicConditions();

  // None of the promos should trigger.
  EXPECT_FALSE(tracker_->WouldTriggerHelpUI(
      feature_engagement::kIPHiOSPromoGenericDefaultBrowserFeature));
  EXPECT_FALSE(tracker_->WouldTriggerHelpUI(
      feature_engagement::kIPHiOSPromoAllTabsFeature));
  EXPECT_FALSE(tracker_->WouldTriggerHelpUI(
      feature_engagement::kIPHiOSPromoMadeForIOSFeature));
  EXPECT_FALSE(tracker_->WouldTriggerHelpUI(
      feature_engagement::kIPHiOSPromoStaySafeFeature));
}

// Checks that none of the promos triggers even when conditions are not met but
// cooldown from FRE is ok.
TEST_F(DefaultBrowserEventExporterTest, TestMigrationConditionsNotMet) {
  // Write to user defaults before creating the tracker.
  SimulateUserInteractionWithPromos(kMoreThan3Day, /*interectedWithFRE=*/true,
                                    /*genericCount=*/0, /*tailoredCount=*/0,
                                    /*totalCount=*/1);

  // Initialize tracker.
  InitTrackerAndSetBasicConditions();

  // None of the promos should trigger.
  EXPECT_FALSE(tracker_->WouldTriggerHelpUI(
      feature_engagement::kIPHiOSPromoGenericDefaultBrowserFeature));
  EXPECT_FALSE(tracker_->WouldTriggerHelpUI(
      feature_engagement::kIPHiOSPromoAllTabsFeature));
  EXPECT_FALSE(tracker_->WouldTriggerHelpUI(
      feature_engagement::kIPHiOSPromoMadeForIOSFeature));
  EXPECT_FALSE(tracker_->WouldTriggerHelpUI(
      feature_engagement::kIPHiOSPromoStaySafeFeature));
}

// Checks that promos trigger when conditions are met and
// cooldown from FRE is ok.
TEST_F(DefaultBrowserEventExporterTest, TestMigrationConditionsMet) {
  // Write to user defaults before creating the tracker.
  SimulateUserInteractionWithPromos(kMoreThan3Day, /*interectedWithFRE=*/true,
                                    /*genericCount=*/0, /*tailoredCount=*/0,
                                    /*totalCount=*/1);
  LogLikelyInterestedDefaultBrowserUserActivity(DefaultPromoTypeGeneral);
  LogLikelyInterestedDefaultBrowserUserActivity(DefaultPromoTypeAllTabs);
  LogLikelyInterestedDefaultBrowserUserActivity(DefaultPromoTypeMadeForIOS);
  LogLikelyInterestedDefaultBrowserUserActivity(DefaultPromoTypeStaySafe);

  // Initialize tracker.
  InitTrackerAndSetBasicConditions();

  // All promos would trigger.
  EXPECT_TRUE(tracker_->WouldTriggerHelpUI(
      feature_engagement::kIPHiOSPromoGenericDefaultBrowserFeature));
  EXPECT_TRUE(tracker_->WouldTriggerHelpUI(
      feature_engagement::kIPHiOSPromoAllTabsFeature));
  EXPECT_TRUE(tracker_->WouldTriggerHelpUI(
      feature_engagement::kIPHiOSPromoMadeForIOSFeature));
  EXPECT_TRUE(tracker_->WouldTriggerHelpUI(
      feature_engagement::kIPHiOSPromoStaySafeFeature));
}

// Checks that generic promo doesn't tigger second time.
TEST_F(DefaultBrowserEventExporterTest, TestMigrationSecondGenericPromo) {
  // Write to user defaults before creating the tracker.
  SimulateUserInteractionWithPromos(kMoreThan30Days, /*interectedWithFRE=*/true,
                                    /*genericCount=*/1, /*tailoredCount=*/0,
                                    /*totalCount=*/2);
  LogLikelyInterestedDefaultBrowserUserActivity(DefaultPromoTypeGeneral);
  LogLikelyInterestedDefaultBrowserUserActivity(DefaultPromoTypeAllTabs);
  LogLikelyInterestedDefaultBrowserUserActivity(DefaultPromoTypeMadeForIOS);
  LogLikelyInterestedDefaultBrowserUserActivity(DefaultPromoTypeStaySafe);

  // Initialize tracker.
  InitTrackerAndSetBasicConditions();

  // Tailored will trigger but generic will not.
  EXPECT_FALSE(tracker_->WouldTriggerHelpUI(
      feature_engagement::kIPHiOSPromoGenericDefaultBrowserFeature));
  EXPECT_TRUE(tracker_->WouldTriggerHelpUI(
      feature_engagement::kIPHiOSPromoAllTabsFeature));
  EXPECT_TRUE(tracker_->WouldTriggerHelpUI(
      feature_engagement::kIPHiOSPromoMadeForIOSFeature));
  EXPECT_TRUE(tracker_->WouldTriggerHelpUI(
      feature_engagement::kIPHiOSPromoStaySafeFeature));
}

// Checks that tailored promos don't trigger second time.
TEST_F(DefaultBrowserEventExporterTest, TestMigrationSecondTailoredPromo) {
  // Write to user defaults before creating the tracker.
  SimulateUserInteractionWithPromos(kMoreThan30Days, /*interectedWithFRE=*/true,
                                    /*genericCount=*/0, /*tailoredCount=*/1,
                                    /*totalCount=*/2);
  LogLikelyInterestedDefaultBrowserUserActivity(DefaultPromoTypeGeneral);
  LogLikelyInterestedDefaultBrowserUserActivity(DefaultPromoTypeAllTabs);
  LogLikelyInterestedDefaultBrowserUserActivity(DefaultPromoTypeMadeForIOS);
  LogLikelyInterestedDefaultBrowserUserActivity(DefaultPromoTypeStaySafe);

  // Initialize tracker.
  InitTrackerAndSetBasicConditions();

  // Generic will trigger but tailored will not.
  EXPECT_TRUE(tracker_->WouldTriggerHelpUI(
      feature_engagement::kIPHiOSPromoGenericDefaultBrowserFeature));
  EXPECT_FALSE(tracker_->WouldTriggerHelpUI(
      feature_engagement::kIPHiOSPromoAllTabsFeature));
  EXPECT_FALSE(tracker_->WouldTriggerHelpUI(
      feature_engagement::kIPHiOSPromoMadeForIOSFeature));
  EXPECT_FALSE(tracker_->WouldTriggerHelpUI(
      feature_engagement::kIPHiOSPromoStaySafeFeature));
}

// Checks that generic promo doesn't trigger when conditions are met but are too
// old.
TEST_F(DefaultBrowserEventExporterTest, TestMigrationGenericOldCondition) {
  // Write to user defaults before creating the tracker.
  SimulateUserInterestedDefaultBrowserUserActivity(DefaultPromoTypeGeneral,
                                                   kMoreThan30Days);
  SimulateUserInteractionWithPromos(kMoreThan30Days, /*interectedWithFRE=*/true,
                                    /*genericCount=*/0, /*tailoredCount=*/0,
                                    /*totalCount=*/1);

  // Initialize tracker.
  InitTrackerAndSetBasicConditions();

  // Generic promo should not trigger.
  EXPECT_FALSE(tracker_->WouldTriggerHelpUI(
      feature_engagement::kIPHiOSPromoGenericDefaultBrowserFeature));
}

// Checks that tailored promo doesn't trigger when conditions are met but are
// too old.
TEST_F(DefaultBrowserEventExporterTest, TestMigrationTailoredOldCondition) {
  // Write to user defaults before creating the tracker.
  SimulateUserInterestedDefaultBrowserUserActivity(DefaultPromoTypeAllTabs,
                                                   kMoreThan30Days);
  SimulateUserInteractionWithPromos(kMoreThan30Days, /*interectedWithFRE=*/true,
                                    /*genericCount=*/0, /*tailoredCount=*/0,
                                    /*totalCount=*/1);

  // Initialize tracker.
  InitTrackerAndSetBasicConditions();

  // All tabs promo should not trigger.
  EXPECT_FALSE(tracker_->WouldTriggerHelpUI(
      feature_engagement::kIPHiOSPromoAllTabsFeature));
}
