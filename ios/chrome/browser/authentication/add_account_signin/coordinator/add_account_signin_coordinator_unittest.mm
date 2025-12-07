// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/authentication/add_account_signin/coordinator/add_account_signin_coordinator.h"

#import <UIKit/UIKit.h>

#import "ios/chrome/app/profile/profile_state.h"
#import "ios/chrome/browser/authentication/add_account_signin/coordinator/add_account_signin_manager.h"
#import "ios/chrome/browser/authentication/ui_bundled/continuation.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/fake_authentication_service_delegate.h"
#import "ios/chrome/test/fakes/fake_ui_view_controller.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

namespace {

class AddAccountSigninCoordinatorTest : public PlatformTest {
 public:
  AddAccountSigninCoordinatorTest() {
    // The profile state will receive UI blocker request. They are not tested
    // here, so it’s a non-strict mock.
    profile_state_ = OCMClassMock([ProfileState class]);
    scene_state_ = [[SceneState alloc] initWithAppState:nil];
    scene_state_.profileState = profile_state_;
    TestProfileIOS::Builder builder = TestProfileIOS::Builder();
    builder.AddTestingFactory(
        AuthenticationServiceFactory::GetInstance(),
        AuthenticationServiceFactory::GetFactoryWithDelegate(
            std::make_unique<FakeAuthenticationServiceDelegate>()));
    profile_ = std::move(builder).Build();
    browser_ = std::make_unique<TestBrowser>(profile_.get(), scene_state_);
    base_view_controller_ = [[FakeUIViewController alloc] init];
    coordinator_ = [[AddAccountSigninCoordinator alloc]
        initWithBaseViewController:base_view_controller_
                           browser:browser_.get()
                      contextStyle:SigninContextStyle::kDefault
                       accessPoint:signin_metrics::AccessPoint::kSettings
                       promoAction:signin_metrics::PromoAction::
                                       PROMO_ACTION_NO_SIGNIN_PROMO
                      signinIntent:AddAccountSigninIntent::kAddAccount
                    prefilledEmail:nil
              continuationProvider:DoNothingContinuationProvider()];

    add_account_signin_manager_mock_ =
        OCMStrictClassMock([AddAccountSigninManager class]);
    OCMExpect([(id)add_account_signin_manager_mock_ alloc])
        .andReturn(add_account_signin_manager_mock_);
    OCMExpect([[(id)add_account_signin_manager_mock_ ignoringNonObjectArgs]
                  initWithBaseViewController:base_view_controller_
                                 prefService:nullptr
                             identityManager:nullptr
                  identityInteractionManager:[OCMArg any]
                              prefilledEmail:nil])
        .andReturn(add_account_signin_manager_mock_);
    OCMExpect([add_account_signin_manager_mock_
        setDelegate:GetAddAccountSigninManagerDelegate()]);
  }

  ~AddAccountSigninCoordinatorTest() override {
    EXPECT_OCMOCK_VERIFY((id)add_account_signin_manager_mock_);
  }

  id<AddAccountSigninManagerDelegate> GetAddAccountSigninManagerDelegate() {
    return static_cast<id<AddAccountSigninManagerDelegate>>(coordinator_);
  }

 protected:
  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<TestBrowser> browser_;
  // Required for UI blocker.
  ProfileState* profile_state_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  SceneState* scene_state_;

  AddAccountSigninCoordinator* coordinator_;
  UIViewController* base_view_controller_;
  AddAccountSigninManager* add_account_signin_manager_mock_;
};

// Tests that AddAccountSigninCoordinator doesn't call its signinCompletion
// block when being stopped while showing an alert dialog.
TEST_F(AddAccountSigninCoordinatorTest, StopCoordinatorWhileShowingErrorAlert) {
  // Open the coordiantor.
  OCMExpect([add_account_signin_manager_mock_
      showSigninWithIntent:AddAccountSigninIntent::kAddAccount]);
  __block BOOL signinCompletionCalled = NO;
  coordinator_.signinCompletion =
      ^(SigninCoordinator* coordinator, SigninCoordinatorResult result,
        id<SystemIdentity> identity) {
        signinCompletionCalled = YES;
      };
  [coordinator_ start];
  // Generate an error from AddAccountSigninManager.
  base::RunLoop run_loop1;
  task_environment_.GetMainThreadTaskRunner()->PostTask(
      FROM_HERE, run_loop1.QuitClosure());
  run_loop1.Run();
  OCMExpect([add_account_signin_manager_mock_ setDelegate:nil]);
  NSError* error = [NSError errorWithDomain:@"Error Domain"
                                       code:-42
                                   userInfo:nil];
  [GetAddAccountSigninManagerDelegate()
      addAccountSigninManagerFinishedWithResult:SigninAddAccountToDeviceResult::
                                                    kError
                                       identity:nil
                                          error:error];
  // Stop the coordinator.
  [coordinator_ stop];
  base::RunLoop run_loop2;
  task_environment_.GetMainThreadTaskRunner()->PostTask(
      FROM_HERE, run_loop2.QuitClosure());
  run_loop2.Run();
  // The sign-in completion should not be called since the owner stopped the
  // coordinator.
  EXPECT_FALSE(signinCompletionCalled);
}

}  // namespace
