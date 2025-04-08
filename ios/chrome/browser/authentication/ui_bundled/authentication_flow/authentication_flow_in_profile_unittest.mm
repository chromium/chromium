// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/authentication/ui_bundled/authentication_flow/authentication_flow_in_profile.h"

#import <memory>

#import "base/run_loop.h"
#import "base/test/scoped_feature_list.h"
#import "base/test/test_future.h"
#import "ios/chrome/browser/authentication/ui_bundled/authentication_flow/authentication_flow_performer.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/features.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_manager_ios.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/fake_authentication_service_delegate.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/browser/signin/model/fake_system_identity_manager.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/gtest_support.h"
#import "third_party/ocmock/ocmock_extensions.h"

namespace {

NSString* const kFakeDMToken = @"fake_dm_token";
NSString* const kFakeClientID = @"fake_client_id";
NSString* const kFakeUserAffiliationID = @"fake_user_affiliation_id";

// The parameter determines whether `kSeparateProfilesForManagedAccounts` is
// enabled.
class AuthenticationFlowInProfileTest
    : public PlatformTest,
      public testing::WithParamInterface<bool> {
 protected:
  AuthenticationFlowInProfileTest() {
    features_.InitWithFeatureState(kSeparateProfilesForManagedAccounts,
                                   GetParam());

    TestProfileIOS::Builder builder;
    builder.AddTestingFactory(
        AuthenticationServiceFactory::GetInstance(),
        AuthenticationServiceFactory::GetFactoryWithDelegate(
            std::make_unique<FakeAuthenticationServiceDelegate>()));
    profile_ = profile_manager_.AddProfileWithBuilder(std::move(builder));
    browser_ = std::make_unique<TestBrowser>(profile_.get());

    FakeSystemIdentityManager* fake_system_identity_manager =
        FakeSystemIdentityManager::FromSystemIdentityManager(
            GetApplicationContext()->GetSystemIdentityManager());
    identity1_ = [FakeSystemIdentity fakeIdentity1];
    fake_system_identity_manager->AddIdentity(identity1_);
    identity2_ = [FakeSystemIdentity fakeIdentity2];
    fake_system_identity_manager->AddIdentity(identity2_);
    managed_identity_ = [FakeSystemIdentity fakeManagedIdentity];
    fake_system_identity_manager->AddIdentity(managed_identity_);

    performer_mock_ = OCMStrictClassMock([AuthenticationFlowPerformer class]);
    OCMExpect([(id)performer_mock_ alloc]).andReturn(performer_mock_);

    // Force explicit instantiation of the AuthenticationService, to ensure
    // accounts get synced over to IdentityManager.
    std::ignore = AuthenticationServiceFactory::GetForProfile(profile_.get());

    signin::IdentityManager* identity_manager =
        IdentityManagerFactory::GetForProfile(profile_.get());
    if (AreSeparateProfilesForManagedAccountsEnabled()) {
      // For the purpose of these tests, ensure that the managed identity is
      // assigned to the personal profile. "Personal" vs "managed" profile
      // doesn't really matter here (AuthenticationFlowInProfile, as its name
      // says, doesn't deal with other profiles); it's just important that all
      // required identities are available in the current/single profile.
      CHECK_EQ(identity_manager->GetAccountsWithRefreshTokens().size(), 2UL);
      GetApplicationContext()
          ->GetAccountProfileMapper()
          ->MoveManagedAccountToPersonalProfileForTesting(
              GaiaId(managed_identity_.gaiaID));
    }
    CHECK_EQ(identity_manager->GetAccountsWithRefreshTokens().size(), 3UL);
  }

  void TearDown() override {
    EXPECT_OCMOCK_VERIFY((id)performer_mock_);
    PlatformTest::TearDown();
  }

  // Creates a new AuthenticationFlowInProfile with default values.
  void CreateAuthenticationFlowInProfile(
      PostSignInActionSet post_sign_in_actions,
      id<SystemIdentity> identity,
      signin_metrics::AccessPoint access_point) {
    BOOL is_managed_identity = identity == managed_identity_;
    authentication_flow_in_profile_ = [[AuthenticationFlowInProfile alloc]
             initWithBrowser:browser_.get()
                    identity:identity
           isManagedIdentity:is_managed_identity
                 accessPoint:access_point
        precedingHistorySync:NO
           postSignInActions:post_sign_in_actions];
    id<AuthenticationFlowPerformerDelegate> performer_delegate =
        GetAuthenticationFlowPerformerDelegate();
    OCMExpect([performer_mock_ initWithDelegate:performer_delegate
                           changeProfileHandler:[OCMArg any]])
        .andReturn(performer_mock_);
  }

  id<AuthenticationFlowPerformerDelegate>
  GetAuthenticationFlowPerformerDelegate() {
    return static_cast<id<AuthenticationFlowPerformerDelegate>>(
        authentication_flow_in_profile_);
  }

  base::test::ScopedFeatureList features_;

  web::WebTaskEnvironment task_environment_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  TestProfileManagerIOS profile_manager_;
  raw_ptr<TestProfileIOS> profile_;
  std::unique_ptr<Browser> browser_;
  AuthenticationFlowInProfile* authentication_flow_in_profile_ = nil;
  id<SystemIdentity> identity1_ = nil;
  id<SystemIdentity> identity2_ = nil;
  id<SystemIdentity> managed_identity_ = nil;
  AuthenticationFlowPerformer* performer_mock_ = nil;
};

// Tests the regular sign-in case.
TEST_P(AuthenticationFlowInProfileTest, TestSignIn) {
  const signin_metrics::AccessPoint access_point =
      signin_metrics::AccessPoint::kStartPage;
  CreateAuthenticationFlowInProfile(PostSignInActionSet(), identity1_,
                                    access_point);
  // Start `authentication_flow_in_profile_` for `identity1_`.
  base::test::TestFuture<SigninCoordinatorResult> future;
  [authentication_flow_in_profile_
      startSignInWithCompletion:base::CallbackToBlock(future.GetCallback())];
  // Expect to call the performer to sign-in.
  OCMExpect([performer_mock_ signInIdentity:identity1_
                              atAccessPoint:access_point
                             currentProfile:profile_.get()]);
  // Expect sign-in completed while running the run loop.
  OCMExpect([performer_mock_ completePostSignInActions:PostSignInActionSet()
                                          withIdentity:identity1_
                                               browser:browser_.get()
                                           accessPoint:access_point]);
  EXPECT_EQ(future.Take(),
            SigninCoordinatorResult::SigninCoordinatorResultSuccess);
}

// Tests sign-in flow with a profile that is already signed-in with the right
// identity.
TEST_P(AuthenticationFlowInProfileTest, TestSignInWhileBeingSignedIn) {
  const signin_metrics::AccessPoint access_point =
      signin_metrics::AccessPoint::kStartPage;

  AuthenticationService* authentication_service =
      AuthenticationServiceFactory::GetForProfile(profile_.get());
  authentication_service->SignIn(identity1_, access_point);
  CreateAuthenticationFlowInProfile(PostSignInActionSet(), identity1_,
                                    access_point);
  // Start `authentication_flow_in_profile_` for `identity1_`.
  base::test::TestFuture<SigninCoordinatorResult> future;
  [authentication_flow_in_profile_
      startSignInWithCompletion:base::CallbackToBlock(future.GetCallback())];
  // Expect sign-in completed while running the run loop.
  OCMExpect([performer_mock_ completePostSignInActions:PostSignInActionSet()
                                          withIdentity:identity1_
                                               browser:browser_.get()
                                           accessPoint:access_point]);
  // Note: No call to `-[AuthenticationFlowPerformer
  // signInIdentity:atAccessPoint:currentProfile:]` since the profile is already
  // signed in with the right identity.
  EXPECT_EQ(future.Take(),
            SigninCoordinatorResult::SigninCoordinatorResultSuccess);
}

// Tests sign-in flow with a profile that is already signed-in with a different
// identity.
TEST_P(AuthenticationFlowInProfileTest, TestSignOutAndSignIn) {
  const signin_metrics::AccessPoint access_point =
      signin_metrics::AccessPoint::kStartPage;

  // Sign-in with `identity2_`.
  AuthenticationService* authentication_service =
      AuthenticationServiceFactory::GetForProfile(profile_.get());
  authentication_service->SignIn(identity2_, access_point);

  CreateAuthenticationFlowInProfile(PostSignInActionSet(), identity1_,
                                    access_point);
  // Start `authentication_flow_in_profile_` for `identity1_`.
  base::test::TestFuture<SigninCoordinatorResult> future;
  [authentication_flow_in_profile_
      startSignInWithCompletion:base::CallbackToBlock(future.GetCallback())];
  // Expect sign-out request.
  __block std::unique_ptr<base::RunLoop> run_loop =
      std::make_unique<base::RunLoop>();
  OCMExpect([performer_mock_ signOutForAccountSwitchWithProfile:profile_.get()])
      .andDo(^(NSInvocation* invocation) {
        run_loop->Quit();
      });
  run_loop->Run();
  run_loop = std::make_unique<base::RunLoop>();
  // Perform sign-out request, simulating what the real performer would do..
  authentication_service->SignOut(
      signin_metrics::ProfileSignout::kSignoutForAccountSwitching,
      base::CallbackToBlock(run_loop->QuitClosure()));
  run_loop->Run();
  // Continue AuthenticationFlowInProfile flow.
  // Expect to call the performer to sign-in.
  OCMExpect([performer_mock_ signInIdentity:identity1_
                              atAccessPoint:access_point
                             currentProfile:profile_.get()]);
  // Expect sign-in completed while running the run loop.
  OCMExpect([performer_mock_ completePostSignInActions:PostSignInActionSet()
                                          withIdentity:identity1_
                                               browser:browser_.get()
                                           accessPoint:access_point]);
  [GetAuthenticationFlowPerformerDelegate() didSignOutForAccountSwitch];
  EXPECT_EQ(future.Take(),
            SigninCoordinatorResult::SigninCoordinatorResultSuccess);
}

// Tests sign-in flow with an identity that is not available in the profile.
TEST_P(AuthenticationFlowInProfileTest, TestSignInWithUnknownIdentity) {
  const signin_metrics::AccessPoint access_point =
      signin_metrics::AccessPoint::kStartPage;
  FakeSystemIdentity* unknown_identity = [FakeSystemIdentity fakeIdentity3];
  CreateAuthenticationFlowInProfile(PostSignInActionSet(), unknown_identity,
                                    access_point);
  OCMExpect([performer_mock_ showAuthenticationError:[OCMArg any]
                                      withCompletion:[OCMArg invokeBlock]
                                      viewController:[OCMArg any]
                                             browser:browser_.get()]);
  // Start `authentication_flow_in_profile_` for `unknown_identity`.
  base::test::TestFuture<SigninCoordinatorResult> future;
  [authentication_flow_in_profile_
      startSignInWithCompletion:base::CallbackToBlock(future.GetCallback())];
  // Expect to `authentication_flow_in_profile_` to fail.
  EXPECT_EQ(future.Take(),
            SigninCoordinatorResult::SigninCoordinatorResultInterrupted);
}

// Tests sign-in flow with a managed identity. The managed identity is assigned
// the unique TestProfile. There is profile switching involved.
TEST_P(AuthenticationFlowInProfileTest, TestSignInWithManagedIdentity) {
  const signin_metrics::AccessPoint access_point =
      signin_metrics::AccessPoint::kStartPage;
  CreateAuthenticationFlowInProfile(PostSignInActionSet(), managed_identity_,
                                    access_point);
  // Start `authentication_flow_in_profile_` for `managed_identity_`.
  base::test::TestFuture<SigninCoordinatorResult> future;
  [authentication_flow_in_profile_
      startSignInWithCompletion:base::CallbackToBlock(future.GetCallback())];
  // Expect to call the performer to sign-in.
  OCMExpect([performer_mock_ signInIdentity:managed_identity_
                              atAccessPoint:access_point
                             currentProfile:profile_.get()]);
  // Expect user policy register request.
  __block std::unique_ptr<base::RunLoop> run_loop =
      std::make_unique<base::RunLoop>();
  OCMExpect([performer_mock_ registerUserPolicy:profile_.get()
                                    forIdentity:managed_identity_])
      .andDo(^(NSInvocation* invocation) {
        run_loop->Quit();
      });
  run_loop->Run();
  // Expect user policy fetch request.
  OCMExpect([performer_mock_ fetchUserPolicy:profile_.get()
                                 withDmToken:kFakeDMToken
                                    clientID:kFakeClientID
                          userAffiliationIDs:@[ kFakeUserAffiliationID ]
                                    identity:managed_identity_]);
  // Simulate the user policy register request.
  [GetAuthenticationFlowPerformerDelegate()
      didRegisterForUserPolicyWithDMToken:kFakeDMToken
                                 clientID:kFakeClientID
                       userAffiliationIDs:@[ kFakeUserAffiliationID ]];
  // Expect sign-in completed while running the run loop.
  OCMExpect([performer_mock_ completePostSignInActions:PostSignInActionSet()
                                          withIdentity:managed_identity_
                                               browser:browser_.get()
                                           accessPoint:access_point]);
  // Simulate the user policy fetch request.
  [GetAuthenticationFlowPerformerDelegate() didFetchUserPolicyWithSuccess:YES];
  EXPECT_EQ(future.Take(),
            SigninCoordinatorResult::SigninCoordinatorResultSuccess);
}

INSTANTIATE_TEST_SUITE_P(,
                         AuthenticationFlowInProfileTest,
                         testing::Bool(),
                         [](testing::TestParamInfo<bool> info) {
                           return info.param ? "WithSeparateProfiles"
                                             : "WithoutSeparateProfiles";
                         });

}  // namespace
