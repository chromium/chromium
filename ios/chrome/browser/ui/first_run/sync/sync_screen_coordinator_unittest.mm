// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/first_run/sync/sync_screen_coordinator.h"

#import "components/sync/driver/mock_sync_service.h"
#import "ios/chrome/app/application_delegate/app_state.h"
#import "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/main/test_browser.h"
#import "ios/chrome/browser/policy/policy_watcher_browser_agent.h"
#import "ios/chrome/browser/signin/authentication_service_factory.h"
#import "ios/chrome/browser/signin/authentication_service_fake.h"
#import "ios/chrome/browser/sync/sync_service_factory.h"
#import "ios/chrome/browser/sync/sync_setup_service.h"
#import "ios/chrome/browser/sync/sync_setup_service_factory.h"
#import "ios/chrome/browser/sync/sync_setup_service_mock.h"
#import "ios/chrome/browser/ui/first_run/first_run_screen_delegate.h"
#import "ios/chrome/browser/ui/main/scene_state.h"
#import "ios/chrome/browser/ui/main/scene_state_browser_agent.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/public/provider/chrome/browser/signin/fake_chrome_identity.h"
#import "ios/web/public/test/web_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"
#include "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
std::unique_ptr<KeyedService> CreateMockSyncService(
    web::BrowserState* context) {
  return std::make_unique<syncer::MockSyncService>();
}
}  // namespace

// This class provides a hook for platform-specific operations across
// SyncScreenCoordinator unit tests.
class SyncScreenCoordinatorTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();

    TestChromeBrowserState::Builder builder;
    builder.AddTestingFactory(SyncServiceFactory::GetInstance(),
                              base::BindRepeating(&CreateMockSyncService));
    builder.AddTestingFactory(
        AuthenticationServiceFactory::GetInstance(),
        base::BindRepeating(
            &AuthenticationServiceFake::CreateAuthenticationService));
    builder.AddTestingFactory(
        SyncSetupServiceFactory::GetInstance(),
        base::BindRepeating(&SyncSetupServiceMock::CreateKeyedService));
    browser_state_ = builder.Build();
    browser_ = std::make_unique<TestBrowser>(browser_state_.get());
    PolicyWatcherBrowserAgent::CreateForBrowser(browser_.get());

    sync_setup_service_mock_ = static_cast<SyncSetupServiceMock*>(
        SyncSetupServiceFactory::GetForBrowserState(browser_state_.get()));
    sync_service_mock_ = static_cast<syncer::MockSyncService*>(
        SyncServiceFactory::GetForBrowserState(browser_state_.get()));
    auth_service_ = static_cast<AuthenticationServiceFake*>(
        AuthenticationServiceFactory::GetInstance()->GetForBrowserState(
            browser_state_.get()));

    // Mock SceneStateBrowserAgent.
    id appStateMock = [OCMockObject mockForClass:[AppState class]];
    [[[appStateMock stub] andReturnValue:@(InitStageFinal)] initStage];
    SceneStateBrowserAgent::CreateForBrowser(
        browser_.get(), [[SceneState alloc] initWithAppState:appStateMock]);

    navigationController_ = [[UINavigationController alloc] init];
    delegate_ = OCMStrictProtocolMock(@protocol(FirstRunScreenDelegate));

    coordinator_ = [[SyncScreenCoordinator alloc]
        initWithBaseNavigationController:navigationController_
                                 browser:browser_.get()
                                delegate:delegate_];
  }

  void TearDown() override {
    [coordinator_ stop];
    coordinator_ = nil;

    PlatformTest::TearDown();
  }

  web::WebTaskEnvironment task_environment_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  std::unique_ptr<Browser> browser_;
  std::unique_ptr<TestChromeBrowserState> browser_state_;
  AuthenticationServiceFake* auth_service_ = nullptr;
  syncer::MockSyncService* sync_service_mock_ = nullptr;
  SyncSetupServiceMock* sync_setup_service_mock_ = nullptr;
  SyncScreenCoordinator* coordinator_;
  UINavigationController* navigationController_;
  id delegate_;
};

// Tests that the delegate is not called when there is a user identity.
TEST_F(SyncScreenCoordinatorTest, TestStart) {
  FakeChromeIdentity* identity = [FakeChromeIdentity identityWithEmail:@"email"
                                                                gaiaID:@"gaiaID"
                                                                  name:@"name"];

  auth_service_->SignIn(identity, nil);

  // The delegate is a strict mock, it will fail if it calls it.
  [coordinator_ start];
}

// Tests that calling the delegate immidiately to stop the coordinator when
// there's no user identity.
TEST_F(SyncScreenCoordinatorTest, TestStartWithoutIdentity) {
  OCMExpect([delegate_ screenWillFinishPresenting]);
  [coordinator_ start];

  EXPECT_OCMOCK_VERIFY(delegate_);
}

// Tests that calling the delegate immidiately to stop the coordinator when
// the user is already syncing.
TEST_F(SyncScreenCoordinatorTest, TestStartWithSyncActivated) {
  ON_CALL(*sync_service_mock_->GetMockUserSettings(), IsFirstSetupComplete())
      .WillByDefault(testing::Return(true));

  FakeChromeIdentity* identity = [FakeChromeIdentity identityWithEmail:@"email"
                                                                gaiaID:@"gaiaID"
                                                                  name:@"name"];

  auth_service_->SignIn(identity, nil);

  OCMExpect([delegate_ screenWillFinishPresenting]);
  [coordinator_ start];

  EXPECT_OCMOCK_VERIFY(delegate_);
}

// Tests that calling the delegate immidiately to stop the coordinator when
// sync is disabled by policy.
TEST_F(SyncScreenCoordinatorTest, TestStartWithSyncPolicyDisabled) {
  ON_CALL(*sync_service_mock_, GetDisableReasons())
      .WillByDefault(testing::Return(
          syncer::SyncService::DISABLE_REASON_ENTERPRISE_POLICY));

  FakeChromeIdentity* identity = [FakeChromeIdentity identityWithEmail:@"email"
                                                                gaiaID:@"gaiaID"
                                                                  name:@"name"];

  auth_service_->SignIn(identity, nil);

  OCMExpect([delegate_ screenWillFinishPresenting]);
  [coordinator_ start];

  EXPECT_OCMOCK_VERIFY(delegate_);
}
