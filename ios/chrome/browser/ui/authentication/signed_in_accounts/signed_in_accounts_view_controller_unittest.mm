// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/signed_in_accounts/signed_in_accounts_view_controller.h"

#import <memory>

#import "base/bind.h"
#import "base/strings/sys_string_conversions.h"
#import "components/signin/public/identity_manager/identity_test_environment.h"
#import "components/variations/scoped_variations_ids_provider.h"
#import "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/signin/authentication_service.h"
#import "ios/chrome/browser/signin/authentication_service_factory.h"
#import "ios/chrome/browser/signin/chrome_account_manager_service.h"
#import "ios/chrome/browser/signin/chrome_account_manager_service_factory.h"
#import "ios/chrome/browser/signin/fake_authentication_service_delegate.h"
#import "ios/chrome/browser/signin/fake_system_identity.h"
#import "ios/chrome/browser/sync/mock_sync_service_utils.h"
#import "ios/chrome/browser/sync/sync_service_factory.h"
#import "ios/chrome/browser/sync/sync_setup_service_factory.h"
#import "ios/chrome/browser/sync/sync_setup_service_mock.h"
#import "ios/chrome/test/block_cleanup_test.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/public/provider/chrome/browser/signin/fake_chrome_identity_service.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

class SignedInAccountsViewControllerTest : public BlockCleanupTest {
 public:
  SignedInAccountsViewControllerTest() : identity_test_env_() {}

  void SetUp() override {
    BlockCleanupTest::SetUp();
    identity_service_ =
        ios::FakeChromeIdentityService::GetInstanceFromChromeProvider();
    identity_service_->AddIdentity([FakeSystemIdentity fakeIdentity1]);

    TestChromeBrowserState::Builder builder;
    builder.AddTestingFactory(
        SyncSetupServiceFactory::GetInstance(),
        base::BindRepeating(&SyncSetupServiceMock::CreateKeyedService));
    builder.AddTestingFactory(
        AuthenticationServiceFactory::GetInstance(),
        AuthenticationServiceFactory::GetDefaultFactory());
    builder.AddTestingFactory(SyncServiceFactory::GetInstance(),
                              base::BindRepeating(&CreateMockSyncService));
    browser_state_ = builder.Build();
    AuthenticationServiceFactory::CreateAndInitializeForBrowserState(
        browser_state_.get(),
        std::make_unique<FakeAuthenticationServiceDelegate>());
    ChromeAccountManagerService* account_manager_service =
        ChromeAccountManagerServiceFactory::GetForBrowserState(
            browser_state_.get());
    AuthenticationService* auth_service =
        AuthenticationServiceFactory::GetForBrowserState(browser_state_.get());
    auth_service->SignIn(account_manager_service->GetDefaultIdentity());
  }

 protected:
  web::WebTaskEnvironment task_environment_{
      web::WebTaskEnvironment::IO_MAINLOOP};
  IOSChromeScopedTestingLocalState local_state_;
  signin::IdentityTestEnvironment identity_test_env_;
  std::unique_ptr<TestChromeBrowserState> browser_state_;
  variations::ScopedVariationsIdsProvider scoped_variations_ids_provider_{
      variations::VariationsIdsProvider::Mode::kUseSignedInState};
  ios::FakeChromeIdentityService* identity_service_;
};

// Tests that the signed in accounts view shouldn't be presented when the
// accounts haven't changed.
TEST_F(SignedInAccountsViewControllerTest,
       ShouldBePresentedForBrowserStateNotNecessary) {
  EXPECT_FALSE([SignedInAccountsViewController
      shouldBePresentedForBrowserState:browser_state_.get()]);
}

// Tests that the signed in accounts view should be presented when the accounts
// have changed.
TEST_F(SignedInAccountsViewControllerTest,
       ShouldBePresentedForBrowserStateNecessary) {
  identity_service_->AddIdentities(@[ @"identity2" ]);
  identity_service_->FireChromeIdentityReload();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE([SignedInAccountsViewController
      shouldBePresentedForBrowserState:browser_state_.get()]);
}
