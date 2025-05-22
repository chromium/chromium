// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/authentication/ui_bundled/authentication_flow/authentication_flow.h"

#import <memory>

#import "base/files/scoped_temp_dir.h"
#import "base/functional/bind.h"
#import "base/functional/callback_helpers.h"
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
#import "components/sync/test/test_sync_service.h"
#import "components/sync_preferences/pref_service_mock_factory.h"
#import "components/sync_preferences/pref_service_syncable.h"
#import "ios/chrome/app/change_profile_commands.h"
#import "ios/chrome/app/change_profile_continuation.h"
#import "ios/chrome/browser/authentication/ui_bundled/authentication_flow/authentication_flow_delegate.h"
#import "ios/chrome/browser/authentication/ui_bundled/authentication_flow/authentication_flow_in_profile.h"
#import "ios/chrome/browser/authentication/ui_bundled/authentication_flow/authentication_flow_in_profile_performer.h"
#import "ios/chrome/browser/authentication/ui_bundled/authentication_flow/authentication_flow_in_profile_performer_delegate.h"
#import "ios/chrome/browser/authentication/ui_bundled/authentication_flow/authentication_flow_performer.h"
#import "ios/chrome/browser/authentication/ui_bundled/authentication_flow/authentication_flow_performer_delegate.h"
#import "ios/chrome/browser/authentication/ui_bundled/authentication_flow/test_authentication_flow_delegate.h"
#import "ios/chrome/browser/authentication/ui_bundled/authentication_test_util.h"
#import "ios/chrome/browser/authentication/ui_bundled/authentication_ui_util.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_feature.h"
#import "ios/chrome/browser/policy/model/cloud/user_policy_constants.h"
#import "ios/chrome/browser/policy/model/enterprise_policy_test_helper.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/prefs/browser_prefs.h"
#import "ios/chrome/browser/shared/model/profile/features.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_manager_ios.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/signin/model/account_profile_mapper.h"
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

// The parameter determines whether `kSeparateProfilesForManagedAccounts` is
// enabled.
class AuthenticationFlowTest : public PlatformTest,
                               public testing::WithParamInterface<bool> {
 protected:
  AuthenticationFlowTest() {
    features_.InitWithFeatureState(kSeparateProfilesForManagedAccounts,
                                   GetParam());
  }

  void SetUp() override {
    PlatformTest::SetUp();

    personal_profile_ = CreateProfile();
    personal_browser_ = std::make_unique<TestBrowser>(personal_profile_.get());

    identity1_ = [FakeSystemIdentity fakeIdentity1];
    fake_system_identity_manager()->AddIdentity(identity1_);
    identity2_ = [FakeSystemIdentity fakeIdentity2];
    fake_system_identity_manager()->AddIdentity(identity2_);
    managed_identity1_ = [FakeSystemIdentity fakeManagedIdentity];
    fake_system_identity_manager()->AddIdentity(managed_identity1_);
    managed_identity2_ = [FakeSystemIdentity identityWithEmail:@"bar@foo.com"];
    fake_system_identity_manager()->AddIdentity(managed_identity2_);

    // Force explicit instantiation of the AuthenticationService, to ensure
    // accounts get synced over to IdentityManager.
    std::ignore =
        AuthenticationServiceFactory::GetForProfile(personal_profile_.get());

    if (AreSeparateProfilesForManagedAccountsEnabled()) {
      managed_profile1_ = CreateProfile(
          *GetApplicationContext()
               ->GetAccountProfileMapper()
               ->FindProfileNameForGaiaID(GaiaId(managed_identity1_.gaiaID)));
      managed_browser1_ =
          std::make_unique<TestBrowser>(managed_profile1_.get());
      managed_profile2_ = CreateProfile(
          *GetApplicationContext()
               ->GetAccountProfileMapper()
               ->FindProfileNameForGaiaID(GaiaId(managed_identity2_.gaiaID)));
      managed_browser2_ =
          std::make_unique<TestBrowser>(managed_profile2_.get());
    }

    run_loop_ = std::make_unique<base::RunLoop>();
  }

  void TearDown() override {
    PlatformTest::TearDown();
    EXPECT_OCMOCK_VERIFY((id)view_controller_mock_);
    EXPECT_OCMOCK_VERIFY((id)in_profile_performer_mock_);
    EXPECT_OCMOCK_VERIFY((id)performer_mock_);
  }

  TestProfileIOS* CreateProfile(
      std::optional<std::string> name = std::nullopt) {
    TestProfileIOS::Builder builder;
    if (name.has_value()) {
      builder.SetName(*name);
    }
    builder.AddTestingFactory(
        AuthenticationServiceFactory::GetInstance(),
        AuthenticationServiceFactory::GetFactoryWithDelegate(
            std::make_unique<FakeAuthenticationServiceDelegate>()));
    builder.AddTestingFactory(
        SyncServiceFactory::GetInstance(),
        base::BindRepeating(
            [](web::BrowserState* context) -> std::unique_ptr<KeyedService> {
              return std::make_unique<syncer::TestSyncService>();
            }));
    builder.SetPrefService(CreatePrefService());
    return profile_manager_.AddProfileWithBuilder(std::move(builder));
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
                                signin_metrics::AccessPoint accessPoint,
                                BOOL shouldHandOverToFlowInProfile) {
    view_controller_mock_ = OCMClassMock([UIViewController class]);
    CHECK(!authentication_flow_);
    authentication_flow_ =
        [[AuthenticationFlow alloc] initWithBrowser:personal_browser_.get()
                                           identity:identity
                                        accessPoint:accessPoint
                               precedingHistorySync:NO
                                  postSignInActions:postSignInActions
                           presentingViewController:view_controller_mock_
                                         anchorView:nil
                                         anchorRect:CGRectNull];
    in_profile_performer_mock_ =
        OCMStrictClassMock([AuthenticationFlowInProfilePerformer class]);
    performer_mock_ = OCMStrictClassMock([AuthenticationFlowPerformer class]);

    // Once AuthenticationFlow is started, it'll create its performer. Replace
    // it with a mock.
    OCMExpect([(id)performer_mock_ alloc]).andReturn(performer_mock_);
    OCMExpect([performer_mock_ initWithDelegate:[OCMArg any]
                           changeProfileHandler:[OCMArg any]])
        .andReturn(performer_mock_);

    if (shouldHandOverToFlowInProfile) {
      // Once the flow progresses into AuthenticationFlowInProfile, that class
      // creates its own performer. For simplicity, reuse the same mock object
      // here. Also capture a reference to the AuthenticationFlowInProfile, so
      // the mock can call back into it.
      OCMExpect([(id)in_profile_performer_mock_ alloc])
          .andReturn(in_profile_performer_mock_);
      OCMExpect([in_profile_performer_mock_
                    initWithInProfileDelegate:[OCMArg any]
                         changeProfileHandler:[OCMArg any]])
          .andDo(^(NSInvocation* invocation) {
            __unsafe_unretained id argument;
            [invocation getArgument:&argument atIndex:2];
            authentication_flow_in_profile_ = argument;
          })
          .andReturn(in_profile_performer_mock_);
    }

    signin_ui::SigninCompletionCallback sign_in_completion =
        ^(SigninCoordinatorResult result) {
          run_loop_->Quit();
          switch (result) {
            case SigninCoordinatorResult::SigninCoordinatorResultSuccess:
              signin_result_ = signin::Tribool::kTrue;
              break;
            case SigninCoordinatorResult::SigninCoordinatorResultInterrupted:
            case SigninCoordinatorResult::SigninCoordinatorResultCanceledByUser:
            case SigninCoordinatorResult::SigninCoordinatorResultDisabled:
            case SigninCoordinatorResult::SigninCoordinatorUINotAvailable:
            case SigninCoordinatorResult::SigninCoordinatorProfileSwitch:
              signin_result_ = signin::Tribool::kFalse;
              break;
          }
          authentication_flow_ = nil;
        };
    // Runs the sign_in_completion with Success and the closure.
    ChangeProfileContinuationProvider continuation_provider =
        base::BindRepeating(
            [](signin_ui::SigninCompletionCallback sign_in_completion) {
              ChangeProfileContinuation continuation = base::BindOnce(
                  [](signin_ui::SigninCompletionCallback sign_in_completion,
                     SceneState* sceneState, base::OnceClosure closure) {
                    sign_in_completion(SigninCoordinatorResult::
                                           SigninCoordinatorResultSuccess);
                    std::move(closure).Run();
                  },
                  sign_in_completion);
              return continuation;
            },
            sign_in_completion);

    // Each mock expect its methods to be called at most once.
    test_authentication_flow_delegate_ = [[TestAuthenticationFlowDelegate alloc]
         initWithSigninCompletionCallback:sign_in_completion
        changeProfileContinuationProvider:continuation_provider];
    authentication_flow_.delegate = test_authentication_flow_delegate_;
  }

  // Checks if the AuthenticationFlow operation has completed, and whether it
  // was successful.
  void CheckSignInCompletion(bool expected_signed_in) {
    run_loop_->Run();

    const signin::Tribool expected_signin_result =
        expected_signed_in ? signin::Tribool::kTrue : signin::Tribool::kFalse;

    EXPECT_EQ(expected_signin_result, signin_result_);
  }

  // Returns the hosted domain from `email`, or nil if this email address
  // doesn't belong to a hosted domain.
  NSString* GetHostedDomainFromEmail(NSString* email) const {
    NSArray* matches =
        [[NSRegularExpression regularExpressionWithPattern:@"^\\w+@([a-z.]+)$"
                                                   options:0
                                                     error:nil]
            matchesInString:email
                    options:0
                      range:NSMakeRange(0, email.length)];
    CHECK_EQ(1u, matches.count);
    NSString* domain = [email substringWithRange:[matches[0] rangeAtIndex:1]];
    return [domain isEqualToString:@"gmail.com"] ? nil : domain;
  }

  // Signs in successfully as `identity`, and checks that all the intermediary
  // steps run. This always starts the signin flow in the personal profile, but
  // may involve a "switch" to a different profile (in these tests, no actual
  // "profile switch" happens, but the second part of the flow may happen in a
  // different profile).
  void SignIn(id<SystemIdentity> identity,
              signin_metrics::AccessPoint access_point,
              bool adds_history_screen_post_profile_switch = true) {
    signin_result_ = signin::Tribool::kUnknown;

    // Can't use a RunLoop multiple times, create a new one.
    run_loop_ = std::make_unique<base::RunLoop>();

    CreateAuthenticationFlow(PostSignInActionSet(), identity, access_point,
                             /*shouldHandOverToFlowInProfile=*/YES);

    NSString* hosted_domain = GetHostedDomainFromEmail(identity.userEmail);
    const bool should_switch_profile =
        hosted_domain.length && AreSeparateProfilesForManagedAccountsEnabled();

    PostSignInActionSet postSignInActions;
    if (should_switch_profile && adds_history_screen_post_profile_switch) {
      postSignInActions.Put(
          PostSignInAction::kShowHistorySyncScreenAfterProfileSwitch);
    }
    auto fetchManagedStatusCallback = ^(NSInvocation*) {
      [authentication_flow_ didFetchManagedStatus:hosted_domain];
    };
    OCMExpect([performer_mock_ fetchManagedStatus:personal_profile_.get()
                                      forIdentity:identity])
        .andDo(fetchManagedStatusCallback);

    ProfileIOS* final_profile = personal_profile_;
    Browser* final_browser = personal_browser_.get();
    if (AreSeparateProfilesForManagedAccountsEnabled()) {
      if (identity == managed_identity1_) {
        final_profile = managed_profile1_;
        final_browser = managed_browser1_.get();
      } else if (identity == managed_identity2_) {
        final_profile = managed_profile2_;
        final_browser = managed_browser2_.get();
      }
    }

    if (hosted_domain.length) {
      if (AreSeparateProfilesForManagedAccountsEnabled()) {
        OCMStub([performer_mock_
                    fetchProfileSeparationPolicies:personal_profile_.get()
                                       forIdentity:identity])
            .andDo(^(NSInvocation*) {
              [authentication_flow_
                  didFetchProfileSeparationPolicies:policy::ALWAYS_SEPARATE];
            });
      }

      BOOL migrationDisabled = AreSeparateProfilesForManagedAccountsEnabled();
      auto showManagedConfirmationForHostedDomainCallback = ^(NSInvocation*) {
        managed_confirmation_dialog_shown_count_++;
        [authentication_flow_
            didAcceptManagedConfirmationWithBrowsingDataSeparate:YES];
      };
      OCMStub([performer_mock_
                  showManagedConfirmationForHostedDomain:hosted_domain
                                                identity:identity
                                          viewController:view_controller_mock_
                                                 browser:personal_browser_.get()
                               skipBrowsingDataMigration:migrationDisabled
                              mergeBrowsingDataByDefault:NO
                   browsingDataMigrationDisabledByPolicy:migrationDisabled])
          .andDo(showManagedConfirmationForHostedDomainCallback);

      if (AreSeparateProfilesForManagedAccountsEnabled()) {
        __block ChangeProfileContinuation continuation;
        auto switchToProfileWithIdentityCallback = ^(NSInvocation*) {
          base::OnceClosure completion = base::BindOnce(
              [](Browser* final_browser,
                 ChangeProfileContinuation continuation) {
                CHECK(continuation);
                // TODO
                std::move(continuation)
                    .Run(final_browser->GetSceneState(), base::DoNothing());
              },
              final_browser, std::move(continuation));
          [authentication_flow_
              didSwitchToProfileWithNewProfileBrowser:final_browser
                                           completion:std::move(completion)];
        };
        id delegateChecker = [OCMArg
            checkWithBlock:^(id<AuthenticationFlowDelegate> request_helper) {
              CHECK(request_helper);
              continuation =
                  [request_helper authenticationFlowWillChangeProfile];
              return true;
            }];
        OCMExpect(
            [performer_mock_
                switchToProfileWithIdentity:identity
                                 sceneState:personal_browser_->GetSceneState()
                                     reason:ChangeProfileReason::
                                                kManagedAccountSignIn
                                   delegate:delegateChecker
                          postSignInActions:postSignInActions
                                accessPoint:access_point])
            .andDo(switchToProfileWithIdentityCallback);
      }
      auto registerUserPolicyCallback = ^(NSInvocation*) {
        [authentication_flow_in_profile_
            didRegisterForUserPolicyWithDMToken:kFakeDMToken
                                       clientID:kFakeClientID
                             userAffiliationIDs:@[ kFakeUserAffiliationID ]];
      };
      OCMExpect([in_profile_performer_mock_ registerUserPolicy:final_profile
                                                   forIdentity:identity])
          .andDo(registerUserPolicyCallback);
      auto fetchUserPolicyCallback = ^(NSInvocation*) {
        [authentication_flow_in_profile_ didFetchUserPolicyWithSuccess:YES];
      };
      OCMExpect([in_profile_performer_mock_
                       fetchUserPolicy:final_profile
                           withDmToken:kFakeDMToken
                              clientID:kFakeClientID
                    userAffiliationIDs:@[ kFakeUserAffiliationID ]
                              identity:identity])
          .andDo(fetchUserPolicyCallback);
    }

    // If switching (to a managed profile), there's no explicit call to sign in,
    // since AuthenticationService does it internally.
    if (!should_switch_profile) {
      OCMExpect([in_profile_performer_mock_
          signInIdentity:identity
           atAccessPoint:access_point
          currentProfile:personal_profile_.get()]);
    }

    [authentication_flow_ startSignIn];
    // The completion block should not be called synchronously.
    EXPECT_EQ(signin::Tribool::kUnknown, signin_result_);
    CheckSignInCompletion(/*expected_signed_in=*/true);
  }

  void SignOutPersonalProfile() {
    AuthenticationService* authentication_service =
        AuthenticationServiceFactory::GetForProfile(personal_profile_.get());
    std::unique_ptr<base::RunLoop> run_loop = std::make_unique<base::RunLoop>();
    authentication_service->SignOut(
        signin_metrics::ProfileSignout::kSignoutForAccountSwitching,
        base::CallbackToBlock(run_loop->QuitClosure()));
    run_loop->Run();
  }

  FakeSystemIdentityManager* fake_system_identity_manager() {
    return FakeSystemIdentityManager::FromSystemIdentityManager(
        GetApplicationContext()->GetSystemIdentityManager());
  }

  base::test::ScopedFeatureList features_;

  web::WebTaskEnvironment task_environment_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  TestProfileManagerIOS profile_manager_;
  raw_ptr<TestProfileIOS> personal_profile_;
  std::unique_ptr<Browser> personal_browser_;
  raw_ptr<TestProfileIOS> managed_profile1_;
  std::unique_ptr<Browser> managed_browser1_;
  raw_ptr<TestProfileIOS> managed_profile2_;
  std::unique_ptr<Browser> managed_browser2_;
  id<SystemIdentity> identity1_ = nil;
  id<SystemIdentity> identity2_ = nil;
  id<SystemIdentity> managed_identity1_ = nil;
  id<SystemIdentity> managed_identity2_ = nil;
  AuthenticationFlow* authentication_flow_ = nil;
  TestAuthenticationFlowDelegate* test_authentication_flow_delegate_ = nil;
  AuthenticationFlowInProfile<AuthenticationFlowInProfilePerformerDelegate>*
      authentication_flow_in_profile_ = nil;
  AuthenticationFlowInProfilePerformer* in_profile_performer_mock_ = nil;
  AuthenticationFlowPerformer* performer_mock_ = nil;
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
TEST_P(AuthenticationFlowTest, TestSignInSimple) {
  SignIn(identity1_, signin_metrics::AccessPoint::kStartPage);

  histogram_tester_.ExpectUniqueSample(
      "Signin.AccountType.SigninConsent",
      signin_metrics::SigninAccountType::kRegular, 1);
}

// Tests the fetch managed status failure case.
TEST_P(AuthenticationFlowTest, TestFailFetchManagedStatus) {
  CreateAuthenticationFlow(PostSignInActionSet(), identity1_,
                           signin_metrics::AccessPoint::kStartPage,
                           /*shouldHandOverToFlowInProfile=*/NO);

  NSError* error = [NSError errorWithDomain:@"foo" code:0 userInfo:nil];
  OCMExpect([performer_mock_ fetchManagedStatus:personal_profile_.get()
                                    forIdentity:identity1_])
      .andDo(^(NSInvocation*) {
        [authentication_flow_ didFailFetchManagedStatus:error];
      });

  OCMExpect([performer_mock_ showAuthenticationError:[OCMArg any]
                                      withCompletion:[OCMArg any]
                                      viewController:view_controller_mock_
                                             browser:personal_browser_.get()])
      .andDo(^(NSInvocation* invocation) {
        __unsafe_unretained ProceduralBlock completionBlock;
        [invocation getArgument:&completionBlock atIndex:3];
        completionBlock();
      });
  [authentication_flow_ startSignIn];

  CheckSignInCompletion(/*expected_signed_in=*/false);
  histogram_tester_.ExpectTotalCount("Signin.AccountType.SigninConsent", 0);
}

// Tests that when signed in only with a managed account, the managed account
// confirmation dialog is shown.
TEST_P(AuthenticationFlowTest,
       TestShowManagedConfirmationForSigninConsentLevel) {
  SignIn(managed_identity1_, signin_metrics::AccessPoint::kSupervisedUser);
  histogram_tester_.ExpectUniqueSample(
      "Signin.AccountType.SigninConsent",
      signin_metrics::SigninAccountType::kManaged, 1);
  EXPECT_EQ(1, managed_confirmation_dialog_shown_count_);
}

// Tests that when the browser is already managed at the machine level, the
// management confirmation dialog is only shown without multiprofile. In all
// cases, the user policies should still be fetched.
TEST_P(AuthenticationFlowTest,
       TestSkipManagedConfirmationWhenAlreadyManagedAtMachineLevel) {
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
  // Iff the signin involved a profile switch, the management confirmation
  // dialog should still be shown.
  const int expected_count =
      AreSeparateProfilesForManagedAccountsEnabled() ? 1 : 0;
  EXPECT_EQ(expected_count, managed_confirmation_dialog_shown_count_);
}

// Tests that the managed confirmation dialog is only show once per account,
// when signing in from the Account Menu.
TEST_P(AuthenticationFlowTest, TestShowManagedConfirmationOnlyOnce) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(kIdentityDiscAccountMenu);

  // First signin, show the dialog.
  SignIn(managed_identity1_, signin_metrics::AccessPoint::kAccountMenu);
  EXPECT_EQ(1, managed_confirmation_dialog_shown_count_);

  // Second signin from the account menu, don't show the dialog.
  SignOutPersonalProfile();
  SignIn(managed_identity1_, signin_metrics::AccessPoint::kAccountMenu,
         /*adds_history_screen_post_profile_switch=*/false);
  EXPECT_EQ(1, managed_confirmation_dialog_shown_count_);

  // Signin from a different UI surface, show the dialog again.
  SignOutPersonalProfile();
  SignIn(managed_identity1_, signin_metrics::AccessPoint::kSupervisedUser,
         /*adds_history_screen_post_profile_switch=*/false);
  EXPECT_EQ(1, managed_confirmation_dialog_shown_count_);

  // Signin with a different account, show the dialog again.
  SignOutPersonalProfile();
  SignIn(managed_identity2_, signin_metrics::AccessPoint::kAccountMenu);
  EXPECT_EQ(2, managed_confirmation_dialog_shown_count_);
}

TEST_P(AuthenticationFlowTest, TestDontShowUnsyncedDataConfirmation) {
  // Another account is already signed in.
  AuthenticationServiceFactory::GetForProfile(personal_profile_.get())
      ->SignIn(identity1_, signin_metrics::AccessPoint::kStartPage);

  // Without signing out first, start signing in with a different identity. This
  // should trigger the check for unsynced data.
  CreateAuthenticationFlow(PostSignInActionSet(), identity2_,
                           signin_metrics::AccessPoint::kStartPage,
                           /*shouldHandOverToFlowInProfile=*/NO);

  syncer::SyncService* sync_service =
      SyncServiceFactory::GetForProfile(personal_profile_.get());
  OCMExpect([performer_mock_ fetchUnsyncedDataWithSyncService:sync_service])
      .andDo(^(NSInvocation*) {
        [authentication_flow_
            didFetchUnsyncedDataWithUnsyncedDataTypes:syncer::DataTypeSet()];
      });
  // There is no unsynced data in this case, so no confirmation should be
  // shown - the next step is fetching the managed status.
  // Don't bother continuing the flow beyond that step for this test.
  OCMExpect([performer_mock_ fetchManagedStatus:personal_profile_.get()
                                    forIdentity:identity2_])
      .andDo(^(NSInvocation*) {
        run_loop_->Quit();
      });

  [authentication_flow_ startSignIn];
  run_loop_->Run();
}

TEST_P(AuthenticationFlowTest, TestShowUnsyncedDataConfirmation) {
  // Another account is already signed in.
  AuthenticationServiceFactory::GetForProfile(personal_profile_.get())
      ->SignIn(identity1_, signin_metrics::AccessPoint::kStartPage);

  // Without signing out first, start signing in with a different identity. This
  // should trigger the check for unsynced data.
  CreateAuthenticationFlow(PostSignInActionSet(), identity2_,
                           signin_metrics::AccessPoint::kStartPage,
                           /*shouldHandOverToFlowInProfile=*/NO);

  syncer::SyncService* sync_service =
      SyncServiceFactory::GetForProfile(personal_profile_.get());
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
                                                              browser:
                                                                  personal_browser_
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

  [authentication_flow_ startSignIn];
  run_loop_->Run();
}

INSTANTIATE_TEST_SUITE_P(,
                         AuthenticationFlowTest,
                         testing::Bool(),
                         [](testing::TestParamInfo<bool> info) {
                           return info.param ? "WithSeparateProfiles"
                                             : "WithoutSeparateProfiles";
                         });

}  // namespace
