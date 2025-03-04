// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/authentication/ui_bundled/authentication_flow/authentication_flow.h"

#import <memory>

#import "base/files/scoped_temp_dir.h"
#import "base/functional/bind.h"
#import "base/memory/ptr_util.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "base/test/metrics/histogram_tester.h"
#import "base/test/scoped_feature_list.h"
#import "base/values.h"
#import "components/policy/core/common/mock_configuration_policy_provider.h"
#import "components/policy/core/common/policy_loader_ios_constants.h"
#import "components/policy/core/common/policy_map.h"
#import "components/policy/core/common/policy_types.h"
#import "components/pref_registry/pref_registry_syncable.h"
#import "components/signin/public/base/signin_metrics.h"
#import "components/signin/public/identity_manager/tribool.h"
#import "components/sync_preferences/pref_service_mock_factory.h"
#import "components/sync_preferences/pref_service_syncable.h"
#import "ios/chrome/browser/authentication/ui_bundled/authentication_flow/authentication_flow_performer.h"
#import "ios/chrome/browser/authentication/ui_bundled/authentication_ui_util.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_feature.h"
#import "ios/chrome/browser/policy/model/cloud/user_policy_constants.h"
#import "ios/chrome/browser/policy/model/enterprise_policy_test_helper.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/prefs/browser_prefs.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service_factory.h"
#import "ios/chrome/browser/signin/model/fake_authentication_service_delegate.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/browser/signin/model/fake_system_identity_manager.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
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
NSString* const kFakeUserAffiliationID = @"fake_user_affiliation_id";

class AuthenticationFlowTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();

    TestProfileIOS::Builder builder;
    builder.AddTestingFactory(
        AuthenticationServiceFactory::GetInstance(),
        AuthenticationServiceFactory::GetFactoryWithDelegate(
            std::make_unique<FakeAuthenticationServiceDelegate>()));
    builder.SetPrefService(CreatePrefService());
    profile_ = std::move(builder).Build();
    browser_ = std::make_unique<TestBrowser>(profile_.get());

    identity1_ = [FakeSystemIdentity fakeIdentity1];
    fake_system_identity_manager()->AddIdentity(identity1_);
    identity2_ = [FakeSystemIdentity fakeIdentity2];
    fake_system_identity_manager()->AddIdentity(identity2_);
    managed_identity1_ = [FakeSystemIdentity fakeManagedIdentity];
    fake_system_identity_manager()->AddIdentity(managed_identity1_);
    managed_identity2_ = [FakeSystemIdentity identityWithEmail:@"bar@foo.com"];
    fake_system_identity_manager()->AddIdentity(managed_identity2_);

    run_loop_ = std::make_unique<base::RunLoop>();
    sign_in_completion_ = ^(SigninCoordinatorResult result) {
      run_loop_->Quit();
      switch (result) {
        case SigninCoordinatorResult::SigninCoordinatorResultSuccess:
          signin_result_ = signin::Tribool::kTrue;
          break;
        case SigninCoordinatorResult::SigninCoordinatorResultInterrupted:
        case SigninCoordinatorResult::SigninCoordinatorResultCanceledByUser:
        case SigninCoordinatorResult::SigninCoordinatorResultDisabled:
        case SigninCoordinatorResult::SigninCoordinatorUINotAvailable:
          signin_result_ = signin::Tribool::kFalse;
          break;
      }
    };
  }

  void TearDown() override {
    PlatformTest::TearDown();
    EXPECT_OCMOCK_VERIFY((id)view_controller_mock_);
    EXPECT_OCMOCK_VERIFY((id)performer_mock_);
  }

  std::unique_ptr<sync_preferences::PrefServiceSyncable> CreatePrefService() {
    sync_preferences::PrefServiceMockFactory factory;
    scoped_refptr<user_prefs::PrefRegistrySyncable> registry(
        new user_prefs::PrefRegistrySyncable);
    std::unique_ptr<sync_preferences::PrefServiceSyncable> prefs =
        factory.CreateSyncable(registry.get());
    RegisterProfilePrefs(registry.get());
    return prefs;
  }

  // Creates a new AuthenticationFlow with default values for fields that are
  // not directly useful.
  void CreateAuthenticationFlow(PostSignInActionSet postSignInActions,
                                id<SystemIdentity> identity,
                                signin_metrics::AccessPoint accessPoint) {
    view_controller_mock_ = OCMClassMock([UIViewController class]);
    authentication_flow_ =
        [[AuthenticationFlow alloc] initWithBrowser:browser_.get()
                                           identity:identity
                                        accessPoint:accessPoint
                                  postSignInActions:postSignInActions
                           presentingViewController:view_controller_mock_
                                         anchorView:nil
                                         anchorRect:CGRectNull];
    performer_mock_ = OCMStrictClassMock([AuthenticationFlowPerformer class]);
    [authentication_flow_ setPerformerForTesting:performer_mock_];
  }

  // Checks if the AuthenticationFlow operation has completed, and whether it
  // was successful.
  void CheckSignInCompletion(bool expected_signed_in) {
    run_loop_->Run();

    const signin::Tribool expected_signin_result =
        expected_signed_in ? signin::Tribool::kTrue : signin::Tribool::kFalse;

    EXPECT_EQ(expected_signin_result, signin_result_);
  }

  void SetSigninSuccessExpectations(id<SystemIdentity> identity,
                                    signin_metrics::AccessPoint accessPoint,
                                    NSString* hosted_domain) {
    OCMExpect([performer_mock_ signInIdentity:identity
                                atAccessPoint:accessPoint
                               currentProfile:profile_.get()]);
  }

  // Signs in successfully as `identity`, and checks that all the intermediary
  // steps run.
  void SignIn(id<SystemIdentity> identity,
              signin_metrics::AccessPoint access_point) {
    // Get the hosted domain from the email.
    NSString* user_email = identity.userEmail;
    NSArray* matches =
        [[NSRegularExpression regularExpressionWithPattern:@"^\\w+@([a-z.]+)$"
                                                   options:0
                                                     error:nil]
            matchesInString:user_email
                    options:0
                      range:NSMakeRange(0, user_email.length)];
    ASSERT_EQ(1u, matches.count);
    NSString* domain =
        [user_email substringWithRange:[matches[0] rangeAtIndex:1]];
    NSString* hosted_domain =
        [domain isEqualToString:@"gmail.com"] ? nil : domain;

    signin_result_ = signin::Tribool::kUnknown;
    // Can't use a RunLoop multiple times, create a new one.
    run_loop_ = std::make_unique<base::RunLoop>();

    CreateAuthenticationFlow(PostSignInActionSet(), identity, access_point);

    OCMExpect([performer_mock_ fetchManagedStatus:profile_.get()
                                      forIdentity:identity])
        .andDo(^(NSInvocation*) {
          [authentication_flow_ didFetchManagedStatus:hosted_domain];
        });

    if (hosted_domain.length) {
      OCMStub([performer_mock_
                  showManagedConfirmationForHostedDomain:hosted_domain
                                               userEmail:user_email
                                          viewController:view_controller_mock_
                                                 browser:browser_.get()
                               skipBrowsingDataMigration:NO
                              mergeBrowsingDataByDefault:NO
                   browsingDataMigrationDisabledByPolicy:NO])
          .andDo(^(NSInvocation*) {
            managed_confirmation_dialog_shown_count_++;
            [authentication_flow_ didAcceptManagedConfirmation:YES];
          });

      OCMExpect([performer_mock_ registerUserPolicy:profile_.get()
                                        forIdentity:identity])
          .andDo(^(NSInvocation*) {
            [authentication_flow_
                didRegisterForUserPolicyWithDMToken:kFakeDMToken
                                           clientID:kFakeClientID
                                 userAffiliationIDs:@[
                                   kFakeUserAffiliationID
                                 ]];
          });

      OCMExpect([performer_mock_ fetchUserPolicy:profile_.get()
                                     withDmToken:kFakeDMToken
                                        clientID:kFakeClientID
                              userAffiliationIDs:@[ kFakeUserAffiliationID ]
                                        identity:identity])
          .andDo(^(NSInvocation*) {
            [authentication_flow_ didFetchUserPolicyWithSuccess:YES];
          });
    }

    SetSigninSuccessExpectations(identity, access_point, hosted_domain);

    [authentication_flow_ startSignInWithCompletion:sign_in_completion_];
    // completion block should not be called synchronously.
    EXPECT_EQ(signin::Tribool::kUnknown, signin_result_);

    OCMExpect([performer_mock_ completePostSignInActions:PostSignInActionSet()
                                            withIdentity:identity
                                                 browser:browser_.get()]);
    CheckSignInCompletion(/*expected_signed_in=*/true);
  }

  void SignOut() {
    AuthenticationService* authentication_service =
        AuthenticationServiceFactory::GetForProfile(profile_.get());
    // Can't use a RunLoop multiple times, create a new one.
    run_loop_ = std::make_unique<base::RunLoop>();
    authentication_service->SignOut(
        signin_metrics::ProfileSignout::kSignoutForAccountSwitching,
        base::CallbackToBlock(run_loop_->QuitClosure()));
    run_loop_->Run();
  }

  FakeSystemIdentityManager* fake_system_identity_manager() {
    return FakeSystemIdentityManager::FromSystemIdentityManager(
        GetApplicationContext()->GetSystemIdentityManager());
  }

  web::WebTaskEnvironment task_environment_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  AuthenticationFlow* authentication_flow_ = nullptr;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<Browser> browser_;
  id<SystemIdentity> identity1_ = nil;
  id<SystemIdentity> identity2_ = nil;
  id<SystemIdentity> managed_identity1_ = nil;
  id<SystemIdentity> managed_identity2_ = nil;
  AuthenticationFlowPerformer* performer_mock_ = nil;
  signin_ui::SigninCompletionCallback sign_in_completion_;
  UIViewController* view_controller_mock_;
  // Used to verify histogram logging.
  base::HistogramTester histogram_tester_;

  // Number of times the management confirmation dialog has been displayed.
  int managed_confirmation_dialog_shown_count_ = 0;

  // Used to wait for sign-in workflow to complete.
  std::unique_ptr<base::RunLoop> run_loop_;
  signin::Tribool signin_result_ = signin::Tribool::kUnknown;
};

// Tests a Sign In of a normal account on the same profile.
TEST_F(AuthenticationFlowTest, TestSignInSimple) {
  base::test::ScopedFeatureList scoped_feature_list;
  // Enable user policy to make sure that the authentication flow doesn't try
  // a registration when the account isn't managed.
  scoped_feature_list.InitAndEnableFeature(
      policy::kUserPolicyForSigninOrSyncConsentLevel);

  SignIn(identity1_, signin_metrics::AccessPoint::kStartPage);

  histogram_tester_.ExpectUniqueSample(
      "Signin.AccountType.SigninConsent",
      signin_metrics::SigninAccountType::kRegular, 1);
}

// Tests the fetch managed status failure case.
TEST_F(AuthenticationFlowTest, TestFailFetchManagedStatus) {
  CreateAuthenticationFlow(PostSignInActionSet(), identity1_,
                           signin_metrics::AccessPoint::kStartPage);

  NSError* error = [NSError errorWithDomain:@"foo" code:0 userInfo:nil];
  OCMExpect([performer_mock_ fetchManagedStatus:profile_.get()
                                    forIdentity:identity1_])
      .andDo(^(NSInvocation*) {
        [authentication_flow_ didFailFetchManagedStatus:error];
      });

  OCMExpect([performer_mock_ showAuthenticationError:[OCMArg any]
                                      withCompletion:[OCMArg any]
                                      viewController:view_controller_mock_
                                             browser:browser_.get()])
      .andDo(^(NSInvocation* invocation) {
        __unsafe_unretained ProceduralBlock completionBlock;
        [invocation getArgument:&completionBlock atIndex:3];
        completionBlock();
      });

  [authentication_flow_ startSignInWithCompletion:sign_in_completion_];

  CheckSignInCompletion(/*expected_signed_in=*/false);
  histogram_tester_.ExpectTotalCount("Signin.AccountType.SigninConsent", 0);
}

// Tests that when signed in only with a managed account and the
// needed features are enabled, the managed account confirmation dialog is
// shown.
TEST_F(AuthenticationFlowTest,
       TestShowManagedConfirmationForSigninConsentLevelIfAllFeaturesEnabled) {
  // Enable user policy and sign-in promos.
  base::test::ScopedFeatureList scoped_feature_list(
      policy::kUserPolicyForSigninAndNoSyncConsentLevel);

  SignIn(managed_identity1_, signin_metrics::AccessPoint::kSupervisedUser);
  histogram_tester_.ExpectUniqueSample(
      "Signin.AccountType.SigninConsent",
      signin_metrics::SigninAccountType::kManaged, 1);
  EXPECT_EQ(1, managed_confirmation_dialog_shown_count_);
}

// Tests that the management confirmation dialog is not shown and the user
// policies still fetched when the browser is already managed at the machine
// level. This only applies to the sign-in consent level.
TEST_F(AuthenticationFlowTest,
       TestSkipManagedConfirmationWhenAlreadyManagedAtMachineLevel) {
  // Enable user policy and sign-in consent only.
  base::test::ScopedFeatureList scoped_feature_list(
      policy::kUserPolicyForSigninAndNoSyncConsentLevel);

  // Set a machine level policy.
  base::ScopedTempDir state_directory;
  ASSERT_TRUE(state_directory.CreateUniqueTempDir());
  EnterprisePolicyTestHelper enterprise_policy_helper(
      state_directory.GetPath());
  policy::PolicyMap map;
  map.Set("test-policy", policy::POLICY_LEVEL_MANDATORY,
          policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_PLATFORM,
          base::Value("hello"), nullptr);
  enterprise_policy_helper.GetPolicyProvider()->UpdateChromePolicy(map);

  SignIn(managed_identity1_, signin_metrics::AccessPoint::kSupervisedUser);
  histogram_tester_.ExpectUniqueSample(
      "Signin.AccountType.SigninConsent",
      signin_metrics::SigninAccountType::kManaged, 1);
  EXPECT_EQ(0, managed_confirmation_dialog_shown_count_);
}

// Tests that the managed confirmation dialog is only show once per account,
// when signing in from the Account Menu.
TEST_F(AuthenticationFlowTest, TestShowManagedConfirmationOnlyOnce) {
  // Enable user policy and sign-in promos.
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {policy::kUserPolicyForSigninAndNoSyncConsentLevel,
       kIdentityDiscAccountMenu},
      {});

  // First signin, show the dialog.
  SignIn(managed_identity1_, signin_metrics::AccessPoint::kAccountMenu);
  EXPECT_EQ(1, managed_confirmation_dialog_shown_count_);

  // Second signin from the account menu, don't show the dialog.
  SignOut();
  SignIn(managed_identity1_, signin_metrics::AccessPoint::kAccountMenu);
  EXPECT_EQ(1, managed_confirmation_dialog_shown_count_);

  // Signin from a different UI surface, show the dialog again.
  SignOut();
  SignIn(managed_identity1_, signin_metrics::AccessPoint::kSupervisedUser);
  EXPECT_EQ(1, managed_confirmation_dialog_shown_count_);

  // Signin with a different account, show the dialog again.
  SignOut();
  SignIn(managed_identity2_, signin_metrics::AccessPoint::kAccountMenu);
  EXPECT_EQ(2, managed_confirmation_dialog_shown_count_);
}

TEST_F(AuthenticationFlowTest, TestDontShowUnsyncedDataConfirmation) {
  // Another account is already signed in.
  AuthenticationServiceFactory::GetForProfile(profile_.get())
      ->SignIn(identity1_, signin_metrics::AccessPoint::kStartPage);

  // Without signing out first, start signing in with a different identity. This
  // should trigger the check for unsynced data.
  CreateAuthenticationFlow(PostSignInActionSet(), identity2_,
                           signin_metrics::AccessPoint::kStartPage);

  syncer::SyncService* sync_service =
      SyncServiceFactory::GetForProfile(profile_.get());
  OCMExpect([performer_mock_ fetchUnsyncedDataWithSyncService:sync_service])
      .andDo(^(NSInvocation*) {
        [authentication_flow_
            didFetchUnsyncedDataWithUnsyncedDataTypes:syncer::DataTypeSet()];
      });
  // There is no unsynced data in this case, so no confirmation should be
  // shown - the next step is fetching the managed status.
  // Don't bother continuing the flow beyond that step for this test.
  OCMExpect([performer_mock_ fetchManagedStatus:profile_.get()
                                    forIdentity:identity2_])
      .andDo(^(NSInvocation*) {
        run_loop_->Quit();
      });

  [authentication_flow_ startSignInWithCompletion:sign_in_completion_];
  run_loop_->Run();
}

TEST_F(AuthenticationFlowTest, TestShowUnsyncedDataConfirmation) {
  // Another account is already signed in.
  AuthenticationServiceFactory::GetForProfile(profile_.get())
      ->SignIn(identity1_, signin_metrics::AccessPoint::kStartPage);

  // Without signing out first, start signing in with a different identity. This
  // should trigger the check for unsynced data.
  CreateAuthenticationFlow(PostSignInActionSet(), identity2_,
                           signin_metrics::AccessPoint::kStartPage);

  syncer::SyncService* sync_service =
      SyncServiceFactory::GetForProfile(profile_.get());
  OCMExpect([performer_mock_ fetchUnsyncedDataWithSyncService:sync_service])
      .andDo(^(NSInvocation*) {
        [authentication_flow_ didFetchUnsyncedDataWithUnsyncedDataTypes:
                                  {syncer::DataType::BOOKMARKS}];
      });
  // There is unsynced data, so a confirmation should be shown.
  // Don't bother continuing the flow beyond that step for this test.
  OCMExpect(
      [performer_mock_
          showLeavingPrimaryAccountConfirmationWithBaseViewController:[OCMArg
                                                                          any]
                                                              browser:browser_
                                                                          .get()
                                                    signedInUserState:
                                                        SignedInUserState::
                                                            kNotSyncingAndReplaceSyncWithSignin
                                                           anchorView:[OCMArg
                                                                          any]
                                                           anchorRect:CGRect()])
      .ignoringNonObjectArgs()  // Don't care about the CGRect values.
      .andDo(^(NSInvocation*) {
        run_loop_->Quit();
      });

  [authentication_flow_ startSignInWithCompletion:sign_in_completion_];
  run_loop_->Run();
}

}  // namespace
