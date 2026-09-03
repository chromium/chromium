// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/backend_promo/ui_bundled/backend_promo_custom_ui_coordinator.h"

#import "base/apple/foundation_util.h"
#import "base/test/ios/wait_util.h"
#import "base/test/scoped_feature_list.h"
#import "ios/chrome/browser/backend_promo/ui_bundled/backend_promo_custom_ui_coordinator_delegate.h"
#import "ios/chrome/browser/backend_promo/ui_bundled/backend_promo_custom_ui_params.h"
#import "ios/chrome/browser/backend_promo/ui_bundled/backend_promo_custom_ui_view_controller.h"
#import "ios/chrome/browser/backend_promo/ui_bundled/backend_promo_lottie_params.h"
#import "ios/chrome/browser/backend_promo/ui_bundled/backend_promo_user_action.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/common/ui/button_stack/button_stack_configuration.h"
#import "ios/chrome/common/ui/confirmation_alert/confirmation_alert_action_handler.h"
#import "ios/chrome/common/ui/confirmation_alert/confirmation_alert_view_controller.h"
#import "ios/chrome/common/ui/instruction_view/instruction_view.h"
#import "ios/chrome/test/scoped_key_window.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

// Fake delegate that records the dismiss action.
@interface FakeBackendPromoCustomUICoordinatorDelegate
    : NSObject <BackendPromoCustomUICoordinatorDelegate>
@property(nonatomic, assign) BOOL delegateCalled;
@property(nonatomic, assign) BackendPromoUserAction userAction;
@end

@implementation FakeBackendPromoCustomUICoordinatorDelegate

- (void)backendPromoCustomUICoordinator:
            (BackendPromoCustomUICoordinator*)coordinator
                   didDismissWithAction:(BackendPromoUserAction)action {
  self.delegateCalled = YES;
  self.userAction = action;
}

@end

class BackendPromoCustomUICoordinatorTest : public PlatformTest {
 protected:
  BackendPromoCustomUICoordinatorTest() {
    scoped_feature_list_.InitAndEnableFeature(kIOSBackendPromoCustomUI);
    profile_ = TestProfileIOS::Builder().Build();
    browser_ = std::make_unique<TestBrowser>(profile_.get());
    base_view_controller_ = [[UIViewController alloc] init];
    [scoped_key_window_.Get() setRootViewController:base_view_controller_];
    delegate_ = [[FakeBackendPromoCustomUICoordinatorDelegate alloc] init];
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<TestBrowser> browser_;
  ScopedKeyWindow scoped_key_window_;
  UIViewController* base_view_controller_;
  FakeBackendPromoCustomUICoordinatorDelegate* delegate_;
};

// Tests that coordinator start presents ConfirmationAlertViewController and
// stop dismisses it.
TEST_F(BackendPromoCustomUICoordinatorTest, TestStartAndStop) {
  BackendPromoCustomUIParams* params =
      [[BackendPromoCustomUIParams alloc] init];
  params.title = @"Test Title";
  params.body = @"Test Body";
  params.primaryActionTitle = @"Primary Action";
  params.secondaryActionTitle = @"Secondary Action";

  BackendPromoCustomUICoordinator* coordinator =
      [[BackendPromoCustomUICoordinator alloc]
          initWithBaseViewController:base_view_controller_
                             browser:browser_.get()
                              params:params];
  coordinator.delegate = delegate_;

  [coordinator start];

  ASSERT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForUIElementTimeout, ^{
        return base_view_controller_.presentedViewController != nil;
      }));

  ASSERT_TRUE([base_view_controller_.presentedViewController
      isKindOfClass:[ConfirmationAlertViewController class]]);

  ConfirmationAlertViewController* view_controller =
      base::apple::ObjCCastStrict<ConfirmationAlertViewController>(
          base_view_controller_.presentedViewController);
  EXPECT_NSEQ(view_controller.titleString, @"Test Title");
  EXPECT_NSEQ(view_controller.subtitleString, @"Test Body");
  EXPECT_NSEQ(view_controller.configuration.primaryActionString,
              @"Primary Action");
  EXPECT_NSEQ(view_controller.configuration.secondaryActionString,
              @"Secondary Action");

  [coordinator stop];

  EXPECT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForUIElementTimeout, ^{
        return base_view_controller_.presentedViewController == nil;
      }));
}

// Tests that primary action invokes delegate.
TEST_F(BackendPromoCustomUICoordinatorTest, TestPrimaryActionTriggersDelegate) {
  BackendPromoCustomUIParams* params =
      [[BackendPromoCustomUIParams alloc] init];
  params.title = @"Test Title";
  params.body = @"Test Body";

  BackendPromoCustomUICoordinator* coordinator =
      [[BackendPromoCustomUICoordinator alloc]
          initWithBaseViewController:base_view_controller_
                             browser:browser_.get()
                              params:params];
  coordinator.delegate = delegate_;

  [coordinator start];

  ASSERT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForUIElementTimeout, ^{
        return base_view_controller_.presentedViewController != nil;
      }));

  ConfirmationAlertViewController* view_controller =
      base::apple::ObjCCastStrict<ConfirmationAlertViewController>(
          base_view_controller_.presentedViewController);

  [view_controller.actionHandler confirmationAlertPrimaryAction];

  EXPECT_TRUE(delegate_.delegateCalled);
  EXPECT_EQ(delegate_.userAction, BackendPromoUserActionAccepted);
}

// Tests that secondary action invokes delegate.
TEST_F(BackendPromoCustomUICoordinatorTest,
       TestSecondaryActionTriggersDelegate) {
  BackendPromoCustomUIParams* params =
      [[BackendPromoCustomUIParams alloc] init];
  params.title = @"Test Title";
  params.body = @"Test Body";

  BackendPromoCustomUICoordinator* coordinator =
      [[BackendPromoCustomUICoordinator alloc]
          initWithBaseViewController:base_view_controller_
                             browser:browser_.get()
                              params:params];
  coordinator.delegate = delegate_;

  [coordinator start];

  ASSERT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForUIElementTimeout, ^{
        return base_view_controller_.presentedViewController != nil;
      }));

  ConfirmationAlertViewController* view_controller =
      base::apple::ObjCCastStrict<ConfirmationAlertViewController>(
          base_view_controller_.presentedViewController);

  [view_controller.actionHandler confirmationAlertSecondaryAction];

  EXPECT_TRUE(delegate_.delegateCalled);
  EXPECT_EQ(delegate_.userAction, BackendPromoUserActionDismissed);
}

// Tests that confirmation alert dismissed invokes delegate.
TEST_F(BackendPromoCustomUICoordinatorTest,
       TestDismissedActionTriggersDelegate) {
  BackendPromoCustomUIParams* params =
      [[BackendPromoCustomUIParams alloc] init];
  params.title = @"Test Title";
  params.body = @"Test Body";

  BackendPromoCustomUICoordinator* coordinator =
      [[BackendPromoCustomUICoordinator alloc]
          initWithBaseViewController:base_view_controller_
                             browser:browser_.get()
                              params:params];
  coordinator.delegate = delegate_;

  [coordinator start];

  ASSERT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForUIElementTimeout, ^{
        return base_view_controller_.presentedViewController != nil;
      }));

  ConfirmationAlertViewController* view_controller =
      base::apple::ObjCCastStrict<ConfirmationAlertViewController>(
          base_view_controller_.presentedViewController);

  [view_controller.actionHandler confirmationAlertDismissed];

  EXPECT_TRUE(delegate_.delegateCalled);
  EXPECT_EQ(delegate_.userAction, BackendPromoUserActionCancelled);
}

// Tests that coordinator start sets the image when imageURL is provided without
// a Lottie JSON animation.
TEST_F(BackendPromoCustomUICoordinatorTest, TestStartWithImageURL) {
  BackendPromoCustomUIParams* params =
      [[BackendPromoCustomUIParams alloc] init];
  params.title = @"Test Title";
  params.body = @"Test Body";
  params.imageURL = @"non_existent_image_asset";

  BackendPromoCustomUICoordinator* coordinator =
      [[BackendPromoCustomUICoordinator alloc]
          initWithBaseViewController:base_view_controller_
                             browser:browser_.get()
                              params:params];
  coordinator.delegate = delegate_;

  [coordinator start];

  ASSERT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForUIElementTimeout, ^{
        return base_view_controller_.presentedViewController != nil;
      }));

  ASSERT_TRUE([base_view_controller_.presentedViewController
      isKindOfClass:[ConfirmationAlertViewController class]]);

  ConfirmationAlertViewController* view_controller =
      base::apple::ObjCCastStrict<ConfirmationAlertViewController>(
          base_view_controller_.presentedViewController);
  EXPECT_NSEQ(view_controller.titleString, @"Test Title");
  EXPECT_NSEQ(view_controller.subtitleString, @"Test Body");
  EXPECT_EQ(view_controller.image, nil);

  [coordinator stop];

  EXPECT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForUIElementTimeout, ^{
        return base_view_controller_.presentedViewController == nil;
      }));
}

// Tests that coordinator start presents BackendPromoCustomUIViewController when
// a Lottie animation resource is specified in imageURL with semantic colors.
TEST_F(BackendPromoCustomUICoordinatorTest, TestStartWithLottieAnimation) {
  BackendPromoCustomUIParams* params =
      [[BackendPromoCustomUIParams alloc] init];
  params.title = @"Test Title";
  params.body = @"Test Body";
  params.primaryActionTitle = @"Primary Action";
  params.secondaryActionTitle = @"Secondary Action";
  params.imageURL = @"docking_promo";
  BackendPromoLottieParams* lottieParams =
      [[BackendPromoLottieParams alloc] init];
  lottieParams.lightColorMapping = @{
    @"grouped_primary_background_color" : @"grouped_primary_background_color",
  };
  lottieParams.textMapping = @{
    @"text_layer" : @"Test Text",
  };
  params.lottieParams = lottieParams;

  BackendPromoCustomUICoordinator* coordinator =
      [[BackendPromoCustomUICoordinator alloc]
          initWithBaseViewController:base_view_controller_
                             browser:browser_.get()
                              params:params];
  coordinator.delegate = delegate_;

  [coordinator start];

  ASSERT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForUIElementTimeout, ^{
        return base_view_controller_.presentedViewController != nil;
      }));

  ASSERT_TRUE([base_view_controller_.presentedViewController
      isKindOfClass:[BackendPromoCustomUIViewController class]]);

  BackendPromoCustomUIViewController* view_controller =
      base::apple::ObjCCastStrict<BackendPromoCustomUIViewController>(
          base_view_controller_.presentedViewController);
  EXPECT_NSEQ(view_controller.titleString, @"Test Title");
  EXPECT_NSEQ(view_controller.subtitleString, @"Test Body");
  EXPECT_NSEQ(view_controller.primaryActionString, @"Primary Action");
  EXPECT_NSEQ(view_controller.secondaryActionString, @"Secondary Action");
  EXPECT_NSEQ(view_controller.animationName, @"docking_promo");
  EXPECT_FALSE(view_controller.useLegacyDarkMode);
  EXPECT_NE(view_controller.lightModeColorProvider, nil);
  EXPECT_NE(view_controller.darkModeColorProvider, nil);
  EXPECT_NSEQ(view_controller
                  .lightModeColorProvider[@"grouped_primary_background_color"],
              view_controller
                  .darkModeColorProvider[@"grouped_primary_background_color"]);
  EXPECT_NSEQ(view_controller.animationTextProvider[@"text_layer"],
              @"Test Text");

  [coordinator stop];

  EXPECT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForUIElementTimeout, ^{
        return base_view_controller_.presentedViewController == nil;
      }));
}

// Tests that coordinator configures distinct light and dark custom colors (e.g.
// hex colors) for the view controller, and strips .json from imageURL.
TEST_F(BackendPromoCustomUICoordinatorTest,
       TestStartWithDistinctDarkAndLightCustomColors) {
  BackendPromoCustomUIParams* params =
      [[BackendPromoCustomUIParams alloc] init];
  params.title = @"Test Title";
  params.body = @"Test Body";
  params.imageURL = @"docking_promo.json";
  BackendPromoLottieParams* lottieParams =
      [[BackendPromoLottieParams alloc] init];
  lottieParams.lightColorMapping = @{
    @"Omnibox" : @"#EDF4FE",
  };
  lottieParams.darkColorMapping = @{
    @"Omnibox" : @"0x232428",
  };
  params.lottieParams = lottieParams;

  BackendPromoCustomUICoordinator* coordinator =
      [[BackendPromoCustomUICoordinator alloc]
          initWithBaseViewController:base_view_controller_
                             browser:browser_.get()
                              params:params];
  coordinator.delegate = delegate_;

  [coordinator start];

  ASSERT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForUIElementTimeout, ^{
        return base_view_controller_.presentedViewController != nil;
      }));

  BackendPromoCustomUIViewController* view_controller =
      base::apple::ObjCCastStrict<BackendPromoCustomUIViewController>(
          base_view_controller_.presentedViewController);
  EXPECT_NSEQ(view_controller.animationName, @"docking_promo");
  EXPECT_NE(view_controller.lightModeColorProvider, nil);
  EXPECT_NE(view_controller.darkModeColorProvider, nil);
  EXPECT_NSNE(view_controller.lightModeColorProvider[@"Omnibox"],
              view_controller.darkModeColorProvider[@"Omnibox"]);

  [coordinator stop];
}

// Tests that setting and reading lottieParams preserves parameters
// for light mode, dark mode, and text replacements.
TEST_F(BackendPromoCustomUICoordinatorTest, TestLottieParamsStorage) {
  BackendPromoCustomUIParams* params =
      [[BackendPromoCustomUIParams alloc] init];
  BackendPromoLottieParams* lottieParams =
      [[BackendPromoLottieParams alloc] init];
  lottieParams.lightColorMapping = @{
    @"Semantic" : @"grouped_primary_background_color",
    @"Custom" : @"#EDF4FE",
  };
  lottieParams.darkColorMapping = @{
    @"Semantic" : @"grouped_secondary_background_color",
    @"Custom" : @"0x232428",
  };
  lottieParams.textMapping = @{
    @"TextKey" : @"TestValue",
  };
  params.lottieParams = lottieParams;

  EXPECT_NSEQ(params.lottieParams, lottieParams);
  EXPECT_NSEQ(params.lottieParams.lightColorMapping[@"Custom"], @"#EDF4FE");
  EXPECT_NSEQ(params.lottieParams.darkColorMapping[@"Custom"], @"0x232428");
  EXPECT_NSEQ(params.lottieParams.textMapping[@"TextKey"], @"TestValue");
}

// Tests that coordinator start attaches InstructionView to underTitleView for
// animated promos with instructionSteps.
TEST_F(BackendPromoCustomUICoordinatorTest,
       TestStartWithInstructionStepsForAnimatedPromo) {
  BackendPromoCustomUIParams* params =
      [[BackendPromoCustomUIParams alloc] init];
  params.title = @"Test Title";
  params.body = @"Test Body";
  params.imageURL = @"docking_promo";
  params.instructionSteps = @[ @"Step 1", @"Step 2" ];

  BackendPromoCustomUICoordinator* coordinator =
      [[BackendPromoCustomUICoordinator alloc]
          initWithBaseViewController:base_view_controller_
                             browser:browser_.get()
                              params:params];
  coordinator.delegate = delegate_;

  [coordinator start];

  ASSERT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForUIElementTimeout, ^{
        return base_view_controller_.presentedViewController != nil;
      }));

  ASSERT_TRUE([base_view_controller_.presentedViewController
      isKindOfClass:[BackendPromoCustomUIViewController class]]);

  BackendPromoCustomUIViewController* view_controller =
      base::apple::ObjCCastStrict<BackendPromoCustomUIViewController>(
          base_view_controller_.presentedViewController);
  EXPECT_NE(view_controller.underTitleView, nil);
  EXPECT_TRUE(
      [view_controller.underTitleView isKindOfClass:[InstructionView class]]);
  EXPECT_EQ(view_controller.subtitleString, nil);

  [coordinator stop];
}

// Tests that coordinator start attaches InstructionView to underTitleView for
// static promos with instructionSteps.
TEST_F(BackendPromoCustomUICoordinatorTest,
       TestStartWithInstructionStepsForStaticPromo) {
  BackendPromoCustomUIParams* params =
      [[BackendPromoCustomUIParams alloc] init];
  params.title = @"Test Title";
  params.body = @"Test Body";
  params.imageURL = @"non_existent_image_asset";
  params.instructionSteps = @[ @"Step 1", @"Step 2" ];

  BackendPromoCustomUICoordinator* coordinator =
      [[BackendPromoCustomUICoordinator alloc]
          initWithBaseViewController:base_view_controller_
                             browser:browser_.get()
                              params:params];
  coordinator.delegate = delegate_;

  [coordinator start];

  ASSERT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForUIElementTimeout, ^{
        return base_view_controller_.presentedViewController != nil;
      }));

  ASSERT_TRUE([base_view_controller_.presentedViewController
      isKindOfClass:[ConfirmationAlertViewController class]]);

  ConfirmationAlertViewController* view_controller =
      base::apple::ObjCCastStrict<ConfirmationAlertViewController>(
          base_view_controller_.presentedViewController);
  EXPECT_NE(view_controller.underTitleView, nil);
  EXPECT_TRUE(
      [view_controller.underTitleView isKindOfClass:[InstructionView class]]);
  EXPECT_EQ(view_controller.subtitleString, nil);

  [coordinator stop];
}

// Tests that coordinator start does not present view controller when feature is
// disabled, and notifies delegate to stop coordinator.
TEST_F(BackendPromoCustomUICoordinatorTest, TestStartWhenFeatureDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(kIOSBackendPromoCustomUI);

  BackendPromoCustomUIParams* params =
      [[BackendPromoCustomUIParams alloc] init];
  params.title = @"Test Title";
  params.body = @"Test Body";

  BackendPromoCustomUICoordinator* coordinator =
      [[BackendPromoCustomUICoordinator alloc]
          initWithBaseViewController:base_view_controller_
                             browser:browser_.get()
                              params:params];
  coordinator.delegate = delegate_;

  [coordinator start];

  EXPECT_EQ(base_view_controller_.presentedViewController, nil);
  EXPECT_TRUE(delegate_.delegateCalled);
  EXPECT_EQ(delegate_.userAction, BackendPromoUserActionCancelled);
}
