// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/authentication/consistency_promo_signin/coordinator/consistency_promo_signin_mediator.h"

#import <UIKit/UIKit.h>

#import <memory>
#import <optional>

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
#import "components/test/ios/test_utils.h"
#import "ios/chrome/browser/authentication/ui_bundled/authentication_flow/authentication_flow.h"
#import "ios/chrome/browser/authentication/ui_bundled/authentication_test_util.h"
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
    EXPECT_OCMOCK_VERIFY((id)authentication_flow_mock_);
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
    mediator_ = [[ConsistencyPromoSigninMediator alloc]
        initWithAccountManagerService:chrome_account_manager_service
                authenticationService:auth_service
                      identityManager:identity_manager
                    accountReconcilor:account_reconcilor
                      userPrefService:GetPrefService()
                          accessPoint:access_point];
    mediator_.delegate = mediator_delegate_mock_;
    return mediator_;
  }

  void SimulateCookieFetchSuccess(id<SystemIdentity> identity) {
    CHECK(!ShouldEnableIdentityInAuthErrorFlag());
    gaia::ListedAccount account;
    account.id = CoreAccountId::FromGaiaId(identity.gaiaId);
    signin::AccountsInCookieJarInfo cookie_jar_info(
        /*accounts_are_fresh=*/true,
        /*accounts=*/{account});
    [(id<IdentityManagerObserverBridgeDelegate>)mediator_
        onAccountsInCookieUpdated:cookie_jar_info
                            error:GoogleServiceAuthError(
                                      GoogleServiceAuthError::State::NONE)];
  }

  void SimulateCookieFetchError() {
    CHECK(!ShouldEnableIdentityInAuthErrorFlag());
    signin::AccountsInCookieJarInfo cookie_jar_info(
        /*accounts_are_fresh=*/false,
        /*accounts=*/{});
    [(id<IdentityManagerObserverBridgeDelegate>)mediator_
        onAccountsInCookieUpdated:cookie_jar_info
                            error:GoogleServiceAuthError(
                                      GoogleServiceAuthError::State::
                                          INVALID_GAIA_CREDENTIALS)];
  }

  void SimulateCookieFetchTimeout() {
    CHECK(!ShouldEnableIdentityInAuthErrorFlag());
    task_environment_.AdvanceClock(base::Seconds(30));
  }

  void ExpectAuthFlowStartAndSetResult(
      id<SystemIdentity> identity,
      signin_metrics::AccessPoint access_point,
      signin_ui::CancelationReason cancelation_reason) {
    bool success =
        cancelation_reason == signin_ui::CancelationReason::kNotCanceled;
    OCMExpect([mediator_delegate_mock_
        consistencyPromoSigninMediatorSigninStarted:[OCMArg any]]);
    OCMExpect([authentication_flow_mock_ identity]).andReturn(identity);
    AuthenticationService* auth_service =
        AuthenticationServiceFactory::GetForProfile(profile_.get());
    auto startSignInCallback = ^(NSInvocation* invocation) {
      if (success) {
        auth_service->SignIn(identity, access_point);
      }
      // The mediator_ is the AuthenticationFlow’s delegate.
      CHECK(authentication_flow_mock_delegate_);
      [authentication_flow_mock_delegate_
          authenticationFlowDidSignInInSameProfileWithCancelationReason:
              cancelation_reason
                                                               identity:
                                                                   identity];
    };
    OCMExpect([authentication_flow_mock_
        setDelegate:[OCMArg
                        checkWithBlock:^(id<AuthenticationFlowDelegate> value) {
                          authentication_flow_mock_delegate_ = value;
                          return value == mediator_;
                        }]]);
    OCMExpect([authentication_flow_mock_ startSignIn])
        .andDo(startSignInCallback);
  }

  void ExpectWebSigninTrackerCreationAndCaptureCallback() {
    if (!ShouldEnableIdentityInAuthErrorFlag()) {
      // WebSigninTracker is not created in the legacy flow.
      return;
    }
    OCMExpect(
        [mediator_delegate_mock_
            trackWebSigninWithIdentityManager:ios::OCM::AnyPointer<
                                                  signin::IdentityManager>()
                            accountReconcilor:ios::OCM::AnyPointer<
                                                  AccountReconcilor>()
                                signinAccount:CoreAccountId()
                                 withCallback:ios::OCM::AnyPointer<
                                                  base::RepeatingCallback<void(
                                                      signin::WebSigninTracker::
                                                          Result)>>()
                                  withTimeout:std::nullopt])
        .ignoringNonObjectArgs()
        .andAssignStructParameterAtAddressToVariable(captured_callback_, 3);
  }

  bool ShouldEnableIdentityInAuthErrorFlag() { return GetParam(); }

 protected:
  AuthenticationFlow* authentication_flow_mock_ =
      OCMStrictClassMock([AuthenticationFlow class]);
  id<ConsistencyPromoSigninMediatorDelegate> mediator_delegate_mock_ =
      OCMStrictProtocolMock(@protocol(ConsistencyPromoSigninMediatorDelegate));
  base::RepeatingCallback<void(signin::WebSigninTracker::Result)>
      captured_callback_;
  ConsistencyPromoSigninMediator* mediator_;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  id<AuthenticationFlowDelegate> authentication_flow_mock_delegate_;
  // Needed for test profile.
  web::WebTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  std::unique_ptr<TestProfileIOS> profile_;
};

// Tests start and cancel by user.
TEST_P(ConsistencyPromoSigninMediatorTest, StartAndStopForCancel) {
  base::HistogramTester histogram_tester;

  mediator_ = BuildConsistencyPromoSigninMediator(
      signin_metrics::AccessPoint::kWebSignin);
  [mediator_ disconnectWithResult:SigninCoordinatorResultCanceledByUser];

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

  mediator_ = BuildConsistencyPromoSigninMediator(
      signin_metrics::AccessPoint::kWebSignin);
  [mediator_ disconnectWithResult:SigninCoordinatorResultInterrupted];

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

  ExpectAuthFlowStartAndSetResult(kDefaultIdentity,
                                  signin_metrics::AccessPoint::kWebSignin,
                                  signin_ui::CancelationReason::kNotCanceled);
  ExpectWebSigninTrackerCreationAndCaptureCallback();

  mediator_ = BuildConsistencyPromoSigninMediator(
      signin_metrics::AccessPoint::kWebSignin);
  [mediator_ signinWithAuthenticationFlow:authentication_flow_mock_];

  OCMExpect([mediator_delegate_mock_
      consistencyPromoSigninMediatorSignInDone:mediator_
                                  withIdentity:kDefaultIdentity]);

  if (ShouldEnableIdentityInAuthErrorFlag()) {
    CHECK(captured_callback_);
    captured_callback_.Run(signin::WebSigninTracker::Result::kSuccess);
  } else {
    SimulateCookieFetchSuccess(kDefaultIdentity);
  }

  [mediator_ disconnectWithResult:SigninCoordinatorResultSuccess];

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

  ExpectAuthFlowStartAndSetResult(kNonDefaultIdentity,
                                  signin_metrics::AccessPoint::kWebSignin,
                                  signin_ui::CancelationReason::kNotCanceled);
  ExpectWebSigninTrackerCreationAndCaptureCallback();

  mediator_ = BuildConsistencyPromoSigninMediator(
      signin_metrics::AccessPoint::kWebSignin);

  [mediator_ signinWithAuthenticationFlow:authentication_flow_mock_];

  OCMExpect([mediator_delegate_mock_
      consistencyPromoSigninMediatorSignInDone:mediator_
                                  withIdentity:kNonDefaultIdentity]);

  if (ShouldEnableIdentityInAuthErrorFlag()) {
    CHECK(captured_callback_);
    captured_callback_.Run(signin::WebSigninTracker::Result::kSuccess);
  } else {
    SimulateCookieFetchSuccess(kNonDefaultIdentity);
  }

  [mediator_ disconnectWithResult:SigninCoordinatorResultSuccess];

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

  mediator_ = BuildConsistencyPromoSigninMediator(
      signin_metrics::AccessPoint::kWebSignin);
  [mediator_ systemIdentityAdded:kDefaultIdentity];

  ExpectAuthFlowStartAndSetResult(kDefaultIdentity,
                                  signin_metrics::AccessPoint::kWebSignin,
                                  signin_ui::CancelationReason::kNotCanceled);
  ExpectWebSigninTrackerCreationAndCaptureCallback();

  [mediator_ signinWithAuthenticationFlow:authentication_flow_mock_];

  OCMExpect([mediator_delegate_mock_
      consistencyPromoSigninMediatorSignInDone:mediator_
                                  withIdentity:kDefaultIdentity]);

  if (ShouldEnableIdentityInAuthErrorFlag()) {
    CHECK(captured_callback_);
    captured_callback_.Run(signin::WebSigninTracker::Result::kSuccess);
  } else {
    SimulateCookieFetchSuccess(kDefaultIdentity);
  }

  [mediator_ disconnectWithResult:SigninCoordinatorResultSuccess];

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

  mediator_ = BuildConsistencyPromoSigninMediator(
      signin_metrics::AccessPoint::kWebSignin);

  ExpectAuthFlowStartAndSetResult(kDefaultIdentity,
                                  signin_metrics::AccessPoint::kWebSignin,
                                  signin_ui::CancelationReason::kNotCanceled);
  ExpectWebSigninTrackerCreationAndCaptureCallback();

  [mediator_ signinWithAuthenticationFlow:authentication_flow_mock_];

  // The error is only signaled after AuthenticationService::Signout() and
  // that's async.
  __block auto error_wait_loop = std::make_unique<base::RunLoop>();
  OCMExpect([mediator_delegate_mock_
                consistencyPromoSigninMediator:mediator_
                                errorDidHappen:
                                    ConsistencyPromoSigninMediatorErrorGeneric
                                  withIdentity:kDefaultIdentity])
      .andDo(^(NSInvocation*) {
        error_wait_loop->Quit();
      });

  if (ShouldEnableIdentityInAuthErrorFlag()) {
    CHECK(captured_callback_);
    captured_callback_.Run(signin::WebSigninTracker::Result::kOtherError);
  } else {
    SimulateCookieFetchError();
  }

  error_wait_loop->Run();

  [mediator_ disconnectWithResult:SigninCoordinatorResultCanceledByUser];

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

  mediator_ = BuildConsistencyPromoSigninMediator(
      signin_metrics::AccessPoint::kWebSignin);

  ExpectAuthFlowStartAndSetResult(kDefaultIdentity,
                                  signin_metrics::AccessPoint::kWebSignin,
                                  signin_ui::CancelationReason::kNotCanceled);
  ExpectWebSigninTrackerCreationAndCaptureCallback();

  [mediator_ signinWithAuthenticationFlow:authentication_flow_mock_];

  // The error is only signaled after AuthenticationService::Signout() and
  // that's async.
  __block auto error_wait_loop = std::make_unique<base::RunLoop>();
  OCMExpect([mediator_delegate_mock_
                consistencyPromoSigninMediator:mediator_
                                errorDidHappen:
                                    ConsistencyPromoSigninMediatorErrorTimeout
                                  withIdentity:kDefaultIdentity])
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

  [mediator_ disconnectWithResult:SigninCoordinatorResultCanceledByUser];

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

  mediator_ = BuildConsistencyPromoSigninMediator(
      signin_metrics::AccessPoint::kWebSignin);

  ExpectAuthFlowStartAndSetResult(kDefaultIdentity,
                                  signin_metrics::AccessPoint::kWebSignin,
                                  signin_ui::CancelationReason::kFailed);

  // The error is only signaled after AuthenticationService::Signout() and
  // that's async (note: the user never really signed-in in this case, but the
  // call is made nonetheless).
  __block auto error_wait_loop = std::make_unique<base::RunLoop>();
  OCMExpect([mediator_delegate_mock_
                consistencyPromoSigninMediatorSignInCancelled:mediator_])
      .andDo(^(NSInvocation*) {
        error_wait_loop->Quit();
      });

  [mediator_ signinWithAuthenticationFlow:authentication_flow_mock_];

  error_wait_loop->Run();

  [mediator_ disconnectWithResult:SigninCoordinatorResultCanceledByUser];

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

  mediator_ = BuildConsistencyPromoSigninMediator(
      signin_metrics::AccessPoint::kSettings);

  ExpectAuthFlowStartAndSetResult(kDefaultIdentity,
                                  signin_metrics::AccessPoint::kSettings,
                                  signin_ui::CancelationReason::kNotCanceled);
  OCMExpect([mediator_delegate_mock_
      consistencyPromoSigninMediatorSignInDone:mediator_
                                  withIdentity:kDefaultIdentity]);

  [mediator_ signinWithAuthenticationFlow:authentication_flow_mock_];
  [mediator_ disconnectWithResult:SigninCoordinatorResultSuccess];

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
