// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/signin/user_signin/user_signin_mediator.h"

#import <UIKit/UIKit.h>

#import "base/strings/sys_string_conversions.h"
#import "components/consent_auditor/fake_consent_auditor.h"
#import "components/sync/driver/mock_sync_service.h"
#import "components/sync_preferences/pref_service_mock_factory.h"
#import "components/sync_preferences/pref_service_syncable.h"
#import "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/main/test_browser.h"
#import "ios/chrome/browser/signin/authentication_service_factory.h"
#import "ios/chrome/browser/signin/authentication_service_fake.h"
#import "ios/chrome/browser/signin/identity_manager_factory.h"
#import "ios/chrome/browser/sync/consent_auditor_factory.h"
#import "ios/chrome/browser/sync/profile_sync_service_factory.h"
#import "ios/chrome/browser/sync/sync_setup_service_factory.h"
#import "ios/chrome/browser/sync/sync_setup_service_mock.h"
#import "ios/chrome/browser/ui/authentication/authentication_flow.h"
#import "ios/chrome/browser/ui/authentication/authentication_flow_performer.h"
#import "ios/chrome/browser/unified_consent/unified_consent_service_factory.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/public/provider/chrome/browser/signin/fake_chrome_identity.h"
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

namespace {
std::unique_ptr<KeyedService> CreateMockSyncService(
    web::BrowserState* context) {
  return std::make_unique<syncer::MockSyncService>();
}

std::unique_ptr<KeyedService> CreateFakeConsentAuditor(
    web::BrowserState* context) {
  return std::make_unique<consent_auditor::FakeConsentAuditor>();
}
}  // namespace

class UserSigninMediatorTest : public PlatformTest {
 public:
  UserSigninMediatorTest() : consent_string_ids_(ExpectedConsentStringIds()) {}

  void SetUp() override {
    PlatformTest::SetUp();
    identity_ = [FakeChromeIdentity identityWithEmail:@"foo1@gmail.com"
                                               gaiaID:@"foo1ID"
                                                 name:@"Fake Foo 1"];
    identity_service()->AddIdentity(identity_);

    TestChromeBrowserState::Builder builder;
    builder.AddTestingFactory(
        AuthenticationServiceFactory::GetInstance(),
        base::BindRepeating(
            &AuthenticationServiceFake::CreateAuthenticationService));
    builder.AddTestingFactory(ConsentAuditorFactory::GetInstance(),
                              base::BindRepeating(&CreateFakeConsentAuditor));
    builder.AddTestingFactory(ProfileSyncServiceFactory::GetInstance(),
                              base::BindRepeating(&CreateMockSyncService));
    builder.AddTestingFactory(
        SyncSetupServiceFactory::GetInstance(),
        base::BindRepeating(&SyncSetupServiceMock::CreateKeyedService));
    browser_state_ = builder.Build();

    SetAuthenticationFlow();

    mediator_delegate_mock_ =
        OCMStrictProtocolMock(@protocol(UserSigninMediatorDelegate));
    mediator_ = [[UserSigninMediator alloc]
        initWithAuthenticationService:authentication_service()
                      identityManager:identity_manager()
                       consentAuditor:consent_auditor()
                unifiedConsentService:unified_consent_service()
                     syncSetupService:sync_setup_service()];
    mediator_.delegate = mediator_delegate_mock_;

    fake_consent_auditor_ =
        static_cast<consent_auditor::FakeConsentAuditor*>(consent_auditor());
    sync_setup_service_mock_ =
        static_cast<SyncSetupServiceMock*>(sync_setup_service());
  }

  void TearDown() override {
    EXPECT_OCMOCK_VERIFY((id)mediator_delegate_mock_);
    EXPECT_OCMOCK_VERIFY((id)performer_mock_);
    EXPECT_OCMOCK_VERIFY((id)presenting_view_controller_mock_);
    PlatformTest::TearDown();
  }

  // Sets up the necessary mocks for authentication operations in
  // |authentication_flow_|.
  void SetAuthenticationFlow() {
    WebStateList* web_state_list = nullptr;
    browser_ =
        std::make_unique<TestBrowser>(browser_state_.get(), web_state_list);
    presenting_view_controller_mock_ =
        OCMStrictClassMock([UIViewController class]);
    performer_mock_ = OCMStrictClassMock([AuthenticationFlowPerformer class]);

    authentication_flow_ = [[AuthenticationFlow alloc]
                 initWithBrowser:browser_.get()
                        identity:identity_
                 shouldClearData:SHOULD_CLEAR_DATA_USER_CHOICE
                postSignInAction:POST_SIGNIN_ACTION_NONE
        presentingViewController:presenting_view_controller_mock_];
    [authentication_flow_ setPerformerForTesting:performer_mock_];
  }

  // Sets up the sign-in expectations for the AuthenticationFlowPerformer.
  void SetPerformerSigninExpectations() {
    OCMExpect([performer_mock_ fetchManagedStatus:browser_state_.get()
                                      forIdentity:identity_])
        .andDo(^(NSInvocation*) {
          [authentication_flow_ didFetchManagedStatus:nil];
        });
    OCMExpect([performer_mock_
                  shouldHandleMergeCaseForIdentity:identity_
                                      browserState:browser_state_.get()])
        .andReturn(NO);
    OCMExpect([performer_mock_ signInIdentity:identity_
                             withHostedDomain:nil
                               toBrowserState:browser_state_.get()])
        .andDo(^(NSInvocation*) {
          authentication_service()->SignIn(identity_);
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
                                      browserState:browser_state_.get()])
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

  ios::FakeChromeIdentityService* identity_service() {
    return ios::FakeChromeIdentityService::GetInstanceFromChromeProvider();
  }

  SyncSetupService* sync_setup_service() {
    return SyncSetupServiceFactory::GetForBrowserState(browser_state_.get());
  }

  unified_consent::UnifiedConsentService* unified_consent_service() {
    return UnifiedConsentServiceFactory::GetForBrowserState(
        browser_state_.get());
  }

 protected:
  // Needed for test browser state created by TestChromeBrowserState().
  web::WebTaskEnvironment task_environment_;
  FakeChromeIdentity* identity_ = nullptr;

  AuthenticationFlow* authentication_flow_ = nullptr;
  std::unique_ptr<Browser> browser_;
  std::unique_ptr<TestChromeBrowserState> browser_state_;
  consent_auditor::FakeConsentAuditor* fake_consent_auditor_ = nullptr;
  const std::vector<int> consent_string_ids_;

  UserSigninMediator* mediator_ = nullptr;

  id<UserSigninMediatorDelegate> mediator_delegate_mock_ = nil;
  AuthenticationFlowPerformer* performer_mock_ = nullptr;
  UIViewController* presenting_view_controller_mock_ = nullptr;
  SyncSetupServiceMock* sync_setup_service_mock_ = nullptr;
};

// Tests a successful authentication for a given identity.
TEST_F(UserSigninMediatorTest, AuthenticateWithIdentitySuccess) {
  SetPerformerSigninExpectations();

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
  ExpectConsent(IDS_IOS_ACCOUNT_UNIFIED_CONSENT_OK_BUTTON);
}

// Tests authenticating the identity when the settings link has been tapped.
TEST_F(UserSigninMediatorTest, AuthenticateWithSettingsLinkTapped) {
  SetPerformerSigninExpectations();

  // Retrieving coordinator data for the mediator delegate.
  OCMExpect(
      [mediator_delegate_mock_ userSigninMediatorGetConsentConfirmationId])
      .andReturn(IDS_IOS_ACCOUNT_UNIFIED_CONSENT_SETTINGS);
  OCMExpect([mediator_delegate_mock_ userSigninMediatorGetConsentStringIds])
      .andReturn(&consent_string_ids_);
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
  ExpectConsent(IDS_IOS_ACCOUNT_UNIFIED_CONSENT_SETTINGS);
}

// Tests authentication failure for a given identity.
TEST_F(UserSigninMediatorTest, AuthenticateWithIdentityError) {
  SetPerformerFailureExpectations();

  // Returns to sign-in flow.
  OCMExpect([mediator_delegate_mock_ userSigninMediatorSigninFailed]);

  [mediator_ authenticateWithIdentity:identity_
                   authenticationFlow:authentication_flow_];
  ExpectNoConsent();
}

// Tests a user sign-in operation cancel when authentication has not begun.
TEST_F(UserSigninMediatorTest, CancelAuthenticationNotInProgress) {
  // Sign-in result cancel.
  OCMExpect(
      [mediator_delegate_mock_ userSigninMediatorSigninFinishedWithResult:
                                   SigninCoordinatorResultCanceledByUser]);

  [mediator_ cancelSignin];
  ExpectNoConsent();
}

// Tests a user sign-in operation cancel when authentication is in progress.
TEST_F(UserSigninMediatorTest, CancelWithAuthenticationInProgress) {
  SetPerformerCancelAndDismissExpectations(/*animated=*/NO);

  // Unsuccessful sign-in completion updates the primary button.
  OCMExpect([mediator_delegate_mock_ userSigninMediatorSigninFailed]);

  [mediator_ authenticateWithIdentity:identity_
                   authenticationFlow:authentication_flow_];
  [mediator_ cancelSignin];
  ExpectNoConsent();
}

// Tests a user sign-in operation cancel and dismiss when authentication has not
// begun.
TEST_F(UserSigninMediatorTest, CancelAndDismissAuthenticationNotInProgress) {
  [mediator_ cancelAndDismissAuthenticationFlowAnimated:NO];
  ExpectNoConsent();
}

// Tests a user sign-in operation cancel and dismiss with animation when
// authentication is in progress.
TEST_F(UserSigninMediatorTest,
       CancelAndDismissAuthenticationInProgressWithAnimation) {
  SetPerformerCancelAndDismissExpectations(/*animated=*/YES);

  // Unsuccessful sign-in completion updates the primary button.
  OCMExpect([mediator_delegate_mock_ userSigninMediatorSigninFailed]);

  [mediator_ authenticateWithIdentity:identity_
                   authenticationFlow:authentication_flow_];
  [mediator_ cancelAndDismissAuthenticationFlowAnimated:YES];
  ExpectNoConsent();
}

// Tests a user sign-in operation cancel and dismiss without animation when
// authentication is in progress.
TEST_F(UserSigninMediatorTest,
       CancelAndDismissAuthenticationInProgressWithoutAnimation) {
  SetPerformerCancelAndDismissExpectations(/*animated=*/NO);

  // Unsuccessful sign-in completion updates the primary button.
  OCMExpect([mediator_delegate_mock_ userSigninMediatorSigninFailed]);

  [mediator_ authenticateWithIdentity:identity_
                   authenticationFlow:authentication_flow_];
  [mediator_ cancelAndDismissAuthenticationFlowAnimated:NO];
  ExpectNoConsent();
}
