// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/authentication/ui_bundled/signin/consistency_promo_signin/consistency_promo_signin_coordinator.h"

#import <UIKit/UIKit.h>

#import "components/signin/core/browser/account_reconcilor.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin/consistency_promo_signin/consistency_default_account/consistency_default_account_coordinator.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin/consistency_promo_signin/consistency_promo_signin_mediator.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin/consistency_promo_signin/consistency_sheet/consistency_sheet_navigation_controller.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

namespace {

class ConsistencyPromoSigninCoordinatorTest : public PlatformTest {
 public:
  ConsistencyPromoSigninCoordinatorTest() {
    profile_ = TestProfileIOS::Builder().Build();
    browser_ = std::make_unique<TestBrowser>(profile_.get());
    base_view_controller_mock_ = OCMStrictClassMock([UIViewController class]);
    coordinator_ = [[ConsistencyPromoSigninCoordinator alloc]
        initWithBaseViewController:base_view_controller_mock_
                           browser:browser_.get()
                       accessPoint:access_point_];
    mediator_mock_ = OCMStrictClassMock([ConsistencyPromoSigninMediator class]);
    consistency_default_account_coordinator_mock_ =
        OCMStrictClassMock([ConsistencyDefaultAccountCoordinator class]);
    consistency_sheet_navigation_controller_mock_ =
        OCMStrictClassMock([ConsistencySheetNavigationController class]);
  }

  void TearDown() override {
    EXPECT_OCMOCK_VERIFY((id)mediator_mock_);
    EXPECT_OCMOCK_VERIFY((id)base_view_controller_mock_);
    EXPECT_OCMOCK_VERIFY((id)consistency_default_account_coordinator_mock_);
    EXPECT_OCMOCK_VERIFY((id)consistency_sheet_navigation_controller_mock_);
    PlatformTest::TearDown();
  }

  void StartCoordinator() {
    // View controller for default account coordinator.
    UIViewController* default_account_view_controller =
        [[UIViewController alloc] init];
    // Setup the navigation controller.
    OCMStub([(id)consistency_sheet_navigation_controller_mock_ alloc])
        .andReturn(consistency_sheet_navigation_controller_mock_);
    OCMStub([consistency_sheet_navigation_controller_mock_
                presentingViewController])
        .andReturn(base_view_controller_mock_);
    OCMExpect([consistency_sheet_navigation_controller_mock_
                  initWithNibName:nil
                           bundle:nil])
        .andReturn(consistency_sheet_navigation_controller_mock_);
    OCMExpect([consistency_sheet_navigation_controller_mock_
        setDelegate:(id)coordinator_]);
    OCMExpect([consistency_sheet_navigation_controller_mock_
        setModalPresentationStyle:UIModalPresentationCustom]);
    OCMExpect([consistency_sheet_navigation_controller_mock_
        setTransitioningDelegate:(id)coordinator_]);
    OCMExpect([consistency_sheet_navigation_controller_mock_
        setViewControllers:@[ default_account_view_controller ]]);
    // Setup the account coordinator.
    OCMStub([(id)consistency_default_account_coordinator_mock_ alloc])
        .andReturn(consistency_default_account_coordinator_mock_);
    OCMExpect([consistency_default_account_coordinator_mock_
                  initWithBaseViewController:
                      consistency_sheet_navigation_controller_mock_
                                     browser:browser_.get()
                                 accessPoint:access_point_])
        .andReturn(consistency_default_account_coordinator_mock_);
    OCMExpect([consistency_default_account_coordinator_mock_
        setDelegate:(id)coordinator_]);
    OCMExpect([consistency_default_account_coordinator_mock_
        setLayoutDelegate:(id)coordinator_]);
    OCMExpect([consistency_default_account_coordinator_mock_ start]);
    OCMStub([consistency_default_account_coordinator_mock_ viewController])
        .andReturn(default_account_view_controller);
    // Setup the mediator.
    OCMExpect([(id)mediator_mock_ alloc]).andReturn(mediator_mock_);
    OCMExpect(
        [mediator_mock_
            initWithAccountManagerService:reinterpret_cast<
                                              ChromeAccountManagerService*>(
                                              [OCMArg anyPointer])
                    authenticationService:reinterpret_cast<
                                              AuthenticationService*>(
                                              [OCMArg anyPointer])
                          identityManager:reinterpret_cast<
                                              signin::IdentityManager*>(
                                              [OCMArg anyPointer])
                        accountReconcilor:reinterpret_cast<AccountReconcilor*>(
                                              [OCMArg anyPointer])
                          userPrefService:reinterpret_cast<PrefService*>(
                                              [OCMArg anyPointer])
                              accessPoint:access_point_])
        .andReturn(mediator_mock_);
    OCMExpect([base_view_controller_mock_ presentViewController:[OCMArg any]
                                                       animated:YES
                                                     completion:nil]);
    OCMExpect([mediator_mock_ setDelegate:(id)coordinator_]);
    [coordinator_ start];
  }

  // Returns coordinator_ as a ConsistencyDefaultAccountCoordinatorDelegate.
  id<ConsistencyDefaultAccountCoordinatorDelegate>
  GetConsistencyDefaultAccountCoordinatorDelegate() {
    return static_cast<id<ConsistencyDefaultAccountCoordinatorDelegate>>(
        coordinator_);
  }

  // Returns coordinator_ as a ConsistencyPromoSigninMediatorDelegate.
  id<ConsistencyPromoSigninMediatorDelegate>
  GetConsistencyPromoSigninMediatorDelegate() {
    return static_cast<id<ConsistencyPromoSigninMediatorDelegate>>(
        coordinator_);
  }

 protected:
  ConsistencyPromoSigninCoordinator* coordinator_ = nil;
  ConsistencyPromoSigninMediator* mediator_mock_ = nil;
  UIViewController* base_view_controller_mock_ = nil;
  const signin_metrics::AccessPoint access_point_ =
      signin_metrics::AccessPoint::kStartPage;
  ConsistencyDefaultAccountCoordinator*
      consistency_default_account_coordinator_mock_ = nil;
  ConsistencySheetNavigationController*
      consistency_sheet_navigation_controller_mock_ = nil;

 private:
  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<TestBrowser> browser_;
};

// Tests that all coordinators are stopped and delegates are set to nil when
// cancelling ConsistencyPromoSigninCoordinator.
// See crbug.com/372272374.
TEST_F(ConsistencyPromoSigninCoordinatorTest, StartAndCancel) {
  __block SigninCoordinatorResult coordinator_result;
  __block id<SystemIdentity> signed_in_identity = nil;
  __block bool signin_completion_called = false;
  coordinator_.signinCompletion =
      ^(SigninCoordinatorResult result, id<SystemIdentity> identity) {
        coordinator_result = result;
        signed_in_identity = identity;
        signin_completion_called = true;
      };
  StartCoordinator();
  // Simulate cancel from the user.
  // Expect the view to be dismissed.
  __block ProceduralBlock dissmiss_completion = nil;
  OCMExpect([base_view_controller_mock_
      dismissViewControllerAnimated:YES
                         completion:[OCMArg checkWithBlock:^(
                                                ProceduralBlock completion) {
                           dissmiss_completion = completion;
                           return YES;
                         }]]);
  // Expect the navigation controller delegates to be reset.
  OCMExpect([consistency_sheet_navigation_controller_mock_ setDelegate:nil]);
  OCMExpect([consistency_sheet_navigation_controller_mock_
      setTransitioningDelegate:nil]);
  // Expect account coordinator delegates to be reset.
  OCMExpect([consistency_default_account_coordinator_mock_ setDelegate:nil]);
  OCMExpect(
      [consistency_default_account_coordinator_mock_ setLayoutDelegate:nil]);
  OCMExpect([consistency_default_account_coordinator_mock_ stop]);
  // Expect the mediator to be stopped and the delegate to be reset.
  OCMExpect([mediator_mock_ setDelegate:nil]);
  OCMExpect([mediator_mock_
      disconnectWithResult:SigninCoordinatorResultCanceledByUser]);
  // Call the cancel skip method.
  [GetConsistencyDefaultAccountCoordinatorDelegate()
      consistencyDefaultAccountCoordinatorSkip:
          consistency_default_account_coordinator_mock_];
  // Call the view controller dismiss completion block.
  EXPECT_NE(nil, dissmiss_completion);
  dissmiss_completion();
  // Expect the sign-in completion block called.
  EXPECT_TRUE(signin_completion_called);
  EXPECT_EQ(SigninCoordinatorResultCanceledByUser, coordinator_result);
  EXPECT_EQ(nil, signed_in_identity);
  [coordinator_ stop];
}

}  // namespace
