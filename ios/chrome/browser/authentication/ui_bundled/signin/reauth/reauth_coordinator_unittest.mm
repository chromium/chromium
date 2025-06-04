// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/authentication/ui_bundled/signin/reauth/reauth_coordinator.h"

#import <concepts>
#import <type_traits>

#import "base/apple/foundation_util.h"
#import "base/functional/bind.h"
#import "base/functional/callback_forward.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/bind.h"
#import "base/test/metrics/histogram_tester.h"
#import "components/signin/public/base/signin_metrics.h"
#import "components/signin/public/identity_manager/account_info.h"
#import "components/signin/public/identity_manager/identity_test_utils.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/browser/signin/model/fake_system_identity_interaction_manager.h"
#import "ios/chrome/browser/signin/model/fake_system_identity_manager.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/signin/model/identity_test_environment_browser_state_adaptor.h"
#import "ios/chrome/browser/signin/model/system_identity_interaction_manager.h"
#import "ios/web/common/uikit_ui_util.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

namespace {

template <typename ArgumentType>
  requires std::is_trivially_copyable_v<__unsafe_unretained ArgumentType>
class ArgumentCaptor {
 public:
  using InvocationHandlingBlock = void (^)(NSInvocation* invocation);

  // Passed to `andDo` to capture an argument.
  InvocationHandlingBlock Capture(int argumentIndex) {
    return ^(NSInvocation* invocation) {
      __unsafe_unretained ArgumentType capturedPtr;
      [invocation getArgument:&capturedPtr atIndex:argumentIndex];
      captured_value_ = capturedPtr;
      did_capture_ = true;
    };
  }

  // Used to obtain the captured value after the capturing happened.
  ArgumentType Get() const {
    CHECK(did_capture_);
    return captured_value_;
  }

 private:
  bool did_capture_ = false;
  ArgumentType captured_value_;
};

}  // namespace

class ReauthCoordinatorTest : public PlatformTest {
 public:
  ReauthCoordinatorTest() {
    TestProfileIOS::Builder builder;
    builder.AddTestingFactory(
        IdentityManagerFactory::GetInstance(),
        base::BindRepeating(IdentityTestEnvironmentBrowserStateAdaptor::
                                BuildIdentityManagerForTests));
    profile_ = std::move(builder).Build();
    browser_ = std::make_unique<TestBrowser>(profile_.get());

    fake_system_identity_manager()->SetInteractionManagerFactory(
        base::BindLambdaForTesting([this]() {
          return static_cast<id<SystemIdentityInteractionManager>>(
              mock_interaction_manager_);
        }));
  }

  ~ReauthCoordinatorTest() override {
    EXPECT_OCMOCK_VERIFY(mock_interaction_manager_);
    EXPECT_OCMOCK_VERIFY(mock_delegate_);
  }

  OCMockObject<ReauthCoordinatorDelegate>* mock_delegate() {
    return mock_delegate_;
  }

  FakeSystemIdentityManager* fake_system_identity_manager() {
    return FakeSystemIdentityManager::FromSystemIdentityManager(
        GetApplicationContext()->GetSystemIdentityManager());
  }

  AccountInfo MakeIdentityAvailable(id<SystemIdentity> identity) {
    fake_system_identity_manager()->AddIdentity(identity);
    return signin::MakeAccountAvailable(
        IdentityManagerFactory::GetForProfile(profile_.get()),
        signin::AccountAvailabilityOptionsBuilder()
            .WithGaiaId(GaiaId(identity.gaiaID))
            .Build(base::SysNSStringToUTF8(identity.userEmail)));
  }

 protected:
  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<TestBrowser> browser_;
  OCMockObject<SystemIdentityInteractionManager>* mock_interaction_manager_ =
      OCMStrictProtocolMock(@protocol(SystemIdentityInteractionManager));
  OCMockObject<ReauthCoordinatorDelegate>* mock_delegate_ =
      OCMStrictProtocolMock(@protocol(ReauthCoordinatorDelegate));
};

TEST_F(ReauthCoordinatorTest, ReauthCompletedSuccessfully) {
  base::HistogramTester histogram_tester;
  id<SystemIdentity> identity = [FakeSystemIdentity fakeIdentity1];
  AccountInfo account = MakeIdentityAvailable(identity);

  ReauthCoordinator* reauth_coordinator = [[ReauthCoordinator alloc]
      initWithBaseViewController:GetAnyKeyWindow().rootViewController
                         browser:browser_.get()
                         account:account
                     accessPoint:signin_metrics::AccessPoint::kWebSignin];
  reauth_coordinator.delegate = mock_delegate_;

  ArgumentCaptor<SigninCompletionBlock> signin_completion_block_captor;
  OCMExpect([mock_interaction_manager_
                startAuthActivityWithViewController:OCMOCK_ANY
                                          userEmail:base::SysUTF8ToNSString(
                                                        account.email)
                                         completion:OCMOCK_ANY])
      .andDo(signin_completion_block_captor.Capture(/*argumentIndex=*/4));
  [reauth_coordinator start];

  OCMExpect([mock_delegate_ reauthFinishedWithResult:ReauthResult::kSuccess]);
  SigninCompletionBlock completion_block = signin_completion_block_captor.Get();
  CHECK(completion_block);
  completion_block(identity, nil);
  histogram_tester.ExpectUniqueSample("Signin.Reauth.InSigninFlow.Started",
                                      signin_metrics::AccessPoint::kWebSignin,
                                      1);
  histogram_tester.ExpectUniqueSample("Signin.Reauth.InSigninFlow.Completed",
                                      signin_metrics::AccessPoint::kWebSignin,
                                      1);
}

TEST_F(ReauthCoordinatorTest, ReauthCancelledByUser) {
  base::HistogramTester histogram_tester;
  AccountInfo account =
      MakeIdentityAvailable([FakeSystemIdentity fakeIdentity1]);

  ReauthCoordinator* reauth_coordinator = [[ReauthCoordinator alloc]
      initWithBaseViewController:GetAnyKeyWindow().rootViewController
                         browser:browser_.get()
                         account:account
                     accessPoint:signin_metrics::AccessPoint::kWebSignin];
  reauth_coordinator.delegate = mock_delegate_;

  ArgumentCaptor<SigninCompletionBlock> signin_completion_block_captor;
  OCMExpect([mock_interaction_manager_
                startAuthActivityWithViewController:OCMOCK_ANY
                                          userEmail:base::SysUTF8ToNSString(
                                                        account.email)
                                         completion:OCMOCK_ANY])
      .andDo(signin_completion_block_captor.Capture(/*argumentIndex=*/4));
  [reauth_coordinator start];

  OCMExpect(
      [mock_delegate_ reauthFinishedWithResult:ReauthResult::kCancelledByUser]);
  SigninCompletionBlock completion_block = signin_completion_block_captor.Get();
  CHECK(completion_block);
  // When the passed identity is is `nil`, it means that the flow was cancelled
  // by the user.
  completion_block(nil, nil);
  histogram_tester.ExpectUniqueSample("Signin.Reauth.InSigninFlow.Started",
                                      signin_metrics::AccessPoint::kWebSignin,
                                      1);
  histogram_tester.ExpectUniqueSample("Signin.Reauth.InSigninFlow.Cancelled",
                                      signin_metrics::AccessPoint::kWebSignin,
                                      1);
}

TEST_F(ReauthCoordinatorTest, ReauthInterrupted) {
  base::HistogramTester histogram_tester;
  AccountInfo account =
      MakeIdentityAvailable([FakeSystemIdentity fakeIdentity1]);

  ReauthCoordinator* reauth_coordinator = [[ReauthCoordinator alloc]
      initWithBaseViewController:GetAnyKeyWindow().rootViewController
                         browser:browser_.get()
                         account:account
                     accessPoint:signin_metrics::AccessPoint::kWebSignin];
  reauth_coordinator.delegate = mock_delegate_;

  OCMExpect([mock_interaction_manager_
      startAuthActivityWithViewController:OCMOCK_ANY
                                userEmail:base::SysUTF8ToNSString(account.email)
                               completion:OCMOCK_ANY]);
  [reauth_coordinator start];

  OCMExpect(
      [mock_delegate_ reauthFinishedWithResult:ReauthResult::kInterrupted]);
  OCMExpect([mock_interaction_manager_ cancelAuthActivityAnimated:NO]);
  [reauth_coordinator stop];
  histogram_tester.ExpectUniqueSample("Signin.Reauth.InSigninFlow.Started",
                                      signin_metrics::AccessPoint::kWebSignin,
                                      1);
  histogram_tester.ExpectUniqueSample("Signin.Reauth.InSigninFlow.Interrupted",
                                      signin_metrics::AccessPoint::kWebSignin,
                                      1);
}
