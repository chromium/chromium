// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/authentication/ui_bundled/signin/deeplink_signin/deeplink_signin_coordinator.h"

#import <UIKit/UIKit.h>

#import <memory>
#import <utility>

#import "base/functional/bind.h"
#import "base/test/test_future.h"
#import "components/signin/public/base/signin_deep_link_payload.h"
#import "components/signin/public/base/signin_metrics.h"
#import "components/test/ios/test_utils.h"
#import "ios/chrome/app/profile/profile_state.h"
#import "ios/chrome/browser/authentication/add_account_signin/coordinator/add_account_signin_coordinator.h"
#import "ios/chrome/browser/authentication/ui_bundled/continuation.h"
#import "ios/chrome/browser/authentication/ui_bundled/fullscreen_signin/coordinator/fullscreen_signin_coordinator.h"
#import "ios/chrome/browser/authentication/ui_bundled/fullscreen_signin/coordinator/fullscreen_signin_coordinator_delegate.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin/signin_constants.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin/signin_context_style.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_manager_ios.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/browser/signin/model/fake_system_identity_manager.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
#import "ios/chrome/browser/sync/model/test_sync_service_utils.h"
#import "ios/chrome/test/fakes/fake_ui_view_controller.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

namespace {

// Test fixture for `DeeplinkSigninCoordinator`.
class DeeplinkSigninCoordinatorTest : public PlatformTest {
 public:
  DeeplinkSigninCoordinatorTest() {
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
    base_view_controller_ = [[FakeUIViewController alloc] init];
    fake_identity_ = [FakeSystemIdentity fakeIdentity1];
  }

  ~DeeplinkSigninCoordinatorTest() override {
    EXPECT_OCMOCK_VERIFY((id)add_account_coordinator_mock_);
    EXPECT_OCMOCK_VERIFY((id)fullscreen_signin_coordinator_mock_);
  }

  void TearDown() override {
    [coordinator_ stop];
    coordinator_ = nil;
    browser_.reset();
    profile_ = nullptr;
    base_view_controller_ = nil;
    scene_state_ = nil;
    profile_state_ = nil;
    PlatformTest::TearDown();
  }

 protected:
  web::WebTaskEnvironment task_environment_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  TestProfileManagerIOS profile_manager_;
  raw_ptr<TestProfileIOS> profile_ = nullptr;
  std::unique_ptr<Browser> browser_;
  ProfileState* profile_state_ = nil;
  SceneState* scene_state_ = nil;
  UIViewController* base_view_controller_ = nil;
  FakeSystemIdentity* fake_identity_ = nil;
  DeeplinkSigninCoordinator* coordinator_ = nil;
  AddAccountSigninCoordinator* add_account_coordinator_mock_ = nil;
  FullscreenSigninCoordinator* fullscreen_signin_coordinator_mock_ = nil;
};

// Tests that when the add-account flow finishes with an identity that is not
// valid in `ChromeAccountManagerService`, `DeeplinkSigninCoordinator` completes
// with `SigninCoordinatorResultCanceledByUser`.
TEST_F(DeeplinkSigninCoordinatorTest, AddAccountWithInvalidIdentityCancels) {
  coordinator_ = [[DeeplinkSigninCoordinator alloc]
             initWithBaseViewController:base_view_controller_
                                browser:browser_.get()
                   selectedAccountEmail:fake_identity_.userEmail
      changeProfileContinuationProvider:DoNothingContinuationProvider()
                     externalEntryPoint:signin::ExternalEntryPoint::
                                            kDesktopDefault];

  __block base::test::TestFuture<SigninCoordinatorResultOrIdentity>
      completion_future;
  coordinator_.signinCompletion = ^(SigninCoordinator* coordinator,
                                    SigninCoordinatorResultOrIdentity result) {
    completion_future.SetValue(std::move(result));
  };

  add_account_coordinator_mock_ =
      OCMStrictClassMock([AddAccountSigninCoordinator class]);
  OCMExpect([(id)add_account_coordinator_mock_ alloc])
      .andReturn(add_account_coordinator_mock_);
  ChangeProfileContinuationProvider* provider = ios::OCM::AnyPointer<
      base::RepeatingCallback<ChangeProfileContinuation()>>();
  OCMExpect([add_account_coordinator_mock_
                initWithBaseViewController:base_view_controller_
                                   browser:browser_.get()
                              contextStyle:SigninContextStyle::kDeeplinkSignin
                               accessPoint:signin_metrics::AccessPoint::
                                               kDeepLinkDefault
                               promoAction:signin_metrics::PromoAction::
                                               PROMO_ACTION_NO_SIGNIN_PROMO
                              signinIntent:AddAccountSigninIntent::kAddAccount
                            prefilledEmail:fake_identity_.userEmail
                      continuationProvider:*provider])
      .andReturn(add_account_coordinator_mock_);

  __block SigninCoordinatorCompletionCallback add_account_completion = nil;
  OCMExpect([add_account_coordinator_mock_
      setSigninCompletion:[OCMArg checkWithBlock:^BOOL(
                                      SigninCoordinatorCompletionCallback
                                          callback) {
        add_account_completion = [callback copy];
        return YES;
      }]]);
  OCMExpect([add_account_coordinator_mock_ start]);

  [coordinator_ start];
  ASSERT_TRUE(add_account_completion);

  OCMExpect([add_account_coordinator_mock_ stop]);
  // `fake_identity_` was not added to `SystemIdentityManager`, so
  // `IsValidIdentity` returns false.
  add_account_completion(add_account_coordinator_mock_, fake_identity_);

  SigninCoordinatorResultOrIdentity result = completion_future.Get();
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), SigninCoordinatorResultCanceledByUser);
}

// Tests that `DeeplinkSigninCoordinator` stops its child
// `FullscreenSigninCoordinator` before invoking `signinCompletion`.
TEST_F(DeeplinkSigninCoordinatorTest,
       FullscreenSigninStopsChildCoordinatorBeforeCompletion) {
  FakeSystemIdentityManager* system_identity_manager =
      FakeSystemIdentityManager::FromSystemIdentityManager(
          GetApplicationContext()->GetSystemIdentityManager());
  system_identity_manager->AddIdentity(fake_identity_);

  coordinator_ = [[DeeplinkSigninCoordinator alloc]
             initWithBaseViewController:base_view_controller_
                                browser:browser_.get()
                   selectedAccountEmail:fake_identity_.userEmail
      changeProfileContinuationProvider:DoNothingContinuationProvider()
                     externalEntryPoint:signin::ExternalEntryPoint::
                                            kDesktopDefault];

  __block bool child_coordinator_stopped = false;
  __block base::test::TestFuture<SigninCoordinatorResultOrIdentity>
      completion_future;
  coordinator_.signinCompletion = ^(SigninCoordinator* coordinator,
                                    SigninCoordinatorResultOrIdentity result) {
    EXPECT_TRUE(child_coordinator_stopped);
    completion_future.SetValue(std::move(result));
  };

  fullscreen_signin_coordinator_mock_ =
      OCMStrictClassMock([FullscreenSigninCoordinator class]);
  OCMExpect([(id)fullscreen_signin_coordinator_mock_ alloc])
      .andReturn(fullscreen_signin_coordinator_mock_);
  ChangeProfileContinuationProvider* provider = ios::OCM::AnyPointer<
      base::RepeatingCallback<ChangeProfileContinuation()>>();
  OCMExpect(
      [fullscreen_signin_coordinator_mock_
                 initWithBaseViewController:base_view_controller_
                                    browser:browser_.get()
                             screenProvider:[OCMArg any]
                               contextStyle:SigninContextStyle::kDeeplinkSignin
                                accessPoint:signin_metrics::AccessPoint::
                                                kDeepLinkDefault
          changeProfileContinuationProvider:*provider])
      .andReturn(fullscreen_signin_coordinator_mock_);
  OCMExpect([fullscreen_signin_coordinator_mock_
      setDelegate:static_cast<id<FullscreenSigninCoordinatorDelegate>>(
                      coordinator_)]);
  OCMExpect([fullscreen_signin_coordinator_mock_ setIdentity:fake_identity_]);
  OCMExpect([fullscreen_signin_coordinator_mock_ setCanSwitchAccount:YES]);
  OCMExpect([fullscreen_signin_coordinator_mock_ start]);

  [coordinator_ start];

  OCMExpect([fullscreen_signin_coordinator_mock_ stop]).andDo(^(NSInvocation*) {
    child_coordinator_stopped = true;
  });

  const SigninCoordinatorResult expected_result =
      SigninCoordinatorResultCanceledByUser;
  [static_cast<id<FullscreenSigninCoordinatorDelegate>>(coordinator_)
      fullscreenSigninCoordinatorDone:fullscreen_signin_coordinator_mock_
                               result:base::unexpected(expected_result)];

  SigninCoordinatorResultOrIdentity result = completion_future.Get();
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), expected_result);
}

}  // namespace
