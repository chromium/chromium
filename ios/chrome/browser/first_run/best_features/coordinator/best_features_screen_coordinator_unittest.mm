// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/first_run/best_features/coordinator/best_features_screen_coordinator.h"

#import "base/apple/foundation_util.h"
#import "base/test/metrics/histogram_tester.h"
#import "base/test/scoped_feature_list.h"
#import "base/test/task_environment.h"
#import "ios/chrome/browser/first_run/best_features/ui/best_features_delegate.h"
#import "ios/chrome/browser/first_run/model/first_run_metrics.h"
#import "ios/chrome/browser/first_run/public/best_features_item.h"
#import "ios/chrome/browser/first_run/public/features.h"
#import "ios/chrome/browser/first_run/public/first_run_screen_delegate.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/common/ui/promo_style/promo_style_view_controller_delegate.h"
#import "testing/platform_test.h"

@interface BestFeaturesFirstRunScreenTestDelegate
    : NSObject <FirstRunScreenDelegate>

@property(nonatomic, strong) ChromeCoordinator* stoppedCoordinator;

@end

@implementation BestFeaturesFirstRunScreenTestDelegate

- (void)firstRunScreenCoordinatorWantsToBeStopped:
    (ChromeCoordinator*)coordinator {
  self.stoppedCoordinator = coordinator;
}

@end

class BestFeaturesScreenCoordinatorTest : public PlatformTest {
 protected:
  BestFeaturesScreenCoordinatorTest() {
    scoped_feature_list_.InitAndEnableFeature(
        first_run::kBestFeaturesScreenInFirstRun);

    profile_ = TestProfileIOS::Builder().Build();
    browser_ = std::make_unique<TestBrowser>(profile_.get());
    navigation_controller_ = [[UINavigationController alloc] init];
    delegate_ = [[BestFeaturesFirstRunScreenTestDelegate alloc] init];

    coordinator_ = [[BestFeaturesScreenCoordinator alloc]
        initWithBaseNavigationController:navigation_controller_
                                 browser:browser_.get()
                                firstRun:YES
                                delegate:delegate_];
  }

  base::test::TaskEnvironment task_environment_;
  base::test::ScopedFeatureList scoped_feature_list_;
  base::HistogramTester histogram_tester_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<TestBrowser> browser_;
  UINavigationController* navigation_controller_;
  BestFeaturesFirstRunScreenTestDelegate* delegate_;
  BestFeaturesScreenCoordinator* coordinator_;
};

// Tests that when a detail screen completes, the coordinator stops the detail
// coordinator and notifies its FirstRunScreenDelegate.
TEST_F(BestFeaturesScreenCoordinatorTest, DetailScreenCompletion) {
  [coordinator_ start];

  BestFeaturesItem* item =
      [[BestFeaturesItem alloc] initWithType:BestFeaturesItemType::kLensSearch];

  id<BestFeaturesDelegate> best_features_delegate =
      (id<BestFeaturesDelegate>)coordinator_;
  [best_features_delegate didTapBestFeaturesItem:item];

  id<FirstRunScreenDelegate> screen_delegate =
      (id<FirstRunScreenDelegate>)coordinator_;
  ChromeCoordinator* detail_coordinator =
      [coordinator_ valueForKey:@"_detailScreenCoordinator"];
  ASSERT_NE(detail_coordinator, nil);

  [screen_delegate
      firstRunScreenCoordinatorWantsToBeStopped:detail_coordinator];

  EXPECT_EQ(delegate_.stoppedCoordinator, coordinator_);

  [coordinator_ stop];
}

// Tests that when `firstRun` is `NO`, the coordinator allows swipe-to-dismiss
// (`modalInPresentation == NO`) and does not emit `FirstRun.Stage` histograms.
TEST_F(BestFeaturesScreenCoordinatorTest,
       SkipsFirstRunMetricsAndAllowsDismissalWhenNotFirstRun) {
  coordinator_ = [[BestFeaturesScreenCoordinator alloc]
      initWithBaseNavigationController:navigation_controller_
                               browser:browser_.get()
                              firstRun:NO
                              delegate:delegate_];

  [coordinator_ start];

  UIViewController* presented_view_controller =
      navigation_controller_.topViewController;
  ASSERT_NE(presented_view_controller, nil);
  EXPECT_FALSE(presented_view_controller.modalInPresentation);
  histogram_tester_.ExpectTotalCount(first_run::kFirstRunStageHistogram, 0);

  id<PromoStyleViewControllerDelegate> promo_delegate =
      static_cast<id<PromoStyleViewControllerDelegate>>(coordinator_);
  [promo_delegate didTapPrimaryActionButton];
  EXPECT_EQ(delegate_.stoppedCoordinator, coordinator_);
  histogram_tester_.ExpectTotalCount(first_run::kFirstRunStageHistogram, 0);

  [coordinator_ stop];
}

// Tests that the coordinator immediately requests to be stopped if the Best
// Features variation is disabled when `-start` is invoked.
TEST_F(BestFeaturesScreenCoordinatorTest,
       SkipsWhenBestFeaturesVariationDisabled) {
  base::test::ScopedFeatureList disable_best_features;
  disable_best_features.InitWithFeatures(
      /*enabled_features=*/{},
      /*disabled_features=*/{first_run::kBestFeaturesScreenInFirstRun});

  [coordinator_ start];

  EXPECT_EQ(delegate_.stoppedCoordinator, coordinator_);
  EXPECT_EQ(navigation_controller_.topViewController, nil);
  histogram_tester_.ExpectTotalCount(first_run::kFirstRunStageHistogram, 0);

  [coordinator_ stop];
}
