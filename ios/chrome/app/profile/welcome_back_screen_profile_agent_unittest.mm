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

- (void)onTrackerInitialized:(BOOL)success;

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
      profile_state_.initStage = ProfileInitStage::kLoadProfile;
      profile_state_.initStage = ProfileInitStage::kPurgeDiscardedSessionsData;
      profile_state_.initStage = ProfileInitStage::kProfileLoaded;
      profile_state_.initStage = ProfileInitStage::kPrepareUI;
      profile_state_.initStage = ProfileInitStage::kUIReady;
      profile_state_.initStage = ProfileInitStage::kFirstRun;
      profile_state_.initStage = ProfileInitStage::kChoiceScreen;
      profile_state_.initStage = ProfileInitStage::kNormalUI;
      profile_state_.initStage = ProfileInitStage::kFinal;
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

  // Helper to mock the active days count returned by the tracker.
  void MockActiveDays(int days) {
    feature_engagement::Tracker::EventList events;
    if (days >= 0) {
      feature_engagement::EventConfig config;
      config.name = feature_engagement::events::kChromeActiveSessionDay;
      config.window = 29;
      events.push_back({config, days});
    }

    EXPECT_CALL(*mock_tracker(),
                ListEvents(testing::Ref(
                    feature_engagement::kIPHiOSActiveDaysTrackingFeature)))
        .WillRepeatedly(testing::Return(events));
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
    RunTransitionsToFinal();
    if (recency_days.has_value()) {
      ForceFirstRunRecency(*recency_days);
    } else {
      ResetFirstRunSentinel();
    }
  }

  // Helper to call onTrackerInitialized directly within an autorelease pool.
  void TriggerTrackerInitializedCallback(BOOL success) {
    @autoreleasepool {
      [agent_ onTrackerInitialized:success];
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

// Tests that onTrackerInitialized registers the Welcome Back promo when the
// active days count <= 1 and the feature has enough eligible items.
TEST_F(WelcomeBackScreenProfileAgentTest, ActiveDaysRegistrationSuccess) {
  SetupActiveDaysVariation();
  base::HistogramTester histogram_tester;

  SetEligibleFeatures({BestFeaturesItemType::kLensSearch,
                       BestFeaturesItemType::kEnhancedSafeBrowsing});
  MockActiveDays(1);
  ExpectPromoRegistrationTimes(1);

  TriggerTrackerInitializedCallback(YES);

  ExpectRegistrationResult(histogram_tester,
                           WelcomeBackPromoRegistrationResult::kSuccess);
  ExpectActiveDaysHistogram(histogram_tester, 1);
  ResetFirstRunSentinel();
}

// Tests that onTrackerInitialized does not register the Welcome Back promo
// when the active days count > 1.
TEST_F(WelcomeBackScreenProfileAgentTest, ActiveDaysRegistrationFailure) {
  SetupActiveDaysVariation();
  base::HistogramTester histogram_tester;

  MockActiveDays(2);
  ExpectPromoRegistrationTimes(0);

  TriggerTrackerInitializedCallback(YES);

  ExpectRegistrationResult(
      histogram_tester,
      WelcomeBackPromoRegistrationResult::kFailureTimeSinceActiveLimitNotMet);
  ExpectActiveDaysHistogram(histogram_tester, 2);
  ResetFirstRunSentinel();
}

// Tests that onTrackerInitialized does not register the Welcome Back promo
// when the app is in its first run, even if active days count <= 1.
TEST_F(WelcomeBackScreenProfileAgentTest, ActiveDaysFirstRunFailure) {
  SetupActiveDaysVariation(std::nullopt);
  base::HistogramTester histogram_tester;

  MockActiveDays(1);
  ExpectPromoRegistrationTimes(0);

  TriggerTrackerInitializedCallback(YES);

  ExpectRegistrationResult(
      histogram_tester, WelcomeBackPromoRegistrationResult::kFailureFirstRun);
  ExpectActiveDaysHistogram(histogram_tester, 1);
  ResetFirstRunSentinel();
}

// Tests that onTrackerInitialized does not register the Welcome Back promo
// when the tracker fails to return the event data (returns -1 / empty list).
TEST_F(WelcomeBackScreenProfileAgentTest, ActiveDaysTrackerFailure) {
  SetupActiveDaysVariation();
  base::HistogramTester histogram_tester;

  MockActiveDays(-1);
  ExpectPromoRegistrationTimes(0);

  TriggerTrackerInitializedCallback(YES);

  ExpectRegistrationResult(
      histogram_tester,
      WelcomeBackPromoRegistrationResult::kFailureTrackerInitialization);
  ResetFirstRunSentinel();
}

// Tests that onTrackerInitialized does not register the Welcome Back promo
// when the app is installed too recently (first run recency < 28 days), even if
// active days count <= 1.
TEST_F(WelcomeBackScreenProfileAgentTest, ActiveDaysRecencyFailure) {
  SetupActiveDaysVariation(5);
  base::HistogramTester histogram_tester;

  MockActiveDays(1);
  ExpectPromoRegistrationTimes(0);

  TriggerTrackerInitializedCallback(YES);

  ExpectRegistrationResult(
      histogram_tester,
      WelcomeBackPromoRegistrationResult::kFailureNotResurrectedUser);
  ExpectActiveDaysHistogram(histogram_tester, 1);
  ResetFirstRunSentinel();
}
