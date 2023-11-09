// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/signin/user_signin/user_signin_mediator.h"

#import <UIKit/UIKit.h>

#import "base/ios/block_types.h"
#import "base/run_loop.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/fake_authentication_service_delegate.h"
#import "ios/chrome/browser/ui/authentication/signin/user_signin/logging/user_signin_logger.h"
#import "ios/chrome/browser/ui/authentication/signin/user_signin/user_signin_coordinator.h"
#import "ios/chrome/browser/ui/authentication/signin/user_signin/user_signin_view_controller.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"
#import "third_party/ocmock/ocmock_extensions.h"

@interface TestUserSigninCoordinator : UserSigninCoordinator

@property(nonatomic, strong)
    UserSigninViewController* userSigninViewControllerMock;

@property(nonatomic, strong) UIViewController* unifiedConsentViewController;

@end

@implementation TestUserSigninCoordinator

- (UserSigninViewController*)
    generateUserSigninViewControllerWithUnifiedConsentViewController:
        (UIViewController*)viewController {
  self.unifiedConsentViewController = viewController;
  return self.userSigninViewControllerMock;
}

@end

class UserSigninCoordinatorTest : public PlatformTest {
 public:
  UserSigninCoordinatorTest() {}

  void SetUp() override {
    PlatformTest::SetUp();

    TestChromeBrowserState::Builder builder;
    builder.AddTestingFactory(
        AuthenticationServiceFactory::GetInstance(),
        AuthenticationServiceFactory::GetDefaultFactory());
    browser_state_ = builder.Build();
    AuthenticationServiceFactory::CreateAndInitializeForBrowserState(
        browser_state_.get(),
        std::make_unique<FakeAuthenticationServiceDelegate>());
    browser_ = std::make_unique<TestBrowser>(browser_state_.get());
    SetupLoggerMock();
    SetupUserSigninViewControllerMock();
    SetupBaseViewControllerMock();
    coordinator_ = [[TestUserSigninCoordinator alloc]
        initWithBaseViewController:base_view_controller_mock_
                           browser:browser_.get()
                          identity:nil
                      signinIntent:UserSigninIntentUpgrade
                            logger:logger_mock_];
    coordinator_.userSigninViewControllerMock =
        user_signin_view_controller_mock_;
  }

  void TearDown() override {
    EXPECT_OCMOCK_VERIFY((id)base_view_controller_mock_);
    EXPECT_OCMOCK_VERIFY((id)logger_mock_);
    EXPECT_OCMOCK_VERIFY((id)user_signin_view_controller_mock_);
    [coordinator_ stop];
    coordinator_ = nil;
    PlatformTest::TearDown();
  }

  // Sets up the logger mock.
  void SetupLoggerMock() {
    DCHECK(!logger_mock_);
    logger_mock_ = OCMStrictClassMock([UserSigninLogger class]);
    OCMStub([logger_mock_ disconnect]);
    OCMStub([logger_mock_ promoAction])
        .andReturn(signin_metrics::PromoAction::PROMO_ACTION_WITH_DEFAULT);
    OCMStub([logger_mock_ accessPoint])
        .andReturn(signin_metrics::AccessPoint::ACCESS_POINT_SIGNIN_PROMO);
    OCMExpect([logger_mock_ logSigninStarted]);
  }

  // Sets up the base view controller mock. Using a view controller mock gives
  // a fine-grained control to simulate UIKit timing when completion blocks are
  // called.
  void SetupBaseViewControllerMock() {
    base_view_controller_mock_ = OCMStrictClassMock([UIViewController class]);
    id present_completion_handler =
        [OCMArg checkWithBlock:^(ProceduralBlock completion) {
          EXPECT_EQ(nil, view_controller_present_completion_);
          view_controller_present_completion_ = [completion copy];
          return YES;
        }];
    OCMExpect([base_view_controller_mock_
        presentViewController:user_signin_view_controller_mock_
                     animated:YES
                   completion:present_completion_handler]);
  }

  // Sets up the user sign-in view controller.
  void SetupUserSigninViewControllerMock() {
    user_signin_view_controller_mock_ =
        OCMStrictClassMock([UserSigninViewController class]);
    OCMExpect([user_signin_view_controller_mock_ setDelegate:[OCMArg any]]);
    OCMExpect([user_signin_view_controller_mock_
        setModalPresentationStyle:UIModalPresentationFormSheet]);
    // Method not used on iOS 12.
    OCMStub([user_signin_view_controller_mock_ presentationController])
        .andDo(^(NSInvocation* invocation) {
          id returnValue = nil;
          [invocation setReturnValue:&returnValue];
        });
    OCMStub([user_signin_view_controller_mock_ presentingViewController])
        .andReturn(user_signin_view_controller_mock_);
    OCMExpect(
        [user_signin_view_controller_mock_ supportedInterfaceOrientations])
        .andReturn(UIInterfaceOrientationMaskAll);
    id dismiss_completion_handler =
        [OCMArg checkWithBlock:^(ProceduralBlock completion) {
          EXPECT_EQ(nil, view_controller_dismiss_completion_);
          view_controller_dismiss_completion_ = [completion copy];
          return YES;
        }];
    OCMExpect([user_signin_view_controller_mock_
        dismissViewControllerAnimated:YES
                           completion:dismiss_completion_handler]);
  }

 protected:
  // Needed for test browser state created by TestChromeBrowserState().
  web::WebTaskEnvironment task_environment_;

  IOSChromeScopedTestingLocalState local_state_;
  std::unique_ptr<Browser> browser_;
  std::unique_ptr<TestChromeBrowserState> browser_state_;

  // UserSigninCoordinator to test.
  TestUserSigninCoordinator* coordinator_ = nil;

  // Procedure to finish the view controller presentation animation.
  ProceduralBlock view_controller_present_completion_ = nil;
  // Procedure to finish the dismiss view controller animation.
  ProceduralBlock view_controller_dismiss_completion_ = nil;

  // Mocks
  UIViewController* base_view_controller_mock_ = nil;
  UserSigninLogger* logger_mock_ = nil;
  UserSigninViewController* user_signin_view_controller_mock_ = nil;
};

// Tests a sequence of start and interrupt the coordinator.
TEST_F(UserSigninCoordinatorTest, StartAndInterruptCoordinator) {
  __block bool completion_done = false;
  __block bool interrupt_done = false;
  coordinator_.signinCompletion =
      ^(SigninCoordinatorResult signinResult,
        SigninCompletionInfo* signinCompletionInfo) {
        EXPECT_FALSE(completion_done);
        EXPECT_FALSE(interrupt_done);
        EXPECT_EQ(SigninCoordinatorResultInterrupted, signinResult);
        EXPECT_EQ(nil, signinCompletionInfo.identity);
        completion_done = true;
      };
  [coordinator_ start];
  EXPECT_NE(nil, coordinator_.unifiedConsentViewController);
  EXPECT_NE(nil, view_controller_present_completion_);
  [coordinator_
      interruptWithAction:SigninCoordinatorInterrupt::DismissWithAnimation
               completion:^{
                 EXPECT_TRUE(completion_done);
                 EXPECT_FALSE(interrupt_done);
                 interrupt_done = true;
               }];
  // The view controller should not be dismissed yet.
  EXPECT_EQ(nil, view_controller_dismiss_completion_);
  // Simulate the end of -[UIViewController
  // presentViewController:animated:completion] by calling the completion block.
  view_controller_present_completion_();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(interrupt_done);
  EXPECT_FALSE(completion_done);
  // Dismiss method is expected to be called.
  EXPECT_NE(nil, view_controller_dismiss_completion_);
  // Simulate the end of -[UIViewController dismissViewController:] by calling
  // the completion block.
  view_controller_dismiss_completion_();
  EXPECT_TRUE(interrupt_done);
  EXPECT_TRUE(completion_done);
}
