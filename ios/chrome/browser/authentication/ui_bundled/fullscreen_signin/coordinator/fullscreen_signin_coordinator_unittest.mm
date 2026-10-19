// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/authentication/ui_bundled/fullscreen_signin/coordinator/fullscreen_signin_coordinator.h"

#import <UIKit/UIKit.h>

#import <memory>
#import <utility>

#import "base/functional/bind.h"
#import "base/test/test_future.h"
#import "components/signin/public/base/signin_metrics.h"
#import "components/test/ios/test_utils.h"
#import "ios/chrome/app/profile/profile_state.h"
#import "ios/chrome/browser/authentication/fullscreen_signin_screen/coordinator/fullscreen_signin_screen_coordinator.h"
#import "ios/chrome/browser/authentication/ui_bundled/continuation.h"
#import "ios/chrome/browser/authentication/ui_bundled/fullscreen_signin/coordinator/fullscreen_signin_coordinator_delegate.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin/signin_constants.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin/signin_context_style.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin/signin_screen_provider.h"
#import "ios/chrome/browser/first_run/public/first_run_screen_delegate.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_manager_ios.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/browser/signin/model/fake_system_identity_manager.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
#import "ios/chrome/browser/sync/model/test_sync_service_utils.h"
#import "ios/chrome/test/app/uikit_test_util.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

namespace {

// Test fixture for `FullscreenSigninCoordinator`.
class FullscreenSigninCoordinatorTest : public PlatformTest {
 public:
  FullscreenSigninCoordinatorTest() {
    TestProfileIOS::Builder builder;
    profile_state_ = [[ProfileState alloc] initWithAppState:nil];
    scene_state_ = [[SceneState alloc] init];
    scene_state_.profileState = profile_state_;
    builder.AddTestingFactory(
        AuthenticationServiceFactory::GetInstance(),
        AuthenticationServiceFactory::GetDefaultFactory());
    builder.AddTestingFactory(SyncServiceFactory::GetInstance(),
                              base::BindRepeating(&CreateTestSyncService));
    profile_ = profile_manager_.AddProfileWithBuilder(std::move(builder));
    browser_ = std::make_unique<TestBrowser>(profile_, scene_state_);

    UIView.animationsEnabled = NO;
    window_ = [[UIWindow alloc]
        initWithWindowScene:chrome_test_util::GetAnyWindowScene()];
    window_.rootViewController = [[UIViewController alloc] init];
    [window_ addSubview:window_.rootViewController.view];

    FakeSystemIdentityManager* system_identity_manager =
        FakeSystemIdentityManager::FromSystemIdentityManager(
            GetApplicationContext()->GetSystemIdentityManager());
    fake_identity_ = [FakeSystemIdentity fakeIdentity1];
    system_identity_manager->AddIdentity(fake_identity_);
  }

  ~FullscreenSigninCoordinatorTest() override {
    EXPECT_OCMOCK_VERIFY((id)fullscreen_signin_screen_coordinator_mock_);
    EXPECT_OCMOCK_VERIFY((id)delegate_mock_);
  }

  void TearDown() override {
    [coordinator_ stop];
    coordinator_ = nil;
    browser_.reset();
    profile_ = nullptr;
    window_ = nil;
    scene_state_ = nil;
    profile_state_ = nil;
    PlatformTest::TearDown();
  }

  // Initializes and starts `coordinator_`.
  void StartCoordinator() {
    delegate_mock_ =
        OCMStrictProtocolMock(@protocol(FullscreenSigninCoordinatorDelegate));
    coordinator_ = [[FullscreenSigninCoordinator alloc]
               initWithBaseViewController:window_.rootViewController
                                  browser:browser_.get()
                           screenProvider:[[SigninScreenProvider alloc] init]
                             contextStyle:SigninContextStyle::kDefault
                              accessPoint:signin_metrics::AccessPoint::kSettings
        changeProfileContinuationProvider:DoNothingContinuationProvider()];
    coordinator_.delegate = delegate_mock_;

    fullscreen_signin_screen_coordinator_mock_ =
        OCMStrictClassMock([FullscreenSigninScreenCoordinator class]);
    OCMExpect([(id)fullscreen_signin_screen_coordinator_mock_ alloc])
        .andReturn(fullscreen_signin_screen_coordinator_mock_);
    ChangeProfileContinuationProvider* provider = ios::OCM::AnyPointer<
        base::RepeatingCallback<ChangeProfileContinuation()>>();
    OCMExpect(
        [fullscreen_signin_screen_coordinator_mock_
             initWithBaseNavigationController:[OCMArg any]
                                      browser:browser_.get()
                                     delegate:static_cast<
                                                  id<FirstRunScreenDelegate>>(
                                                  coordinator_)
                                 contextStyle:SigninContextStyle::kDefault
                                  accessPoint:signin_metrics::AccessPoint::
                                                  kSettings
                                  promoAction:signin_metrics::PromoAction::
                                                  PROMO_ACTION_NO_SIGNIN_PROMO
            changeProfileContinuationProvider:*provider])
        .andReturn(fullscreen_signin_screen_coordinator_mock_);
    OCMExpect([fullscreen_signin_screen_coordinator_mock_ setIdentity:nil]);
    OCMExpect(
        [fullscreen_signin_screen_coordinator_mock_ setCanSwitchAccount:NO]);
    OCMExpect([fullscreen_signin_screen_coordinator_mock_ start]);

    [coordinator_ start];
  }

 protected:
  web::WebTaskEnvironment task_environment_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  TestProfileManagerIOS profile_manager_;
  raw_ptr<TestProfileIOS> profile_ = nullptr;
  std::unique_ptr<Browser> browser_;
  FullscreenSigninCoordinator* coordinator_ = nil;
  id<FullscreenSigninCoordinatorDelegate> delegate_mock_ = nil;
  FullscreenSigninScreenCoordinator*
      fullscreen_signin_screen_coordinator_mock_ = nil;
  UIWindow* window_ = nil;
  FakeSystemIdentity* fake_identity_ = nil;
  ProfileState* profile_state_ = nil;
  SceneState* scene_state_ = nil;
};

// Tests that `FullscreenSigninCoordinator` queries the primary identity when
// dismissal completes rather than snapshotting it before dismissal.
TEST_F(FullscreenSigninCoordinatorTest,
       PrimaryIdentityQueriedOnDismissalCompletion) {
  StartCoordinator();

  AuthenticationService* auth_service =
      AuthenticationServiceFactory::GetForProfile(profile_);
  auth_service->SignIn(fake_identity_, signin_metrics::AccessPoint::kStartPage);

  OCMExpect([fullscreen_signin_screen_coordinator_mock_ stop]);
  __block base::test::TestFuture<SigninCoordinatorResult> result_future;
  const SigninCoordinatorResult expected_result =
      SigninCoordinatorResultCanceledByUser;
  OCMExpect([delegate_mock_
                fullscreenSigninCoordinatorWantsToBeStopped:coordinator_
                                                     result:expected_result])
      .andDo(^(NSInvocation*) {
        result_future.SetValue(expected_result);
      });

  [static_cast<id<FirstRunScreenDelegate>>(coordinator_)
      firstRunScreenCoordinatorWantsToBeStopped:
          fullscreen_signin_screen_coordinator_mock_];

  // Sign out while the dismissal animation is still in progress before its
  // completion block runs.
  auth_service->SignOut(signin_metrics::ProfileSignout::kTest);

  EXPECT_EQ(result_future.Get(), expected_result);
}

// Tests that `FullscreenSigninCoordinator` reports success when the primary
// identity remains signed in after dismissal completes.
TEST_F(FullscreenSigninCoordinatorTest, CompletesWithSuccessWhenSignedIn) {
  StartCoordinator();

  AuthenticationService* auth_service =
      AuthenticationServiceFactory::GetForProfile(profile_);
  auth_service->SignIn(fake_identity_, signin_metrics::AccessPoint::kStartPage);

  OCMExpect([fullscreen_signin_screen_coordinator_mock_ stop]);
  __block base::test::TestFuture<SigninCoordinatorResult> result_future;
  const SigninCoordinatorResult expected_result =
      SigninCoordinatorResultSuccess;
  OCMExpect([delegate_mock_
                fullscreenSigninCoordinatorWantsToBeStopped:coordinator_
                                                     result:expected_result])
      .andDo(^(NSInvocation*) {
        result_future.SetValue(expected_result);
      });

  [static_cast<id<FirstRunScreenDelegate>>(coordinator_)
      firstRunScreenCoordinatorWantsToBeStopped:
          fullscreen_signin_screen_coordinator_mock_];

  EXPECT_EQ(result_future.Get(), expected_result);
}

}  // namespace
