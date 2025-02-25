// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/authentication/ui_bundled/signin/consistency_promo_signin/consistency_promo_signin_mediator.h"

#import <UIKit/UIKit.h>

#import <memory>
#import <optional>

#import "base/functional/callback_forward.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/metrics/histogram_tester.h"
#import "base/test/scoped_feature_list.h"
#import "base/time/time.h"
#import "components/signin/public/base/signin_metrics.h"
#import "components/signin/public/base/signin_switches.h"
#import "components/signin/public/browser/web_signin_tracker.h"
#import "components/signin/public/identity_manager/accounts_in_cookie_jar_info.h"
#import "components/signin/public/identity_manager/identity_manager.h"
#import "components/signin/public/identity_manager/objc/identity_manager_observer_bridge.h"
#import "components/sync_preferences/testing_pref_service_syncable.h"
#import "ios/chrome/browser/authentication/ui_bundled/authentication_flow/authentication_flow.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/signin/model/account_reconcilor_factory.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service_factory.h"
#import "ios/chrome/browser/signin/model/fake_authentication_service_delegate.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/browser/signin/model/fake_system_identity_manager.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

namespace {

const FakeSystemIdentity* kDefaultIdentity = [FakeSystemIdentity fakeIdentity1];
const FakeSystemIdentity* kNonDefaultIdentity =
    [FakeSystemIdentity fakeIdentity2];

class ConsistencyPromoSigninMediatorTest
    : public PlatformTest,
      public testing::WithParamInterface<bool> {
 public:
  void SetUp() override {
    scoped_feature_list_.InitWithFeatureState(
        switches::kEnableIdentityInAuthError,
        ShouldEnableIdentityInAuthErrorFlag());
    PlatformTest::SetUp();
    GetSystemIdentityManager()->AddIdentity(kDefaultIdentity);
    GetSystemIdentityManager()->AddIdentity(kNonDefaultIdentity);
    TestProfileIOS::Builder builder;
    builder.AddTestingFactory(
        AuthenticationServiceFactory::GetInstance(),
        AuthenticationServiceFactory::GetFactoryWithDelegate(
            std::make_unique<FakeAuthenticationServiceDelegate>()));
    profile_ = std::move(builder).Build();
    ASSERT_EQ(ChromeAccountManagerServiceFactory::GetForProfile(profile_.get())
                  ->GetDefaultIdentity(),
              kDefaultIdentity);
  }

  void TearDown() override {
    EXPECT_OCMOCK_VERIFY((id)mediator_delegate_mock_);
    EXPECT_OCMOCK_VERIFY((id)authentication_flow_);
    PlatformTest::TearDown();
  }

  sync_preferences::TestingPrefServiceSyncable* GetPrefService() {
    return profile_->GetTestingPrefService();
  }

  FakeSystemIdentityManager* GetSystemIdentityManager() {
    return FakeSystemIdentityManager::FromSystemIdentityManager(
        GetApplicationContext()->GetSystemIdentityManager());
  }

  ConsistencyPromoSigninMediator* BuildConsistencyPromoSigninMediator(
      signin_metrics::AccessPoint access_point) {
    ChromeAccountManagerService* chrome_account_manager_service =
        ChromeAccountManagerServiceFactory::GetForProfile(profile_.get());
    AuthenticationService* auth_service =
        AuthenticationServiceFactory::GetForProfile(profile_.get());
    signin::IdentityManager* identity_manager =
        IdentityManagerFactory::GetForProfile(profile_.get());
    AccountReconcilor* account_reconcilor =
        ios::AccountReconcilorFactory::GetForProfile(profile_.get());
    ConsistencyPromoSigninMediator* mediator =
        [[ConsistencyPromoSigninMediator alloc]
            initWithAccountManagerService:chrome_account_manager_service
                    authenticationService:auth_service
                          identityManager:identity_manager
                        accountReconcilor:account_reconcilor
                          userPrefService:GetPrefService()
                              accessPoint:access_point];
    mediator.delegate = mediator_delegate_mock_;
    return mediator;
  }

  void SimulateCookieFetchSuccess(ConsistencyPromoSigninMediator* mediator,
                                  id<SystemIdentity> identity) {
    CHECK(!ShouldEnableIdentityInAuthErrorFlag());
    gaia::ListedAccount account;
    account.id = CoreAccountId::FromGaiaId(GaiaId(identity.gaiaID));
    signin::AccountsInCookieJarInfo cookie_jar_info(
        /*accounts_are_fresh=*/true,
        /*accounts=*/{account});
    [(id<IdentityManagerObserverBridgeDelegate>)mediator
        onAccountsInCookieUpdated:cookie_jar_info
                            error:GoogleServiceAuthError(
                                      GoogleServiceAuthError::State::NONE)];
  }

  void SimulateCookieFetchError(ConsistencyPromoSigninMediator* mediator) {
    CHECK(!ShouldEnableIdentityInAuthErrorFlag());
    signin::AccountsInCookieJarInfo cookie_jar_info(
        /*accounts_are_fresh=*/false,
        /*accounts=*/{});
    [(id<IdentityManagerObserverBridgeDelegate>)mediator
        onAccountsInCookieUpdated:cookie_jar_info
                            error:GoogleServiceAuthError(
                                      GoogleServiceAuthError::State::
                                          INVALID_GAIA_CREDENTIALS)];
  }

  void SimulateCookieFetchTimeout() {
    CHECK(!ShouldEnableIdentityInAuthErrorFlag());
    task_environment_.AdvanceClock(base::Seconds(30));
  }

  void ExpectAuthFlowStartAndSetResult(id<SystemIdentity> identity,
                                       signin_metrics::AccessPoint access_point,
                                       SigninCoordinatorResult result) {
    bool success =
        result == SigninCoordinatorResult::SigninCoordinatorResultSuccess;
    OCMExpect([mediator_delegate_mock_
        consistencyPromoSigninMediatorSigninStarted:[OCMArg any]]);
    OCMExpect([authentication_flow_ identity]).andReturn(identity);
    AuthenticationService* auth_service =
        AuthenticationServiceFactory::GetForProfile(profile_.get());
    OCMExpect([authentication_flow_
        startSignInWithCompletion:[OCMArg
                                      checkWithBlock:^BOOL(
                                          signin_ui::SigninCompletionCallback
                                              callback) {
                                        if (success) {
                                          auth_service->SignIn(identity,
                                                               access_point);
                                        }
                                        callback(result);
                                        return YES;
                                      }]]);
  }

  void ExpectWebSigninTrackerCreationAndCaptureCallback() {
    if (!ShouldEnableIdentityInAuthErrorFlag()) {
      // WebSigninTracker is not created in the legacy flow.
      return;
    }
    OCMExpect(
        [mediator_delegate_mock_
            trackWebSigninWithIdentityManager:static_cast<
                                                  signin::IdentityManager*>(
                                                  [OCMArg anyPointer])
                            accountReconcilor:static_cast<AccountReconcilor*>(
                                                  [OCMArg anyPointer])
                                signinAccount:CoreAccountId()
                                 withCallback:
                                     static_cast<base::RepeatingCallback<void(
                                         signin::WebSigninTracker::Result)>*>(
                                         [OCMArg anyPointer])
                                  withTimeout:std::nullopt])
        .ignoringNonObjectArgs()
        .andDo(^(NSInvocation* invocation) {
          [invocation retainArguments];
          const base::RepeatingCallback<void(signin::WebSigninTracker::Result)>*
              callbackPtr = nullptr;
          [invocation getArgument:&callbackPtr atIndex:5];
          captured_callback_ = *callbackPtr;
        });
  }

  bool ShouldEnableIdentityInAuthErrorFlag() { return GetParam(); }

 protected:
  AuthenticationFlow* authentication_flow_ =
      OCMStrictClassMock([AuthenticationFlow class]);
  id<ConsistencyPromoSigninMediatorDelegate> mediator_delegate_mock_ =
      OCMStrictProtocolMock(@protocol(ConsistencyPromoSigninMediatorDelegate));
  base::RepeatingCallback<void(signin::WebSigninTracker::Result)>
      captured_callback_;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  // Needed for test profile.
  web::WebTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  std::unique_ptr<TestProfileIOS> profile_;
};

// Tests start and cancel by user.
TEST_P(ConsistencyPromoSigninMediatorTest, StartAndStopForCancel) {
  base::HistogramTester histogram_tester;

  ConsistencyPromoSigninMediator* mediator =
      BuildConsistencyPromoSigninMediator(
          signin_metrics::AccessPoint::kWebSignin);
  [mediator disconnectWithResult:SigninCoordinatorResultCanceledByUser];

  histogram_tester.ExpectTotalCount(
      "Signin.AccountConsistencyPromoAction.Shown", 1);
  histogram_tester.ExpectBucketCount(
      "Signin.AccountConsistencyPromoAction.Shown",
      signin_metrics::AccessPoint::kWebSignin, 1);
  histogram_tester.ExpectTotalCount(
      "Signin.AccountConsistencyPromoAction.DismissedButton", 1);
  histogram_tester.ExpectBucketCount(
      "Signin.AccountConsistencyPromoAction.DismissedButton",
      signin_metrics::AccessPoint::kWebSignin, 1);
}

// Tests start and interrupt.
TEST_P(ConsistencyPromoSigninMediatorTest, StartAndStopForInterrupt) {
  base::HistogramTester histogram_tester;

  ConsistencyPromoSigninMediator* mediator =
      BuildConsistencyPromoSigninMediator(
          signin_metrics::AccessPoint::kWebSignin);
  [mediator disconnectWithResult:SigninCoordinatorResultInterrupted];

  histogram_tester.ExpectTotalCount(
      "Signin.AccountConsistencyPromoAction.Shown", 1);
  histogram_tester.ExpectBucketCount(
      "Signin.AccountConsistencyPromoAction.Shown",
      signin_metrics::AccessPoint::kWebSignin, 1);
  histogram_tester.ExpectTotalCount(
      "Signin.AccountConsistencyPromoAction.DismissedOther", 1);
  histogram_tester.ExpectBucketCount(
      "Signin.AccountConsistencyPromoAction.DismissedOther",
      signin_metrics::AccessPoint::kWebSignin, 1);
}

// Tests start and sign-in with default identity.
TEST_P(ConsistencyPromoSigninMediatorTest,
       SigninCoordinatorResultSuccessWithDefaultIdentity) {
  base::HistogramTester histogram_tester;
  GetPrefService()->SetInteger(prefs::kSigninWebSignDismissalCount, 1);

  ExpectAuthFlowStartAndSetResult(
      kDefaultIdentity, signin_metrics::AccessPoint::kWebSignin,
      SigninCoordinatorResult::SigninCoordinatorResultSuccess);
  ExpectWebSigninTrackerCreationAndCaptureCallback();

  ConsistencyPromoSigninMediator* mediator =
      BuildConsistencyPromoSigninMediator(
          signin_metrics::AccessPoint::kWebSignin);
  [mediator signinWithAuthenticationFlow:authentication_flow_];

  OCMExpect([mediator_delegate_mock_
      consistencyPromoSigninMediatorSignInDone:mediator
                                  withIdentity:kDefaultIdentity]);

  if (ShouldEnableIdentityInAuthErrorFlag()) {
    CHECK(captured_callback_);
    captured_callback_.Run(signin::WebSigninTracker::Result::kSuccess);
  } else {
    SimulateCookieFetchSuccess(mediator, kDefaultIdentity);
  }

  [mediator disconnectWithResult:SigninCoordinatorResultSuccess];

  EXPECT_EQ(0,
            GetPrefService()->GetInteger(prefs::kSigninWebSignDismissalCount));
  histogram_tester.ExpectTotalCount(
      "Signin.AccountConsistencyPromoAction.Shown", 1);
  histogram_tester.ExpectBucketCount(
      "Signin.AccountConsistencyPromoAction.Shown",
      signin_metrics::AccessPoint::kWebSignin, 1);
  histogram_tester.ExpectTotalCount(
      "Signin.AccountConsistencyPromoAction.SignedInWithDefaultAccount", 1);
  histogram_tester.ExpectBucketCount(
      "Signin.AccountConsistencyPromoAction.SignedInWithDefaultAccount",
      signin_metrics::AccessPoint::kWebSignin, 1);
}

// Tests start and sign-in with secondary identity.
TEST_P(ConsistencyPromoSigninMediatorTest,
       SigninCoordinatorResultSuccessWithSecondaryIdentity) {
  base::HistogramTester histogram_tester;
  GetPrefService()->SetInteger(prefs::kSigninWebSignDismissalCount, 1);

  ExpectAuthFlowStartAndSetResult(
      kNonDefaultIdentity, signin_metrics::AccessPoint::kWebSignin,
      SigninCoordinatorResult::SigninCoordinatorResultSuccess);
  ExpectWebSigninTrackerCreationAndCaptureCallback();

  ConsistencyPromoSigninMediator* mediator =
      BuildConsistencyPromoSigninMediator(
          signin_metrics::AccessPoint::kWebSignin);
  [mediator signinWithAuthenticationFlow:authentication_flow_];

  OCMExpect([mediator_delegate_mock_
      consistencyPromoSigninMediatorSignInDone:mediator
                                  withIdentity:kNonDefaultIdentity]);

  if (ShouldEnableIdentityInAuthErrorFlag()) {
    CHECK(captured_callback_);
    captured_callback_.Run(signin::WebSigninTracker::Result::kSuccess);
  } else {
    SimulateCookieFetchSuccess(mediator, kNonDefaultIdentity);
  }

  [mediator disconnectWithResult:SigninCoordinatorResultSuccess];

  histogram_tester.ExpectTotalCount(
      "Signin.AccountConsistencyPromoAction.Shown", 1);
  histogram_tester.ExpectBucketCount(
      "Signin.AccountConsistencyPromoAction.Shown",
      signin_metrics::AccessPoint::kWebSignin, 1);
  histogram_tester.ExpectTotalCount(
      "Signin.AccountConsistencyPromoAction.SignedInWithNonDefaultAccount", 1);
  histogram_tester.ExpectBucketCount(
      "Signin.AccountConsistencyPromoAction.SignedInWithNonDefaultAccount",
      signin_metrics::AccessPoint::kWebSignin, 1);
}

// Tests start and sign-in with an added identity.
TEST_P(ConsistencyPromoSigninMediatorTest,
       SigninCoordinatorResultSuccessWithAddedIdentity) {
  base::HistogramTester histogram_tester;

  ConsistencyPromoSigninMediator* mediator =
      BuildConsistencyPromoSigninMediator(
          signin_metrics::AccessPoint::kWebSignin);
  [mediator systemIdentityAdded:kDefaultIdentity];

  ExpectAuthFlowStartAndSetResult(
      kDefaultIdentity, signin_metrics::AccessPoint::kWebSignin,
      SigninCoordinatorResult::SigninCoordinatorResultSuccess);
  ExpectWebSigninTrackerCreationAndCaptureCallback();

  [mediator signinWithAuthenticationFlow:authentication_flow_];

  OCMExpect([mediator_delegate_mock_
      consistencyPromoSigninMediatorSignInDone:mediator
                                  withIdentity:kDefaultIdentity]);

  if (ShouldEnableIdentityInAuthErrorFlag()) {
    CHECK(captured_callback_);
    captured_callback_.Run(signin::WebSigninTracker::Result::kSuccess);
  } else {
    SimulateCookieFetchSuccess(mediator, kDefaultIdentity);
  }

  [mediator disconnectWithResult:SigninCoordinatorResultSuccess];

  histogram_tester.ExpectTotalCount(
      "Signin.AccountConsistencyPromoAction.Shown", 1);
  histogram_tester.ExpectBucketCount(
      "Signin.AccountConsistencyPromoAction.Shown",
      signin_metrics::AccessPoint::kWebSignin, 1);
  histogram_tester.ExpectTotalCount(
      "Signin.AccountConsistencyPromoAction.SignedInWithAddedAccount", 1);
  histogram_tester.ExpectBucketCount(
      "Signin.AccountConsistencyPromoAction.SignedInWithAddedAccount",
      signin_metrics::AccessPoint::kWebSignin, 1);
}

// Tests the case where browser sign-in succeeds but the request to fetch
// cookies comes back with an error, causing the user to be signed out from the
// browser too.
TEST_P(ConsistencyPromoSigninMediatorTest, CookiesError) {
  base::HistogramTester histogram_tester;

  ConsistencyPromoSigninMediator* mediator =
      BuildConsistencyPromoSigninMediator(
          signin_metrics::AccessPoint::kWebSignin);

  ExpectAuthFlowStartAndSetResult(
      kDefaultIdentity, signin_metrics::AccessPoint::kWebSignin,
      SigninCoordinatorResult::SigninCoordinatorResultSuccess);
  ExpectWebSigninTrackerCreationAndCaptureCallback();

  [mediator signinWithAuthenticationFlow:authentication_flow_];

  // The error is only signaled after AuthenticationService::Signout() and
  // that's async.
  __block auto error_wait_loop = std::make_unique<base::RunLoop>();
  OCMExpect([mediator_delegate_mock_
                consistencyPromoSigninMediator:mediator
                                errorDidHappen:
                                    ConsistencyPromoSigninMediatorErrorGeneric])
      .andDo(^(NSInvocation*) {
        error_wait_loop->Quit();
      });

  if (ShouldEnableIdentityInAuthErrorFlag()) {
    CHECK(captured_callback_);
    captured_callback_.Run(signin::WebSigninTracker::Result::kOtherError);
  } else {
    SimulateCookieFetchError(mediator);
  }

  error_wait_loop->Run();

  [mediator disconnectWithResult:SigninCoordinatorResultCanceledByUser];

  histogram_tester.ExpectTotalCount(
      "Signin.AccountConsistencyPromoAction.Shown", 1);
  histogram_tester.ExpectBucketCount(
      "Signin.AccountConsistencyPromoAction.Shown",
      signin_metrics::AccessPoint::kWebSignin, 1);
  histogram_tester.ExpectTotalCount(
      "Signin.AccountConsistencyPromoAction.DismissedButton", 1);
  histogram_tester.ExpectBucketCount(
      "Signin.AccountConsistencyPromoAction.DismissedButton",
      signin_metrics::AccessPoint::kWebSignin, 1);
  histogram_tester.ExpectTotalCount(
      "Signin.AccountConsistencyPromoAction.GenericErrorShown", 1);
  histogram_tester.ExpectBucketCount(
      "Signin.AccountConsistencyPromoAction.GenericErrorShown",
      signin_metrics::AccessPoint::kWebSignin, 1);
}

// Tests the case where browser sign-in succeeds but cookies never arrive on
// time, causing the user to be signed out from the browser too.
TEST_P(ConsistencyPromoSigninMediatorTest, CookiesTimeout) {
  base::HistogramTester histogram_tester;

  ConsistencyPromoSigninMediator* mediator =
      BuildConsistencyPromoSigninMediator(
          signin_metrics::AccessPoint::kWebSignin);

  ExpectAuthFlowStartAndSetResult(
      kDefaultIdentity, signin_metrics::AccessPoint::kWebSignin,
      SigninCoordinatorResult::SigninCoordinatorResultSuccess);
  ExpectWebSigninTrackerCreationAndCaptureCallback();

  [mediator signinWithAuthenticationFlow:authentication_flow_];

  // The error is only signaled after AuthenticationService::Signout() and
  // that's async.
  __block auto error_wait_loop = std::make_unique<base::RunLoop>();
  OCMExpect([mediator_delegate_mock_
                consistencyPromoSigninMediator:mediator
                                errorDidHappen:
                                    ConsistencyPromoSigninMediatorErrorTimeout])
      .andDo(^(NSInvocation*) {
        error_wait_loop->Quit();
      });

  if (ShouldEnableIdentityInAuthErrorFlag()) {
    CHECK(captured_callback_);
    captured_callback_.Run(signin::WebSigninTracker::Result::kTimeout);
  } else {
    SimulateCookieFetchTimeout();
  }

  error_wait_loop->Run();

  [mediator disconnectWithResult:SigninCoordinatorResultCanceledByUser];

  histogram_tester.ExpectTotalCount(
      "Signin.AccountConsistencyPromoAction.Shown", 1);
  histogram_tester.ExpectBucketCount(
      "Signin.AccountConsistencyPromoAction.Shown",
      signin_metrics::AccessPoint::kWebSignin, 1);
  histogram_tester.ExpectTotalCount(
      "Signin.AccountConsistencyPromoAction.TimeoutErrorShown", 1);
  histogram_tester.ExpectBucketCount(
      "Signin.AccountConsistencyPromoAction.TimeoutErrorShown",
      signin_metrics::AccessPoint::kWebSignin, 1);
  histogram_tester.ExpectTotalCount(
      "Signin.AccountConsistencyPromoAction.DismissedButton", 1);
  histogram_tester.ExpectBucketCount(
      "Signin.AccountConsistencyPromoAction.DismissedButton",
      signin_metrics::AccessPoint::kWebSignin, 1);
}

// Tests the case where browser sign-in fails.
TEST_P(ConsistencyPromoSigninMediatorTest, AuthFlowError) {
  base::HistogramTester histogram_tester;

  ConsistencyPromoSigninMediator* mediator =
      BuildConsistencyPromoSigninMediator(
          signin_metrics::AccessPoint::kWebSignin);

  ExpectAuthFlowStartAndSetResult(
      kDefaultIdentity, signin_metrics::AccessPoint::kWebSignin,
      SigninCoordinatorResult::SigninCoordinatorResultInterrupted);

  // The error is only signaled after AuthenticationService::Signout() and
  // that's async (note: the user never really signed-in in this case, but the
  // call is made nonetheless).
  __block auto error_wait_loop = std::make_unique<base::RunLoop>();
  OCMExpect([mediator_delegate_mock_
                consistencyPromoSigninMediatorSignInCancelled:mediator])
      .andDo(^(NSInvocation*) {
        error_wait_loop->Quit();
      });

  [mediator signinWithAuthenticationFlow:authentication_flow_];

  error_wait_loop->Run();

  [mediator disconnectWithResult:SigninCoordinatorResultCanceledByUser];

  histogram_tester.ExpectTotalCount(
      "Signin.AccountConsistencyPromoAction.Shown", 1);
  histogram_tester.ExpectBucketCount(
      "Signin.AccountConsistencyPromoAction.Shown",
      signin_metrics::AccessPoint::kWebSignin, 1);
  histogram_tester.ExpectTotalCount(
      "Signin.AccountConsistencyPromoAction.SignInFailed", 1);
  histogram_tester.ExpectBucketCount(
      "Signin.AccountConsistencyPromoAction.SignInFailed",
      signin_metrics::AccessPoint::kWebSignin, 1);
  histogram_tester.ExpectTotalCount(
      "Signin.AccountConsistencyPromoAction.DismissedButton", 1);
  histogram_tester.ExpectBucketCount(
      "Signin.AccountConsistencyPromoAction.DismissedButton",
      signin_metrics::AccessPoint::kWebSignin, 1);
}

// Tests start and sign-in with default identity from Settings access point, and
// then update the cookies. Related to crrev.com/1471140.
TEST_P(ConsistencyPromoSigninMediatorTest, SigninWithoutCookies) {
  base::HistogramTester histogram_tester;
  GetPrefService()->SetInteger(prefs::kSigninWebSignDismissalCount, 1);

  ConsistencyPromoSigninMediator* mediator =
      BuildConsistencyPromoSigninMediator(
          signin_metrics::AccessPoint::kSettings);

  ExpectAuthFlowStartAndSetResult(
      kDefaultIdentity, signin_metrics::AccessPoint::kSettings,
      SigninCoordinatorResult::SigninCoordinatorResultSuccess);
  OCMExpect([mediator_delegate_mock_
      consistencyPromoSigninMediatorSignInDone:mediator
                                  withIdentity:kDefaultIdentity]);

  [mediator signinWithAuthenticationFlow:authentication_flow_];
  [mediator disconnectWithResult:SigninCoordinatorResultSuccess];

  EXPECT_EQ(1,
            GetPrefService()->GetInteger(prefs::kSigninWebSignDismissalCount));
  histogram_tester.ExpectTotalCount(
      "Signin.AccountConsistencyPromoAction.Shown", 1);
  histogram_tester.ExpectBucketCount(
      "Signin.AccountConsistencyPromoAction.Shown",
      signin_metrics::AccessPoint::kSettings, 1);
  histogram_tester.ExpectTotalCount(
      "Signin.AccountConsistencyPromoAction.SignedInWithDefaultAccount", 1);
  histogram_tester.ExpectBucketCount(
      "Signin.AccountConsistencyPromoAction.SignedInWithDefaultAccount",
      signin_metrics::AccessPoint::kSettings, 1);
}

INSTANTIATE_TEST_SUITE_P(, ConsistencyPromoSigninMediatorTest, testing::Bool());

}  // namespace
