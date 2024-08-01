// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/docking_promo/coordinator/docking_promo_mediator.h"

#import "base/files/file.h"
#import "base/test/scoped_feature_list.h"
#import "base/threading/thread_restrictions.h"
#import "base/time/time.h"
#import "components/prefs/pref_registry_simple.h"
#import "components/prefs/pref_service.h"
#import "components/startup_metric_utils/browser/startup_metric_utils.h"
#import "ios/chrome/app/application_delegate/app_state.h"
#import "ios/chrome/browser/default_browser/model/utils_test_support.h"
#import "ios/chrome/browser/docking_promo/ui/docking_promo_consumer.h"
#import "ios/chrome/browser/first_run/model/first_run.h"
#import "ios/chrome/browser/promos_manager/model/mock_promos_manager.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/public/commands/docking_promo_commands.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/chrome/test/testing_application_context.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {

NSString* kFirstRunRecencyKey = @"FirstRunRecency";
NSString* kStartSurfaceSceneEnterIntoBackgroundTime =
    @"StartSurfaceSceneEnterIntoBackgroundTime";

}  // namespace

// Test fixture for testing the DockingPromoMediator class.
class DockingPromoMediatorTest : public PlatformTest {
 public:
  void SetUp() override {
    ClearUsageData();
    WriteFirstRunSentinel();
  }

  void TearDown() override {
    ClearUsageData();
    if (FirstRun::RemoveSentinel()) {
      FirstRun::LoadSentinelInfo();
      FirstRun::ClearStateForTesting();
    }
  }

 protected:
  DockingPromoMediatorTest() { EnableDockingPromoFlag(); }

  void EnableDockingPromoFlag() {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {{kIOSDockingPromo, {{kIOSDockingPromoExperimentType, "1"}}}},
        {/* disabled_features */});
  }

  void CreateDockingPromoMediator(base::TimeDelta time_since_last_foreground) {
    promos_manager_ = std::make_unique<MockPromosManager>();
    consumer_ = OCMProtocolMock(@protocol(DockingPromoConsumer));

    mediator_ = [[DockingPromoMediator alloc]
          initWithPromosManager:promos_manager_.get()
        timeSinceLastForeground:time_since_last_foreground];

    mediator_.consumer = consumer_;
  }

  void ExpectConsumerSetFieldsForPromo() {
    NSString* title_string =
        l10n_util::GetNSString(IDS_IOS_DOCKING_PROMO_TITLE);
    NSString* primary_action_string =
        l10n_util::GetNSString(IDS_IOS_DOCKING_PROMO_PRIMARY_BUTTON_TITLE);
    NSString* secondary_action_string =
        l10n_util::GetNSString(IDS_IOS_DOCKING_PROMO_SECONDARY_BUTTON_TITLE);
    NSString* animation_name = @"docking_promo";

    OCMExpect([consumer_ setTitleString:title_string
                    primaryActionString:primary_action_string
                  secondaryActionString:secondary_action_string
                          animationName:animation_name]);
  }

  // Sets the First Run occurred `days_ago`.
  void SetFirstRunRecency(NSInteger days_ago) {
    [[NSUserDefaults standardUserDefaults] setInteger:days_ago
                                               forKey:kFirstRunRecencyKey];
  }

  // Clears the First Run Recency and Start Surface Session data. Used before
  // each test to ensure a clean state.
  void ClearUsageData() {
    [[NSUserDefaults standardUserDefaults]
        removeObjectForKey:kFirstRunRecencyKey];
    ClearDefaultBrowserPromoData();
  }

  void WriteFirstRunSentinel() {
    base::ScopedAllowBlockingForTesting allow_blocking;
    FirstRun::RemoveSentinel();
    base::File::Error file_error = base::File::FILE_OK;
    startup_metric_utils::FirstRunSentinelCreationResult sentinel_created =
        FirstRun::CreateSentinel(&file_error);
    ASSERT_EQ(sentinel_created,
              startup_metric_utils::FirstRunSentinelCreationResult::kSuccess)
        << "Error creating FirstRun sentinel: "
        << base::File::ErrorToString(file_error);
    FirstRun::LoadSentinelInfo();
    FirstRun::ClearStateForTesting();
    EXPECT_FALSE(FirstRun::IsChromeFirstRun());
  }

  std::unique_ptr<MockPromosManager> promos_manager_;
  DockingPromoMediator* mediator_;
  base::test::ScopedFeatureList scoped_feature_list_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  id consumer_;
};

// Tests that promo is eligible for display if:
// - The user is no more than 14 days old
// - The user's last activity happened 10 days ago.
TEST_F(DockingPromoMediatorTest,
       ShouldShowDockingPromoForTwoWeekOldInactiveUsers) {
  CreateDockingPromoMediator(base::Days(10));
  SetFirstRunRecency(14);

  EXPECT_TRUE([mediator_ canShowDockingPromo]);
}

// Tests that promo is eligible for display if:
// - The user is no more than 14 days old
// - The user's last activity happened more than 3 days ago.
TEST_F(DockingPromoMediatorTest,
       ShouldShowDockingPromoForTwoWeekOldRecentlyInactiveUsers) {
  CreateDockingPromoMediator(base::Days(3) + base::Seconds(1));
  SetFirstRunRecency(14);

  EXPECT_TRUE([mediator_ canShowDockingPromo]);
}

// Tests that promo is eligible for display if:
// - The user is no more than 2 days old.
// - The user's last activity happened more than 1 day ago.
TEST_F(DockingPromoMediatorTest,
       ShouldShowDockingPromoForTwoDaysOldInactiveUsers) {
  CreateDockingPromoMediator(base::Days(1) + base::Seconds(1));
  SetFirstRunRecency(2);

  EXPECT_TRUE([mediator_ canShowDockingPromo]);
}

// Tests that promo is not eligible for display if:
// - The user is more than 14 days old
TEST_F(DockingPromoMediatorTest,
       ShouldNotShowDockingPromoForUsersMoreThanTwoWeeksOld) {
  CreateDockingPromoMediator(base::Days(3));
  SetFirstRunRecency(18);

  EXPECT_FALSE([mediator_ canShowDockingPromo]);
}

// Tests whether the docking promo should display when the user is eligible (met
// criteria) and the eligibility feature is enabled.
TEST_F(DockingPromoMediatorTest,
       ShouldShowDockingPromoForEligibleUserWithFeatureEnabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      kIOSDockingPromoForEligibleUsersOnly);

  PrefService* local_pref_service =
      TestingApplicationContext::GetGlobal()->GetLocalState();
  local_pref_service->SetBoolean(prefs::kIosDockingPromoEligibilityMet, true);

  CreateDockingPromoMediator(base::Days(3));
  SetFirstRunRecency(18);

  EXPECT_TRUE([mediator_ canShowDockingPromo]);
}

// Tests whether the docking promo should NOT display when the user is eligible
// (met criteria) but the eligibility feature is disabled.
TEST_F(DockingPromoMediatorTest,
       ShouldNotShowDockingPromoForEligibleUserWithFeatureDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      kIOSDockingPromoForEligibleUsersOnly);

  PrefService* local_pref_service =
      TestingApplicationContext::GetGlobal()->GetLocalState();
  local_pref_service->SetBoolean(prefs::kIosDockingPromoEligibilityMet, true);

  CreateDockingPromoMediator(base::Days(3));
  SetFirstRunRecency(18);

  EXPECT_FALSE([mediator_ canShowDockingPromo]);
}

// Tests the Docking Promo consumer is correctly configured.
TEST_F(DockingPromoMediatorTest, DockingPromoConsumerProperlyConfigured) {
  CreateDockingPromoMediator(base::Days(3));

  ExpectConsumerSetFieldsForPromo();

  [mediator_ configureConsumer];

  EXPECT_OCMOCK_VERIFY(consumer_);
}
