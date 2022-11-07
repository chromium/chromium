// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/signin/user_signin/user_signin_mediator.h"

#import <UIKit/UIKit.h>

#import "base/strings/sys_string_conversions.h"
#import "components/consent_auditor/fake_consent_auditor.h"
#import "components/sync/test/mock_sync_service.h"
#import "components/sync_preferences/pref_service_mock_factory.h"
#import "components/sync_preferences/pref_service_syncable.h"
#import "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/consent_auditor/consent_auditor_factory.h"
#import "ios/chrome/browser/consent_auditor/consent_auditor_test_utils.h"
#import "ios/chrome/browser/main/test_browser.h"
#import "ios/chrome/browser/signin/authentication_service.h"
#import "ios/chrome/browser/signin/authentication_service_factory.h"
#import "ios/chrome/browser/signin/chrome_account_manager_service_factory.h"
#import "ios/chrome/browser/signin/fake_authentication_service_delegate.h"
#import "ios/chrome/browser/signin/fake_system_identity.h"
#import "ios/chrome/browser/signin/identity_manager_factory.h"
#import "ios/chrome/browser/sync/mock_sync_service_utils.h"
#import "ios/chrome/browser/sync/sync_service_factory.h"
#import "ios/chrome/browser/sync/sync_setup_service_factory.h"
#import "ios/chrome/browser/sync/sync_setup_service_mock.h"
#import "ios/chrome/browser/ui/authentication/authentication_flow.h"
#import "ios/chrome/browser/ui/authentication/authentication_flow_performer.h"
#import "ios/chrome/browser/unified_consent/unified_consent_service_factory.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/public/provider/chrome/browser/signin/fake_chrome_identity_service.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"
#import "third_party/ocmock/ocmock_extensions.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

class UserSigninMediatorTest : public PlatformTest {
 public:
  UserSigninMediatorTest() : consent_string_ids_(ExpectedConsentStringIds()) {}

  void SetUp() override {
    PlatformTest::SetUp();
    identity_ = [FakeSystemIdentity fakeIdentity1];
    identity_service()->AddIdentity(identity_);
    TestChromeBrowserState::Builder builder;
    builder.AddTestingFactory(
        AuthenticationServiceFactory::GetInstance(),
        AuthenticationServiceFactory::GetDefaultFactory());
    builder.AddTestingFactory(ConsentAuditorFactory::GetInstance(),
                              base::BindRepeating(&BuildFakeConsentAuditor));
    builder.AddTestingFactory(SyncServiceFactory::GetInstance(),
                              base::BindRepeating(&CreateMockSyncService));
    builder.AddTestingFactory(
        SyncSetupServiceFactory::GetInstance(),
        base::BindRepeating(&SyncSetupServiceMock::CreateKeyedService));
    browser_state_ = builder.Build();

    AuthenticationServiceFactory::CreateAndInitializeForBrowserState(
        browser_state_.get(),
        std::make_unique<FakeAuthenticationServiceDelegate>());
    browser_ = std::make_unique<TestBrowser>(browser_state_.get());

    mediator_delegate_mock_ =
        OCMStrictProtocolMock(@protocol(UserSigninMediatorDelegate));
    mediator_ = [[UserSigninMediator alloc]
        initWithAuthenticationService:authentication_service()
                      identityManager:identity_manager()
                accountManagerService:account_manager_service()
                       consentAuditor:consent_auditor()
                unifiedConsentService:unified_consent_service()
                     syncSetupService:sync_setup_service()
                          syncService:sync_service()];

    mediator_.delegate = mediator_delegate_mock_;

    fake_consent_auditor_ =
        static_cast<consent_auditor::FakeConsentAuditor*>(consent_auditor());
    sync_setup_service_mock_ =
        static_cast<SyncSetupServiceMock*>(sync_setup_service());
    sync_service_mock_ = static_cast<syncer::MockSyncService*>(sync_service());
  }

  void TearDown() override {
    [mediator_ disconnect];
    EXPECT_OCMOCK_VERIFY((id)mediator_delegate_mock_);
    EXPECT_OCMOCK_VERIFY((id)performer_mock_);
    EXPECT_OCMOCK_VERIFY((id)presenting_view_controller_mock_);
    PlatformTest::TearDown();
  }

  // Sets up the necessary mocks for authentication operations in
  // `authentication_flow_`.
  void CreateAuthenticationFlow(PostSignInAction postSignInAction) {
    presenting_view_controller_mock_ =
        OCMStrictClassMock([UIViewController class]);
    performer_mock_ = OCMStrictClassMock([AuthenticationFlowPerformer class]);

    authentication_flow_ = [[AuthenticationFlow alloc]
                 initWithBrowser:browser_.get()
                        identity:identity_
                postSignInAction:postSignInAction
        presentingViewController:presenting_view_controller_mock_];
    [authentication_flow_ setPerformerForTesting:performer_mock_];
  }

  // Sets up the sign-in expectations for the AuthenticationFlowPerformer.
  void SetPerformerSigninExpectations(PostSignInAction postSignInAction) {
    OCMExpect([performer_mock_ fetchManagedStatus:browser_state_.get()
                                      forIdentity:identity_])
        .andDo(^(NSInvocation*) {
          NSLog(@" fetchManagedStatus ");
          [authentication_flow_ didFetchManagedStatus:nil];
        });
    OCMExpect([performer_mock_ signInIdentity:identity_
                             withHostedDomain:nil
                               toBrowserState:browser_state_.get()])
        .andDo(^(NSInvocation* invocation) {
          NSLog(@" signInIdentity ");
          authentication_service()->SignIn(identity_);
        });
    if (postSignInAction == POST_SIGNIN_ACTION_COMMIT_SYNC) {
      OCMExpect(
          [performer_mock_
              shouldHandleMergeCaseForIdentity:identity_
                             browserStatePrefs:browser_state_->GetPrefs()])
          .andReturn(NO);
      NSLog(@" shouldHandleMergeCaseForIdentity ");
      OCMExpect(
          [performer_mock_ commitSyncForBrowserState:browser_state_.get()]);
    }
  }

  void SetPerformerSignoutExpectations() {
    OCMExpect([performer_mock_ signOutBrowserState:browser_state_.get()])
        .andDo(^(NSInvocation*) {
          authentication_service()->SignOut(
              signin_metrics::ProfileSignout::SIGNOUT_TEST, false, ^{
                [authentication_flow_ didSignOut];
              });
        });
  }

  // Sets up the sign-in failure expectations for the
  // AuthenticationFlowPerformer.
  void SetPerformerFailureExpectations() {
    NSError* error = [NSError errorWithDomain:@"foo" code:0 userInfo:nil];
    OCMExpect([performer_mock_ fetchManagedStatus:browser_state_.get()
                                      forIdentity:identity_])
        .andDo(^(NSInvocation*) {
          [authentication_flow_ didFailFetchManagedStatus:error];
        });

    OCMExpect([performer_mock_
                  showAuthenticationError:[OCMArg any]
                           withCompletion:[OCMArg any]
                           viewController:presenting_view_controller_mock_
                                  browser:browser_.get()])
        .andDo(^(NSInvocation* invocation) {
          __weak ProceduralBlock completionBlock;
          [invocation getArgument:&completionBlock atIndex:3];
          if (completionBlock) {
            completionBlock();
          }
        });
  }

  // Sets up the expectations for cancelAndDismissAnimated in the
  // AuthenticationFlowPerformer.
  void SetPerformerCancelAndDismissExpectations(BOOL animated) {
    OCMExpect([performer_mock_ fetchManagedStatus:browser_state_.get()
                                      forIdentity:identity_])
        .andDo(^(NSInvocation*) {
          [authentication_flow_ didFetchManagedStatus:nil];
        });
    OCMExpect([performer_mock_
                  shouldHandleMergeCaseForIdentity:identity_
                                 browserStatePrefs:browser_state_->GetPrefs()])
        .andReturn(YES);
    OCMExpect([performer_mock_
        promptMergeCaseForIdentity:identity_
                           browser:browser_.get()
                    viewController:presenting_view_controller_mock_]);
    OCMExpect([performer_mock_ cancelAndDismissAnimated:animated]);
  }

  void ExpectNoConsent() {
    EXPECT_EQ(0ul, fake_consent_auditor_->recorded_id_vectors().size());
    EXPECT_EQ(0ul, fake_consent_auditor_->recorded_confirmation_ids().size());
  }

  void ExpectConsent(int consentType) {
    const std::vector<int>& recorded_ids =
        fake_consent_auditor_->recorded_id_vectors().at(0);
    EXPECT_EQ(ExpectedConsentStringIds(), recorded_ids);
    EXPECT_EQ(consentType,
              fake_consent_auditor_->recorded_confirmation_ids().at(0));
    EXPECT_EQ(consent_auditor::ConsentStatus::GIVEN,
              fake_consent_auditor_->recorded_statuses().at(0));
    EXPECT_EQ(consent_auditor::Feature::CHROME_SYNC,
              fake_consent_auditor_->recorded_features().at(0));
    EXPECT_EQ(identity_manager()->PickAccountIdForAccount(
                  base::SysNSStringToUTF8([identity_ gaiaID]),
                  base::SysNSStringToUTF8([identity_ userEmail])),
              fake_consent_auditor_->account_id());
  }

  // Returns the list of string id that should be given to RecordGaiaConsent()
  // then the consent is given. The list is ordered according to the position
  // on the screen.
  const std::vector<int> ExpectedConsentStringIds() const {
    return {
        IDS_IOS_ACCOUNT_UNIFIED_CONSENT_TITLE,
        IDS_IOS_ACCOUNT_UNIFIED_CONSENT_SYNC_TITLE,
        IDS_IOS_ACCOUNT_UNIFIED_CONSENT_SYNC_SUBTITLE,
        IDS_IOS_ACCOUNT_UNIFIED_CONSENT_SETTINGS,
    };
  }

  // Identity services.
  AuthenticationService* authentication_service() {
    return AuthenticationServiceFactory::GetForBrowserState(
        browser_state_.get());
  }

  consent_auditor::ConsentAuditor* consent_auditor() {
    return ConsentAuditorFactory::GetForBrowserState(browser_state_.get());
  }

  signin::IdentityManager* identity_manager() {
    return IdentityManagerFactory::GetForBrowserState(browser_state_.get());
  }

  ChromeAccountManagerService* account_manager_service() {
    return ChromeAccountManagerServiceFactory::GetForBrowserState(
        browser_state_.get());
  }

  ios::FakeChromeIdentityService* identity_service() {
    return ios::FakeChromeIdentityService::GetInstanceFromChromeProvider();
  }

  SyncSetupService* sync_setup_service() {
    return SyncSetupServiceFactory::GetForBrowserState(browser_state_.get());
  }

  syncer::SyncService* sync_service() {
    return SyncServiceFactory::GetForBrowserState(browser_state_.get());
  }

  unified_consent::UnifiedConsentService* unified_consent_service() {
    return UnifiedConsentServiceFactory::GetForBrowserState(
        browser_state_.get());
  }

 protected:
  // Needed for test browser state created by TestChromeBrowserState().
  web::WebTaskEnvironment task_environment_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  id<SystemIdentity> identity_ = nil;

  AuthenticationFlow* authentication_flow_ = nullptr;
  std::unique_ptr<Browser> browser_;
  std::unique_ptr<TestChromeBrowserState> browser_state_;
  consent_auditor::FakeConsentAuditor* fake_consent_auditor_ = nullptr;
  const std::vector<int> consent_string_ids_;

  UserSigninMediator* mediator_ = nil;

  id<UserSigninMediatorDelegate> mediator_delegate_mock_ = nil;
  AuthenticationFlowPerformer* performer_mock_ = nil;
  UIViewController* presenting_view_controller_mock_ = nil;
  SyncSetupServiceMock* sync_setup_service_mock_ = nullptr;
  syncer::MockSyncService* sync_service_mock_ = nullptr;
};

// Tests a successful authentication for a given identity.
TEST_F(UserSigninMediatorTest, AuthenticateWithIdentitySuccess) {
  CreateAuthenticationFlow(POST_SIGNIN_ACTION_COMMIT_SYNC);
  SetPerformerSigninExpectations(POST_SIGNIN_ACTION_COMMIT_SYNC);

  // Retrieving coordinator data for the mediator delegate.
  OCMExpect(
      [mediator_delegate_mock_ userSigninMediatorGetConsentConfirmationId])
      .andReturn(IDS_IOS_ACCOUNT_UNIFIED_CONSENT_OK_BUTTON);
  OCMExpect([mediator_delegate_mock_ userSigninMediatorGetConsentStringIds])
      .andReturn(&consent_string_ids_);
  OCMExpect(
      [mediator_delegate_mock_ userSigninMediatorGetSettingsLinkWasTapped])
      .andReturn(NO);

  // Sign-in result successful.
  OCMExpect([mediator_delegate_mock_ userSigninMediatorSigninFinishedWithResult:
                                         SigninCoordinatorResultSuccess]);
  EXPECT_CALL(
      *sync_setup_service_mock_,
      SetFirstSetupComplete(syncer::SyncFirstSetupCompleteSource::BASIC_FLOW));

  [mediator_ authenticateWithIdentity:identity_
                   authenticationFlow:authentication_flow_];
  base::RunLoop().RunUntilIdle();
  ExpectConsent(IDS_IOS_ACCOUNT_UNIFIED_CONSENT_OK_BUTTON);
}

// Tests authenticating the identity when the settings link has been tapped.
TEST_F(UserSigninMediatorTest, AuthenticateWithSettingsLinkTapped) {
  CreateAuthenticationFlow(POST_SIGNIN_ACTION_COMMIT_SYNC);
  SetPerformerSigninExpectations(POST_SIGNIN_ACTION_COMMIT_SYNC);

  OCMExpect(
      [mediator_delegate_mock_ userSigninMediatorGetSettingsLinkWasTapped])
      .andReturn(YES);

  // Sign-in result successful.
  OCMExpect([mediator_delegate_mock_ userSigninMediatorSigninFinishedWithResult:
                                         SigninCoordinatorResultSuccess]);
  EXPECT_CALL(
      *sync_setup_service_mock_,
      SetFirstSetupComplete(syncer::SyncFirstSetupCompleteSource::BASIC_FLOW))
      .Times(0);

  [mediator_ authenticateWithIdentity:identity_
                   authenticationFlow:authentication_flow_];
  base::RunLoop().RunUntilIdle();
}

// Tests authentication failure for a given identity.
TEST_F(UserSigninMediatorTest, AuthenticateWithIdentityError) {
  CreateAuthenticationFlow(POST_SIGNIN_ACTION_COMMIT_SYNC);
  SetPerformerFailureExpectations();

  OCMExpect(
      [mediator_delegate_mock_ userSigninMediatorGetSettingsLinkWasTapped])
      .andReturn(NO);
  // Returns to sign-in flow.
  OCMExpect([mediator_delegate_mock_ userSigninMediatorSigninFailed]);

  [mediator_ authenticateWithIdentity:identity_
                   authenticationFlow:authentication_flow_];
  base::RunLoop().RunUntilIdle();
  ExpectNoConsent();
}

// Tests a user sign-in operation cancel when authentication has not begun.
TEST_F(UserSigninMediatorTest, CancelAuthenticationNotInProgress) {
  // Sign-in result cancel.
  OCMExpect(
      [mediator_delegate_mock_ userSigninMediatorSigninFinishedWithResult:
                                   SigninCoordinatorResultCanceledByUser]);
  OCMExpect([mediator_delegate_mock_ signinStateOnStart])
      .andReturn(IdentitySigninStateSignedOut);

  [mediator_ cancelSignin];
  ExpectNoConsent();
  EXPECT_FALSE(authentication_service()->HasPrimaryIdentity(
      signin::ConsentLevel::kSignin));
}

// Tests a user sign-in operation cancel when authentication is in progress.
TEST_F(UserSigninMediatorTest, CancelWithAuthenticationInProgress) {
  SetPerformerCancelAndDismissExpectations(/*animated=*/NO);

  OCMExpect(
      [mediator_delegate_mock_ userSigninMediatorGetSettingsLinkWasTapped])
      .andReturn(NO);
  // Unsuccessful sign-in completion updates the primary button.
  OCMExpect(
      [mediator_delegate_mock_ userSigninMediatorSigninFinishedWithResult:
                                   SigninCoordinatorResultCanceledByUser]);
  OCMExpect([mediator_delegate_mock_ signinStateOnStart])
      .andReturn(IdentitySigninStateSignedOut);

  [mediator_ authenticateWithIdentity:identity_
                   authenticationFlow:authentication_flow_];
  [mediator_ cancelSignin];
  base::RunLoop().RunUntilIdle();
  ExpectNoConsent();
  EXPECT_FALSE(authentication_service()->HasPrimaryIdentity(
      signin::ConsentLevel::kSignin));
}

// Tests a user sign-in operation cancel and dismiss when authentication has not
// begun.
TEST_F(UserSigninMediatorTest, CancelAndDismissAuthenticationNotInProgress) {
  OCMExpect([mediator_delegate_mock_ signinStateOnStart])
      .andReturn(IdentitySigninStateSignedOut);
  __block bool completion_called = false;
  [mediator_ cancelAndDismissAuthenticationFlowAnimated:NO
                                             completion:^() {
                                               completion_called = true;
                                             }];
  base::RunLoop().RunUntilIdle();
  ExpectNoConsent();
  EXPECT_TRUE(completion_called);
  EXPECT_FALSE(authentication_service()->HasPrimaryIdentity(
      signin::ConsentLevel::kSignin));
}

// Tests a user sign-in operation cancel and dismiss with animation when
// authentication is in progress.
TEST_F(UserSigninMediatorTest,
       CancelAndDismissAuthenticationInProgressWithAnimation) {
  CreateAuthenticationFlow(POST_SIGNIN_ACTION_COMMIT_SYNC);
  SetPerformerCancelAndDismissExpectations(/*animated=*/YES);

  OCMExpect(
      [mediator_delegate_mock_ userSigninMediatorGetSettingsLinkWasTapped])
      .andReturn(NO);
  // Unsuccessful sign-in completion updates the primary button.
  OCMExpect([mediator_delegate_mock_ userSigninMediatorSigninFailed]);
  OCMExpect([mediator_delegate_mock_ signinStateOnStart])
      .andReturn(IdentitySigninStateSignedOut);

  [mediator_ authenticateWithIdentity:identity_
                   authenticationFlow:authentication_flow_];
  __block bool completion_called = false;
  [mediator_ cancelAndDismissAuthenticationFlowAnimated:YES
                                             completion:^() {
                                               completion_called = true;
                                             }];
  base::RunLoop().RunUntilIdle();
  ExpectNoConsent();
  EXPECT_TRUE(completion_called);
  EXPECT_FALSE(authentication_service()->HasPrimaryIdentity(
      signin::ConsentLevel::kSignin));
}

// Tests a user sign-in operation cancel and dismiss without animation when
// authentication is in progress.
TEST_F(UserSigninMediatorTest,
       CancelAndDismissAuthenticationInProgressWithoutAnimation) {
  CreateAuthenticationFlow(POST_SIGNIN_ACTION_COMMIT_SYNC);
  SetPerformerCancelAndDismissExpectations(/*animated=*/NO);

  OCMExpect(
      [mediator_delegate_mock_ userSigninMediatorGetSettingsLinkWasTapped])
      .andReturn(NO);
  // Unsuccessful sign-in completion updates the primary button.
  OCMExpect([mediator_delegate_mock_ userSigninMediatorSigninFailed]);
  OCMExpect([mediator_delegate_mock_ signinStateOnStart])
      .andReturn(IdentitySigninStateSignedOut);

  [mediator_ authenticateWithIdentity:identity_
                   authenticationFlow:authentication_flow_];
  __block bool completion_called = false;
  [mediator_ cancelAndDismissAuthenticationFlowAnimated:NO
                                             completion:^() {
                                               completion_called = true;
                                             }];
  base::RunLoop().RunUntilIdle();
  ExpectNoConsent();
  EXPECT_TRUE(completion_called);
  EXPECT_FALSE(authentication_service()->HasPrimaryIdentity(
      signin::ConsentLevel::kSignin));
}

// Tests a user sign-in operation cancel and dismiss without animation when
// authentication is in progress.
TEST_F(UserSigninMediatorTest, CancelSyncAndStaySignin) {
  CreateAuthenticationFlow(POST_SIGNIN_ACTION_COMMIT_SYNC);
  SetPerformerSigninExpectations(POST_SIGNIN_ACTION_COMMIT_SYNC);

  OCMExpect(
      [mediator_delegate_mock_ userSigninMediatorGetSettingsLinkWasTapped])
      .andReturn(YES);

  // Sign-in result successful.
  OCMExpect([mediator_delegate_mock_ userSigninMediatorSigninFinishedWithResult:
                                         SigninCoordinatorResultSuccess]);
  EXPECT_CALL(
      *sync_setup_service_mock_,
      SetFirstSetupComplete(syncer::SyncFirstSetupCompleteSource::BASIC_FLOW))
      .Times(0);

  [mediator_ authenticateWithIdentity:identity_
                   authenticationFlow:authentication_flow_];
  base::RunLoop().RunUntilIdle();
  OCMStub([mediator_delegate_mock_ signinStateOnStart])
      .andReturn(IdentitySigninStateSignedInWithSyncDisabled);
  OCMStub([mediator_delegate_mock_ signinIdentityOnStart]).andReturn(identity_);
  __block bool completion_called = false;
  [mediator_ cancelAndDismissAuthenticationFlowAnimated:YES
                                             completion:^() {
                                               completion_called = true;
                                             }];
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(completion_called);
  EXPECT_TRUE(authentication_service()->HasPrimaryIdentity(
      signin::ConsentLevel::kSignin));
}

// Tests the following scenario:
//   * Open the user sign-in dialog to turn on sync, with identity 2
//   * Select identity 1
//   * Open settings link
//   * Cancel the user sign-in dialog
TEST_F(UserSigninMediatorTest, OpenSettingsLinkWithDifferentIdentityAndCancel) {
  // Signs in with identity 2.
  id<SystemIdentity> identity2 = [FakeSystemIdentity fakeIdentity2];
  identity_service()->AddIdentity(identity2);
  authentication_service()->SignIn(identity2);

  // Opens the settings link with identity 1.
  CreateAuthenticationFlow(POST_SIGNIN_ACTION_NONE);
  SetPerformerSignoutExpectations();
  SetPerformerSigninExpectations(POST_SIGNIN_ACTION_NONE);
  OCMExpect(
      [mediator_delegate_mock_ userSigninMediatorGetSettingsLinkWasTapped])
      .andReturn(YES);
  OCMExpect([mediator_delegate_mock_ userSigninMediatorSigninFinishedWithResult:
                                         SigninCoordinatorResultSuccess]);
  EXPECT_CALL(
      *sync_setup_service_mock_,
      SetFirstSetupComplete(syncer::SyncFirstSetupCompleteSource::BASIC_FLOW))
      .Times(0);
  [mediator_ authenticateWithIdentity:identity_
                   authenticationFlow:authentication_flow_];
  base::RunLoop().RunUntilIdle();

  // Cancels the sign-in dialog.
  OCMStub([mediator_delegate_mock_ signinStateOnStart])
      .andReturn(IdentitySigninStateSignedInWithSyncDisabled);
  OCMStub([mediator_delegate_mock_ signinIdentityOnStart]).andReturn(identity2);
  __block bool completion_called = false;
  [mediator_ cancelAndDismissAuthenticationFlowAnimated:YES
                                             completion:^() {
                                               completion_called = true;
                                             }];
  base::RunLoop().RunUntilIdle();

  // Expects to be signed in with identity 2.
  EXPECT_TRUE(completion_called);
  EXPECT_TRUE(authentication_service()->HasPrimaryIdentity(
      signin::ConsentLevel::kSignin));
  EXPECT_TRUE([identity2 isEqual:authentication_service()->GetPrimaryIdentity(
                                     signin::ConsentLevel::kSignin)]);
}

// Tests the following scenario:
//   * Open the user sign-in dialog to turn on sync, with identity 2
//   * Select identity 1
//   * Open settings link
//   * Forget identity 2 (from another Google app)
//   * Cancel the user sign-in dialog
TEST_F(UserSigninMediatorTest,
       OpenSettingsLinkWithDifferentIdentityAndForgetIdentity) {
  // Signs in with identity 2.
  id<SystemIdentity> identity2 =
      [FakeSystemIdentity identityWithEmail:@"foo2@gmail.com"
                                     gaiaID:@"foo2ID"
                                       name:@"Fake Foo 2"];
  identity_service()->AddIdentity(identity2);
  authentication_service()->SignIn(identity2);

  // Opens the settings link with identity 1.
  CreateAuthenticationFlow(POST_SIGNIN_ACTION_NONE);
  SetPerformerSignoutExpectations();
  SetPerformerSigninExpectations(POST_SIGNIN_ACTION_NONE);

  OCMExpect(
      [mediator_delegate_mock_ userSigninMediatorGetSettingsLinkWasTapped])
      .andReturn(YES);
  OCMExpect([mediator_delegate_mock_ userSigninMediatorSigninFinishedWithResult:
                                         SigninCoordinatorResultSuccess]);
  EXPECT_CALL(
      *sync_setup_service_mock_,
      SetFirstSetupComplete(syncer::SyncFirstSetupCompleteSource::BASIC_FLOW))
      .Times(0);
  [mediator_ authenticateWithIdentity:identity_
                   authenticationFlow:authentication_flow_];
  base::RunLoop().RunUntilIdle();

  // Forgets identity 2.
  identity_service()->ForgetIdentity(identity2, nil);

  // Cancels the sign-in dialog.
  OCMStub([mediator_delegate_mock_ signinStateOnStart])
      .andReturn(IdentitySigninStateSignedInWithSyncDisabled);
  OCMStub([mediator_delegate_mock_ signinIdentityOnStart]).andReturn(identity2);
  __block bool completion_called = false;
  [mediator_ cancelAndDismissAuthenticationFlowAnimated:YES
                                             completion:^() {
                                               completion_called = true;
                                             }];
  base::RunLoop().RunUntilIdle();

  // Expects to be signed out.
  EXPECT_TRUE(completion_called);
  EXPECT_FALSE(authentication_service()->HasPrimaryIdentity(
      signin::ConsentLevel::kSignin));
}

// Tests the following scenario:
//   * Open the user sign-in dialog to turn on sync, with identity_
//   * Forget identity_
//   * Cancel the user sign-in dialog
TEST_F(UserSigninMediatorTest, ForgetSignedInIdentityWhileTurnOnSyncIsOpened) {
  identity_service()->ForgetIdentity(identity_, nil);

  // Cancels the sign-in dialog.
  __block bool completion_called = false;
  OCMStub([mediator_delegate_mock_ signinStateOnStart])
      .andReturn(IdentitySigninStateSignedInWithSyncDisabled);
  OCMStub([mediator_delegate_mock_ signinIdentityOnStart])
      .andReturn(static_cast<id>(nil));
  [mediator_ cancelAndDismissAuthenticationFlowAnimated:YES
                                             completion:^() {
                                               completion_called = true;
                                             }];
  base::RunLoop().RunUntilIdle();

  // Expects to be signed out.
  EXPECT_TRUE(completion_called);
  EXPECT_FALSE(authentication_service()->HasPrimaryIdentity(
      signin::ConsentLevel::kSignin));
}
