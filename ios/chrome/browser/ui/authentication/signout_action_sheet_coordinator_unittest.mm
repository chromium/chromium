// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/signout_action_sheet_coordinator.h"

#import <UIKit/UIKit.h>

#import "base/mac/foundation_util.h"
#include "base/test/task_environment.h"
#include "components/prefs/pref_service.h"
#include "components/sync/driver/mock_sync_service.h"
#import "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/main/test_browser.h"
#import "ios/chrome/browser/policy/policy_util.h"
#include "ios/chrome/browser/pref_names.h"
#import "ios/chrome/browser/signin/authentication_service_factory.h"
#import "ios/chrome/browser/signin/authentication_service_fake.h"
#include "ios/chrome/browser/sync/sync_service_factory.h"
#include "ios/chrome/browser/sync/sync_setup_service_factory.h"
#include "ios/chrome/browser/sync/sync_setup_service_mock.h"
#include "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/chrome/test/scoped_key_window.h"
#import "ios/public/provider/chrome/browser/signin/fake_chrome_identity.h"
#import "ios/public/provider/chrome/browser/signin/fake_chrome_identity_service.h"
#include "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#include "third_party/ocmock/gtest_support.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/strings/grit/ui_strings.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

std::unique_ptr<KeyedService> BuildMockSyncService(web::BrowserState* context) {
  return std::make_unique<syncer::MockSyncService>();
}

}  // namespace

class SignoutActionSheetCoordinatorTest : public PlatformTest {
 public:
  SignoutActionSheetCoordinatorTest() {
    view_controller_ = [[UIViewController alloc] init];
    [scoped_key_window_.Get() setRootViewController:view_controller_];
  }

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
    builder.AddTestingFactory(SyncServiceFactory::GetInstance(),
                              base::BindRepeating(&BuildMockSyncService));
    builder.AddTestingFactory(
        SyncSetupServiceFactory::GetInstance(),
        base::BindRepeating(&SyncSetupServiceMock::CreateKeyedService));
    browser_state_ = builder.Build();
    browser_ = std::make_unique<TestBrowser>(browser_state_.get());

    sync_service_mock_ = static_cast<syncer::MockSyncService*>(
        SyncServiceFactory::GetForBrowserState(browser_state_.get()));
  }

  // Identity services.
  AuthenticationService* authentication_service() {
    return AuthenticationServiceFactory::GetForBrowserState(
        browser_state_.get());
  }

  ios::FakeChromeIdentityService* identity_service() {
    return ios::FakeChromeIdentityService::GetInstanceFromChromeProvider();
  }

  // Sign-out coordinator.
  SignoutActionSheetCoordinator* CreateCoordinator() {
    signout_coordinator_ = [[SignoutActionSheetCoordinator alloc]
        initWithBaseViewController:view_controller_
                           browser:browser_.get()
                              rect:view_controller_.view.frame
                              view:view_controller_.view];
    signout_coordinator_.completion = ^(BOOL success) {
    };
    return signout_coordinator_;
  }

  PrefService* GetLocalState() { return scoped_testing_local_state_.Get(); }

 protected:
  // Needed for test browser state created by TestChromeBrowserState().
  base::test::TaskEnvironment task_environment_;

  IOSChromeScopedTestingLocalState scoped_testing_local_state_;

  SignoutActionSheetCoordinator* signout_coordinator_ = nullptr;
  ScopedKeyWindow scoped_key_window_;
  UIViewController* view_controller_ = nullptr;
  std::unique_ptr<Browser> browser_;
  std::unique_ptr<TestChromeBrowserState> browser_state_;
  FakeChromeIdentity* identity_ = nullptr;

  syncer::MockSyncService* sync_service_mock_ = nullptr;
};

// Tests that a signed-in user with Sync enabled will have an action sheet with
// a sign-out title.
TEST_F(SignoutActionSheetCoordinatorTest, SignedInUserWithSync) {
  authentication_service()->SignIn(identity_, nil);
  ON_CALL(*sync_service_mock_->GetMockUserSettings(), IsFirstSetupComplete())
      .WillByDefault(testing::Return(true));

  SignoutActionSheetCoordinator* signout_coordinator = CreateCoordinator();
  [signout_coordinator start];

  ASSERT_NE(nil, signout_coordinator.title);
}

// Tests that a signed-in user with Sync disabled will have an action sheet with
// no title.
TEST_F(SignoutActionSheetCoordinatorTest, SignedInUserWithoutSync) {
  authentication_service()->SignIn(identity_, nil);
  ON_CALL(*sync_service_mock_->GetMockUserSettings(), IsFirstSetupComplete())
      .WillByDefault(testing::Return(false));

  SignoutActionSheetCoordinator* signout_coordinator = CreateCoordinator();
  [signout_coordinator start];

  ASSERT_EQ(nil, signout_coordinator.title);
}

// Tests that a signed-in user with the forced sign-in policy enabled will have
// an action sheet with a title and a message.
TEST_F(SignoutActionSheetCoordinatorTest, SignedInUserWithForcedSignin) {
  // Enable forced sign-in.
  GetLocalState()->SetInteger(prefs::kBrowserSigninPolicy,
                              static_cast<int>(BrowserSigninMode::kForced));

  authentication_service()->SignIn(identity_, nil);
  ON_CALL(*sync_service_mock_->GetMockUserSettings(), IsFirstSetupComplete())
      .WillByDefault(testing::Return(false));

  SignoutActionSheetCoordinator* signout_coordinator = CreateCoordinator();
  [signout_coordinator start];

  ASSERT_NE(nil, signout_coordinator.title);
  ASSERT_NE(nil, signout_coordinator.message);
}

// Tests that a signed-in managed user with Sync enabled will have an action
// sheet with a sign-out title.
TEST_F(SignoutActionSheetCoordinatorTest, SignedInManagedUserWithSync) {
  authentication_service()->SignIn(identity_, nil);
  ON_CALL(*sync_service_mock_->GetMockUserSettings(), IsFirstSetupComplete())
      .WillByDefault(testing::Return(true));

  SignoutActionSheetCoordinator* signout_coordinator = CreateCoordinator();
  [signout_coordinator start];

  ASSERT_NE(nil, signout_coordinator.title);
}

// Tests that a signed-in managed user with Sync disabled will have an action
// sheet with no title.
TEST_F(SignoutActionSheetCoordinatorTest, SignedInManagedUserWithoutSync) {
  authentication_service()->SignIn(identity_, nil);
  ON_CALL(*sync_service_mock_->GetMockUserSettings(), IsFirstSetupComplete())
      .WillByDefault(testing::Return(false));

  SignoutActionSheetCoordinator* signout_coordinator = CreateCoordinator();
  [signout_coordinator start];

  ASSERT_EQ(nil, signout_coordinator.title);
}
