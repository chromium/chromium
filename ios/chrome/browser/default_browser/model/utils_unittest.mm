// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/default_browser/model/utils.h"

#import "base/ios/ios_util.h"
#import "base/test/scoped_feature_list.h"
#import "base/time/time.h"
#import "components/feature_engagement/public/feature_constants.h"
#import "components/prefs/testing_pref_service.h"
#import "components/sync_preferences/testing_pref_service_syncable.h"
#import "ios/chrome/browser/default_browser/model/utils_test_support.h"
#import "ios/chrome/browser/shared/model/prefs/browser_prefs.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/test/testing_application_context.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {

// More than 14 days.
constexpr base::TimeDelta kMoreThan14Days = base::Days(14) + base::Minutes(1);

// More than 6 hours.
constexpr base::TimeDelta kMoreThan6Hours = base::Hours(6) + base::Minutes(1);

// About 5 months.
constexpr base::TimeDelta k5Months = base::Days(5 * 365 / 12);

// About 1 year.
constexpr base::TimeDelta kMoreThan1Year = base::Days(365) + base::Days(1);

// About 2 years.
constexpr base::TimeDelta k2Years = base::Days(2 * 365);

// About 5 years.
constexpr base::TimeDelta k5Years = base::Days(5 * 365);

// TODO(crbug.com/41496010): We should reuse the ones from utils directly to
// avoid manual errors. Test key for recording the last time a http link
// was opened via Chrome, which indicates that it's set as default browser.
NSString* const kLastHTTPURLOpenTime = @"lastHTTPURLOpenTime";

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
  void SetUp() override {
    PlatformTest::SetUp();
    ClearDefaultBrowserPromoData();

    local_state_ = std::make_unique<TestingPrefServiceSimple>();
    RegisterLocalStatePrefs(local_state_->registry());
    TestingApplicationContext::GetGlobal()->SetLocalState(local_state_.get());
  }
  void TearDown() override {
    ClearDefaultBrowserPromoData();

    TestingApplicationContext::GetGlobal()->SetLocalState(nullptr);
    local_state_.reset();
    PlatformTest::TearDown();
  }

  web::WebTaskEnvironment task_environment_;
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<TestingPrefServiceSimple> local_state_;
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

// Tests logging user interactions with a non-modal promo multiple times with
// the same current interactions count doesn't over-increment the value.
TEST_F(DefaultBrowserUtilsTest,
       LogNonModalUserInteractionMultipleTimesSameArguments) {
  LogUserInteractionWithNonModalPromo(2);
  EXPECT_EQ(UserInteractionWithNonModalPromoCount(), 3);

  LogUserInteractionWithNonModalPromo(2);
  EXPECT_EQ(UserInteractionWithNonModalPromoCount(), 3);

  LogUserInteractionWithNonModalPromo(2);
  EXPECT_EQ(UserInteractionWithNonModalPromoCount(), 3);
}

// Tests no 2 tailored promos are not shown.
TEST_F(DefaultBrowserUtilsTest, TailoredPromoDoesNotAppearTwoTimes) {
  LogUserInteractionWithTailoredFullscreenPromo();
  EXPECT_TRUE(HasUserInteractedWithTailoredFullscreenPromoBefore());
}

// Tests that past interactions with the default browser promo are correctly
// detected.
TEST_F(DefaultBrowserUtilsTest,
       HasUserInteractedWithFullscreenPromoBeforeTest) {
  // Test when there are no interaction recorded yet.
  EXPECT_FALSE(HasUserInteractedWithFullscreenPromoBefore());

  // Test that logging first run doesn't affect it.
  LogUserInteractionWithFirstRunPromo();
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
  SimulateUserInteractionWithFullscreenPromo(k5Months, 1, 2);
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
  SimulateUserInteractionWithFullscreenPromo(k5Months, 3, 4);
  EXPECT_TRUE(HasUserInteractedWithFullscreenPromoBefore());
  SimulateUserInteractionWithFullscreenPromo(kMoreThan14Days, 4, 5);
  EXPECT_TRUE(HasUserInteractedWithFullscreenPromoBefore());
}

// Tests that cooldown from FRE is correct.
TEST_F(DefaultBrowserUtilsTest, CooldownFromFRETest) {
  // Test when there are no interaction recorded yet.
  EXPECT_FALSE(HasUserInteractedWithFullscreenPromoBefore());

  // Test that logging first run doesn't affect it.
  LogUserInteractionWithFirstRunPromo();
  EXPECT_FALSE(HasUserInteractedWithFullscreenPromoBefore());

  // Test that logging a generic promo interaction will affect it.
  LogUserInteractionWithFullscreenPromo();
  LogFullscreenDefaultBrowserPromoDisplayed();
  EXPECT_TRUE(HasUserInteractedWithFullscreenPromoBefore());
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

// Test that Blue dot display timestamp is recorded first time and is not
// updated afterwards.
TEST_F(DefaultBrowserUtilsTest, TestDefaultBrowserBlueDotFirstDisplay) {
  EXPECT_FALSE(HasDefaultBrowserBlueDotDisplayTimestamp());

  RecordDefaultBrowserBlueDotFirstDisplay();
  EXPECT_TRUE(HasDefaultBrowserBlueDotDisplayTimestamp());

  // Save current pref value and try calling
  // `RecordDefaultBrowserBlueDotFirstDisplay` again.
  base::Time timestamp =
      local_state_->GetTime(prefs::kIosDefaultBrowserBlueDotPromoFirstDisplay);
  RecordDefaultBrowserBlueDotFirstDisplay();

  // Get pref value again and check that it's same.
  EXPECT_EQ(timestamp, local_state_->GetTime(
                           prefs::kIosDefaultBrowserBlueDotPromoFirstDisplay));
}

// Test that timestamp will be reset when needed.
TEST_F(DefaultBrowserUtilsTest,
       TestResetDefaultBrowserBlueDotDisplayTimestampIfNeeded) {
  // It will not recent if the timestamp is less than 1 year old.
  base::Time timestamp = base::Time::Now() - k5Months;
  local_state_->SetTime(prefs::kIosDefaultBrowserBlueDotPromoFirstDisplay,
                        timestamp);
  ResetDefaultBrowserBlueDotDisplayTimestampIfNeeded();

  // Check that didn't reset.
  EXPECT_EQ(timestamp, local_state_->GetTime(
                           prefs::kIosDefaultBrowserBlueDotPromoFirstDisplay));
  EXPECT_TRUE(HasDefaultBrowserBlueDotDisplayTimestamp());

  // Set the timestamp to over 1 year ago.
  local_state_->SetTime(prefs::kIosDefaultBrowserBlueDotPromoFirstDisplay,
                        base::Time::Now() - kMoreThan1Year);
  ResetDefaultBrowserBlueDotDisplayTimestampIfNeeded();

  // Check that it got reset.
  EXPECT_FALSE(HasDefaultBrowserBlueDotDisplayTimestamp());
}
}  // namespace
