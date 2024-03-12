// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/default_browser/model/utils.h"

#import "base/ios/ios_util.h"
#import "base/test/scoped_feature_list.h"
#import "base/time/time.h"
#import "components/feature_engagement/public/feature_constants.h"
#import "ios/chrome/browser/default_browser/model/utils_test_support.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {

// A bit more than a day.
constexpr base::TimeDelta kMoreThan1Day = base::Days(1) + base::Minutes(1);

// Less than 7 days.
constexpr base::TimeDelta kLessThan7Days = base::Days(7) - base::Minutes(1);

// More than 7 days.
constexpr base::TimeDelta kMoreThan7Days = base::Days(7) + base::Minutes(1);

// More than 14 days.
constexpr base::TimeDelta kMoreThan14Days = base::Days(14) + base::Minutes(1);

// Less than 6 hours.
constexpr base::TimeDelta kLessThan6Hours = base::Hours(6) - base::Minutes(1);

// More than 6 hours.
constexpr base::TimeDelta kMoreThan6Hours = base::Hours(6) + base::Minutes(1);

// About 6 months.
constexpr base::TimeDelta k6Months = base::Days(6 * 365 / 12);

// About 2 years.
constexpr base::TimeDelta k2Years = base::Days(2 * 365);

// About 5 years.
constexpr base::TimeDelta k5Years = base::Days(5 * 365);

// TODO(crbug.com/1523056): We should reuse the ones from utils directly to
// avoid manual errors. Test key for recording the last time a http link
// was opened via Chrome, which indicates that it's set as default browser.
NSString* const kLastHTTPURLOpenTime = @"lastHTTPURLOpenTime";

// Test key for a generic timestamp in NSUserDefaults.
NSString* const kTestTimestampKey = @"testTimestampKeyDefaultBrowserUtils";

// Test key in storage for timestamp of last first party intent launch.
NSString* const kTimestampAppLastOpenedViaFirstPartyIntent =
    @"TimestampAppLastOpenedViaFirstPartyIntent";

// Test key in storage for timestamp of last valid URL pasted.
NSString* const kTimestampLastValidURLPasted = @"TimestampLastValidURLPasted";

// Test key in storage for flagging default browser promo interaction.
NSString* const kUserHasInteractedWithFullscreenPromo =
    @"userHasInteractedWithFullscreenPromo";

// Test key in storage for the timestamp of last default browser promo
// interaction.
NSString* const kLastTimeUserInteractedWithFullscreenPromo =
    @"lastTimeUserInteractedWithFullscreenPromo";

// Test key in storage for counting past default browser promo interactions.
NSString* const kGenericPromoInteractionCount = @"genericPromoInteractionCount";

// Test Key in storage for counting all past default browser promo displays.
NSString* const kDisplayedFullscreenPromoCount = @"displayedPromoCount";

class DefaultBrowserUtilsTest : public PlatformTest {
 protected:
  void SetUp() override { ClearDefaultBrowserPromoData(); }
  void TearDown() override { ClearDefaultBrowserPromoData(); }

  base::test::ScopedFeatureList feature_list_;
};

// Overwrite local storage with the provided interaction information.
void SimulateUserInteractionWithFullscreenPromo(const base::TimeDelta& timeAgo,
                                                int count,
                                                int totalCount) {
  NSDictionary<NSString*, NSObject*>* values = @{
    kUserHasInteractedWithFullscreenPromo : @YES,
    kLastTimeUserInteractedWithFullscreenPromo : (base::Time::Now() - timeAgo)
        .ToNSDate(),
    kGenericPromoInteractionCount : [NSNumber numberWithInt:count],
    kDisplayedFullscreenPromoCount : [NSNumber numberWithInt:totalCount]
  };
  SetValuesInStorage(values);
}

// Tests interesting information for each type.
TEST_F(DefaultBrowserUtilsTest, LogInterestingActivityEach) {
  // General promo.
  EXPECT_FALSE(IsLikelyInterestedDefaultBrowserUser(DefaultPromoTypeGeneral));
  LogLikelyInterestedDefaultBrowserUserActivity(DefaultPromoTypeGeneral);
  EXPECT_TRUE(IsLikelyInterestedDefaultBrowserUser(DefaultPromoTypeGeneral));
  ClearDefaultBrowserPromoData();

  // Stay safe promo.
  EXPECT_FALSE(IsLikelyInterestedDefaultBrowserUser(DefaultPromoTypeStaySafe));
  LogLikelyInterestedDefaultBrowserUserActivity(DefaultPromoTypeStaySafe);
  EXPECT_TRUE(IsLikelyInterestedDefaultBrowserUser(DefaultPromoTypeStaySafe));
  ClearDefaultBrowserPromoData();

  // Made for iOS promo.
  EXPECT_FALSE(
      IsLikelyInterestedDefaultBrowserUser(DefaultPromoTypeMadeForIOS));
  LogLikelyInterestedDefaultBrowserUserActivity(DefaultPromoTypeMadeForIOS);
  EXPECT_TRUE(IsLikelyInterestedDefaultBrowserUser(DefaultPromoTypeMadeForIOS));
  ClearDefaultBrowserPromoData();

  // All tabs promo.
  EXPECT_FALSE(IsLikelyInterestedDefaultBrowserUser(DefaultPromoTypeAllTabs));
  LogLikelyInterestedDefaultBrowserUserActivity(DefaultPromoTypeAllTabs);
  EXPECT_TRUE(IsLikelyInterestedDefaultBrowserUser(DefaultPromoTypeAllTabs));
}

// Tests most recent interest type.
TEST_F(DefaultBrowserUtilsTest, MostRecentInterestDefaultPromoType) {
  DefaultPromoType type = MostRecentInterestDefaultPromoType(NO);
  EXPECT_EQ(type, DefaultPromoTypeGeneral);

  LogLikelyInterestedDefaultBrowserUserActivity(DefaultPromoTypeAllTabs);
  type = MostRecentInterestDefaultPromoType(NO);
  EXPECT_EQ(type, DefaultPromoTypeAllTabs);
  type = MostRecentInterestDefaultPromoType(YES);
  EXPECT_NE(type, DefaultPromoTypeAllTabs);

  LogLikelyInterestedDefaultBrowserUserActivity(DefaultPromoTypeStaySafe);
  type = MostRecentInterestDefaultPromoType(NO);
  EXPECT_EQ(type, DefaultPromoTypeStaySafe);

  LogLikelyInterestedDefaultBrowserUserActivity(DefaultPromoTypeMadeForIOS);
  type = MostRecentInterestDefaultPromoType(NO);
  EXPECT_EQ(type, DefaultPromoTypeMadeForIOS);
}

// Tests cooldown between fullscreen promos with cooldown refactor disabled and
// no recent non-modal promo interaction.
TEST_F(DefaultBrowserUtilsTest, FullscreenPromoCoolDownRefactorDisabled) {
  feature_list_.InitWithFeatures(
      {/*enabled=*/},
      {/*disabled=*/kNonModalDefaultBrowserPromoCooldownRefactor});

  EXPECT_FALSE(UserInFullscreenPromoCooldown());

  LogUserInteractionWithFullscreenPromo();
  EXPECT_TRUE(UserInFullscreenPromoCooldown());

  ClearDefaultBrowserPromoData();
  LogUserInteractionWithTailoredFullscreenPromo();
  EXPECT_TRUE(UserInFullscreenPromoCooldown());
}

// Tests cooldown between fullscreen promos with cooldown refactor disabled and
// a more recent non-modal promo interaction.
TEST_F(DefaultBrowserUtilsTest,
       FullscreenPromoCoolDownRefactorDisabledRecentNonModalInteraction) {
  feature_list_.InitWithFeatures(
      {/*enabled=*/},
      {/*disabled=*/kNonModalDefaultBrowserPromoCooldownRefactor});

  EXPECT_FALSE(UserInFullscreenPromoCooldown());

  LogUserInteractionWithNonModalPromo(UserInteractionWithNonModalPromoCount(),
                                      DisplayedFullscreenPromoCount());
  EXPECT_TRUE(UserInFullscreenPromoCooldown());

  ClearDefaultBrowserPromoData();
  LogUserInteractionWithTailoredFullscreenPromo();
  LogUserInteractionWithNonModalPromo(UserInteractionWithNonModalPromoCount(),
                                      DisplayedFullscreenPromoCount());
  EXPECT_TRUE(UserInFullscreenPromoCooldown());
}

// Tests cooldown between non-modal promos with a prior non-modal promo
// interaction.
TEST_F(DefaultBrowserUtilsTest, NonModalPromoCoolDownWithPriorInteraction) {
  EXPECT_FALSE(UserInNonModalPromoCooldown());

  ResetStorageAndSetTimestampForKey(kLastTimeUserInteractedWithNonModalPromo,
                                    base::Time::Now());

  EXPECT_TRUE(UserInNonModalPromoCooldown());
}

// Tests cooldown between non-modal promos without a prior non-modal promo
// interaction, but with a fullscreen promo interaction.
TEST_F(DefaultBrowserUtilsTest, NonModalPromoCoolDownWithoutPriorInteraction) {
  EXPECT_FALSE(UserInNonModalPromoCooldown());

  ResetStorageAndSetTimestampForKey(kLastTimeUserInteractedWithFullscreenPromo,
                                    base::Time::Now());

  EXPECT_TRUE(UserInNonModalPromoCooldown());
}

// Tests logging user interactions with a non-modal promo with cooldown refactor
// enabled.
TEST_F(DefaultBrowserUtilsTest,
       LogNonModalUserInteractionCooldownRefactorEnabled) {
  feature_list_.InitWithFeatures(
      {/*enabled=*/kNonModalDefaultBrowserPromoCooldownRefactor},
      {/*disabled=*/});

  EXPECT_FALSE(UserInNonModalPromoCooldown());

  LogUserInteractionWithNonModalPromo(UserInteractionWithNonModalPromoCount(),
                                      DisplayedFullscreenPromoCount());
  EXPECT_EQ(UserInteractionWithNonModalPromoCount(), 1);
  EXPECT_TRUE(UserInNonModalPromoCooldown());
  EXPECT_FALSE(UserInFullscreenPromoCooldown());

  LogUserInteractionWithNonModalPromo(UserInteractionWithNonModalPromoCount(),
                                      DisplayedFullscreenPromoCount());
  EXPECT_EQ(UserInteractionWithNonModalPromoCount(), 2);
  EXPECT_TRUE(UserInNonModalPromoCooldown());
  EXPECT_FALSE(UserInFullscreenPromoCooldown());
}

// Tests logging user interactions with a non-modal promo with cooldown refactor
// disabled.
TEST_F(DefaultBrowserUtilsTest,
       LogNonModalUserInteractionCooldownRefactorDisabled) {
  feature_list_.InitWithFeatures(
      {/*enabled=*/},
      {/*disabled=*/kNonModalDefaultBrowserPromoCooldownRefactor});

  EXPECT_FALSE(UserInNonModalPromoCooldown());

  LogUserInteractionWithNonModalPromo(UserInteractionWithNonModalPromoCount(),
                                      DisplayedFullscreenPromoCount());
  EXPECT_EQ(UserInteractionWithNonModalPromoCount(), 1);
  EXPECT_TRUE(UserInNonModalPromoCooldown());
  EXPECT_TRUE(UserInFullscreenPromoCooldown());

  LogUserInteractionWithNonModalPromo(UserInteractionWithNonModalPromoCount(),
                                      DisplayedFullscreenPromoCount());
  EXPECT_EQ(UserInteractionWithNonModalPromoCount(), 2);
  EXPECT_TRUE(UserInNonModalPromoCooldown());
  EXPECT_TRUE(UserInFullscreenPromoCooldown());
}

// Tests logging user interactions with a non-modal promo multiple times with
// the same current interactions count doesn't over-increment the value.
TEST_F(DefaultBrowserUtilsTest,
       LogNonModalUserInteractionMultipleTimesSameArguments) {
  feature_list_.InitWithFeatures(
      {/*enabled=*/},
      {/*disabled=*/kNonModalDefaultBrowserPromoCooldownRefactor});

  LogUserInteractionWithNonModalPromo(2, 2);
  EXPECT_EQ(3, 3);

  LogUserInteractionWithNonModalPromo(2, 2);
  EXPECT_EQ(3, 3);

  LogUserInteractionWithNonModalPromo(2, 2);
  EXPECT_EQ(3, 3);
}

// Tests that the cooldown refactor flag is enabled.
TEST_F(DefaultBrowserUtilsTest, CooldownRefactorFlagEnabled) {
  feature_list_.InitWithFeatures(
      {/*enabled=*/kNonModalDefaultBrowserPromoCooldownRefactor},
      {/*disabled=*/});

  EXPECT_TRUE(IsNonModalDefaultBrowserPromoCooldownRefactorEnabled());
}

// Tests that the cooldown refactor flag is disabled.
TEST_F(DefaultBrowserUtilsTest, CooldownRefactorFlagDisabled) {
  feature_list_.InitWithFeatures(
      {/*enabled=*/},
      {/*disabled=*/kNonModalDefaultBrowserPromoCooldownRefactor});

  EXPECT_FALSE(IsNonModalDefaultBrowserPromoCooldownRefactorEnabled());
}

// Tests no 2 tailored promos are not shown.
TEST_F(DefaultBrowserUtilsTest, TailoredPromoDoesNotAppearTwoTimes) {
  LogUserInteractionWithTailoredFullscreenPromo();
  EXPECT_TRUE(HasUserInteractedWithTailoredFullscreenPromoBefore());
}

// Tests that two recent first party intent launches are less than 6 hours
// apart, both returning false.
TEST_F(DefaultBrowserUtilsTest,
       HasRecentFirstPartyIntentLaunchesAndRecordsCurrentLaunchLessThan6Hours) {
  // Calling this more than once tests different code paths, but should return
  // false on both calls. This is due to there being less than 6 hours between
  // each call.
  EXPECT_FALSE(HasRecentFirstPartyIntentLaunchesAndRecordsCurrentLaunch());
  EXPECT_FALSE(HasRecentFirstPartyIntentLaunchesAndRecordsCurrentLaunch());
}

// Manually tests that two recent first party intent launches are less than 6
// hours apart.
TEST_F(
    DefaultBrowserUtilsTest,
    ManualHasRecentFirstPartyIntentLaunchesAndRecordsCurrentLaunchLessThan6Hours) {
  ResetStorageAndSetTimestampForKey(kTimestampAppLastOpenedViaFirstPartyIntent,
                                    (base::Time::Now() - kLessThan6Hours));
  EXPECT_FALSE(HasRecentFirstPartyIntentLaunchesAndRecordsCurrentLaunch());
}

// Manually tests that two recent first party intent launches are more than 7
// days apart.
TEST_F(
    DefaultBrowserUtilsTest,
    ManualHasRecentFirstPartyIntentLaunchesAndRecordsCurrentLaunchMoreThan7Days) {
  ResetStorageAndSetTimestampForKey(kTimestampAppLastOpenedViaFirstPartyIntent,
                                    (base::Time::Now() - kMoreThan7Days));
  EXPECT_FALSE(HasRecentFirstPartyIntentLaunchesAndRecordsCurrentLaunch());
}

// Manually tests that two recent first party intent launches are more than 6
// hours apart, but less than 7 days apart. Returns true.
TEST_F(
    DefaultBrowserUtilsTest,
    ManualHasRecentFirstPartyIntentLaunchesAndRecordsCurrentLaunchLessThan7DaysMoreThan6Hours) {
  ResetStorageAndSetTimestampForKey(kTimestampAppLastOpenedViaFirstPartyIntent,
                                    (base::Time::Now() - kLessThan7Days));
  EXPECT_TRUE(HasRecentFirstPartyIntentLaunchesAndRecordsCurrentLaunch());
}

// Tests two consecutive pastes recorded within 7 days, second call returns
// true.
TEST_F(DefaultBrowserUtilsTest, TwoConsecutivePastesUnder7Days) {
  // Should return false at first, then true since an event has already been
  // recorded within 7 days when the second one is called.
  EXPECT_FALSE(HasRecentValidURLPastesAndRecordsCurrentPaste());
  EXPECT_TRUE(HasRecentValidURLPastesAndRecordsCurrentPaste());
}

// Manually tests two consecutive pastes recorded within 7 days, should return
// true.
TEST_F(DefaultBrowserUtilsTest, ManualTwoConsecutivePastesUnder7Days) {
  ResetStorageAndSetTimestampForKey(kTimestampLastValidURLPasted,
                                    (base::Time::Now() - kLessThan7Days));
  EXPECT_TRUE(HasRecentValidURLPastesAndRecordsCurrentPaste());
}

// Manually tests two consecutive pastes recorded with more than 7 days between,
// should return false.
TEST_F(DefaultBrowserUtilsTest, ManualTwoConsecutivePastesOver7Days) {
  ResetStorageAndSetTimestampForKey(kTimestampLastValidURLPasted,
                                    (base::Time::Now() - kMoreThan7Days));
  EXPECT_FALSE(HasRecentValidURLPastesAndRecordsCurrentPaste());
}

// Tests that a recent event timestamp (less than 6 hours) has already been
// recorded.
TEST_F(DefaultBrowserUtilsTest, HasRecentTimestampForKeyUnder6Hours) {
  // Should return false at first, then true since an event has already been
  // recorded when the second one is called.
  EXPECT_FALSE(HasRecentTimestampForKey(kTestTimestampKey));
  EXPECT_TRUE(HasRecentTimestampForKey(kTestTimestampKey));
}

// Manually tests that a recent event timestamp (less than 6 hours) has already
// been recorded.
TEST_F(DefaultBrowserUtilsTest, ManualHasRecentTimestampForKeyUnder6Hours) {
  ResetStorageAndSetTimestampForKey(kTestTimestampKey,
                                    (base::Time::Now() - kLessThan6Hours));
  EXPECT_TRUE(HasRecentTimestampForKey(kTestTimestampKey));
}

// Manually tests that no recent event timestamp (more than 6 hours) has already
// been recorded.
TEST_F(DefaultBrowserUtilsTest, ManualRecentTimestampForKeyOver6Hours) {
  ResetStorageAndSetTimestampForKey(kTestTimestampKey,
                                    (base::Time::Now() - kMoreThan6Hours));
  EXPECT_FALSE(HasRecentTimestampForKey(kTestTimestampKey));
}

// Tests that past interactions with the default browser promo are correctly
// detected when the sliding eligibility window experiment is disabled.
TEST_F(DefaultBrowserUtilsTest,
       HasUserInteractedWithFullscreenPromoBeforeSlidingWindowDisabled) {
  feature_list_.InitWithFeatures({/*enabled=*/},
                                 {/*disabled=*/feature_engagement::
                                      kDefaultBrowserEligibilitySlidingWindow});

  // Test when there are no interaction recorded yet.
  EXPECT_FALSE(HasUserInteractedWithFullscreenPromoBefore());

  // Test that logging first run doesn't affect it.
  LogUserInteractionWithFirstRunPromo(true);
  EXPECT_FALSE(HasUserInteractedWithFullscreenPromoBefore());

  // Test with multiple interactions.
  SimulateUserInteractionWithFullscreenPromo(kMoreThan6Hours, 1, 2);
  EXPECT_TRUE(HasUserInteractedWithFullscreenPromoBefore());
  SimulateUserInteractionWithFullscreenPromo(kMoreThan14Days, 2, 3);
  EXPECT_TRUE(HasUserInteractedWithFullscreenPromoBefore());

  // Test with a single, more distant interaction.
  ClearDefaultBrowserPromoData();
  EXPECT_FALSE(HasUserInteractedWithFullscreenPromoBefore());
  SimulateUserInteractionWithFullscreenPromo(k6Months, 1, 2);
  EXPECT_TRUE(HasUserInteractedWithFullscreenPromoBefore());

  // Test with a single, even more distant interaction.
  ClearDefaultBrowserPromoData();
  EXPECT_FALSE(HasUserInteractedWithFullscreenPromoBefore());
  SimulateUserInteractionWithFullscreenPromo(k2Years, 1, 2);
  EXPECT_TRUE(HasUserInteractedWithFullscreenPromoBefore());
}

// Tests that past interactions with the default browser promo are correctly
// detected when the sliding eligibility window experiment is enabled and set
// to 365 days.
TEST_F(DefaultBrowserUtilsTest,
       HasUserInteractedWithFullscreenPromoBeforeSlidingWindowEnabled) {
  base::FieldTrialParams feature_params;
  feature_params["sliding-window-days"] = "365";
  feature_list_.InitAndEnableFeatureWithParameters(
      feature_engagement::kDefaultBrowserEligibilitySlidingWindow,
      feature_params);

  // Test when there are no interaction recorded yet.
  EXPECT_FALSE(HasUserInteractedWithFullscreenPromoBefore());

  // Test that logging first run doesn't affect it.
  LogUserInteractionWithFirstRunPromo(true);
  EXPECT_FALSE(HasUserInteractedWithFullscreenPromoBefore());

  // Test with multiple interactions.
  SimulateUserInteractionWithFullscreenPromo(kMoreThan6Hours, 1, 2);
  EXPECT_TRUE(HasUserInteractedWithFullscreenPromoBefore());
  SimulateUserInteractionWithFullscreenPromo(kMoreThan14Days, 2, 3);
  EXPECT_TRUE(HasUserInteractedWithFullscreenPromoBefore());

  // Test with a single, more distant interaction (but still within the sliding
  // window limit).
  ClearDefaultBrowserPromoData();
  EXPECT_FALSE(HasUserInteractedWithFullscreenPromoBefore());
  SimulateUserInteractionWithFullscreenPromo(k6Months, 1, 2);
  EXPECT_TRUE(HasUserInteractedWithFullscreenPromoBefore());

  // Test with a single interaction that's outside the sliding window limit.
  ClearDefaultBrowserPromoData();
  EXPECT_FALSE(HasUserInteractedWithFullscreenPromoBefore());
  SimulateUserInteractionWithFullscreenPromo(k2Years, 1, 2);
  EXPECT_FALSE(HasUserInteractedWithFullscreenPromoBefore());

  // Test with multiple interactions, some within and some outside the sliding
  // window limit.
  ClearDefaultBrowserPromoData();
  EXPECT_FALSE(HasUserInteractedWithFullscreenPromoBefore());
  SimulateUserInteractionWithFullscreenPromo(k5Years, 1, 2);
  EXPECT_FALSE(HasUserInteractedWithFullscreenPromoBefore());
  SimulateUserInteractionWithFullscreenPromo(k2Years, 2, 3);
  EXPECT_FALSE(HasUserInteractedWithFullscreenPromoBefore());
  SimulateUserInteractionWithFullscreenPromo(k6Months, 3, 4);
  EXPECT_TRUE(HasUserInteractedWithFullscreenPromoBefore());
  SimulateUserInteractionWithFullscreenPromo(kMoreThan14Days, 4, 5);
  EXPECT_TRUE(HasUserInteractedWithFullscreenPromoBefore());
}

// Tests that sliding window experiment doesn't not affect the cooldown from
// FRE.
TEST_F(DefaultBrowserUtilsTest, CooldownFromFRESlidingWindowEnabled) {
  base::FieldTrialParams feature_params;
  feature_params["sliding-window-days"] = "365";
  feature_list_.InitAndEnableFeatureWithParameters(
      feature_engagement::kDefaultBrowserEligibilitySlidingWindow,
      feature_params);

  // Test when there are no interaction recorded yet.
  EXPECT_FALSE(HasUserInteractedWithFullscreenPromoBefore());

  // Test that logging first run doesn't affect it.
  LogUserInteractionWithFirstRunPromo(true);
  EXPECT_FALSE(HasUserInteractedWithFullscreenPromoBefore());

  // Test that logging a generic promo interaction will affect it.
  LogUserInteractionWithFullscreenPromo();
  LogFullscreenDefaultBrowserPromoDisplayed();
  EXPECT_TRUE(HasUserInteractedWithFullscreenPromoBefore());
}

// Test `CalculatePromoStatistics` when feature flag is disabled.
TEST_F(DefaultBrowserUtilsTest, CalculatePromoStatisticsTest_FlagDisabled) {
  feature_list_.InitWithFeatures(
      {}, {feature_engagement::kDefaultBrowserTriggerCriteriaExperiment});
  {
    PromoStatistics* promo_stats = CalculatePromoStatistics();
    EXPECT_EQ(0, promo_stats.promoDisplayCount);
    EXPECT_EQ(0, promo_stats.numDaysSinceLastPromo);
    EXPECT_EQ(0, promo_stats.chromeColdStartCount);
    EXPECT_EQ(0, promo_stats.chromeWarmStartCount);
    EXPECT_EQ(0, promo_stats.chromeIndirectStartCount);
  }

  LogFullscreenDefaultBrowserPromoDisplayed();
  LogUserInteractionWithFullscreenPromo();
  LogBrowserLaunched(true);
  LogBrowserLaunched(false);
  LogBrowserIndirectlylaunched();

  {
    PromoStatistics* promo_stats = CalculatePromoStatistics();
    EXPECT_EQ(0, promo_stats.promoDisplayCount);
    EXPECT_EQ(0, promo_stats.numDaysSinceLastPromo);
    EXPECT_EQ(0, promo_stats.chromeColdStartCount);
    EXPECT_EQ(0, promo_stats.chromeWarmStartCount);
    EXPECT_EQ(0, promo_stats.chromeIndirectStartCount);
  }
}

// Test `CalculatePromoStatistics` when feature flag is enabled.
TEST_F(DefaultBrowserUtilsTest, CalculatePromoStatisticsTest_FlagEnabled) {
  feature_list_.InitWithFeatures(
      {feature_engagement::kDefaultBrowserTriggerCriteriaExperiment}, {});
  {
    PromoStatistics* promo_stats = CalculatePromoStatistics();
    EXPECT_EQ(0, promo_stats.promoDisplayCount);
    EXPECT_EQ(0, promo_stats.numDaysSinceLastPromo);
  }

  ResetStorageAndSetTimestampForKey(kLastTimeUserInteractedWithFullscreenPromo,
                                    (base::Time::Now() - kMoreThan1Day));

  LogFullscreenDefaultBrowserPromoDisplayed();

  {
    PromoStatistics* promo_stats = CalculatePromoStatistics();
    EXPECT_EQ(1, promo_stats.promoDisplayCount);
    EXPECT_EQ(1, promo_stats.numDaysSinceLastPromo);
  }

  LogFullscreenDefaultBrowserPromoDisplayed();
  LogUserInteractionWithFullscreenPromo();

  {
    PromoStatistics* promo_stats = CalculatePromoStatistics();
    EXPECT_EQ(2, promo_stats.promoDisplayCount);
    EXPECT_EQ(0, promo_stats.numDaysSinceLastPromo);
  }
}

// Test `CalculatePromoStatistics` for chrome open metrics.
TEST_F(DefaultBrowserUtilsTest, CalculatePromoStatisticsTest_ChromeOpen) {
  feature_list_.InitWithFeatures(
      {feature_engagement::kDefaultBrowserTriggerCriteriaExperiment}, {});
  {
    PromoStatistics* promo_stats = CalculatePromoStatistics();
    EXPECT_EQ(0, promo_stats.chromeColdStartCount);
    EXPECT_EQ(0, promo_stats.chromeWarmStartCount);
    EXPECT_EQ(0, promo_stats.chromeIndirectStartCount);
  }

  // Adding timestamps that are older than 14 days should not change the promo
  // stats.
  NSDate* moreThan14DaysAgo = (base::Time::Now() - kMoreThan14Days).ToNSDate();
  SetObjectIntoStorageForKey(kAllTimestampsAppLaunchColdStart,
                             @[ moreThan14DaysAgo ]);
  SetObjectIntoStorageForKey(kAllTimestampsAppLaunchWarmStart,
                             @[ moreThan14DaysAgo ]);
  SetObjectIntoStorageForKey(kAllTimestampsAppLaunchIndirectStart,
                             @[ moreThan14DaysAgo ]);
  {
    PromoStatistics* promo_stats = CalculatePromoStatistics();
    EXPECT_EQ(0, promo_stats.chromeColdStartCount);
    EXPECT_EQ(0, promo_stats.chromeWarmStartCount);
    EXPECT_EQ(0, promo_stats.chromeIndirectStartCount);
  }

  // Adding timestamps that are between 7 - 14 days should be counted.
  NSDate* moreThan7DaysAgo = (base::Time::Now() - kMoreThan7Days).ToNSDate();

  SetObjectIntoStorageForKey(kAllTimestampsAppLaunchColdStart,
                             @[ moreThan7DaysAgo, moreThan14DaysAgo ]);
  SetObjectIntoStorageForKey(kAllTimestampsAppLaunchWarmStart,
                             @[ moreThan7DaysAgo, moreThan14DaysAgo ]);
  SetObjectIntoStorageForKey(kAllTimestampsAppLaunchIndirectStart,
                             @[ moreThan7DaysAgo, moreThan14DaysAgo ]);
  {
    PromoStatistics* promo_stats = CalculatePromoStatistics();
    EXPECT_EQ(1, promo_stats.chromeColdStartCount);
    EXPECT_EQ(1, promo_stats.chromeWarmStartCount);
    EXPECT_EQ(1, promo_stats.chromeIndirectStartCount);
  }

  LogBrowserLaunched(true);

  {
    PromoStatistics* promo_stats = CalculatePromoStatistics();
    EXPECT_EQ(2, promo_stats.chromeColdStartCount);
    EXPECT_EQ(1, promo_stats.chromeWarmStartCount);
    EXPECT_EQ(1, promo_stats.chromeIndirectStartCount);
  }

  LogBrowserLaunched(true);
  LogBrowserLaunched(false);

  {
    PromoStatistics* promo_stats = CalculatePromoStatistics();
    EXPECT_EQ(3, promo_stats.chromeColdStartCount);
    EXPECT_EQ(2, promo_stats.chromeWarmStartCount);
    EXPECT_EQ(1, promo_stats.chromeIndirectStartCount);
  }

  LogBrowserLaunched(true);
  LogBrowserLaunched(false);
  LogBrowserIndirectlylaunched();

  {
    PromoStatistics* promo_stats = CalculatePromoStatistics();
    EXPECT_EQ(4, promo_stats.chromeColdStartCount);
    EXPECT_EQ(3, promo_stats.chromeWarmStartCount);
    EXPECT_EQ(2, promo_stats.chromeIndirectStartCount);
  }
}

// Test `CalculatePromoStatistics` for active day count metrics.
TEST_F(DefaultBrowserUtilsTest, CalculatePromoStatisticsTest_ActiveDayCount) {
  feature_list_.InitWithFeatures(
      {feature_engagement::kDefaultBrowserTriggerCriteriaExperiment}, {});
  {
    PromoStatistics* promo_stats = CalculatePromoStatistics();
    EXPECT_EQ(0, promo_stats.activeDayCount);
  }

  // Adding timestamps that are older than 14 days should not change the promo
  // stats.
  NSDate* moreThan14DaysAgo = (base::Time::Now() - kMoreThan14Days).ToNSDate();
  SetObjectIntoStorageForKey(kAllTimestampsAppLaunchColdStart,
                             @[ moreThan14DaysAgo ]);
  SetObjectIntoStorageForKey(kAllTimestampsAppLaunchWarmStart,
                             @[ moreThan14DaysAgo ]);
  SetObjectIntoStorageForKey(kAllTimestampsAppLaunchIndirectStart,
                             @[ moreThan14DaysAgo ]);
  {
    PromoStatistics* promo_stats = CalculatePromoStatistics();
    EXPECT_EQ(0, promo_stats.activeDayCount);
  }

  // Adding timestamps that are between 7 - 14 days should be counted.
  NSDate* moreThan7DaysAgo = (base::Time::Now() - kMoreThan7Days).ToNSDate();

  SetObjectIntoStorageForKey(kAllTimestampsAppLaunchColdStart,
                             @[ moreThan7DaysAgo, moreThan14DaysAgo ]);
  SetObjectIntoStorageForKey(kAllTimestampsAppLaunchWarmStart,
                             @[ moreThan7DaysAgo, moreThan14DaysAgo ]);
  SetObjectIntoStorageForKey(kAllTimestampsAppLaunchIndirectStart,
                             @[ moreThan7DaysAgo, moreThan14DaysAgo ]);
  {
    PromoStatistics* promo_stats = CalculatePromoStatistics();
    EXPECT_EQ(1, promo_stats.activeDayCount);
  }

  // Adding current timestamp should be counted.
  LogBrowserLaunched(true);
  LogBrowserLaunched(false);
  LogBrowserIndirectlylaunched();

  {
    PromoStatistics* promo_stats = CalculatePromoStatistics();
    EXPECT_EQ(2, promo_stats.activeDayCount);
  }
}

// Test `CalculatePromoStatistics` for password manager use.
TEST_F(DefaultBrowserUtilsTest,
       CalculatePromoStatisticsTest_PasswordManagerUseCount) {
  feature_list_.InitWithFeatures(
      {feature_engagement::kDefaultBrowserTriggerCriteriaExperiment}, {});
  {
    PromoStatistics* promo_stats = CalculatePromoStatistics();
    EXPECT_EQ(0, promo_stats.passwordManagerUseCount);
  }

  // Adding timestamps that are older than 14 days should not change the promo
  // stats.
  NSDate* moreThan14DaysAgo = (base::Time::Now() - kMoreThan14Days).ToNSDate();
  SetObjectIntoStorageForKey(kLastSignificantUserEventStaySafe,
                             @[ moreThan14DaysAgo ]);
  {
    PromoStatistics* promo_stats = CalculatePromoStatistics();
    EXPECT_EQ(0, promo_stats.passwordManagerUseCount);
  }

  // Adding timestamps that are between 7 - 14 days should be counted.
  NSDate* moreThan7DaysAgo = (base::Time::Now() - kMoreThan7Days).ToNSDate();

  SetObjectIntoStorageForKey(kLastSignificantUserEventStaySafe,
                             @[ moreThan7DaysAgo, moreThan14DaysAgo ]);
  {
    PromoStatistics* promo_stats = CalculatePromoStatistics();
    EXPECT_EQ(1, promo_stats.passwordManagerUseCount);
  }

  // Adding current timestamp should be counted.
  LogLikelyInterestedDefaultBrowserUserActivity(DefaultPromoTypeStaySafe);

  {
    PromoStatistics* promo_stats = CalculatePromoStatistics();
    EXPECT_EQ(2, promo_stats.passwordManagerUseCount);
  }
}

// Test `CalculatePromoStatistics` for omnibox use count.
TEST_F(DefaultBrowserUtilsTest,
       CalculatePromoStatisticsTest_OmniboxClipboardUseCount) {
  feature_list_.InitWithFeatures(
      {feature_engagement::kDefaultBrowserTriggerCriteriaExperiment}, {});
  {
    PromoStatistics* promo_stats = CalculatePromoStatistics();
    EXPECT_EQ(0, promo_stats.omniboxClipboardUseCount);
  }

  // Adding timestamps that are older than 14 days should not change the promo
  // stats.
  NSDate* moreThan14DaysAgo = (base::Time::Now() - kMoreThan14Days).ToNSDate();
  SetObjectIntoStorageForKey(kOmniboxUseCount, @[ moreThan14DaysAgo ]);
  {
    PromoStatistics* promo_stats = CalculatePromoStatistics();
    EXPECT_EQ(0, promo_stats.omniboxClipboardUseCount);
  }

  // Adding timestamps that are between 7 - 14 days should be counted.
  NSDate* moreThan7DaysAgo = (base::Time::Now() - kMoreThan7Days).ToNSDate();

  SetObjectIntoStorageForKey(kOmniboxUseCount,
                             @[ moreThan7DaysAgo, moreThan14DaysAgo ]);
  {
    PromoStatistics* promo_stats = CalculatePromoStatistics();
    EXPECT_EQ(1, promo_stats.omniboxClipboardUseCount);
  }

  // Adding current timestamp should be counted.
  LogCopyPasteInOmniboxForCriteriaExperiment();

  {
    PromoStatistics* promo_stats = CalculatePromoStatistics();
    EXPECT_EQ(2, promo_stats.omniboxClipboardUseCount);
  }
}

// Test `CalculatePromoStatistics` for bookmark use count.
TEST_F(DefaultBrowserUtilsTest, CalculatePromoStatisticsTest_BookmarkUseCount) {
  feature_list_.InitWithFeatures(
      {feature_engagement::kDefaultBrowserTriggerCriteriaExperiment}, {});
  {
    PromoStatistics* promo_stats = CalculatePromoStatistics();
    EXPECT_EQ(0, promo_stats.bookmarkUseCount);
  }

  // Adding timestamps that are older than 14 days should not change the promo
  // stats.
  NSDate* moreThan14DaysAgo = (base::Time::Now() - kMoreThan14Days).ToNSDate();
  SetObjectIntoStorageForKey(kBookmarkUseCount, @[ moreThan14DaysAgo ]);
  {
    PromoStatistics* promo_stats = CalculatePromoStatistics();
    EXPECT_EQ(0, promo_stats.bookmarkUseCount);
  }

  // Adding timestamps that are between 7 - 14 days should be counted.
  NSDate* moreThan7DaysAgo = (base::Time::Now() - kMoreThan7Days).ToNSDate();

  SetObjectIntoStorageForKey(kBookmarkUseCount,
                             @[ moreThan7DaysAgo, moreThan14DaysAgo ]);
  {
    PromoStatistics* promo_stats = CalculatePromoStatistics();
    EXPECT_EQ(1, promo_stats.bookmarkUseCount);
  }

  // Adding current timestamp should be counted.
  LogBookmarkUseForCriteriaExperiment();

  {
    PromoStatistics* promo_stats = CalculatePromoStatistics();
    EXPECT_EQ(2, promo_stats.bookmarkUseCount);
  }
}

// Test `CalculatePromoStatistics` for autofill use count.
TEST_F(DefaultBrowserUtilsTest, CalculatePromoStatisticsTest_AutofillUseCount) {
  feature_list_.InitWithFeatures(
      {feature_engagement::kDefaultBrowserTriggerCriteriaExperiment}, {});
  {
    PromoStatistics* promo_stats = CalculatePromoStatistics();
    EXPECT_EQ(0, promo_stats.autofillUseCount);
  }

  // Adding timestamps that are older than 14 days should not change the promo
  // stats.
  NSDate* moreThan14DaysAgo = (base::Time::Now() - kMoreThan14Days).ToNSDate();
  SetObjectIntoStorageForKey(kAutofillUseCount, @[ moreThan14DaysAgo ]);
  SetObjectIntoStorageForKey(kAutofillUseCount, @[ moreThan14DaysAgo ]);
  {
    PromoStatistics* promo_stats = CalculatePromoStatistics();
    EXPECT_EQ(0, promo_stats.autofillUseCount);
  }

  // Adding timestamps that are between 7 - 14 days should be counted.
  NSDate* moreThan7DaysAgo = (base::Time::Now() - kMoreThan7Days).ToNSDate();

  SetObjectIntoStorageForKey(kAutofillUseCount,
                             @[ moreThan7DaysAgo, moreThan14DaysAgo ]);
  {
    PromoStatistics* promo_stats = CalculatePromoStatistics();
    EXPECT_EQ(1, promo_stats.autofillUseCount);
  }

  // Adding current timestamp should be counted.
  LogAutofillUseForCriteriaExperiment();

  {
    PromoStatistics* promo_stats = CalculatePromoStatistics();
    EXPECT_EQ(2, promo_stats.autofillUseCount);
  }
}

// Test `CalculatePromoStatistics` for pinned or remote tab use.
TEST_F(DefaultBrowserUtilsTest,
       CalculatePromoStatisticsTest_SpecialTabUseCount) {
  feature_list_.InitWithFeatures(
      {feature_engagement::kDefaultBrowserTriggerCriteriaExperiment}, {});
  {
    PromoStatistics* promo_stats = CalculatePromoStatistics();
    EXPECT_EQ(0, promo_stats.specialTabsUseCount);
  }

  // Adding timestamps that are older than 14 days should not change the promo
  // stats.
  NSDate* moreThan14DaysAgo = (base::Time::Now() - kMoreThan14Days).ToNSDate();
  SetObjectIntoStorageForKey(kLastSignificantUserEventStaySafe,
                             @[ moreThan14DaysAgo ]);
  {
    PromoStatistics* promo_stats = CalculatePromoStatistics();
    EXPECT_EQ(0, promo_stats.specialTabsUseCount);
  }

  // Adding timestamps that are between 7 - 14 days should be counted.
  NSDate* moreThan7DaysAgo = (base::Time::Now() - kMoreThan7Days).ToNSDate();

  SetObjectIntoStorageForKey(kSpecialTabsUseCount,
                             @[ moreThan7DaysAgo, moreThan14DaysAgo ]);
  {
    PromoStatistics* promo_stats = CalculatePromoStatistics();
    EXPECT_EQ(1, promo_stats.specialTabsUseCount);
  }

  // Adding current timestamp should be counted.
  LogRemoteTabsUseForCriteriaExperiment();

  {
    PromoStatistics* promo_stats = CalculatePromoStatistics();
    EXPECT_EQ(2, promo_stats.specialTabsUseCount);
  }
}

// Test IsChromeLikelyDefaultBrowser in multiple senarios.
TEST_F(DefaultBrowserUtilsTest, IsChromeLikelyDefaultBrowser) {
  // Initial test with no value kLastHTTPURLOpenTime value recorded.
  EXPECT_FALSE(IsChromeLikelyDefaultBrowser());  // 21 days.
  EXPECT_FALSE(IsChromeLikelyDefaultBrowser7Days());
  EXPECT_FALSE(IsChromeLikelyDefaultBrowserXDays(60));
  EXPECT_FALSE(IsChromeLikelyDefaultBrowserXDays(120));

  NSDate* just_less_than_sixty_days_ago =
      (base::Time::Now() - base::Days(60) + base::Minutes(10)).ToNSDate();
  SetObjectIntoStorageForKey(kLastHTTPURLOpenTime,
                             just_less_than_sixty_days_ago);
  EXPECT_FALSE(IsChromeLikelyDefaultBrowser());  // 21 days.
  EXPECT_FALSE(IsChromeLikelyDefaultBrowser7Days());
  EXPECT_TRUE(IsChromeLikelyDefaultBrowserXDays(60));
  EXPECT_FALSE(IsChromeLikelyDefaultBrowserXDays(59));
  EXPECT_TRUE(IsChromeLikelyDefaultBrowserXDays(80));

  NSDate* just_less_than_twenty_one_days_ago =
      (base::Time::Now() - base::Days(21) + base::Minutes(10)).ToNSDate();
  SetObjectIntoStorageForKey(kLastHTTPURLOpenTime,
                             just_less_than_twenty_one_days_ago);
  EXPECT_TRUE(IsChromeLikelyDefaultBrowser());  // 21 days.
  EXPECT_FALSE(IsChromeLikelyDefaultBrowser7Days());
  EXPECT_TRUE(IsChromeLikelyDefaultBrowserXDays(21));
  EXPECT_FALSE(IsChromeLikelyDefaultBrowserXDays(20));
  EXPECT_TRUE(IsChromeLikelyDefaultBrowserXDays(40));

  NSDate* just_less_than_seven_days_ago =
      (base::Time::Now() - base::Days(7) + base::Minutes(10)).ToNSDate();
  SetObjectIntoStorageForKey(kLastHTTPURLOpenTime,
                             just_less_than_seven_days_ago);
  EXPECT_TRUE(IsChromeLikelyDefaultBrowser());  // 21 days.
  EXPECT_TRUE(IsChromeLikelyDefaultBrowser7Days());
  EXPECT_TRUE(IsChromeLikelyDefaultBrowserXDays(7));
  EXPECT_FALSE(IsChromeLikelyDefaultBrowserXDays(6));
  EXPECT_TRUE(IsChromeLikelyDefaultBrowserXDays(15));

  NSDate* just_less_than_two_days_ago =
      (base::Time::Now() - base::Days(2) + base::Minutes(10)).ToNSDate();
  SetObjectIntoStorageForKey(kLastHTTPURLOpenTime, just_less_than_two_days_ago);
  EXPECT_TRUE(IsChromeLikelyDefaultBrowser());  // 21 days.
  EXPECT_TRUE(IsChromeLikelyDefaultBrowser7Days());
  EXPECT_TRUE(IsChromeLikelyDefaultBrowserXDays(2));
  EXPECT_FALSE(IsChromeLikelyDefaultBrowserXDays(1));
  EXPECT_TRUE(IsChromeLikelyDefaultBrowserXDays(8));
}

// Test IsChromePotentiallyNoLongerDefaultBrowser* in multiple senarios.
TEST_F(DefaultBrowserUtilsTest, IsChromePotentiallyNoLongerDefaultBrowser) {
  // Initial test with no kLastHTTPURLOpenTime value recorded.
  EXPECT_FALSE(IsChromePotentiallyNoLongerDefaultBrowser(10, 4));
  EXPECT_FALSE(IsChromePotentiallyNoLongerDefaultBrowser(21, 7));
  EXPECT_FALSE(IsChromePotentiallyNoLongerDefaultBrowser(28, 14));
  EXPECT_FALSE(IsChromePotentiallyNoLongerDefaultBrowser(35, 14));
  EXPECT_FALSE(IsChromePotentiallyNoLongerDefaultBrowser(42, 21));

  NSDate* under_four_days_ago =
      (base::Time::Now() - base::Days(4) + base::Minutes(10)).ToNSDate();
  SetObjectIntoStorageForKey(kLastHTTPURLOpenTime, under_four_days_ago);
  EXPECT_FALSE(IsChromePotentiallyNoLongerDefaultBrowser(10, 4));
  EXPECT_FALSE(IsChromePotentiallyNoLongerDefaultBrowser(21, 7));
  EXPECT_FALSE(IsChromePotentiallyNoLongerDefaultBrowser(28, 14));
  EXPECT_FALSE(IsChromePotentiallyNoLongerDefaultBrowser(35, 14));
  EXPECT_FALSE(IsChromePotentiallyNoLongerDefaultBrowser(42, 21));

  NSDate* over_four_days_ago =
      (base::Time::Now() - base::Days(4) - base::Minutes(10)).ToNSDate();
  SetObjectIntoStorageForKey(kLastHTTPURLOpenTime, over_four_days_ago);
  EXPECT_TRUE(IsChromePotentiallyNoLongerDefaultBrowser(10, 4));
  EXPECT_FALSE(IsChromePotentiallyNoLongerDefaultBrowser(21, 7));
  EXPECT_FALSE(IsChromePotentiallyNoLongerDefaultBrowser(28, 14));
  EXPECT_FALSE(IsChromePotentiallyNoLongerDefaultBrowser(35, 14));
  EXPECT_FALSE(IsChromePotentiallyNoLongerDefaultBrowser(42, 21));

  NSDate* under_seven_days_ago =
      (base::Time::Now() - base::Days(7) + base::Minutes(10)).ToNSDate();
  SetObjectIntoStorageForKey(kLastHTTPURLOpenTime, under_seven_days_ago);
  EXPECT_TRUE(IsChromePotentiallyNoLongerDefaultBrowser(10, 4));
  EXPECT_FALSE(IsChromePotentiallyNoLongerDefaultBrowser(21, 7));
  EXPECT_FALSE(IsChromePotentiallyNoLongerDefaultBrowser(28, 14));
  EXPECT_FALSE(IsChromePotentiallyNoLongerDefaultBrowser(35, 14));
  EXPECT_FALSE(IsChromePotentiallyNoLongerDefaultBrowser(42, 21));

  NSDate* over_seven_days_ago =
      (base::Time::Now() - base::Days(7) - base::Minutes(10)).ToNSDate();
  SetObjectIntoStorageForKey(kLastHTTPURLOpenTime, over_seven_days_ago);
  EXPECT_TRUE(IsChromePotentiallyNoLongerDefaultBrowser(10, 4));
  EXPECT_TRUE(IsChromePotentiallyNoLongerDefaultBrowser(21, 7));
  EXPECT_FALSE(IsChromePotentiallyNoLongerDefaultBrowser(28, 14));
  EXPECT_FALSE(IsChromePotentiallyNoLongerDefaultBrowser(35, 14));
  EXPECT_FALSE(IsChromePotentiallyNoLongerDefaultBrowser(42, 21));

  NSDate* under_ten_days_ago =
      (base::Time::Now() - base::Days(10) + base::Minutes(10)).ToNSDate();
  SetObjectIntoStorageForKey(kLastHTTPURLOpenTime, under_ten_days_ago);
  EXPECT_TRUE(IsChromePotentiallyNoLongerDefaultBrowser(10, 4));
  EXPECT_TRUE(IsChromePotentiallyNoLongerDefaultBrowser(21, 7));
  EXPECT_FALSE(IsChromePotentiallyNoLongerDefaultBrowser(28, 14));
  EXPECT_FALSE(IsChromePotentiallyNoLongerDefaultBrowser(35, 14));
  EXPECT_FALSE(IsChromePotentiallyNoLongerDefaultBrowser(42, 21));

  NSDate* over_ten_days_ago =
      (base::Time::Now() - base::Days(10) - base::Minutes(10)).ToNSDate();
  SetObjectIntoStorageForKey(kLastHTTPURLOpenTime, over_ten_days_ago);
  EXPECT_FALSE(IsChromePotentiallyNoLongerDefaultBrowser(10, 4));
  EXPECT_TRUE(IsChromePotentiallyNoLongerDefaultBrowser(21, 7));
  EXPECT_FALSE(IsChromePotentiallyNoLongerDefaultBrowser(28, 14));
  EXPECT_FALSE(IsChromePotentiallyNoLongerDefaultBrowser(35, 14));
  EXPECT_FALSE(IsChromePotentiallyNoLongerDefaultBrowser(42, 21));

  NSDate* under_fourteen_days_ago =
      (base::Time::Now() - base::Days(14) + base::Minutes(10)).ToNSDate();
  SetObjectIntoStorageForKey(kLastHTTPURLOpenTime, under_fourteen_days_ago);
  EXPECT_FALSE(IsChromePotentiallyNoLongerDefaultBrowser(10, 4));
  EXPECT_TRUE(IsChromePotentiallyNoLongerDefaultBrowser(21, 7));
  EXPECT_FALSE(IsChromePotentiallyNoLongerDefaultBrowser(28, 14));
  EXPECT_FALSE(IsChromePotentiallyNoLongerDefaultBrowser(35, 14));
  EXPECT_FALSE(IsChromePotentiallyNoLongerDefaultBrowser(42, 21));

  NSDate* over_fourteen_days_ago =
      (base::Time::Now() - base::Days(14) - base::Minutes(10)).ToNSDate();
  SetObjectIntoStorageForKey(kLastHTTPURLOpenTime, over_fourteen_days_ago);
  EXPECT_FALSE(IsChromePotentiallyNoLongerDefaultBrowser(10, 4));
  EXPECT_TRUE(IsChromePotentiallyNoLongerDefaultBrowser(21, 7));
  EXPECT_TRUE(IsChromePotentiallyNoLongerDefaultBrowser(28, 14));
  EXPECT_TRUE(IsChromePotentiallyNoLongerDefaultBrowser(35, 14));
  EXPECT_FALSE(IsChromePotentiallyNoLongerDefaultBrowser(42, 21));

  NSDate* under_twenty_one_days_ago =
      (base::Time::Now() - base::Days(21) + base::Minutes(10)).ToNSDate();
  SetObjectIntoStorageForKey(kLastHTTPURLOpenTime, under_twenty_one_days_ago);
  EXPECT_FALSE(IsChromePotentiallyNoLongerDefaultBrowser(10, 4));
  EXPECT_TRUE(IsChromePotentiallyNoLongerDefaultBrowser(21, 7));
  EXPECT_TRUE(IsChromePotentiallyNoLongerDefaultBrowser(28, 14));
  EXPECT_TRUE(IsChromePotentiallyNoLongerDefaultBrowser(35, 14));
  EXPECT_FALSE(IsChromePotentiallyNoLongerDefaultBrowser(42, 21));

  NSDate* over_twenty_one_days_ago =
      (base::Time::Now() - base::Days(21) - base::Minutes(10)).ToNSDate();
  SetObjectIntoStorageForKey(kLastHTTPURLOpenTime, over_twenty_one_days_ago);
  EXPECT_FALSE(IsChromePotentiallyNoLongerDefaultBrowser(10, 4));
  EXPECT_FALSE(IsChromePotentiallyNoLongerDefaultBrowser(21, 7));
  EXPECT_TRUE(IsChromePotentiallyNoLongerDefaultBrowser(28, 14));
  EXPECT_TRUE(IsChromePotentiallyNoLongerDefaultBrowser(35, 14));
  EXPECT_TRUE(IsChromePotentiallyNoLongerDefaultBrowser(42, 21));

  NSDate* under_twenty_eight_days_ago =
      (base::Time::Now() - base::Days(28) + base::Minutes(10)).ToNSDate();
  SetObjectIntoStorageForKey(kLastHTTPURLOpenTime, under_twenty_eight_days_ago);
  EXPECT_FALSE(IsChromePotentiallyNoLongerDefaultBrowser(10, 4));
  EXPECT_FALSE(IsChromePotentiallyNoLongerDefaultBrowser(21, 7));
  EXPECT_TRUE(IsChromePotentiallyNoLongerDefaultBrowser(28, 14));
  EXPECT_TRUE(IsChromePotentiallyNoLongerDefaultBrowser(35, 14));
  EXPECT_TRUE(IsChromePotentiallyNoLongerDefaultBrowser(42, 21));

  NSDate* over_twenty_eight_days_ago =
      (base::Time::Now() - base::Days(28) - base::Minutes(10)).ToNSDate();
  SetObjectIntoStorageForKey(kLastHTTPURLOpenTime, over_twenty_eight_days_ago);
  EXPECT_FALSE(IsChromePotentiallyNoLongerDefaultBrowser(10, 4));
  EXPECT_FALSE(IsChromePotentiallyNoLongerDefaultBrowser(21, 7));
  EXPECT_FALSE(IsChromePotentiallyNoLongerDefaultBrowser(28, 14));
  EXPECT_TRUE(IsChromePotentiallyNoLongerDefaultBrowser(35, 14));
  EXPECT_TRUE(IsChromePotentiallyNoLongerDefaultBrowser(42, 21));

  NSDate* under_thirty_five_days_ago =
      (base::Time::Now() - base::Days(35) + base::Minutes(10)).ToNSDate();
  SetObjectIntoStorageForKey(kLastHTTPURLOpenTime, under_thirty_five_days_ago);
  EXPECT_FALSE(IsChromePotentiallyNoLongerDefaultBrowser(10, 4));
  EXPECT_FALSE(IsChromePotentiallyNoLongerDefaultBrowser(21, 7));
  EXPECT_FALSE(IsChromePotentiallyNoLongerDefaultBrowser(28, 14));
  EXPECT_TRUE(IsChromePotentiallyNoLongerDefaultBrowser(35, 14));
  EXPECT_TRUE(IsChromePotentiallyNoLongerDefaultBrowser(42, 21));

  NSDate* over_thirty_five_days_ago =
      (base::Time::Now() - base::Days(35) - base::Minutes(10)).ToNSDate();
  SetObjectIntoStorageForKey(kLastHTTPURLOpenTime, over_thirty_five_days_ago);
  EXPECT_FALSE(IsChromePotentiallyNoLongerDefaultBrowser(10, 4));
  EXPECT_FALSE(IsChromePotentiallyNoLongerDefaultBrowser(21, 7));
  EXPECT_FALSE(IsChromePotentiallyNoLongerDefaultBrowser(28, 14));
  EXPECT_FALSE(IsChromePotentiallyNoLongerDefaultBrowser(35, 14));
  EXPECT_TRUE(IsChromePotentiallyNoLongerDefaultBrowser(42, 21));

  NSDate* under_fourty_two_days_ago =
      (base::Time::Now() - base::Days(42) + base::Minutes(10)).ToNSDate();
  SetObjectIntoStorageForKey(kLastHTTPURLOpenTime, under_fourty_two_days_ago);
  EXPECT_FALSE(IsChromePotentiallyNoLongerDefaultBrowser(10, 4));
  EXPECT_FALSE(IsChromePotentiallyNoLongerDefaultBrowser(21, 7));
  EXPECT_FALSE(IsChromePotentiallyNoLongerDefaultBrowser(28, 14));
  EXPECT_FALSE(IsChromePotentiallyNoLongerDefaultBrowser(35, 14));
  EXPECT_TRUE(IsChromePotentiallyNoLongerDefaultBrowser(42, 21));

  NSDate* over_fourty_two_days_ago =
      (base::Time::Now() - base::Days(42) - base::Minutes(10)).ToNSDate();
  SetObjectIntoStorageForKey(kLastHTTPURLOpenTime, over_fourty_two_days_ago);
  EXPECT_FALSE(IsChromePotentiallyNoLongerDefaultBrowser(10, 4));
  EXPECT_FALSE(IsChromePotentiallyNoLongerDefaultBrowser(21, 7));
  EXPECT_FALSE(IsChromePotentiallyNoLongerDefaultBrowser(28, 14));
  EXPECT_FALSE(IsChromePotentiallyNoLongerDefaultBrowser(35, 14));
  EXPECT_FALSE(IsChromePotentiallyNoLongerDefaultBrowser(42, 21));
}
}  // namespace
