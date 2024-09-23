// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/store_kit/model/store_kit_coordinator.h"

#import <StoreKit/StoreKit.h>

#import "base/test/ios/wait_util.h"
#import "base/test/task_environment.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/test/fakes/fake_ui_view_controller.h"
#import "ios/chrome/test/scoped_key_window.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

// Test fixture for StoreKitCoordinator class.
class StoreKitCoordinatorTest : public PlatformTest {
 protected:
  StoreKitCoordinatorTest() {
    profile_ = TestProfileIOS::Builder().Build();
    browser_ = std::make_unique<TestBrowser>(profile_.get());
    root_view_controller_ = [[UIViewController alloc] init];
    base_view_controller_ = [[UIViewController alloc] init];
    coordinator_ = [[StoreKitCoordinator alloc]
        initWithBaseViewController:base_view_controller_
                           browser:browser_.get()];
    [scoped_key_window_.Get() setRootViewController:root_view_controller_];
    [root_view_controller_ presentViewController:base_view_controller_
                                        animated:NO
                                      completion:nil];
  }

  ~StoreKitCoordinatorTest() override {
    // Make sure StoreKit has been dismissed.
    if (base_view_controller_.presentedViewController) {
      [coordinator_ stop];
      EXPECT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
          base::test::ios::kWaitForActionTimeout, ^bool {
            return !base_view_controller_.presentedViewController;
          }));
    }
  }

  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<TestBrowser> browser_;
  UIViewController* root_view_controller_;
  UIViewController* base_view_controller_;
  StoreKitCoordinator* coordinator_;
  ScopedKeyWindow scoped_key_window_;
};

// Tests that StoreKitCoordinator presents SKStoreProductViewController when
// product parameters are set and the coordinator is started.
TEST_F(StoreKitCoordinatorTest, OpenStoreWithParamsPresentViewController) {
  NSDictionary* product_params = @{
    SKStoreProductParameterITunesItemIdentifier : @"TestITunesItemIdentifier",
    SKStoreProductParameterAffiliateToken : @"TestToken"
  };
  coordinator_.iTunesProductParameters = product_params;
  [coordinator_ start];
  EXPECT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForActionTimeout, ^bool {
        return base_view_controller_.presentedViewController;
      }));

  EXPECT_NSEQ(product_params, coordinator_.iTunesProductParameters);

  EXPECT_EQ([SKStoreProductViewController class],
            [base_view_controller_.presentedViewController class]);
  [coordinator_ stop];
  EXPECT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForActionTimeout, ^bool {
        return !base_view_controller_.presentedViewController;
      }));

  EXPECT_FALSE(base_view_controller_.presentedViewController);
}

// Tests that when there is a SKStoreProductViewController presented, starting
// the coordinator doesn't present new view controller.
TEST_F(StoreKitCoordinatorTest, NoOverlappingStoreKitsPresented) {
  NSString* kTestITunesItemIdentifier = @"TestITunesItemIdentifier";
  coordinator_.iTunesProductParameters = @{
    SKStoreProductParameterITunesItemIdentifier : kTestITunesItemIdentifier,
  };
  [coordinator_ start];
  EXPECT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForActionTimeout, ^bool {
        return base_view_controller_.presentedViewController;
      }));

  EXPECT_EQ([SKStoreProductViewController class],
            [base_view_controller_.presentedViewController class]);

  UIViewController* presented_controller =
      base_view_controller_.presentedViewController;

  [coordinator_ start];
  // Verify that that presented view controlled is not changed.
  EXPECT_NSEQ(presented_controller,
              base_view_controller_.presentedViewController);

  [coordinator_ stop];
  EXPECT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForActionTimeout, ^bool {
        return !base_view_controller_.presentedViewController;
      }));

  EXPECT_FALSE(base_view_controller_.presentedViewController);

  [coordinator_ start];
  EXPECT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForActionTimeout, ^bool {
        return base_view_controller_.presentedViewController;
      }));

  // After reseting the view controller, a new storekit view should be
  // presented.
  EXPECT_EQ([SKStoreProductViewController class],
            [base_view_controller_.presentedViewController class]);
  EXPECT_NSNE(presented_controller,
              base_view_controller_.presentedViewController);
}

// Tests that if the base view controller is presenting any view controller,
// starting the coordinator doesn't present new view controller.
// TODO:(crbug.com/968514): Re-enable this test on devices.
#if TARGET_OS_SIMULATOR
#define MAYBE_NoOverlappingPresentedViewControllers \
  NoOverlappingPresentedViewControllers
#else
#define MAYBE_NoOverlappingPresentedViewControllers \
  FLAKY_NoOverlappingPresentedViewControllers
#endif
TEST_F(StoreKitCoordinatorTest, MAYBE_NoOverlappingPresentedViewControllers) {
  NSString* kTestITunesItemIdentifier = @"TestITunesItemIdentifier";
  coordinator_.iTunesProductParameters = @{
    SKStoreProductParameterITunesItemIdentifier : kTestITunesItemIdentifier,
  };
  EXPECT_FALSE(base_view_controller_.presentedViewController);
  UIViewController* dummy_view_controller = [[UIViewController alloc] init];
  [base_view_controller_ presentViewController:dummy_view_controller
                                      animated:NO
                                    completion:nil];
  EXPECT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForUIElementTimeout, ^bool {
        return base_view_controller_.presentedViewController;
      }));
  EXPECT_NSEQ(dummy_view_controller,
              base_view_controller_.presentedViewController);

  [coordinator_ start];
  EXPECT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForActionTimeout, ^bool {
        return base_view_controller_.presentedViewController;
      }));

  // Verify that that presented view controlled is not changed.
  EXPECT_NSEQ(dummy_view_controller,
              base_view_controller_.presentedViewController);
  [coordinator_ stop];
  [dummy_view_controller dismissViewControllerAnimated:NO completion:nil];
  EXPECT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForActionTimeout, ^bool {
        return !base_view_controller_.presentedViewController;
      }));

  EXPECT_FALSE(base_view_controller_.presentedViewController);

  [coordinator_ start];
  EXPECT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForActionTimeout, ^bool {
        return base_view_controller_.presentedViewController;
      }));

  // After reseting the view controller, a new storekit view should be
  // presented.
  EXPECT_EQ([SKStoreProductViewController class],
            [base_view_controller_.presentedViewController class]);
}

// iOS 13 dismisses SKStoreProductViewController when user taps "Done". This
// test makes sure that StoreKitCoordinator gracefully handles the situation.
TEST_F(StoreKitCoordinatorTest, StopAfterDismissingPresentedViewController) {
  coordinator_.iTunesProductParameters = @{
    SKStoreProductParameterITunesItemIdentifier : @"TestITunesItemIdentifier",
  };
  [coordinator_ start];
  EXPECT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForUIElementTimeout, ^bool {
        return base_view_controller_.presentedViewController;
      }));

  // iOS 13 dismisses SKStoreProductViewController when user taps "Done".
  [base_view_controller_.presentedViewController
      dismissViewControllerAnimated:NO
                         completion:nil];
  EXPECT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForUIElementTimeout, ^{
        return !base_view_controller_.presentedViewController;
      }));

  // Make sure that base view controller is not dismissed (crbug.com.1027058).
  [coordinator_ stop];
  EXPECT_FALSE(base::test::ios::WaitUntilConditionOrTimeout(base::Seconds(1), ^{
    return !root_view_controller_.presentedViewController;
  }));
}
