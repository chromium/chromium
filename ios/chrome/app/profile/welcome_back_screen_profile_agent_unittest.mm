// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/profile/welcome_back_screen_profile_agent.h"

#import "base/time/time.h"
#import "base/values.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/first_run/public/best_features_item.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/welcome_back/metrics/welcome_back_metrics.h"
#import "ios/chrome/browser/welcome_back/model/features.h"
#import "ios/chrome/browser/welcome_back/model/welcome_back_prefs.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"

// Declare category to expose private methods for testing.
@interface WelcomeBackScreenProfileAgent (Testing)

- (WelcomeBackPromoRegistrationResult)
    promoRegistrationResultWithLastSessionEndTime:(NSDate*)lastSessionEndTime
                                  timeSinceActive:
                                      (base::TimeDelta)timeSinceActive;

- (base::TimeDelta)timeSinceActiveWithLastSessionEndTime:
    (NSDate*)lastSessionEndTime;

@end

class WelcomeBackScreenProfileAgentTest : public PlatformTest {
 public:
  WelcomeBackScreenProfileAgentTest() {
    agent_ = [[WelcomeBackScreenProfileAgent alloc] init];
  }

 protected:
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  web::WebTaskEnvironment task_environment_;
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
  PrefService* local_state = GetApplicationContext()->GetLocalState();
  base::ListValue empty_list;
  local_state->SetList(kWelcomeBackEligibleItems, std::move(empty_list));

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
  base::ListValue eligible_items;
  eligible_items.Append(static_cast<int>(BestFeaturesItemType::kLensSearch));
  eligible_items.Append(
      static_cast<int>(BestFeaturesItemType::kEnhancedSafeBrowsing));

  PrefService* local_state = GetApplicationContext()->GetLocalState();
  local_state->SetList(kWelcomeBackEligibleItems, std::move(eligible_items));

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
