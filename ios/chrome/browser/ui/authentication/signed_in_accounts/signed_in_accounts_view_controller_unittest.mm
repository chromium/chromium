// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/signed_in_accounts/signed_in_accounts_view_controller.h"

#import <memory>

#import "base/functional/bind.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/scoped_feature_list.h"
#import "components/signin/public/base/signin_switches.h"
#import "components/signin/public/identity_manager/identity_test_environment.h"
#import "components/variations/scoped_variations_ids_provider.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/signin/authentication_service.h"
#import "ios/chrome/browser/signin/authentication_service_factory.h"
#import "ios/chrome/browser/signin/chrome_account_manager_service.h"
#import "ios/chrome/browser/signin/chrome_account_manager_service_factory.h"
#import "ios/chrome/browser/signin/fake_authentication_service_delegate.h"
#import "ios/chrome/browser/signin/fake_system_identity.h"
#import "ios/chrome/browser/signin/fake_system_identity_manager.h"
#import "ios/chrome/browser/sync/model/mock_sync_service_utils.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
#import "ios/chrome/browser/sync/model/sync_setup_service_factory.h"
#import "ios/chrome/browser/sync/model/sync_setup_service_mock.h"
#import "ios/chrome/test/block_cleanup_test.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

class SignedInAccountsViewControllerTest : public BlockCleanupTest {
 public:
  SignedInAccountsViewControllerTest() : identity_test_env_() {}

  void SetUp() override {
    BlockCleanupTest::SetUp();
    FakeSystemIdentityManager* system_identity_manager =
        FakeSystemIdentityManager::FromSystemIdentityManager(
            GetApplicationContext()->GetSystemIdentityManager());
    system_identity_manager->AddIdentity([FakeSystemIdentity fakeIdentity1]);

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
    auth_service->SignIn(account_manager_service->GetDefaultIdentity(),
                         signin_metrics::AccessPoint::ACCESS_POINT_UNKNOWN);
  }

 protected:
  web::WebTaskEnvironment task_environment_{
      web::WebTaskEnvironment::IO_MAINLOOP};
  IOSChromeScopedTestingLocalState local_state_;
  signin::IdentityTestEnvironment identity_test_env_;
  std::unique_ptr<TestChromeBrowserState> browser_state_;
  variations::ScopedVariationsIdsProvider scoped_variations_ids_provider_{
      variations::VariationsIdsProvider::Mode::kUseSignedInState};
};

// Tests that the signed in accounts view shouldn't be presented when the
// accounts haven't changed.
TEST_F(SignedInAccountsViewControllerTest,
       ShouldBePresentedForBrowserStateNotNecessary) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      switches::kRemoveSignedInAccountsDialog);
  EXPECT_FALSE([SignedInAccountsViewController
      shouldBePresentedForBrowserState:browser_state_.get()]);
}

// Tests that the signed in accounts view should be presented when the accounts
// have changed.
TEST_F(SignedInAccountsViewControllerTest,
       ShouldBePresentedForBrowserStateNecessary_DialogShown) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      switches::kRemoveSignedInAccountsDialog);
  FakeSystemIdentityManager* system_identity_manager =
      FakeSystemIdentityManager::FromSystemIdentityManager(
          GetApplicationContext()->GetSystemIdentityManager());
  system_identity_manager->AddIdentities(@[ @"identity2" ]);
  system_identity_manager->FireSystemIdentityReloaded();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE([SignedInAccountsViewController
      shouldBePresentedForBrowserState:browser_state_.get()]);
}

TEST_F(SignedInAccountsViewControllerTest,
       ShouldBePresentedForBrowserStateNecessary_DialogRemoved) {
  ASSERT_TRUE(
      base::FeatureList::IsEnabled(switches::kRemoveSignedInAccountsDialog));
  FakeSystemIdentityManager* system_identity_manager =
      FakeSystemIdentityManager::FromSystemIdentityManager(
          GetApplicationContext()->GetSystemIdentityManager());
  system_identity_manager->AddIdentities(@[ @"identity2" ]);
  system_identity_manager->FireSystemIdentityReloaded();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE([SignedInAccountsViewController
      shouldBePresentedForBrowserState:browser_state_.get()]);
}
