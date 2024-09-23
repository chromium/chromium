// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/default_promo/ui_bundled/base_default_browser_promo_view_provider.h"

#import "base/test/metrics/histogram_tester.h"
#import "base/test/metrics/user_action_tester.h"
#import "components/feature_engagement/public/feature_constants.h"
#import "ios/chrome/browser/default_browser/model/utils.h"
#import "ios/chrome/browser/default_browser/model/utils_test_support.h"
#import "ios/chrome/browser/default_promo/ui_bundled/all_tabs_default_browser_promo_view_provider.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

#pragma mark - Fixture.

@interface TestDefaultBrowserPromoViewProvider
    : BaseDefaultBrowserPromoViewProvider
@end

@implementation TestDefaultBrowserPromoViewProvider

- (UIImage*)promoImage {
  return [UIImage imageNamed:@"all_your_tabs"];
}

- (NSString*)promoTitle {
  return @"Test Title";
}

- (NSString*)promoSubtitle {
  return @"Test Subtitle";
}

- (promos_manager::Promo)promoIdentifier {
  return promos_manager::Promo::AllTabsDefaultBrowser;
}

- (const base::Feature*)featureEngagmentIdentifier {
  return &feature_engagement::kIPHiOSPromoAllTabsFeature;
}

- (DefaultPromoType)defaultBrowserPromoType {
  return DefaultPromoTypeAllTabs;
}

// Open settings.
- (void)openSettings {
  // Do nothing.
}

@end

// Fixture to test BaseDefaultBrowserPromoViewProvider.
class BaseDefaultBrowserPromoViewProviderTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();

    test_provider_ = [[TestDefaultBrowserPromoViewProvider alloc] init];

    ClearDefaultBrowserPromoData();
  }

  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  TestDefaultBrowserPromoViewProvider* test_provider_;
};

#pragma mark - Tests.

// Tests that all expected metrics and logs are recorded on promo display.
TEST_F(BaseDefaultBrowserPromoViewProviderTest, TestRecordMetricsOnDisplay) {
  base::UserActionTester user_action_tester;
  base::HistogramTester histogram_tester;

  EXPECT_EQ(0, DisplayedFullscreenPromoCount());

  // Notify the view provider that promo was displayed.
  [test_provider_ promoWasDisplayed];

  // Check that all expected UMA histograms are recorded.
  histogram_tester.ExpectTotalCount(
      "IOS.DefaultBrowserPromo.DaysSinceLastPromoInteraction", 1);
  histogram_tester.ExpectTotalCount(
      "IOS.DefaultBrowserPromo.GenericPromoDisplayCount", 1);
  histogram_tester.ExpectTotalCount(
      "IOS.DefaultBrowserPromo.TailoredPromoDisplayCount", 1);
  histogram_tester.ExpectTotalCount("IOS.DefaultBrowserPromo.Shown", 1);
  histogram_tester.ExpectBucketCount("IOS.DefaultBrowserPromo.Shown", 3, 1);

  EXPECT_EQ(1, user_action_tester.GetActionCount(
                   "IOS.DefaultBrowserPromo.TailoredFullscreen.Appear"));

  // Check that user defaults are updated.
  EXPECT_EQ(1, DisplayedFullscreenPromoCount());
}

// Tests that all expected metrics and logs are recorded on primary action.
TEST_F(BaseDefaultBrowserPromoViewProviderTest,
       TestRecordMetricsOnPrimaryAction) {
  base::UserActionTester user_action_tester;
  base::HistogramTester histogram_tester;

  EXPECT_FALSE(HasUserInteractedWithTailoredFullscreenPromoBefore());

  // Notify the view provider that primary button was tapped.
  [test_provider_ standardPromoPrimaryAction];

  // Check that all expected UMA histograms are recorded.
  histogram_tester.ExpectTotalCount(
      "IOS.DefaultBrowserFullscreenTailoredPromoAllTabs", 1);
  histogram_tester.ExpectBucketCount(
      "IOS.DefaultBrowserFullscreenTailoredPromoAllTabs", 0, 1);

  EXPECT_EQ(1, user_action_tester.GetActionCount(
                   "IOS.DefaultBrowserPromo.TailoredFullscreen.Accepted"));

  // Check that user defaults are updated.
  EXPECT_TRUE(HasUserInteractedWithTailoredFullscreenPromoBefore());
}

// Tests that all expected metrics and logs are recorded on secondary action.
TEST_F(BaseDefaultBrowserPromoViewProviderTest,
       TestRecordMetricsOnSecondaryAction) {
  base::UserActionTester user_action_tester;
  base::HistogramTester histogram_tester;

  EXPECT_FALSE(HasUserInteractedWithTailoredFullscreenPromoBefore());

  // Notify the view provider that secondary button was tapped.
  [test_provider_ standardPromoSecondaryAction];

  // Check that all expected UMA histograms are recorded.
  histogram_tester.ExpectTotalCount(
      "IOS.DefaultBrowserFullscreenTailoredPromoAllTabs", 1);
  histogram_tester.ExpectBucketCount(
      "IOS.DefaultBrowserFullscreenTailoredPromoAllTabs", 1, 1);

  EXPECT_EQ(1, user_action_tester.GetActionCount(
                   "IOS.DefaultBrowserPromo.TailoredFullscreen.Cancel"));

  // Check that user defaults are updated.
  EXPECT_TRUE(HasUserInteractedWithTailoredFullscreenPromoBefore());
}

// Tests that all expected metrics and logs are recorded on learn more.
TEST_F(BaseDefaultBrowserPromoViewProviderTest, TestRecordMetricsOnLearnMore) {
  base::UserActionTester user_action_tester;

  EXPECT_FALSE(HasUserInteractedWithTailoredFullscreenPromoBefore());

  // Notify the view provider that learn more was tapped.
  [test_provider_ standardPromoLearnMoreAction];

  // Check that all expected UMA histograms are recorded.
  EXPECT_EQ(1,
            user_action_tester.GetActionCount(
                "IOS.DefaultBrowserPromo.TailoredFullscreen.MoreInfoTapped"));

  // Check that user defaults are updated.
  EXPECT_TRUE(HasUserInteractedWithTailoredFullscreenPromoBefore());
}

// Tests that all expected metrics and logs are recorded on dismiss.
TEST_F(BaseDefaultBrowserPromoViewProviderTest, TestRecordMetricsOnDismiss) {
  base::UserActionTester user_action_tester;
  base::HistogramTester histogram_tester;

  EXPECT_FALSE(HasUserInteractedWithTailoredFullscreenPromoBefore());

  // Notify the view provider that promo was dismissed.
  [test_provider_ standardPromoDismissSwipe];

  // Check that all expected UMA histograms are recorded.
  histogram_tester.ExpectTotalCount(
      "IOS.DefaultBrowserFullscreenTailoredPromoAllTabs", 1);
  histogram_tester.ExpectBucketCount(
      "IOS.DefaultBrowserFullscreenTailoredPromoAllTabs", 3, 1);
  EXPECT_EQ(1, user_action_tester.GetActionCount(
                   "IOS.DefaultBrowserPromo.TailoredFullscreen.Dismiss"));

  // Check that user defaults are updated.
  EXPECT_TRUE(HasUserInteractedWithTailoredFullscreenPromoBefore());
}
