// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/default_browser/utils.h"

#import "base/ios/ios_util.h"
#import "base/test/scoped_feature_list.h"
#import "base/time/time.h"
#import "ios/chrome/browser/default_browser/utils_test_support.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Less than 7 days.
constexpr base::TimeDelta kLessThan7Days = base::Days(7) - base::Minutes(1);

// More than 7 days.
constexpr base::TimeDelta kMoreThan7Days = base::Days(7) + base::Minutes(1);

// Less than 6 hours.
constexpr base::TimeDelta kLessThan6Hours = base::Hours(6) - base::Minutes(1);

// More than 6 hours.
constexpr base::TimeDelta kMoreThan6Hours = base::Hours(6) + base::Minutes(1);

// Test key for a generic timestamp in NSUserDefaults.
NSString* const kTestTimestampKey = @"testTimestampKeyDefaultBrowserUtils";

// Test key in storage for timestamp of last first party intent launch.
NSString* const kTimestampAppLastOpenedViaFirstPartyIntent =
    @"TimestampAppLastOpenedViaFirstPartyIntent";

// Test key in storage for timestamp of last valid URL pasted.
NSString* const kTimestampLastValidURLPasted = @"TimestampLastValidURLPasted";

class DefaultBrowserUtilsTest : public PlatformTest {
 protected:
  void SetUp() override { ClearDefaultBrowserPromoData(); }
  void TearDown() override { ClearDefaultBrowserPromoData(); }

  base::test::ScopedFeatureList feature_list_;
};

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

// Tests cool down between promos.
TEST_F(DefaultBrowserUtilsTest, PromoCoolDown) {
  LogUserInteractionWithFullscreenPromo();
  EXPECT_TRUE(UserInPromoCooldown());

  ClearDefaultBrowserPromoData();
  LogUserInteractionWithTailoredFullscreenPromo();
  EXPECT_TRUE(UserInPromoCooldown());
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
  SetObjectInStorageForKey(
      kTimestampAppLastOpenedViaFirstPartyIntent,
      [[NSDate alloc]
          initWithTimeIntervalSinceNow:-kLessThan6Hours.InSecondsF()]);
  EXPECT_FALSE(HasRecentFirstPartyIntentLaunchesAndRecordsCurrentLaunch());
}

// Manually tests that two recent first party intent launches are more than 7
// days apart.
TEST_F(
    DefaultBrowserUtilsTest,
    ManualHasRecentFirstPartyIntentLaunchesAndRecordsCurrentLaunchMoreThan7Days) {
  SetObjectInStorageForKey(
      kTimestampAppLastOpenedViaFirstPartyIntent,
      [[NSDate alloc]
          initWithTimeIntervalSinceNow:-kMoreThan7Days.InSecondsF()]);
  EXPECT_FALSE(HasRecentFirstPartyIntentLaunchesAndRecordsCurrentLaunch());
}

// Manually tests that two recent first party intent launches are more than 6
// hours apart, but less than 7 days apart. Returns true.
TEST_F(
    DefaultBrowserUtilsTest,
    ManualHasRecentFirstPartyIntentLaunchesAndRecordsCurrentLaunchLessThan7DaysMoreThan6Hours) {
  SetObjectInStorageForKey(
      kTimestampAppLastOpenedViaFirstPartyIntent,
      [[NSDate alloc]
          initWithTimeIntervalSinceNow:-kLessThan7Days.InSecondsF()]);
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
  SetObjectInStorageForKey(
      kTimestampLastValidURLPasted,
      [[NSDate alloc]
          initWithTimeIntervalSinceNow:-kLessThan7Days.InSecondsF()]);
  EXPECT_TRUE(HasRecentValidURLPastesAndRecordsCurrentPaste());
}

// Manually tests two consecutive pastes recorded with more than 7 days between,
// should return false.
TEST_F(DefaultBrowserUtilsTest, ManualTwoConsecutivePastesOver7Days) {
  SetObjectInStorageForKey(
      kTimestampLastValidURLPasted,
      [[NSDate alloc]
          initWithTimeIntervalSinceNow:-kMoreThan7Days.InSecondsF()]);
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
  SetObjectInStorageForKey(
      kTestTimestampKey,
      [[NSDate alloc]
          initWithTimeIntervalSinceNow:-kLessThan6Hours.InSecondsF()]);
  EXPECT_TRUE(HasRecentTimestampForKey(kTestTimestampKey));
}

// Manually tests that no recent event timestamp (more than 6 hours) has already
// been recorded.
TEST_F(DefaultBrowserUtilsTest, ManualRecentTimestampForKeyOver6Hours) {
  SetObjectInStorageForKey(
      kTestTimestampKey,
      [[NSDate alloc]
          initWithTimeIntervalSinceNow:-kMoreThan6Hours.InSecondsF()]);
  EXPECT_FALSE(HasRecentTimestampForKey(kTestTimestampKey));
}

}  // namespace
