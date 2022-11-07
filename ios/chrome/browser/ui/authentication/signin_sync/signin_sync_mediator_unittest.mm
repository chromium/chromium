// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/signin_sync/signin_sync_mediator.h"

#import "base/run_loop.h"
#import "base/test/ios/wait_util.h"
#import "components/consent_auditor/consent_auditor.h"
#import "components/consent_auditor/fake_consent_auditor.h"
#import "components/signin/public/identity_manager/identity_manager.h"
#import "components/sync/driver/sync_service.h"
#import "components/sync/test/mock_sync_service.h"
#import "components/unified_consent/pref_names.h"
#import "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/consent_auditor/consent_auditor_factory.h"
#import "ios/chrome/browser/consent_auditor/consent_auditor_test_utils.h"
#import "ios/chrome/browser/main/test_browser.h"
#import "ios/chrome/browser/signin/authentication_service.h"
#import "ios/chrome/browser/signin/authentication_service_factory.h"
#import "ios/chrome/browser/signin/chrome_account_manager_service.h"
#import "ios/chrome/browser/signin/chrome_account_manager_service_factory.h"
#import "ios/chrome/browser/signin/constants.h"
#import "ios/chrome/browser/signin/fake_authentication_service_delegate.h"
#import "ios/chrome/browser/signin/fake_system_identity.h"
#import "ios/chrome/browser/signin/identity_manager_factory.h"
#import "ios/chrome/browser/signin/signin_util.h"
#import "ios/chrome/browser/sync/mock_sync_service_utils.h"
#import "ios/chrome/browser/sync/sync_service_factory.h"
#import "ios/chrome/browser/sync/sync_setup_service.h"
#import "ios/chrome/browser/sync/sync_setup_service_factory.h"
#import "ios/chrome/browser/sync/sync_setup_service_mock.h"
#import "ios/chrome/browser/ui/authentication/authentication_flow.h"
#import "ios/chrome/browser/ui/authentication/authentication_flow_performer.h"
#import "ios/chrome/browser/ui/authentication/signin_sync/signin_sync_consumer.h"
#import "ios/chrome/browser/ui/authentication/signin_sync/signin_sync_mediator_delegate.h"
#import "ios/chrome/browser/unified_consent/unified_consent_service_factory.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/public/provider/chrome/browser/signin/fake_chrome_identity_service.h"
#import "ios/public/provider/chrome/browser/test_chrome_browser_provider.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using base::test::ios::kWaitForActionTimeout;
using base::test::ios::WaitUntilConditionOrTimeout;

// Fake implementing the consumer protocol.
@interface FakeSigninSyncConsumer : NSObject <SigninSyncConsumer>

@property(nonatomic, assign) BOOL hidden;
@property(nonatomic, copy) NSString* userName;
@property(nonatomic, copy) NSString* email;
@property(nonatomic, copy) NSString* givenName;
@property(nonatomic, strong) UIImage* avatar;
@property(nonatomic, assign) BOOL UIWasEnabled;

@end

@implementation FakeSigninSyncConsumer

- (void)noIdentityAvailable {
  self.hidden = YES;
}

- (void)setSelectedIdentityUserName:(NSString*)userName
                              email:(NSString*)email
                          givenName:(NSString*)givenName
                             avatar:(UIImage*)avatar {
  self.userName = userName;
  self.email = email;
  self.givenName = givenName;
  self.avatar = avatar;
}

- (void)setUIEnabled:(BOOL)UIEnabled {
  self.UIWasEnabled = UIEnabled;
}

- (void)setActionToDone {
  self.UIWasEnabled = YES;
}

@end

class SigninSyncMediatorTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();
    identity_service_ =
        ios::FakeChromeIdentityService::GetInstanceFromChromeProvider();
    identity_ = [FakeSystemIdentity fakeIdentity1];
    identity_service_->AddIdentity(identity_);
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
    AuthenticationService* authentication_service =
        AuthenticationServiceFactory::GetForBrowserState(browser_state_.get());
    signin::IdentityManager* identity_manager =
        IdentityManagerFactory::GetForBrowserState(browser_state_.get());
    consent_auditor::ConsentAuditor* consent_auditor =
        ConsentAuditorFactory::GetForBrowserState(browser_state_.get());
    SyncSetupService* sync_setup_service =
        SyncSetupServiceFactory::GetForBrowserState(browser_state_.get());
    syncer::SyncService* sync_service =
        SyncServiceFactory::GetForBrowserState(browser_state_.get());
    ChromeAccountManagerService* account_manager_service =
        ChromeAccountManagerServiceFactory::GetForBrowserState(
            browser_state_.get());

    mediator_ = [[SigninSyncMediator alloc]
        initWithAuthenticationService:authentication_service
                      identityManager:identity_manager
                accountManagerService:account_manager_service
                       consentAuditor:consent_auditor
                     syncSetupService:sync_setup_service
                unifiedConsentService:UnifiedConsentServiceFactory::
                                          GetForBrowserState(
                                              browser_state_.get())
                          syncService:(syncer::SyncService*)sync_service];

    consumer_ = [[FakeSigninSyncConsumer alloc] init];

    sync_setup_service_mock_ =
        static_cast<SyncSetupServiceMock*>(sync_setup_service);
    sync_service_mock_ = static_cast<syncer::MockSyncService*>(sync_service);
  }

  void TearDown() override {
    PlatformTest::TearDown();
    [mediator_ disconnect];
    browser_state_.reset();
    identity_service_->WaitForServiceCallbacksToComplete();
  }

  void SetIdentityService(
      std::unique_ptr<ios::FakeChromeIdentityService> service) {
    // Run all callbacks on the old service.
    identity_service_->WaitForServiceCallbacksToComplete();

    // Update the service in the browser provider.
    identity_service_ = service.get();
    ios::TestChromeBrowserProvider::GetTestProvider()
        .SetChromeIdentityServiceForTesting(std::move(service));
  }

  web::WebTaskEnvironment task_environment_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;

  SigninSyncMediator* mediator_;
  std::unique_ptr<ChromeBrowserState> browser_state_;
  ios::FakeChromeIdentityService* identity_service_;
  FakeSigninSyncConsumer* consumer_;
  id<SystemIdentity> identity_;
  SyncSetupServiceMock* sync_setup_service_mock_;
  syncer::MockSyncService* sync_service_mock_;
};

// Tests that setting the consumer after the selected identity is set is
// correctly notifying the consumer.
TEST_F(SigninSyncMediatorTest, TestSettingConsumerWithExistingIdentity) {
  mediator_.selectedIdentity = identity_;
  mediator_.consumer = consumer_;

  EXPECT_EQ(identity_.userEmail, consumer_.email);
  EXPECT_EQ(identity_.userFullName, consumer_.userName);
  EXPECT_FALSE(consumer_.hidden);
  UIImage* avatar = consumer_.avatar;
  EXPECT_NE(nil, avatar);
  CGSize expected_size =
      GetSizeForIdentityAvatarSize(IdentityAvatarSize::Regular);
  EXPECT_TRUE(CGSizeEqualToSize(expected_size, avatar.size));
}

// Tests that the consumer is correctly updated when the selected identity is
// updated.
TEST_F(SigninSyncMediatorTest, TestUpdatingSelectedIdentity) {
  mediator_.consumer = consumer_;

  EXPECT_EQ(nil, consumer_.email);
  EXPECT_EQ(nil, consumer_.userName);
  // True because the selected identity is nil.
  EXPECT_TRUE(consumer_.hidden);
  EXPECT_EQ(nil, consumer_.avatar);

  consumer_.hidden = NO;
  mediator_.selectedIdentity = identity_;

  EXPECT_EQ(identity_.userEmail, consumer_.email);
  EXPECT_EQ(identity_.userFullName, consumer_.userName);
  EXPECT_FALSE(consumer_.hidden);
  UIImage* avatar = consumer_.avatar;
  EXPECT_NE(nil, avatar);
  CGSize expected_size =
      GetSizeForIdentityAvatarSize(IdentityAvatarSize::Regular);
  EXPECT_TRUE(CGSizeEqualToSize(expected_size, avatar.size));
}

// Tests IdentityService observations of the identity list.
TEST_F(SigninSyncMediatorTest, TestIdentityListChanged) {
  mediator_.consumer = consumer_;

  EXPECT_EQ(nil, consumer_.email);
  EXPECT_EQ(nil, consumer_.userName);
  // True because the selected identity is nil.
  EXPECT_TRUE(consumer_.hidden);
  EXPECT_EQ(nil, consumer_.avatar);

  consumer_.hidden = NO;

  // Adding an identity is selecting it.
  identity_service_->AddIdentity(identity_);

  EXPECT_EQ(identity_.userEmail, consumer_.email);
  EXPECT_EQ(identity_.userFullName, consumer_.userName);
  EXPECT_FALSE(consumer_.hidden);
  UIImage* avatar = consumer_.avatar;
  EXPECT_NE(nil, avatar);
  CGSize expected_size =
      GetSizeForIdentityAvatarSize(IdentityAvatarSize::Regular);
  EXPECT_TRUE(CGSizeEqualToSize(expected_size, avatar.size));

  // Removing all the identity is resetting the selected identity.
  __block bool callback_done = false;
  identity_service_->ForgetIdentity(identity_, ^(NSError*) {
    callback_done = true;
  });
  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForActionTimeout, ^bool {
    return callback_done;
  }));

  EXPECT_TRUE(consumer_.hidden);
}

// Tests BrowserProvider observation of the identity service.
TEST_F(SigninSyncMediatorTest, TestProfileUpdate) {
  mediator_.selectedIdentity = identity_;
  mediator_.consumer = consumer_;

  ASSERT_EQ(identity_.userEmail, consumer_.email);
  ASSERT_EQ(identity_.userFullName, consumer_.userName);

  NSString* email = @"second@email.com";
  NSString* name = @"Second identity";
  FakeSystemIdentity* second_identity =
      [FakeSystemIdentity identityWithEmail:email
                                     gaiaID:@"second gaiaID"
                                       name:name];
  std::unique_ptr<ios::FakeChromeIdentityService> second_service_unique =
      std::make_unique<ios::FakeChromeIdentityService>();
  ios::FakeChromeIdentityService* second_service = second_service_unique.get();
  SetIdentityService(std::move(second_service_unique));

  second_service->AddIdentity(second_identity);

  EXPECT_EQ(email, consumer_.email);
  EXPECT_EQ(name, consumer_.userName);
  // Get the avatar before the fetch (the default avatar).
  UIImage* default_avatar = consumer_.avatar;
  EXPECT_NE(nil, default_avatar);

  // Wait for the avatar to be fetched.
  second_service->WaitForServiceCallbacksToComplete();

  NSString* updated_email = @"updated@email.com";
  NSString* updated_name = @"Second - Updated";

  second_identity.userEmail = updated_email;
  second_identity.userFullName = updated_name;

  // The name shouldn't have changed yet.
  EXPECT_EQ(email, consumer_.email);
  EXPECT_EQ(name, consumer_.userName);

  // Triggering the update is updating the consumer.
  second_service->TriggerIdentityUpdateNotification(second_identity);

  EXPECT_EQ(updated_email, consumer_.email);
  EXPECT_EQ(updated_name, consumer_.userName);
  // With the notification the real avatar is expected instead of the default
  // avatar.
  UIImage* real_avatar = consumer_.avatar;
  EXPECT_NE(default_avatar, real_avatar);
}

TEST_F(SigninSyncMediatorTest, TestStartSyncService) {
  mediator_.selectedIdentity = identity_;
  mediator_.consumer = consumer_;

  NSMutableArray* consentStringIDs = [[NSMutableArray alloc] init];
  [consentStringIDs addObject:@1];
  [consentStringIDs addObject:@2];
  [consentStringIDs addObject:@3];

  AuthenticationService* authentication_service =
      AuthenticationServiceFactory::GetForBrowserState(browser_state_.get());
  id mock_flow = OCMClassMock([AuthenticationFlow class]);
  OCMStub([mock_flow startSignInWithCompletion:[OCMArg any]])
      .andDo(^(NSInvocation* invocation) {
        __weak signin_ui::CompletionCallback block;
        [invocation getArgument:&block atIndex:2];
        authentication_service->SignIn(identity_);
        block(YES);
      });

  EXPECT_CALL(
      *sync_setup_service_mock_,
      SetFirstSetupComplete(syncer::SyncFirstSetupCompleteSource::BASIC_FLOW));
  [mediator_ startSyncWithConfirmationID:1
                              consentIDs:consentStringIDs
                      authenticationFlow:mock_flow];
}

// Tests the authentication flow for the mediator.
TEST_F(SigninSyncMediatorTest, TestAuthenticationFlow) {
  mediator_.consumer = consumer_;
  mediator_.selectedIdentity = identity_;
  consumer_.UIEnabled = YES;
  // TestBrowser browser;

  id mock_delegate = OCMProtocolMock(@protocol(SigninSyncMediatorDelegate));
  id mock_flow = OCMClassMock([AuthenticationFlow class]);

  mediator_.delegate = mock_delegate;

  __block signin_ui::CompletionCallback completion = nil;

  OCMStub([mock_flow startSignInWithCompletion:[OCMArg any]])
      .andDo(^(NSInvocation* invocation) {
        __weak signin_ui::CompletionCallback block;
        [invocation getArgument:&block atIndex:2];
        completion = [block copy];
      });

  EXPECT_EQ(nil, completion);
  EXPECT_TRUE(consumer_.UIWasEnabled);

  [mediator_ startSyncWithConfirmationID:1
                              consentIDs:@[ @(1) ]
                      authenticationFlow:mock_flow];

  EXPECT_FALSE(consumer_.UIWasEnabled);
  ASSERT_NE(nil, completion);

  OCMExpect(
      [mock_delegate signinSyncMediatorDidSuccessfulyFinishSignin:mediator_]);

  EXPECT_FALSE(browser_state_->GetPrefs()->GetBoolean(
      unified_consent::prefs::kUrlKeyedAnonymizedDataCollectionEnabled));

  AuthenticationService* authentication_service =
      AuthenticationServiceFactory::GetForBrowserState(browser_state_.get());
  authentication_service->SignIn(identity_);
  // Simulate the signin completion being successful.
  completion(YES);

  EXPECT_TRUE(browser_state_->GetPrefs()->GetBoolean(
      unified_consent::prefs::kUrlKeyedAnonymizedDataCollectionEnabled));
  EXPECT_TRUE(consumer_.UIWasEnabled);
  EXPECT_OCMOCK_VERIFY(mock_delegate);
}
