// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/first_run/sync/sync_screen_mediator.h"

#import "components/consent_auditor/fake_consent_auditor.h"
#import "components/prefs/pref_service.h"
#import "components/sync/test/mock_sync_service.h"
#import "components/unified_consent/pref_names.h"
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
#import "ios/chrome/browser/ui/first_run/sync/sync_screen_consumer.h"
#import "ios/chrome/browser/ui/first_run/sync/sync_screen_mediator_delegate.h"
#import "ios/chrome/browser/unified_consent/unified_consent_service_factory.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/public/provider/chrome/browser/signin/fake_chrome_identity_service.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// Fake implementing the consumer protocol.
@interface FakeSyncScreenConsumer : NSObject <SyncScreenConsumer>

@property(nonatomic, assign) BOOL UIEnabled;

@end

@implementation FakeSyncScreenConsumer

@end

// This class provides a hook for platform-specific operations across
// SyncScreenMediator unit tests.
class SyncScreenMediatorTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();

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

    identity_ = [FakeSystemIdentity fakeIdentity1];
    // Identity services.
    ios::FakeChromeIdentityService* identity_service_ =
        ios::FakeChromeIdentityService::GetInstanceFromChromeProvider();
    identity_service_->AddIdentity(identity_);
    AuthenticationService* authentication_service =
        AuthenticationServiceFactory::GetForBrowserState(browser_state_.get());
    authentication_service->SignIn(identity_);
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

    mediator_ = [[SyncScreenMediator alloc]
        initWithAuthenticationService:authentication_service
                      identityManager:identity_manager
                accountManagerService:account_manager_service
                       consentAuditor:consent_auditor
                     syncSetupService:sync_setup_service
                unifiedConsentService:UnifiedConsentServiceFactory::
                                          GetForBrowserState(
                                              browser_state_.get())
                          syncService:sync_service];

    consumer_ = [[FakeSyncScreenConsumer alloc] init];

    sync_setup_service_mock_ =
        static_cast<SyncSetupServiceMock*>(sync_setup_service);
    sync_service_mock_ = static_cast<syncer::MockSyncService*>(sync_service);
  }

  void TearDown() override {
    PlatformTest::TearDown();
    [mediator_ disconnect];
  }

  web::WebTaskEnvironment task_environment_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  SyncScreenMediator* mediator_;
  FakeSystemIdentity* identity_;
  FakeSyncScreenConsumer* consumer_;
  std::unique_ptr<TestChromeBrowserState> browser_state_;
  SyncSetupServiceMock* sync_setup_service_mock_;
  syncer::MockSyncService* sync_service_mock_;
};

// Tests that the FirstSetupComplete flag is turned on after the mediator has
// started Sync.
TEST_F(SyncScreenMediatorTest, TestStartSyncService) {
  NSMutableArray* consentStringIDs = [[NSMutableArray alloc] init];
  [consentStringIDs addObject:@1];
  [consentStringIDs addObject:@2];
  [consentStringIDs addObject:@3];

  id mock_flow = OCMClassMock([AuthenticationFlow class]);
  OCMStub([mock_flow startSignInWithCompletion:[OCMArg any]])
      .andDo(^(NSInvocation* invocation) {
        __weak signin_ui::CompletionCallback block;
        [invocation getArgument:&block atIndex:2];
        block(YES);
      });

  EXPECT_CALL(
      *sync_setup_service_mock_,
      SetFirstSetupComplete(syncer::SyncFirstSetupCompleteSource::BASIC_FLOW));
  [mediator_ startSyncWithConfirmationID:1
                              consentIDs:consentStringIDs
                      authenticationFlow:mock_flow
       advancedSyncSettingsLinkWasTapped:NO];
}

// Tests the authentication flow for the mediator.
TEST_F(SyncScreenMediatorTest, TestAuthenticationFlow) {
  mediator_.consumer = consumer_;
  consumer_.UIEnabled = YES;
  TestBrowser browser(browser_state_.get());

  id mock_delegate = OCMProtocolMock(@protocol(SyncScreenMediatorDelegate));
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
  EXPECT_TRUE(consumer_.UIEnabled);

  [mediator_ startSyncWithConfirmationID:1
                              consentIDs:@[ @(1) ]
                      authenticationFlow:mock_flow
       advancedSyncSettingsLinkWasTapped:NO];

  EXPECT_FALSE(consumer_.UIEnabled);
  ASSERT_NE(nil, completion);

  OCMExpect(
      [mock_delegate syncScreenMediatorDidSuccessfulyFinishSignin:mediator_]);

  EXPECT_FALSE(browser_state_->GetPrefs()->GetBoolean(
      unified_consent::prefs::kUrlKeyedAnonymizedDataCollectionEnabled));

  // Simulate the signin completion being successful.
  completion(YES);

  EXPECT_TRUE(browser_state_->GetPrefs()->GetBoolean(
      unified_consent::prefs::kUrlKeyedAnonymizedDataCollectionEnabled));
  EXPECT_TRUE(consumer_.UIEnabled);
  EXPECT_OCMOCK_VERIFY(mock_delegate);
}
