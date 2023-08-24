// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/authentication_flow.h"

#import <memory>

#import "base/functional/bind.h"
#import "base/memory/ptr_util.h"
#import "base/test/ios/wait_util.h"
#import "base/test/metrics/histogram_tester.h"
#import "base/test/scoped_feature_list.h"
#import "components/pref_registry/pref_registry_syncable.h"
#import "components/signin/public/base/signin_metrics.h"
#import "components/signin/public/identity_manager/tribool.h"
#import "components/sync/base/features.h"
#import "components/sync_preferences/pref_service_mock_factory.h"
#import "components/sync_preferences/pref_service_syncable.h"
#import "ios/chrome/browser/policy/cloud/user_policy_constants.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/shared/model/prefs/browser_prefs.h"
#import "ios/chrome/browser/signin/authentication_service.h"
#import "ios/chrome/browser/signin/authentication_service_factory.h"
#import "ios/chrome/browser/signin/chrome_account_manager_service.h"
#import "ios/chrome/browser/signin/chrome_account_manager_service_factory.h"
#import "ios/chrome/browser/signin/fake_authentication_service_delegate.h"
#import "ios/chrome/browser/signin/fake_system_identity.h"
#import "ios/chrome/browser/signin/fake_system_identity_manager.h"
#import "ios/chrome/browser/ui/authentication/authentication_flow_performer.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/gtest_support.h"
#import "third_party/ocmock/ocmock_extensions.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

NSString* const kFakeDMToken = @"fake_dm_token";
NSString* const kFakeClientID = @"fake_client_id";

class AuthenticationFlowTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();

    TestChromeBrowserState::Builder builder;
    builder.AddTestingFactory(
        AuthenticationServiceFactory::GetInstance(),
        AuthenticationServiceFactory::GetDefaultFactory());
    builder.SetPrefService(CreatePrefService());
    browser_state_ = builder.Build();
    AuthenticationServiceFactory::CreateAndInitializeForBrowserState(
        browser_state_.get(),
        std::make_unique<FakeAuthenticationServiceDelegate>());
    browser_ = std::make_unique<TestBrowser>(browser_state_.get());

    fake_system_identity_manager()->AddIdentities(
        @[ @"identity1", @"identity2" ]);

    ChromeAccountManagerService* account_manager_service =
        ChromeAccountManagerServiceFactory::GetForBrowserState(
            browser_state_.get());
    NSArray<id<SystemIdentity>>* identities =
        account_manager_service->GetAllIdentities();
    identity1_ = identities[0];
    identity2_ = identities[1];
    managed_identity_ = [FakeSystemIdentity identityWithEmail:@"managed@foo.com"
                                                       gaiaID:@"managed"
                                                         name:@"managed"];
    fake_system_identity_manager()->AddIdentity(managed_identity_);

    sign_in_completion_ = ^(BOOL success) {
      run_loop_.Quit();
      signin_result_ =
          success ? signin::Tribool::kTrue : signin::Tribool::kFalse;
    };
  }

  std::unique_ptr<sync_preferences::PrefServiceSyncable> CreatePrefService() {
    sync_preferences::PrefServiceMockFactory factory;
    scoped_refptr<user_prefs::PrefRegistrySyncable> registry(
        new user_prefs::PrefRegistrySyncable);
    std::unique_ptr<sync_preferences::PrefServiceSyncable> prefs =
        factory.CreateSyncable(registry.get());
    RegisterBrowserStatePrefs(registry.get());
    return prefs;
  }

  AuthenticationFlowPerformer* GetAuthenticationFlowPerformer() {
    return static_cast<AuthenticationFlowPerformer*>(performer_);
  }

  // Creates a new AuthenticationFlow with default values for fields that are
  // not directly useful.
  void CreateAuthenticationFlow(PostSignInAction postSignInAction,
                                id<SystemIdentity> identity,
                                signin_metrics::AccessPoint accessPoint) {
    view_controller_ = [OCMockObject niceMockForClass:[UIViewController class]];
    authentication_flow_ =
        [[AuthenticationFlow alloc] initWithBrowser:browser_.get()
                                           identity:identity
                                        accessPoint:accessPoint
                                   postSignInAction:postSignInAction
                           presentingViewController:view_controller_];
    performer_ =
        [OCMockObject mockForClass:[AuthenticationFlowPerformer class]];
    [authentication_flow_
        setPerformerForTesting:GetAuthenticationFlowPerformer()];
  }

  // Checks if the AuthenticationFlow operation has completed, and whether it
  // was successful.
  void CheckSignInCompletion(bool expected_signed_in) {
    run_loop_.Run();

    const signin::Tribool expected_signin_result =
        expected_signed_in ? signin::Tribool::kTrue : signin::Tribool::kFalse;

    EXPECT_EQ(expected_signin_result, signin_result_);
    [performer_ verify];
  }

  void SetSigninSuccessExpectations(id<SystemIdentity> identity,
                                    signin_metrics::AccessPoint accessPoint,
                                    NSString* hosted_domain) {
    [[performer_ expect] signInIdentity:identity
                          atAccessPoint:accessPoint
                       withHostedDomain:hosted_domain
                         toBrowserState:browser_state_.get()];
  }

  FakeSystemIdentityManager* fake_system_identity_manager() {
    return FakeSystemIdentityManager::FromSystemIdentityManager(
        GetApplicationContext()->GetSystemIdentityManager());
  }

  web::WebTaskEnvironment task_environment_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  AuthenticationFlow* authentication_flow_ = nullptr;
  std::unique_ptr<TestChromeBrowserState> browser_state_;
  std::unique_ptr<Browser> browser_;
  id<SystemIdentity> identity1_ = nil;
  id<SystemIdentity> identity2_ = nil;
  id<SystemIdentity> managed_identity_ = nil;
  OCMockObject* performer_ = nil;
  signin_ui::CompletionCallback sign_in_completion_;
  UIViewController* view_controller_;
  // Used to verify histogram logging.
  base::HistogramTester histogram_tester_;

  // Used to wait for sign-in workflow to complete.
  base::RunLoop run_loop_;
  signin::Tribool signin_result_ = signin::Tribool::kUnknown;
};

// Tests a Sign In of a normal account on the same profile with Sync
// consent granted.
TEST_F(AuthenticationFlowTest, TestSignInSimple) {
  CreateAuthenticationFlow(
      PostSignInAction::kCommitSync, identity1_,
      signin_metrics::AccessPoint::ACCESS_POINT_START_PAGE);

  [[[performer_ expect] andDo:^(NSInvocation*) {
    [authentication_flow_ didFetchManagedStatus:nil];
  }] fetchManagedStatus:browser_state_.get() forIdentity:identity1_];

  [[[performer_ expect] andReturnBool:NO]
      shouldHandleMergeCaseForIdentity:identity1_
                     browserStatePrefs:browser_state_->GetPrefs()];

  SetSigninSuccessExpectations(
      identity1_, signin_metrics::AccessPoint::ACCESS_POINT_START_PAGE, nil);

  [authentication_flow_ startSignInWithCompletion:sign_in_completion_];
  // completion block should not be called synchronously.
  EXPECT_EQ(signin::Tribool::kUnknown, signin_result_);

  CheckSignInCompletion(/*expected_signed_in=*/true);
  histogram_tester_.ExpectUniqueSample(
      "Signin.AccountType.SigninConsent",
      signin_metrics::SigninAccountType::kRegular, 1);
  histogram_tester_.ExpectUniqueSample(
      "Signin.AccountType.SyncConsent",
      signin_metrics::SigninAccountType::kRegular, 1);
}

// Tests that starting sync while the user is already signed in only.
TEST_F(AuthenticationFlowTest, TestAlreadySignedIn) {
  CreateAuthenticationFlow(
      PostSignInAction::kCommitSync, identity1_,
      signin_metrics::AccessPoint::ACCESS_POINT_START_PAGE);

  [[[performer_ expect] andDo:^(NSInvocation*) {
    [authentication_flow_ didFetchManagedStatus:nil];
  }] fetchManagedStatus:browser_state_.get() forIdentity:identity1_];

  [[[performer_ expect] andReturnBool:NO]
      shouldHandleMergeCaseForIdentity:identity1_
                     browserStatePrefs:browser_state_->GetPrefs()];

  SetSigninSuccessExpectations(
      identity1_, signin_metrics::AccessPoint::ACCESS_POINT_START_PAGE, nil);

  AuthenticationServiceFactory::GetForBrowserState(browser_state_.get())
      ->SignIn(identity1_, signin_metrics::AccessPoint::ACCESS_POINT_SETTINGS);
  [authentication_flow_ startSignInWithCompletion:sign_in_completion_];

  CheckSignInCompletion(/*expected_signed_in=*/true);
  histogram_tester_.ExpectUniqueSample(
      "Signin.AccountType.SigninConsent",
      signin_metrics::SigninAccountType::kRegular, 1);
  histogram_tester_.ExpectUniqueSample(
      "Signin.AccountType.SyncConsent",
      signin_metrics::SigninAccountType::kRegular, 1);
}

// Tests a Sign In&Sync of a different account, requiring a sign out of the
// already signed in account, and asking the user whether data should be cleared
// or merged.
TEST_F(AuthenticationFlowTest, TestSignOutUserChoice) {
  CreateAuthenticationFlow(
      PostSignInAction::kCommitSync, identity1_,
      signin_metrics::AccessPoint::ACCESS_POINT_START_PAGE);

  [[[performer_ expect] andDo:^(NSInvocation*) {
    [authentication_flow_ didFetchManagedStatus:nil];
  }] fetchManagedStatus:browser_state_.get() forIdentity:identity1_];

  [[[performer_ expect] andReturnBool:YES]
      shouldHandleMergeCaseForIdentity:identity1_
                     browserStatePrefs:browser_state_->GetPrefs()];

  [[[performer_ expect] andDo:^(NSInvocation*) {
    [authentication_flow_
        didChooseClearDataPolicy:SHOULD_CLEAR_DATA_CLEAR_DATA];
  }] promptMergeCaseForIdentity:identity1_
                        browser:browser_.get()
                 viewController:view_controller_];

  [[[performer_ expect] andDo:^(NSInvocation*) {
    [authentication_flow_ didSignOut];
  }] signOutBrowserState:browser_state_.get()];

  [[[performer_ expect] andDo:^(NSInvocation*) {
    [authentication_flow_ didClearData];
  }] clearDataFromBrowser:browser_.get() commandHandler:nil];

  SetSigninSuccessExpectations(
      identity1_, signin_metrics::AccessPoint::ACCESS_POINT_START_PAGE, nil);

  AuthenticationServiceFactory::GetForBrowserState(browser_state_.get())
      ->SignIn(identity2_,
               signin_metrics::AccessPoint::ACCESS_POINT_RESIGNIN_INFOBAR);
  [authentication_flow_ startSignInWithCompletion:sign_in_completion_];

  CheckSignInCompletion(/*expected_signed_in=*/true);
  histogram_tester_.ExpectUniqueSample(
      "Signin.AccountType.SigninConsent",
      signin_metrics::SigninAccountType::kRegular, 1);
  histogram_tester_.ExpectUniqueSample(
      "Signin.AccountType.SyncConsent",
      signin_metrics::SigninAccountType::kRegular, 1);
}

// Tests interrupting the Sign In flow. When AuthenticationFlow is interrupted
// with UIShutdownNoDismiss, the sign-in completion block needs to be called
// before -[AuthenticationFlow interruptWithAction:] ends.
TEST_F(AuthenticationFlowTest, TestInterruptWithoutDismiss) {
  CreateAuthenticationFlow(
      PostSignInAction::kCommitSync, identity1_,
      signin_metrics::AccessPoint::ACCESS_POINT_START_PAGE);

  [[[performer_ expect] andDo:^(NSInvocation*) {
    [authentication_flow_ didFetchManagedStatus:nil];
  }] fetchManagedStatus:browser_state_.get() forIdentity:identity1_];

  [[[performer_ expect] andReturnBool:YES]
      shouldHandleMergeCaseForIdentity:identity1_
                     browserStatePrefs:browser_state_->GetPrefs()];

  [[[performer_ expect] andDo:^(NSInvocation*) {
    run_loop_.Quit();
  }] promptMergeCaseForIdentity:identity1_
                        browser:browser_.get()
                 viewController:view_controller_];

  [[performer_ expect]
      interruptWithAction:SigninCoordinatorInterrupt::UIShutdownNoDismiss
               completion:[OCMArg
                              checkWithBlock:^BOOL(ProceduralBlock callback) {
                                callback();
                                return YES;
                              }]];

  [authentication_flow_ startSignInWithCompletion:sign_in_completion_];
  EXPECT_EQ(signin::Tribool::kUnknown, signin_result_);
  run_loop_.Run();
  EXPECT_EQ(signin::Tribool::kUnknown, signin_result_);
  [authentication_flow_
      interruptWithAction:SigninCoordinatorInterrupt::UIShutdownNoDismiss];
  // The sign-in completion needs to be called synchronously in the interrupt
  // method.
  EXPECT_EQ(signin::Tribool::kFalse, signin_result_);
  [performer_ verify];
  histogram_tester_.ExpectTotalCount("Signin.AccountType.SigninConsent", 0);
  histogram_tester_.ExpectTotalCount("Signin.AccountType.SyncConsent", 0);
}

// Tests the fetch managed status failure case.
TEST_F(AuthenticationFlowTest, TestFailFetchManagedStatus) {
  CreateAuthenticationFlow(
      PostSignInAction::kCommitSync, identity1_,
      signin_metrics::AccessPoint::ACCESS_POINT_START_PAGE);

  NSError* error = [NSError errorWithDomain:@"foo" code:0 userInfo:nil];
  [[[performer_ expect] andDo:^(NSInvocation*) {
    [authentication_flow_ didFailFetchManagedStatus:error];
  }] fetchManagedStatus:browser_state_.get()
             forIdentity:identity1_];

  [[[performer_ expect] andDo:^(NSInvocation* invocation) {
    __unsafe_unretained ProceduralBlock completionBlock;
    [invocation getArgument:&completionBlock atIndex:3];
    completionBlock();
  }] showAuthenticationError:[OCMArg any]
              withCompletion:[OCMArg any]
              viewController:view_controller_
                     browser:browser_.get()];

  [authentication_flow_ startSignInWithCompletion:sign_in_completion_];

  CheckSignInCompletion(/*expected_signed_in=*/false);
  histogram_tester_.ExpectTotalCount("Signin.AccountType.SigninConsent", 0);
  histogram_tester_.ExpectTotalCount("Signin.AccountType.SyncConsent", 0);
}

// Tests the managed sign in confirmation dialog is shown when signing in to
// a managed identity with sync consent level.
TEST_F(AuthenticationFlowTest, TestShowManagedConfirmationWithSyncConsent) {
  CreateAuthenticationFlow(
      PostSignInAction::kCommitSync, managed_identity_,
      signin_metrics::AccessPoint::ACCESS_POINT_SUPERVISED_USER);

  [[[performer_ expect] andDo:^(NSInvocation*) {
    [authentication_flow_ didFetchManagedStatus:@"foo.com"];
  }] fetchManagedStatus:browser_state_.get() forIdentity:managed_identity_];

  [[[performer_ expect] andReturnBool:NO]
      shouldHandleMergeCaseForIdentity:managed_identity_
                     browserStatePrefs:browser_state_->GetPrefs()];

  [[[performer_ expect] andDo:^(NSInvocation*) {
    [authentication_flow_ didAcceptManagedConfirmation];
  }] showManagedConfirmationForHostedDomain:@"foo.com"
                             viewController:view_controller_
                                    browser:browser_.get()
                                syncConsent:YES];

  SetSigninSuccessExpectations(
      managed_identity_,
      signin_metrics::AccessPoint::ACCESS_POINT_SUPERVISED_USER, @"foo.com");

  [authentication_flow_ startSignInWithCompletion:sign_in_completion_];

  CheckSignInCompletion(/*expected_signed_in=*/true);
  histogram_tester_.ExpectUniqueSample(
      "Signin.AccountType.SigninConsent",
      signin_metrics::SigninAccountType::kManaged, 1);
  histogram_tester_.ExpectUniqueSample(
      "Signin.AccountType.SyncConsent",
      signin_metrics::SigninAccountType::kManaged, 1);
}

// Tests that when signed in without sync with a managed account and the
// policy::kUserPolicyForSigninAndNoSyncConsentLevel feature is disabled, the
// managed account confirmation dialog isn't shown.
TEST_F(AuthenticationFlowTest,
       TestDontShowManagedConfirmationForSigninOnlyIfUserPolicyDisabled) {
  // Enable sign-in promos but leave user policy disabled.
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {syncer::kReplaceSyncPromosWithSignInPromos}, {});

  CreateAuthenticationFlow(
      PostSignInAction::kNone, managed_identity_,
      signin_metrics::AccessPoint::ACCESS_POINT_SUPERVISED_USER);

  [[[performer_ expect] andDo:^(NSInvocation*) {
    [authentication_flow_ didFetchManagedStatus:@"foo.com"];
  }] fetchManagedStatus:browser_state_.get() forIdentity:managed_identity_];

  SetSigninSuccessExpectations(
      managed_identity_,
      signin_metrics::AccessPoint::ACCESS_POINT_SUPERVISED_USER, @"foo.com");

  [authentication_flow_ startSignInWithCompletion:sign_in_completion_];

  CheckSignInCompletion(/*expected_signed_in=*/true);
  histogram_tester_.ExpectUniqueSample(
      "Signin.AccountType.SigninConsent",
      signin_metrics::SigninAccountType::kManaged, 1);
  histogram_tester_.ExpectTotalCount("Signin.AccountType.SyncConsent", 0);
}

// Tests that when signed in only with a managed account and the
// policy::kUserPolicyForSigninAndNoSyncConsentLevel feature is disabled, the
// managed account confirmation dialog isn't shown.
TEST_F(AuthenticationFlowTest,
       TestDontShowManagedConfirmationForSigninConsetLevelIfPromoDisabled) {
  // Enable user policy but leave sign-in promos disabled.
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {syncer::kReplaceSyncPromosWithSignInPromos}, {});

  CreateAuthenticationFlow(
      PostSignInAction::kNone, managed_identity_,
      signin_metrics::AccessPoint::ACCESS_POINT_SUPERVISED_USER);

  [[[performer_ expect] andDo:^(NSInvocation*) {
    [authentication_flow_ didFetchManagedStatus:@"foo.com"];
  }] fetchManagedStatus:browser_state_.get() forIdentity:managed_identity_];

  SetSigninSuccessExpectations(
      managed_identity_,
      signin_metrics::AccessPoint::ACCESS_POINT_SUPERVISED_USER, @"foo.com");

  [authentication_flow_ startSignInWithCompletion:sign_in_completion_];

  CheckSignInCompletion(/*expected_signed_in=*/true);
  histogram_tester_.ExpectUniqueSample(
      "Signin.AccountType.SigninConsent",
      signin_metrics::SigninAccountType::kManaged, 1);
  histogram_tester_.ExpectTotalCount("Signin.AccountType.SyncConsent", 0);
}

// Tests that when signed in only with a managed account and the
// needed features are enabled, the managed account confirmation dialog is
// shown.
TEST_F(AuthenticationFlowTest,
       TestShowManagedConfirmationForSigninConsentLevelIfAllFeaturesEnabled) {
  // Enable user policy and sign-in promos.
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {syncer::kReplaceSyncPromosWithSignInPromos,
       policy::kUserPolicyForSigninAndNoSyncConsentLevel},
      {});

  CreateAuthenticationFlow(
      PostSignInAction::kNone, managed_identity_,
      signin_metrics::AccessPoint::ACCESS_POINT_SUPERVISED_USER);

  [[[performer_ expect] andDo:^(NSInvocation*) {
    [authentication_flow_ didFetchManagedStatus:@"foo.com"];
  }] fetchManagedStatus:browser_state_.get() forIdentity:managed_identity_];
  [[[performer_ expect] andDo:^(NSInvocation*) {
    [authentication_flow_ didAcceptManagedConfirmation];
  }] showManagedConfirmationForHostedDomain:@"foo.com"
                             viewController:view_controller_
                                    browser:browser_.get()
                                syncConsent:NO];

  [[[performer_ expect] andDo:^(NSInvocation*) {
    [authentication_flow_ didRegisterForUserPolicyWithDMToken:kFakeDMToken
                                                     clientID:kFakeClientID];
  }] registerUserPolicy:browser_state_.get() forIdentity:managed_identity_];

  [[[performer_ expect] andDo:^(NSInvocation*) {
    [authentication_flow_ didFetchUserPolicyWithSuccess:YES];
  }] fetchUserPolicy:browser_state_.get()
         withDmToken:kFakeDMToken
            clientID:kFakeClientID
            identity:managed_identity_];

  SetSigninSuccessExpectations(
      managed_identity_,
      signin_metrics::AccessPoint::ACCESS_POINT_SUPERVISED_USER, @"foo.com");

  [authentication_flow_ startSignInWithCompletion:sign_in_completion_];

  CheckSignInCompletion(/*expected_signed_in=*/true);
  histogram_tester_.ExpectUniqueSample(
      "Signin.AccountType.SigninConsent",
      signin_metrics::SigninAccountType::kManaged, 1);
  histogram_tester_.ExpectTotalCount("Signin.AccountType.SyncConsent", 0);
}

// Tests sign-in only with a managed account, and then starts sync. The managed
// account confirmation dialog should be shown only in sync.
TEST_F(AuthenticationFlowTest, TestSyncAfterSigninAndSync) {
  CreateAuthenticationFlow(
      PostSignInAction::kCommitSync, managed_identity_,
      signin_metrics::AccessPoint::ACCESS_POINT_SUPERVISED_USER);

  [[[performer_ expect] andDo:^(NSInvocation*) {
    [authentication_flow_ didFetchManagedStatus:@"foo.com"];
  }] fetchManagedStatus:browser_state_.get() forIdentity:managed_identity_];

  [[[performer_ expect] andReturnBool:NO]
      shouldHandleMergeCaseForIdentity:managed_identity_
                     browserStatePrefs:browser_state_->GetPrefs()];

  SetSigninSuccessExpectations(
      managed_identity_,
      signin_metrics::AccessPoint::ACCESS_POINT_SUPERVISED_USER, @"foo.com");

  [[[performer_ expect] andDo:^(NSInvocation*) {
    [authentication_flow_ didAcceptManagedConfirmation];
  }] showManagedConfirmationForHostedDomain:@"foo.com"
                             viewController:view_controller_
                                    browser:browser_.get()
                                syncConsent:YES];

  [authentication_flow_ startSignInWithCompletion:sign_in_completion_];

  CheckSignInCompletion(/*expected_signed_in=*/true);
  histogram_tester_.ExpectUniqueSample(
      "Signin.AccountType.SigninConsent",
      signin_metrics::SigninAccountType::kManaged, 1);
  histogram_tester_.ExpectUniqueSample(
      "Signin.AccountType.SyncConsent",
      signin_metrics::SigninAccountType::kManaged, 1);
}

// Tests sign-in without sync flow with a managed account that is elible for
// user policy. A managed account is eligible for user policy if it has the
// corresponding user policy feature enabled.
TEST_F(AuthenticationFlowTest,
       TestUserPolicyForManagedAccountForSigninConsentLevelWhenEligible) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {policy::kUserPolicyForSigninAndNoSyncConsentLevel}, {});

  CreateAuthenticationFlow(
      PostSignInAction::kNone, managed_identity_,
      signin_metrics::AccessPoint::ACCESS_POINT_SUPERVISED_USER);

  [[[performer_ expect] andDo:^(NSInvocation*) {
    [authentication_flow_ didFetchManagedStatus:@"foo.com"];
  }] fetchManagedStatus:browser_state_.get() forIdentity:managed_identity_];

  SetSigninSuccessExpectations(
      managed_identity_,
      signin_metrics::AccessPoint::ACCESS_POINT_SUPERVISED_USER, @"foo.com");

  [[[performer_ expect] andDo:^(NSInvocation*) {
    [authentication_flow_ didRegisterForUserPolicyWithDMToken:kFakeDMToken
                                                     clientID:kFakeClientID];
  }] registerUserPolicy:browser_state_.get() forIdentity:managed_identity_];

  [[[performer_ expect] andDo:^(NSInvocation*) {
    [authentication_flow_ didFetchUserPolicyWithSuccess:YES];
  }] fetchUserPolicy:browser_state_.get()
         withDmToken:kFakeDMToken
            clientID:kFakeClientID
            identity:managed_identity_];

  [authentication_flow_ startSignInWithCompletion:sign_in_completion_];

  CheckSignInCompletion(/*expected_signed_in=*/true);
  histogram_tester_.ExpectUniqueSample(
      "Signin.AccountType.SigninConsent",
      signin_metrics::SigninAccountType::kManaged, 1);
  histogram_tester_.ExpectTotalCount("Signin.AccountType.SyncConsent", 0);
}

// Tests sign-in+sync flow with a managed account that is elible for user
// policy.  A managed account is eligible for user policy if it has the
// corresponding user policy feature enabled.
TEST_F(AuthenticationFlowTest,
       TestUserPolicyForManagedAccountForSigninOrSyncWhenEligible) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {policy::kUserPolicyForSigninOrSyncConsentLevel}, {});

  CreateAuthenticationFlow(
      PostSignInAction::kCommitSync, managed_identity_,
      signin_metrics::AccessPoint::ACCESS_POINT_SUPERVISED_USER);

  [[[performer_ expect] andDo:^(NSInvocation*) {
    [authentication_flow_ didFetchManagedStatus:@"foo.com"];
  }] fetchManagedStatus:browser_state_.get() forIdentity:managed_identity_];

  [[[performer_ expect] andReturnBool:NO]
      shouldHandleMergeCaseForIdentity:managed_identity_
                     browserStatePrefs:browser_state_->GetPrefs()];

  SetSigninSuccessExpectations(
      managed_identity_,
      signin_metrics::AccessPoint::ACCESS_POINT_SUPERVISED_USER, @"foo.com");

  [[[performer_ expect] andDo:^(NSInvocation*) {
    [authentication_flow_ didAcceptManagedConfirmation];
  }] showManagedConfirmationForHostedDomain:@"foo.com"
                             viewController:view_controller_
                                    browser:browser_.get()
                                syncConsent:YES];

  [[[performer_ expect] andDo:^(NSInvocation*) {
    [authentication_flow_ didRegisterForUserPolicyWithDMToken:kFakeDMToken
                                                     clientID:kFakeClientID];
  }] registerUserPolicy:browser_state_.get() forIdentity:managed_identity_];

  [[[performer_ expect] andDo:^(NSInvocation*) {
    [authentication_flow_ didFetchUserPolicyWithSuccess:YES];
  }] fetchUserPolicy:browser_state_.get()
         withDmToken:kFakeDMToken
            clientID:kFakeClientID
            identity:managed_identity_];

  [authentication_flow_ startSignInWithCompletion:sign_in_completion_];

  CheckSignInCompletion(/*expected_signed_in=*/true);
  histogram_tester_.ExpectUniqueSample(
      "Signin.AccountType.SigninConsent",
      signin_metrics::SigninAccountType::kManaged, 1);
  histogram_tester_.ExpectUniqueSample(
      "Signin.AccountType.SyncConsent",
      signin_metrics::SigninAccountType::kManaged, 1);
}

// Tests that the user policy fetch is skipped when registration failed and
// provided an empty dmtoken. The user should still be able to sign-in and
// sync.
TEST_F(AuthenticationFlowTest,
       TestSkipFetchUserPolicyWithManagedAccountWhenRegistrationFailed) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {policy::kUserPolicyForSigninOrSyncConsentLevel}, {});

  CreateAuthenticationFlow(
      PostSignInAction::kCommitSync, managed_identity_,
      signin_metrics::AccessPoint::ACCESS_POINT_SUPERVISED_USER);

  [[[performer_ expect] andDo:^(NSInvocation*) {
    [authentication_flow_ didFetchManagedStatus:@"foo.com"];
  }] fetchManagedStatus:browser_state_.get() forIdentity:managed_identity_];

  [[[performer_ expect] andReturnBool:NO]
      shouldHandleMergeCaseForIdentity:managed_identity_
                     browserStatePrefs:browser_state_->GetPrefs()];

  SetSigninSuccessExpectations(
      managed_identity_,
      signin_metrics::AccessPoint::ACCESS_POINT_SUPERVISED_USER, @"foo.com");

  [[[performer_ expect] andDo:^(NSInvocation*) {
    [authentication_flow_ didAcceptManagedConfirmation];
  }] showManagedConfirmationForHostedDomain:@"foo.com"
                             viewController:view_controller_
                                    browser:browser_.get()
                                syncConsent:YES];

  [[[performer_ expect] andDo:^(NSInvocation*) {
    [authentication_flow_ didRegisterForUserPolicyWithDMToken:@""
                                                     clientID:kFakeClientID];
  }] registerUserPolicy:browser_state_.get() forIdentity:managed_identity_];

  [[performer_ reject] fetchUserPolicy:browser_state_.get()
                           withDmToken:@""
                              clientID:kFakeClientID
                              identity:managed_identity_];

  [authentication_flow_ startSignInWithCompletion:sign_in_completion_];

  CheckSignInCompletion(/*expected_signed_in=*/true);
  histogram_tester_.ExpectUniqueSample(
      "Signin.AccountType.SigninConsent",
      signin_metrics::SigninAccountType::kManaged, 1);
  histogram_tester_.ExpectUniqueSample(
      "Signin.AccountType.SyncConsent",
      signin_metrics::SigninAccountType::kManaged, 1);
}

// Tests that the user policy fetch can fail without interrupting the
// authentication flow. The user should sill be able to sign-in and
// sync.
TEST_F(AuthenticationFlowTest, TestCanSyncWithUserPolicyFetchFailure) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {policy::kUserPolicyForSigninOrSyncConsentLevel}, {});

  CreateAuthenticationFlow(
      PostSignInAction::kCommitSync, managed_identity_,
      signin_metrics::AccessPoint::ACCESS_POINT_SUPERVISED_USER);

  [[[performer_ expect] andDo:^(NSInvocation*) {
    [authentication_flow_ didFetchManagedStatus:@"foo.com"];
  }] fetchManagedStatus:browser_state_.get() forIdentity:managed_identity_];

  [[[performer_ expect] andReturnBool:NO]
      shouldHandleMergeCaseForIdentity:managed_identity_
                     browserStatePrefs:browser_state_->GetPrefs()];

  SetSigninSuccessExpectations(
      managed_identity_,
      signin_metrics::AccessPoint::ACCESS_POINT_SUPERVISED_USER, @"foo.com");

  [[[performer_ expect] andDo:^(NSInvocation*) {
    [authentication_flow_ didAcceptManagedConfirmation];
  }] showManagedConfirmationForHostedDomain:@"foo.com"
                             viewController:view_controller_
                                    browser:browser_.get()
                                syncConsent:YES];

  [[[performer_ expect] andDo:^(NSInvocation*) {
    [authentication_flow_ didRegisterForUserPolicyWithDMToken:kFakeDMToken
                                                     clientID:kFakeClientID];
  }] registerUserPolicy:browser_state_.get() forIdentity:managed_identity_];

  [[[performer_ expect] andDo:^(NSInvocation*) {
    [authentication_flow_ didFetchUserPolicyWithSuccess:NO];
  }] fetchUserPolicy:browser_state_.get()
         withDmToken:kFakeDMToken
            clientID:kFakeClientID
            identity:managed_identity_];

  [authentication_flow_ startSignInWithCompletion:sign_in_completion_];

  CheckSignInCompletion(/*expected_signed_in=*/true);
  histogram_tester_.ExpectUniqueSample(
      "Signin.AccountType.SigninConsent",
      signin_metrics::SigninAccountType::kManaged, 1);
  histogram_tester_.ExpectUniqueSample(
      "Signin.AccountType.SyncConsent",
      signin_metrics::SigninAccountType::kManaged, 1);
}

}  // namespace
