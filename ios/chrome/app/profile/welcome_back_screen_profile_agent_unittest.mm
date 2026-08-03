// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/profile/welcome_back_screen_profile_agent.h"

#import "base/functional/bind.h"
#import "base/test/metrics/histogram_tester.h"
#import "base/test/scoped_feature_list.h"
#import "base/time/time.h"
#import "base/values.h"
#import "components/feature_engagement/public/event_constants.h"
#import "components/feature_engagement/public/tracker.h"
#import "components/feature_engagement/test/mock_tracker.h"
#import "components/prefs/pref_service.h"
#import "components/prefs/testing_pref_service.h"
#import "ios/chrome/app/profile/profile_state.h"
#import "ios/chrome/app/profile/profile_state_test_utils.h"
#import "ios/chrome/browser/feature_engagement/model/tracker_factory.h"
#import "ios/chrome/browser/first_run/public/best_features_item.h"
#import "ios/chrome/browser/promos_manager/model/mock_promos_manager.h"
#import "ios/chrome/browser/promos_manager/model/promos_manager_factory.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/prefs/browser_prefs.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/utils/first_run_test_util.h"
#import "ios/chrome/browser/welcome_back/metrics/welcome_back_metrics.h"
#import "ios/chrome/browser/welcome_back/model/features.h"
#import "ios/chrome/browser/welcome_back/model/welcome_back_prefs.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/chrome/test/testing_application_context.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/platform_test.h"

// Declare category to expose private methods for testing.
@interface WelcomeBackScreenProfileAgent (Testing)

- (WelcomeBackPromoRegistrationResult)
    promoRegistrationResultWithLastSessionEndTime:(NSDate*)lastSessionEndTime
                                  timeSinceActive:
                                      (base::TimeDelta)timeSinceActive;

- (base::TimeDelta)timeSinceActiveWithLastSessionEndTime:
    (NSDate*)lastSessionEndTime;

- (WelcomeBackPromoRegistrationResult)promoRegistrationResultWithActiveDays:
    (int)days;

@end

namespace {

std::unique_ptr<KeyedService> BuildFeatureEngagementTracker(
    ProfileIOS* profile) {
  return std::make_unique<
      testing::NiceMock<feature_engagement::test::MockTracker>>();
}

std::unique_ptr<KeyedService> CreateMockPromosManager(ProfileIOS* profile) {
  return std::make_unique<testing::NiceMock<MockPromosManager>>();
}

}  // namespace

class WelcomeBackScreenProfileAgentTest : public PlatformTest {
 public:
  WelcomeBackScreenProfileAgentTest() {
    TestProfileIOS::Builder builder;
    builder.AddTestingFactory(
        feature_engagement::TrackerFactory::GetInstance(),
        base::BindRepeating(&BuildFeatureEngagementTracker));
    builder.AddTestingFactory(PromosManagerFactory::GetInstance(),
                              base::BindRepeating(&CreateMockPromosManager));
    profile_ = std::move(builder).Build();

    profile_state_ = [[ProfileState alloc] initWithAppState:nil];
    profile_state_.profile = profile_.get();

    agent_ = [[WelcomeBackScreenProfileAgent alloc] init];
    [profile_state_ addAgent:agent_];
  }

  void RunTransitionsToFinal() {
    // Stage transitions can trigger observer notifications that may place the
    // agent in the autorelease pool. Wrapping this block in an autorelease pool
    // ensures those references are immediately cleared.
    @autoreleasepool {
      SetProfileStateInitStage(profile_state_, ProfileInitStage::kFinal);
    }
  }

  void TearDown() override {
    // Clear and release the agent and profile state immediately to prevent
    // dangling raw_ptr (e.g. mock tracker) crashes when the profile is
    // destroyed in the test fixture destructor.
    @autoreleasepool {
      if ([profile_state_.connectedAgents containsObject:agent_]) {
        [profile_state_ removeAgent:agent_];
      }
      agent_ = nil;
      profile_state_.profile = nullptr;
      profile_state_ = nil;
    }
    PlatformTest::TearDown();
  }

 protected:
  testing::NiceMock<feature_engagement::test::MockTracker>* mock_tracker() {
    return static_cast<
        testing::NiceMock<feature_engagement::test::MockTracker>*>(
        feature_engagement::TrackerFactory::GetForProfile(profile_.get()));
  }

  MockPromosManager* mock_promos_manager() {
    return static_cast<MockPromosManager*>(
        PromosManagerFactory::GetForProfile(profile_.get()));
  }

  ProfileIOS* profile() { return profile_.get(); }
  ProfileState* profile_state() { return profile_state_; }

  // Helper to enable the Welcome Back promo variation using Active Days.
  void EnableActiveDaysVariation() {
    feature_list_.InitAndEnableFeatureWithParameters(
        kWelcomeBack, {{kWelcomeBackUseActiveDaysParam, "true"}});
  }

  // Helper to set active days count in local state pref.
  void SetActiveDaysPref(int days) {
    PrefService* local_state = GetApplicationContext()->GetLocalState();
    local_state->SetInteger(prefs::kLastRecordedActiveDaysInPast28Days, days);
  }

  // Helper to configure the eligible Welcome Back features in the local state.
  void SetEligibleFeatures(const std::vector<BestFeaturesItemType>& items) {
    base::ListValue eligible_items;
    for (auto item : items) {
      eligible_items.Append(static_cast<int>(item));
    }
    PrefService* local_state = GetApplicationContext()->GetLocalState();
    local_state->SetList(kWelcomeBackEligibleItems, std::move(eligible_items));
  }

  // Helper to enable variation, transition to final init stage, and configure
  // first run.
  void SetupActiveDaysVariation(std::optional<int> recency_days = 50) {
    EnableActiveDaysVariation();
    if (recency_days.has_value()) {
      ForceFirstRunRecency(*recency_days);
    } else {
      ResetFirstRunSentinel();
    }
  }

  // Helper to expect a specific PromoRegistrationResult histogram count.
  void ExpectRegistrationResult(const base::HistogramTester& tester,
                                WelcomeBackPromoRegistrationResult result) {
    tester.ExpectBucketCount("IOS.WelcomeBack.PromoRegistrationResult",
                             static_cast<int>(result), 1);
  }

  // Helper to expect a specific ActiveDaysInPast28Days histogram count.
  void ExpectActiveDaysHistogram(const base::HistogramTester& tester,
                                 int days,
                                 int count = 1) {
    tester.ExpectBucketCount("IOS.WelcomeBack.ActiveDaysInPast28Days", days,
                             count);
  }

  // Helper to expect a specific ActiveDaysInPast28DaysForInactives histogram
  // count.
  void ExpectInactivesActiveDaysHistogram(const base::HistogramTester& tester,
                                          int days,
                                          int count = 1) {
    tester.ExpectBucketCount(
        "IOS.WelcomeBack.ActiveDaysInPast28DaysForInactives", days, count);
  }

  // Helper to mock expectations on PromosManager registration calls.
  void ExpectPromoRegistrationTimes(int times) {
    EXPECT_CALL(*mock_promos_manager(), RegisterPromoForSingleDisplay(
                                            promos_manager::Promo::WelcomeBack))
        .Times(times);
  }

  base::test::ScopedFeatureList feature_list_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
  ProfileState* profile_state_;
  WelcomeBackScreenProfileAgent* agent_;
};

// Tests that timeSinceActiveWithLastSessionEndTime returns zero for nil input.
TEST_F(WelcomeBackScreenProfileAgentTest, TimeSinceActiveWithNilEndTime) {
  base::TimeDelta time_since_active =
      [agent_ timeSinceActiveWithLastSessionEndTime:nil];
  EXPECT_EQ(time_since_active, base::TimeDelta());
}

// Tests that promoRegistrationResultWithLastSessionEndTime returns failure when
// the time active limit is not met.
TEST_F(WelcomeBackScreenProfileAgentTest, RegistrationResultTimeLimitNotMet) {
  WelcomeBackPromoRegistrationResult result =
      [agent_ promoRegistrationResultWithLastSessionEndTime:[NSDate date]
                                            timeSinceActive:base::Days(27)];
  EXPECT_EQ(
      result,
      WelcomeBackPromoRegistrationResult::kFailureTimeSinceActiveLimitNotMet);
}

// Tests that promoRegistrationResultWithLastSessionEndTime returns failure when
// there are not enough eligible features.
TEST_F(WelcomeBackScreenProfileAgentTest, RegistrationResultNotEnoughFeatures) {
  SetEligibleFeatures({});

  WelcomeBackPromoRegistrationResult result =
      [agent_ promoRegistrationResultWithLastSessionEndTime:[NSDate date]
                                            timeSinceActive:base::Days(29)];
  EXPECT_EQ(
      result,
      WelcomeBackPromoRegistrationResult::kFailureMinEligibleFeaturesNotMet);
}

// Tests that promoRegistrationResultWithLastSessionEndTime returns success when
// all conditions are met.
TEST_F(WelcomeBackScreenProfileAgentTest, RegistrationResultSuccess) {
  SetEligibleFeatures({BestFeaturesItemType::kLensSearch,
                       BestFeaturesItemType::kEnhancedSafeBrowsing});

  WelcomeBackPromoRegistrationResult result =
      [agent_ promoRegistrationResultWithLastSessionEndTime:[NSDate date]
                                            timeSinceActive:base::Days(29)];
  EXPECT_EQ(result, WelcomeBackPromoRegistrationResult::kSuccess);
}

// Tests that promoRegistrationResultWithLastSessionEndTime returns failure when
// the last session end time is nil.
TEST_F(WelcomeBackScreenProfileAgentTest, RegistrationResultSessionEndTimeNil) {
  WelcomeBackPromoRegistrationResult result =
      [agent_ promoRegistrationResultWithLastSessionEndTime:nil
                                            timeSinceActive:base::Days(29)];
  EXPECT_EQ(result,
            WelcomeBackPromoRegistrationResult::kFailureSessionEndTimeNil);
}

// Tests that maybeRegisterPromo registers the Welcome Back promo when the
// active days count <= 1 and the feature has enough eligible items.
TEST_F(WelcomeBackScreenProfileAgentTest, ActiveDaysRegistrationSuccess) {
  SetupActiveDaysVariation();
  SetActiveDaysPref(1);
  base::HistogramTester histogram_tester;

  SetEligibleFeatures({BestFeaturesItemType::kLensSearch,
                       BestFeaturesItemType::kEnhancedSafeBrowsing});
  ExpectPromoRegistrationTimes(1);

  RunTransitionsToFinal();

  ExpectRegistrationResult(histogram_tester,
                           WelcomeBackPromoRegistrationResult::kSuccess);
  ExpectActiveDaysHistogram(histogram_tester, 1);
  ExpectInactivesActiveDaysHistogram(histogram_tester, 1);
  ResetFirstRunSentinel();
}

// Tests that maybeRegisterPromo does not register the Welcome Back promo
// when the active days count > 1.
TEST_F(WelcomeBackScreenProfileAgentTest, ActiveDaysRegistrationFailure) {
  SetupActiveDaysVariation();
  SetActiveDaysPref(2);
  base::HistogramTester histogram_tester;

  ExpectPromoRegistrationTimes(0);

  RunTransitionsToFinal();

  ExpectRegistrationResult(
      histogram_tester,
      WelcomeBackPromoRegistrationResult::kFailureTimeSinceActiveLimitNotMet);
  ExpectActiveDaysHistogram(histogram_tester, 2);
  histogram_tester.ExpectTotalCount(
      "IOS.WelcomeBack.ActiveDaysInPast28DaysForInactives", 0);
  ResetFirstRunSentinel();
}

// Tests that maybeRegisterPromo does not register the Welcome Back promo
// when the app is in its first run, even if active days count <= 1.
TEST_F(WelcomeBackScreenProfileAgentTest, ActiveDaysFirstRunFailure) {
  SetupActiveDaysVariation(std::nullopt);
  SetActiveDaysPref(1);
  base::HistogramTester histogram_tester;

  ExpectPromoRegistrationTimes(0);

  RunTransitionsToFinal();

  ExpectRegistrationResult(
      histogram_tester, WelcomeBackPromoRegistrationResult::kFailureFirstRun);
  ExpectActiveDaysHistogram(histogram_tester, 1);
  ResetFirstRunSentinel();
}

// Tests that maybeRegisterPromo does not register the Welcome Back promo
// when the pref returns failure / uninitialized (-1).
TEST_F(WelcomeBackScreenProfileAgentTest, ActiveDaysTrackerFailure) {
  SetupActiveDaysVariation();
  SetActiveDaysPref(-1);
  base::HistogramTester histogram_tester;

  ExpectPromoRegistrationTimes(0);

  RunTransitionsToFinal();

  ExpectRegistrationResult(
      histogram_tester,
      WelcomeBackPromoRegistrationResult::kFailureTrackerInitialization);
  ResetFirstRunSentinel();
}

// Tests that maybeRegisterPromo does not register the Welcome Back promo
// when the app is installed too recently (first run recency < 28 days), even if
// active days count <= 1.
TEST_F(WelcomeBackScreenProfileAgentTest, ActiveDaysRecencyFailure) {
  SetupActiveDaysVariation(5);
  SetActiveDaysPref(1);
  base::HistogramTester histogram_tester;

  ExpectPromoRegistrationTimes(0);

  RunTransitionsToFinal();

  ExpectRegistrationResult(
      histogram_tester,
      WelcomeBackPromoRegistrationResult::kFailureNotResurrectedUser);
  ExpectActiveDaysHistogram(histogram_tester, 1);
  ResetFirstRunSentinel();
}
