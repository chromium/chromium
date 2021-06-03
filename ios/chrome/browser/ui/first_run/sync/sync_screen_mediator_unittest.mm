// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/first_run/sync/sync_screen_mediator.h"

#import "components/consent_auditor/fake_consent_auditor.h"
#import "components/sync/driver/mock_sync_service.h"
#import "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/main/test_browser.h"
#import "ios/chrome/browser/signin/authentication_service_factory.h"
#import "ios/chrome/browser/signin/authentication_service_fake.h"
#import "ios/chrome/browser/signin/identity_manager_factory.h"
#import "ios/chrome/browser/sync/consent_auditor_factory.h"
#import "ios/chrome/browser/sync/sync_service_factory.h"
#import "ios/chrome/browser/sync/sync_setup_service_factory.h"
#import "ios/chrome/browser/sync/sync_setup_service_mock.h"
#import "ios/chrome/browser/unified_consent/unified_consent_service_factory.h"
#import "ios/public/provider/chrome/browser/signin/fake_chrome_identity.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/gtest_support.h"

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

// This class provides a hook for platform-specific operations across
// SyncScreenMediator unit tests.
class SyncScreenMediatorTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();

    TestChromeBrowserState::Builder builder;
    builder.AddTestingFactory(
        AuthenticationServiceFactory::GetInstance(),
        base::BindRepeating(
            &AuthenticationServiceFake::CreateAuthenticationService));
    builder.AddTestingFactory(ConsentAuditorFactory::GetInstance(),
                              base::BindRepeating(&CreateFakeConsentAuditor));
    builder.AddTestingFactory(SyncServiceFactory::GetInstance(),
                              base::BindRepeating(&CreateMockSyncService));
    builder.AddTestingFactory(
        SyncSetupServiceFactory::GetInstance(),
        base::BindRepeating(&SyncSetupServiceMock::CreateKeyedService));
    browser_state_ = builder.Build();

    identity_ = [FakeChromeIdentity identityWithEmail:@"test@email.com"
                                               gaiaID:@"gaiaID"
                                                 name:@"Test Name"];

    // Identity services.
    AuthenticationService* authentication_service =
        AuthenticationServiceFactory::GetForBrowserState(browser_state_.get());
    authentication_service->SignIn(identity_);
    signin::IdentityManager* identity_manager =
        IdentityManagerFactory::GetForBrowserState(browser_state_.get());
    consent_auditor::ConsentAuditor* consent_auditor =
        ConsentAuditorFactory::GetForBrowserState(browser_state_.get());
    unified_consent::UnifiedConsentService* unified_consent_service =
        UnifiedConsentServiceFactory::GetForBrowserState(browser_state_.get());
    SyncSetupService* sync_setup_service =
        SyncSetupServiceFactory::GetForBrowserState(browser_state_.get());

    mediator_ = [[SyncScreenMediator alloc]
        initWithAuthenticationService:authentication_service
                      identityManager:identity_manager
                       consentAuditor:consent_auditor
                unifiedConsentService:unified_consent_service
                     syncSetupService:sync_setup_service];

    sync_setup_service_mock_ =
        static_cast<SyncSetupServiceMock*>(sync_setup_service);
  }

  void TearDown() override { PlatformTest::TearDown(); }

  web::WebTaskEnvironment task_environment_;
  SyncScreenMediator* mediator_;
  FakeChromeIdentity* identity_;
  std::unique_ptr<Browser> browser_;
  std::unique_ptr<TestChromeBrowserState> browser_state_;
  SyncSetupServiceMock* sync_setup_service_mock_;
};

// Tests that the FirstSetupComplete flag is turned on after the mediator has
// started Sync.
TEST_F(SyncScreenMediatorTest, TestStartSyncService) {
  EXPECT_CALL(
      *sync_setup_service_mock_,
      SetFirstSetupComplete(syncer::SyncFirstSetupCompleteSource::BASIC_FLOW));
  [mediator_ startSync];
}
