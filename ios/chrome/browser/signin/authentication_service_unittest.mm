// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/bind.h"
#include "base/run_loop.h"
#include "base/strings/sys_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/gtest_util.h"
#include "base/test/scoped_feature_list.h"
#include "components/keyed_service/core/service_access_type.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_registry_simple.h"
#import "components/signin/internal/identity_manager/account_capabilities_constants.h"
#include "components/signin/ios/browser/features.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/signin/public/identity_manager/device_accounts_synchronizer.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/signin/public/identity_manager/test_identity_manager_observer.h"
#include "components/sync/driver/mock_sync_service.h"
#include "components/sync_preferences/pref_service_mock_factory.h"
#include "components/sync_preferences/pref_service_syncable.h"
#include "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#include "ios/chrome/browser/browser_state/test_chrome_browser_state_manager.h"
#include "ios/chrome/browser/content_settings/cookie_settings_factory.h"
#include "ios/chrome/browser/content_settings/host_content_settings_map_factory.h"
#import "ios/chrome/browser/policy/policy_util.h"
#include "ios/chrome/browser/pref_names.h"
#include "ios/chrome/browser/prefs/browser_prefs.h"
#import "ios/chrome/browser/signin/authentication_service.h"
#import "ios/chrome/browser/signin/authentication_service_delegate_fake.h"
#import "ios/chrome/browser/signin/authentication_service_factory.h"
#import "ios/chrome/browser/signin/authentication_service_observer_bridge.h"
#import "ios/chrome/browser/signin/chrome_account_manager_service.h"
#import "ios/chrome/browser/signin/chrome_account_manager_service_factory.h"
#import "ios/chrome/browser/signin/identity_manager_factory.h"
#include "ios/chrome/browser/sync/sync_service_factory.h"
#include "ios/chrome/browser/sync/sync_setup_service_factory.h"
#include "ios/chrome/browser/sync/sync_setup_service_mock.h"
#include "ios/chrome/browser/system_flags.h"
#include "ios/chrome/test/testing_application_context.h"
#import "ios/public/provider/chrome/browser/signin/chrome_identity.h"
#import "ios/public/provider/chrome/browser/signin/fake_chrome_identity_service.h"
#include "ios/web/public/test/web_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"
#include "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using testing::_;
using testing::Invoke;
using testing::Return;

namespace {

std::unique_ptr<KeyedService> BuildMockSyncService(web::BrowserState* context) {
  return std::make_unique<syncer::MockSyncService>();
}

CoreAccountId GetAccountId(ChromeIdentity* identity) {
  return CoreAccountId(base::SysNSStringToUTF8([identity gaiaID]));
}

}  // namespace

class AuthenticationServiceTest : public PlatformTest {
 protected:
  AuthenticationServiceTest() : identity_test_env_() {
    identity_service()->AddIdentities(@[ @"foo", @"foo2" ]);

    TestChromeBrowserState::Builder builder;
    builder.SetPrefService(CreatePrefService());
    builder.AddTestingFactory(SyncServiceFactory::GetInstance(),
                              base::BindRepeating(&BuildMockSyncService));
    builder.AddTestingFactory(
        SyncSetupServiceFactory::GetInstance(),
        base::BindRepeating(&SyncSetupServiceMock::CreateKeyedService));
    builder.AddTestingFactory(
        AuthenticationServiceFactory::GetInstance(),
        AuthenticationServiceFactory::GetDefaultFactory());

    browser_state_ = builder.Build();

    account_manager_ = ChromeAccountManagerServiceFactory::GetForBrowserState(
        browser_state_.get());

    AuthenticationServiceFactory::CreateAndInitializeForBrowserState(
        browser_state_.get(),
        std::make_unique<AuthenticationServiceDelegateFake>());
    // Account ID migration is done on iOS.
    DCHECK_EQ(signin::IdentityManager::MIGRATION_DONE,
              identity_manager()->GetAccountIdMigrationState());
  }

  std::unique_ptr<sync_preferences::PrefServiceSyncable> CreatePrefService() {
    sync_preferences::PrefServiceMockFactory factory;
    scoped_refptr<user_prefs::PrefRegistrySyncable> registry(
        new user_prefs::PrefRegistrySyncable);
    std::unique_ptr<sync_preferences::PrefServiceSyncable> prefs =
        factory.CreateSyncable(registry.get());
    RegisterBrowserStatePrefs(registry.get());
    return prefs;
  }

  void SetExpectationsForSignIn() {
    EXPECT_CALL(*sync_setup_service_mock(), PrepareForFirstSyncSetup).Times(0);
  }

  void SetExpectationsForSignInAndSync() {
    EXPECT_CALL(*sync_setup_service_mock(), PrepareForFirstSyncSetup).Times(1);
  }

  void FireApplicationWillEnterForeground() {
    authentication_service()->OnApplicationWillEnterForeground();
  }

  void FireAccessTokenRefreshFailed(ChromeIdentity* identity,
                                    NSDictionary* user_info) {
    authentication_service()->OnAccessTokenRefreshFailed(identity, user_info);
  }

  void FireIdentityListChanged(bool notify_user) {
    authentication_service()->OnIdentityListChanged(notify_user);
  }

  void SetCachedMDMInfo(ChromeIdentity* identity, NSDictionary* user_info) {
    authentication_service()->cached_mdm_infos_[GetAccountId(identity)] =
        user_info;
  }

  bool HasCachedMDMInfo(ChromeIdentity* identity) {
    return authentication_service()->cached_mdm_infos_.count(
               GetAccountId(identity)) > 0;
  }

  int ClearBrowsingDataCount() {
    return authentication_service()->delegate_->clear_browsing_data_counter_;
  }

  AuthenticationService* authentication_service() {
    return AuthenticationServiceFactory::GetForBrowserState(
        browser_state_.get());
  }

  signin::IdentityManager* identity_manager() {
    return IdentityManagerFactory::GetForBrowserState(browser_state_.get());
  }

  ios::FakeChromeIdentityService* identity_service() {
    return ios::FakeChromeIdentityService::GetInstanceFromChromeProvider();
  }

  syncer::MockSyncService* mock_sync_service() {
    return static_cast<syncer::MockSyncService*>(
        SyncServiceFactory::GetForBrowserState(browser_state_.get()));
  }

  SyncSetupServiceMock* sync_setup_service_mock() {
    return static_cast<SyncSetupServiceMock*>(
        SyncSetupServiceFactory::GetForBrowserState(browser_state_.get()));
  }

  ChromeIdentity* identity(NSUInteger index) {
    return [account_manager_->GetAllIdentities() objectAtIndex:index];
  }

  // Sets a restricted pattern.
  void SetPattern(const std::string pattern) {
    base::Value allowed_patterns(base::Value::Type::LIST);
    allowed_patterns.Append(pattern);
    GetApplicationContext()->GetLocalState()->Set(
        prefs::kRestrictAccountsToPatterns, allowed_patterns);
  }

  IOSChromeScopedTestingLocalState local_state_;
  ChromeAccountManagerService* account_manager_;
  web::WebTaskEnvironment task_environment_;
  signin::IdentityTestEnvironment identity_test_env_;
  std::unique_ptr<TestChromeBrowserState> browser_state_;
};

TEST_F(AuthenticationServiceTest, TestDefaultGetPrimaryIdentity) {
  EXPECT_FALSE(authentication_service()->GetPrimaryIdentity(
      signin::ConsentLevel::kSignin));
  EXPECT_FALSE(authentication_service()->HasPrimaryIdentity(
      signin::ConsentLevel::kSignin));
}

TEST_F(AuthenticationServiceTest, TestSignInAndGetPrimaryIdentity) {
  // Sign in.
  SetExpectationsForSignIn();
  authentication_service()->SignIn(identity(0), nil);

  EXPECT_NSEQ(identity(0), authentication_service()->GetPrimaryIdentity(
                               signin::ConsentLevel::kSignin));

  std::string user_email = base::SysNSStringToUTF8([identity(0) userEmail]);
  AccountInfo account_info =
      identity_manager()->FindExtendedAccountInfoByEmailAddress(user_email);
  EXPECT_EQ(user_email, account_info.email);
  EXPECT_EQ(base::SysNSStringToUTF8([identity(0) gaiaID]), account_info.gaia);
  EXPECT_TRUE(
      identity_manager()->HasAccountWithRefreshToken(account_info.account_id));
  EXPECT_TRUE(authentication_service()->HasPrimaryIdentity(
      signin::ConsentLevel::kSignin));
}

// Tests that reauth prompt can be set and reset.
TEST_F(AuthenticationServiceTest, TestSetReauthPromptForSignInAndSync) {
  // Verify that the default value of this flag is off.
  EXPECT_FALSE(authentication_service()->ShouldReauthPromptForSignInAndSync());
  // Verify that prompt-flag setter and getter functions are working correctly.
  authentication_service()->SetReauthPromptForSignInAndSync();
  EXPECT_TRUE(authentication_service()->ShouldReauthPromptForSignInAndSync());
  authentication_service()->ResetReauthPromptForSignInAndSync();
  EXPECT_FALSE(authentication_service()->ShouldReauthPromptForSignInAndSync());
}

// Tests that reauth prompt is not set when the user signs out.
TEST_F(AuthenticationServiceTest, TestHandleForgottenIdentityNoPromptSignIn) {
  // Sign in.
  SetExpectationsForSignInAndSync();
  authentication_service()->SignIn(identity(0), nil);
  authentication_service()->GrantSyncConsent(identity(0));

  // Set the authentication service as "In Foreground", remove identity and run
  // the loop.
  FireApplicationWillEnterForeground();
  identity_service()->ForgetIdentity(identity(0), nil);
  base::RunLoop().RunUntilIdle();

  // User is signed out (no corresponding identity), but not prompted for sign
  // in (as the action was user initiated).
  EXPECT_TRUE(identity_manager()
                  ->GetPrimaryAccountInfo(signin::ConsentLevel::kSync)
                  .email.empty());
  EXPECT_FALSE(authentication_service()->GetPrimaryIdentity(
      signin::ConsentLevel::kSignin));
  EXPECT_FALSE(authentication_service()->HasPrimaryIdentity(
      signin::ConsentLevel::kSignin));
  EXPECT_FALSE(authentication_service()->ShouldReauthPromptForSignInAndSync());
}

// Tests that reauth prompt is set if the primary identity is remove from
// an other app when the user was signed and syncing.
TEST_F(AuthenticationServiceTest, TestHandleForgottenIdentityPromptSignIn) {
  // Sign in.
  SetExpectationsForSignInAndSync();
  authentication_service()->SignIn(identity(0), nil);
  authentication_service()->GrantSyncConsent(identity(0));

  // Set the authentication service as "In Background", remove identity and run
  // the loop.
  identity_service()->SimulateForgetIdentityFromOtherApp(identity(0));
  base::RunLoop().RunUntilIdle();

  // User is signed out (no corresponding identity), and reauth prompt is set.
  EXPECT_TRUE(identity_manager()
                  ->GetPrimaryAccountInfo(signin::ConsentLevel::kSync)
                  .email.empty());
  EXPECT_FALSE(authentication_service()->GetPrimaryIdentity(
      signin::ConsentLevel::kSignin));
  EXPECT_FALSE(authentication_service()->HasPrimaryIdentity(
      signin::ConsentLevel::kSignin));
  EXPECT_TRUE(authentication_service()->ShouldReauthPromptForSignInAndSync());
}

// Tests that reauth prompt is not set if the primary identity is remove from
// an other app when the user was only signed in (and not syncing).
TEST_F(AuthenticationServiceTest,
       TestHandleForgottenIdentityNoPromptSignInAndSync) {
  // Sign in.
  SetExpectationsForSignIn();
  authentication_service()->SignIn(identity(0), nil);

  // Set the authentication service as "In Background", remove identity and run
  // the loop.
  identity_service()->SimulateForgetIdentityFromOtherApp(identity(0));
  base::RunLoop().RunUntilIdle();

  // User is signed out (no corresponding identity), and reauth prompt is not
  // set since the user was not syncing.
  EXPECT_TRUE(identity_manager()
                  ->GetPrimaryAccountInfo(signin::ConsentLevel::kSync)
                  .email.empty());
  EXPECT_FALSE(authentication_service()->GetPrimaryIdentity(
      signin::ConsentLevel::kSignin));
  EXPECT_FALSE(authentication_service()->HasPrimaryIdentity(
      signin::ConsentLevel::kSignin));
  EXPECT_FALSE(authentication_service()->ShouldReauthPromptForSignInAndSync());
}

TEST_F(AuthenticationServiceTest,
       OnApplicationEnterForegroundReloadCredentials) {
  // Sign in.
  SetExpectationsForSignIn();
  authentication_service()->SignIn(identity(0), nil);

  identity_service()->AddIdentities(@[ @"foo3" ]);

  auto account_compare_func = [](const CoreAccountInfo& first,
                                 const CoreAccountInfo& second) {
    return first.account_id < second.account_id;
  };
  std::vector<CoreAccountInfo> accounts =
      identity_manager()->GetAccountsWithRefreshTokens();
  std::sort(accounts.begin(), accounts.end(), account_compare_func);
  ASSERT_EQ(2u, accounts.size());
  EXPECT_EQ(CoreAccountId("foo2ID"), accounts[0].account_id);
  EXPECT_EQ(CoreAccountId("fooID"), accounts[1].account_id);

  // Simulate a switching to background and back to foreground, triggering a
  // credentials reload.
  identity_service()->FireChromeIdentityReload();
  base::RunLoop().RunUntilIdle();

  // Accounts are reloaded, "foo3@foo.com" is added as it is now in
  // ChromeIdentityService.
  accounts = identity_manager()->GetAccountsWithRefreshTokens();
  std::sort(accounts.begin(), accounts.end(), account_compare_func);
  ASSERT_EQ(3u, accounts.size());
  EXPECT_EQ(CoreAccountId("foo2ID"), accounts[0].account_id);
  EXPECT_EQ(CoreAccountId("foo3ID"), accounts[1].account_id);
  EXPECT_EQ(CoreAccountId("fooID"), accounts[2].account_id);
}

// Tests the account list is approved after adding an account with in Chrome.
TEST_F(AuthenticationServiceTest, AccountListApprovedByUser_AddedByUser) {
  SetExpectationsForSignIn();
  authentication_service()->SignIn(identity(0), nil);

  identity_service()->AddIdentities(@[ @"foo3" ]);
  FireIdentityListChanged(/*notify_user=*/false);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(authentication_service()->IsAccountListApprovedByUser());
}

// Tests the account list is unapproved after an account is added by an other
// app (through the keychain).
TEST_F(AuthenticationServiceTest, AccountListApprovedByUser_ChangedByKeychain) {
  SetExpectationsForSignIn();
  authentication_service()->SignIn(identity(0), nil);

  identity_service()->AddIdentities(@[ @"foo3" ]);
  FireIdentityListChanged(/*notify_user=*/true);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(authentication_service()->IsAccountListApprovedByUser());
}

// Tests the account list is unapproved after two accounts are added by an other
// app (through the keychain).
TEST_F(AuthenticationServiceTest,
       AccountListApprovedByUser_ChangedTwiceByKeychain) {
  SetExpectationsForSignIn();
  authentication_service()->SignIn(identity(0), nil);

  identity_service()->AddIdentities(@[ @"foo3" ]);
  FireIdentityListChanged(/*notify_user=*/true);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(authentication_service()->IsAccountListApprovedByUser());

  // Simulate a switching to background, changing the accounts while in
  // background.
  identity_service()->AddIdentities(@[ @"foo4" ]);
  FireIdentityListChanged(/*notify_user=*/true);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(authentication_service()->IsAccountListApprovedByUser());
}

// Regression test for http://crbug.com/1006717
TEST_F(AuthenticationServiceTest,
       AccountListApprovedByUser_ResetOntwoBackgrounds) {
  SetExpectationsForSignIn();
  authentication_service()->SignIn(identity(0), nil);

  identity_service()->AddIdentities(@[ @"foo3" ]);
  FireIdentityListChanged(/*notify_user=*/true);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(authentication_service()->IsAccountListApprovedByUser());

  // Clear |kSigninLastAccounts| pref to simulate a case when the list of
  // accounts in pref |kSigninLastAccounts| are no the same as the ones
  browser_state_->GetPrefs()->ClearPref(prefs::kSigninLastAccounts);

  // When entering foreground, the have accounts changed state should be
  // updated.
  FireApplicationWillEnterForeground();
  EXPECT_FALSE(authentication_service()->IsAccountListApprovedByUser());

  // Backgrounding and foregrounding the application a second time should update
  // the list of accounts in |kSigninLastAccounts| and should reset the have
  // account changed state.
  FireApplicationWillEnterForeground();
  EXPECT_FALSE(authentication_service()->IsAccountListApprovedByUser());
}

TEST_F(AuthenticationServiceTest, HasPrimaryIdentityBackground) {
  // Sign in.
  SetExpectationsForSignIn();
  authentication_service()->SignIn(identity(0), nil);
  EXPECT_TRUE(authentication_service()->HasPrimaryIdentity(
      signin::ConsentLevel::kSignin));

  // Remove the signed in identity while in background, and check that
  // HasPrimaryIdentity is up-to-date.
  identity_service()->ForgetIdentity(identity(0), nil);
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(authentication_service()->HasPrimaryIdentity(
      signin::ConsentLevel::kSignin));
}

// Tests that MDM errors are correctly cleared on foregrounding, sending
// notifications that the state of error has changed.
TEST_F(AuthenticationServiceTest, MDMErrorsClearedOnForeground) {
  SetExpectationsForSignIn();
  authentication_service()->SignIn(identity(0), nil);
  EXPECT_EQ(identity_manager()->GetAccountsWithRefreshTokens().size(), 2UL);

  NSDictionary* user_info = [NSDictionary dictionary];
  SetCachedMDMInfo(identity(0), user_info);
  GoogleServiceAuthError error(
      GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS);
  signin::UpdatePersistentErrorOfRefreshTokenForAccount(
      identity_manager(), GetAccountId(identity(0)), error);

  // MDM error for |identity_| is being cleared and the error state of refresh
  // token will be updated.
  {
    bool notification_received = false;
    signin::TestIdentityManagerObserver observer(identity_manager());
    observer.SetOnErrorStateOfRefreshTokenUpdatedCallback(
        base::BindLambdaForTesting([&]() { notification_received = true; }));

    FireApplicationWillEnterForeground();
    EXPECT_TRUE(notification_received);
    EXPECT_EQ(
        base::SysNSStringToUTF8([identity(0) gaiaID]),
        observer.AccountFromErrorStateOfRefreshTokenUpdatedCallback().gaia);
  }

  // MDM error has already been cleared, no notification will be sent.
  {
    bool notification_received = false;
    signin::TestIdentityManagerObserver observer(identity_manager());
    observer.SetOnErrorStateOfRefreshTokenUpdatedCallback(
        base::BindLambdaForTesting([&]() { notification_received = true; }));

    FireApplicationWillEnterForeground();
    EXPECT_FALSE(notification_received);
  }
}

// Tests that MDM errors are correctly cleared when signing out.
TEST_F(AuthenticationServiceTest, MDMErrorsClearedOnSignout) {
  SetExpectationsForSignIn();
  authentication_service()->SignIn(identity(0), nil);
  EXPECT_EQ(identity_manager()->GetAccountsWithRefreshTokens().size(), 2UL);

  NSDictionary* user_info = [NSDictionary dictionary];
  SetCachedMDMInfo(identity(0), user_info);

  authentication_service()->SignOut(signin_metrics::ABORT_SIGNIN,
                                    /*force_clear_browsing_data=*/false, nil);
  EXPECT_FALSE(HasCachedMDMInfo(identity(0)));
  EXPECT_EQ(identity_manager()->GetAccountsWithRefreshTokens().size(), 0UL);
  EXPECT_EQ(ClearBrowsingDataCount(), 0);
}

// Tests that MDM errors are correctly cleared when signing out with clearing
// browsing data.
TEST_F(AuthenticationServiceTest,
       MDMErrorsClearedOnSignoutAndClearBrowsingData) {
  SetExpectationsForSignIn();
  authentication_service()->SignIn(identity(0), nil);
  EXPECT_EQ(identity_manager()->GetAccountsWithRefreshTokens().size(), 2UL);

  NSDictionary* user_info = [NSDictionary dictionary];
  SetCachedMDMInfo(identity(0), user_info);

  authentication_service()->SignOut(signin_metrics::ABORT_SIGNIN,
                                    /*force_clear_browsing_data=*/true, nil);
  EXPECT_FALSE(HasCachedMDMInfo(identity(0)));
  EXPECT_EQ(identity_manager()->GetAccountsWithRefreshTokens().size(), 0UL);
  EXPECT_EQ(ClearBrowsingDataCount(), 1);
}

// Tests that local data are not cleared when signing out of a non-syncing
// managed account.
TEST_F(AuthenticationServiceTest, SignedInManagedAccountSignOut) {
  identity_service()->AddManagedIdentities(@[ @"foo3" ]);

  SetExpectationsForSignIn();
  authentication_service()->SignIn(identity(2), nil);
  EXPECT_EQ(identity_manager()->GetAccountsWithRefreshTokens().size(), 3UL);
  EXPECT_TRUE(authentication_service()->HasPrimaryIdentityManaged(
      signin::ConsentLevel::kSignin));

  NSDictionary* user_info = [NSDictionary dictionary];
  SetCachedMDMInfo(identity(2), user_info);

  authentication_service()->SignOut(signin_metrics::ABORT_SIGNIN,
                                    /*force_clear_browsing_data=*/false, nil);
  EXPECT_FALSE(HasCachedMDMInfo(identity(2)));
  EXPECT_EQ(identity_manager()->GetAccountsWithRefreshTokens().size(), 0UL);
  EXPECT_EQ(ClearBrowsingDataCount(), 0);
}

// Tests that MDM errors are correctly cleared when signing out of a managed
// account.
TEST_F(AuthenticationServiceTest, ManagedAccountSignOut) {
  identity_service()->AddManagedIdentities(@[ @"foo3" ]);

  SetExpectationsForSignIn();
  authentication_service()->SignIn(identity(2), nil);
  EXPECT_EQ(identity_manager()->GetAccountsWithRefreshTokens().size(), 3UL);
  EXPECT_TRUE(authentication_service()->HasPrimaryIdentityManaged(
      signin::ConsentLevel::kSignin));
  ON_CALL(*mock_sync_service()->GetMockUserSettings(), IsFirstSetupComplete())
      .WillByDefault(Return(true));

  NSDictionary* user_info = [NSDictionary dictionary];
  SetCachedMDMInfo(identity(2), user_info);

  authentication_service()->SignOut(signin_metrics::ABORT_SIGNIN,
                                    /*force_clear_browsing_data=*/false, nil);
  EXPECT_FALSE(HasCachedMDMInfo(identity(2)));
  EXPECT_EQ(identity_manager()->GetAccountsWithRefreshTokens().size(), 0UL);
  EXPECT_EQ(ClearBrowsingDataCount(), 1);
}

// Tests that MDM errors are correctly cleared when signing out with clearing
// browsing data of a managed account.
TEST_F(AuthenticationServiceTest, ManagedAccountSignOutAndClearBrowsingData) {
  identity_service()->AddManagedIdentities(@[ @"foo3" ]);

  SetExpectationsForSignIn();
  authentication_service()->SignIn(identity(2), nil);
  EXPECT_EQ(identity_manager()->GetAccountsWithRefreshTokens().size(), 3UL);
  EXPECT_TRUE(authentication_service()->HasPrimaryIdentityManaged(
      signin::ConsentLevel::kSignin));

  NSDictionary* user_info = [NSDictionary dictionary];
  SetCachedMDMInfo(identity(2), user_info);

  authentication_service()->SignOut(signin_metrics::ABORT_SIGNIN,
                                    /*force_clear_browsing_data=*/true, nil);
  EXPECT_FALSE(HasCachedMDMInfo(identity(2)));
  EXPECT_EQ(identity_manager()->GetAccountsWithRefreshTokens().size(), 0UL);
  EXPECT_EQ(ClearBrowsingDataCount(), 1);
}

// Tests that potential MDM notifications are correctly handled and dispatched
// to MDM service when necessary.
TEST_F(AuthenticationServiceTest, HandleMDMNotification) {
  SetExpectationsForSignIn();
  authentication_service()->SignIn(identity(0), nil);
  GoogleServiceAuthError error(
      GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS);
  signin::UpdatePersistentErrorOfRefreshTokenForAccount(
      identity_manager(), GetAccountId(identity(0)), error);

  NSDictionary* user_info1 = @{ @"foo" : @1 };
  ON_CALL(*identity_service(), GetMDMDeviceStatus(user_info1))
      .WillByDefault(Return(1));
  NSDictionary* user_info2 = @{ @"foo" : @2 };
  ON_CALL(*identity_service(), GetMDMDeviceStatus(user_info2))
      .WillByDefault(Return(2));

  // Notification will show the MDM dialog the first time.
  EXPECT_CALL(*identity_service(),
              HandleMDMNotification(identity(0), user_info1, _))
      .WillOnce(Return(true));
  FireAccessTokenRefreshFailed(identity(0), user_info1);

  // Same notification won't show the MDM dialog the second time.
  EXPECT_CALL(*identity_service(),
              HandleMDMNotification(identity(0), user_info1, _))
      .Times(0);
  FireAccessTokenRefreshFailed(identity(0), user_info1);

  // New notification will show the MDM dialog on the same identity.
  EXPECT_CALL(*identity_service(),
              HandleMDMNotification(identity(0), user_info2, _))
      .WillOnce(Return(true));
  FireAccessTokenRefreshFailed(identity(0), user_info2);
}

// Tests that MDM blocked notifications are correctly signing out the user if
// the primary account is blocked.
TEST_F(AuthenticationServiceTest, HandleMDMBlockedNotification) {
  SetExpectationsForSignIn();
  authentication_service()->SignIn(identity(0), nil);
  GoogleServiceAuthError error(
      GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS);
  signin::UpdatePersistentErrorOfRefreshTokenForAccount(
      identity_manager(), GetAccountId(identity(0)), error);

  NSDictionary* user_info1 = @{ @"foo" : @1 };
  ON_CALL(*identity_service(), GetMDMDeviceStatus(user_info1))
      .WillByDefault(Return(1));

  auto handle_mdm_notification_callback = [](ChromeIdentity*, NSDictionary*,
                                             ios::MDMStatusCallback callback) {
    callback(true /* is_blocked */);
    return true;
  };

  // User not signed out as |identity(1)| isn't the primary account.
  EXPECT_CALL(*identity_service(),
              HandleMDMNotification(identity(1), user_info1, _))
      .WillOnce(Invoke(handle_mdm_notification_callback));
  FireAccessTokenRefreshFailed(identity(1), user_info1);
  EXPECT_TRUE(authentication_service()->HasPrimaryIdentity(
      signin::ConsentLevel::kSignin));

  // User signed out as |identity_| is the primary account.
  EXPECT_CALL(*identity_service(),
              HandleMDMNotification(identity(0), user_info1, _))
      .WillOnce(Invoke(handle_mdm_notification_callback));
  FireAccessTokenRefreshFailed(identity(0), user_info1);
  EXPECT_FALSE(authentication_service()->HasPrimaryIdentity(
      signin::ConsentLevel::kSignin));
}

// Tests that MDM dialog isn't shown when there is no cached MDM error.
TEST_F(AuthenticationServiceTest, ShowMDMErrorDialogNoCachedError) {
  EXPECT_CALL(*identity_service(), HandleMDMNotification(identity(0), _, _))
      .Times(0);

  EXPECT_FALSE(
      authentication_service()->ShowMDMErrorDialogForIdentity(identity(0)));
}

// Tests that MDM dialog isn't shown when there is a cached MDM error but no
// corresponding error for the account.
TEST_F(AuthenticationServiceTest, ShowMDMErrorDialogInvalidCachedError) {
  NSDictionary* user_info = [NSDictionary dictionary];
  SetCachedMDMInfo(identity(0), user_info);

  EXPECT_CALL(*identity_service(),
              HandleMDMNotification(identity(0), user_info, _))
      .Times(0);

  EXPECT_FALSE(
      authentication_service()->ShowMDMErrorDialogForIdentity(identity(0)));
}

// Tests that MDM dialog is shown when there is a cached error and a
// corresponding error for the account.
TEST_F(AuthenticationServiceTest, ShowMDMErrorDialog) {
  authentication_service()->SignIn(identity(0), nil);
  GoogleServiceAuthError error(
      GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS);
  signin::UpdatePersistentErrorOfRefreshTokenForAccount(
      identity_manager(), GetAccountId(identity(0)), error);

  NSDictionary* user_info = [NSDictionary dictionary];
  SetCachedMDMInfo(identity(0), user_info);

  EXPECT_CALL(*identity_service(),
              HandleMDMNotification(identity(0), user_info, _))
      .WillOnce(Return(true));

  EXPECT_TRUE(
      authentication_service()->ShowMDMErrorDialogForIdentity(identity(0)));
}

TEST_F(AuthenticationServiceTest, SigninAndSyncDecoupled) {
  // Sign in.
  SetExpectationsForSignIn();
  authentication_service()->SignIn(identity(0), nil);

  EXPECT_NSEQ(identity(0), authentication_service()->GetPrimaryIdentity(
                               signin::ConsentLevel::kSignin));
  EXPECT_TRUE(
      identity_manager()->HasPrimaryAccount(signin::ConsentLevel::kSignin));
  EXPECT_FALSE(
      identity_manager()->HasPrimaryAccount(signin::ConsentLevel::kSync));
  EXPECT_TRUE(authentication_service()->HasPrimaryIdentity(
      signin::ConsentLevel::kSignin));

  // Grant Sync consent.
  EXPECT_CALL(*sync_setup_service_mock(), PrepareForFirstSyncSetup).Times(1);
  EXPECT_CALL(*mock_sync_service()->GetMockUserSettings(),
              SetSyncRequested(true));
  authentication_service()->GrantSyncConsent(identity(0));

  EXPECT_NSEQ(identity(0), authentication_service()->GetPrimaryIdentity(
                               signin::ConsentLevel::kSignin));
  EXPECT_TRUE(
      identity_manager()->HasPrimaryAccount(signin::ConsentLevel::kSignin));
  EXPECT_TRUE(
      identity_manager()->HasPrimaryAccount(signin::ConsentLevel::kSync));
  EXPECT_TRUE(authentication_service()->HasPrimaryIdentity(
      signin::ConsentLevel::kSignin));
}

TEST_F(AuthenticationServiceTest, SigninDisallowedCrash) {
  // Disable sign-in.
  browser_state_->GetPrefs()->SetBoolean(prefs::kSigninAllowed, false);

  // Attempt to sign in, and verify there is a crash.
  EXPECT_CHECK_DEATH(authentication_service()->SignIn(identity(0), nil));
}

// Tests that reauth prompt is not set if the primary identity is restricted and
// |OnPrimaryAccountRestricted| is forwarded.
TEST_F(AuthenticationServiceTest, TestHandleRestrictedIdentityPromptSignIn) {
  id<AuthenticationServiceObserving> observer_delegate =
      OCMStrictProtocolMock(@protocol(AuthenticationServiceObserving));
  AuthenticationServiceObserverBridge observer_bridge(authentication_service(),
                                                      observer_delegate);

  // Sign in.
  OCMExpect([observer_delegate onPrimaryAccountRestricted]);
  SetExpectationsForSignInAndSync();
  authentication_service()->SignIn(identity(0), nil);
  authentication_service()->GrantSyncConsent(identity(0));

  // Set the account restriction.
  SetPattern("foo");
  EXPECT_FALSE(account_manager_->HasIdentities());

  // Set the authentication service as "In Background" and run the loop.
  base::RunLoop().RunUntilIdle();

  // User is signed out (no corresponding identity), and reauth prompt is set.
  EXPECT_TRUE(identity_manager()
                  ->GetPrimaryAccountInfo(signin::ConsentLevel::kSync)
                  .gaia.empty());
  EXPECT_FALSE(authentication_service()->GetPrimaryIdentity(
      signin::ConsentLevel::kSignin));
  EXPECT_FALSE(authentication_service()->HasPrimaryIdentity(
      signin::ConsentLevel::kSignin));
  EXPECT_FALSE(authentication_service()->ShouldReauthPromptForSignInAndSync());
  EXPECT_OCMOCK_VERIFY(observer_delegate);
}

// Tests AuthenticationService::GetServiceStatus() using
// prefs::kBrowserSigninPolicy.
TEST_F(AuthenticationServiceTest, TestGetServiceStatus) {
  id<AuthenticationServiceObserving> observer_delegate =
      OCMStrictProtocolMock(@protocol(AuthenticationServiceObserving));
  AuthenticationServiceObserverBridge observer_bridge(authentication_service(),
                                                      observer_delegate);

  // Expect sign-in allowed by default.
  EXPECT_EQ(AuthenticationService::ServiceStatus::SigninAllowed,
            authentication_service()->GetServiceStatus());

  // Expect onServiceStatus notification called.
  OCMExpect([observer_delegate onServiceStatusChanged]);
  browser_state_->GetPrefs()->SetBoolean(prefs::kSigninAllowed, false);
  // Expect sign-in disabled by user.
  EXPECT_EQ(AuthenticationService::ServiceStatus::SigninDisabledByUser,
            authentication_service()->GetServiceStatus());

  // Expect onServiceStatus notification called.
  OCMExpect([observer_delegate onServiceStatusChanged]);
  // Set sign-in disabled by policy.
  local_state_.Get()->SetInteger(
      prefs::kBrowserSigninPolicy,
      static_cast<int>(BrowserSigninMode::kDisabled));
  // Expect sign-in to be disabled by policy.
  EXPECT_EQ(AuthenticationService::ServiceStatus::SigninDisabledByPolicy,
            authentication_service()->GetServiceStatus());

  // Expect onServiceStatus notification called.
  OCMExpect([observer_delegate onServiceStatusChanged]);
  // Set sign-in forced by policy.
  local_state_.Get()->SetInteger(prefs::kBrowserSigninPolicy,
                                 static_cast<int>(BrowserSigninMode::kForced));
  // Expect sign-in to be forced by policy.
  EXPECT_EQ(AuthenticationService::ServiceStatus::SigninForcedByPolicy,
            authentication_service()->GetServiceStatus());

  // Expect onServiceStatus notification called.
  OCMExpect([observer_delegate onServiceStatusChanged]);
  browser_state_->GetPrefs()->SetBoolean(prefs::kSigninAllowed, true);
  // Expect sign-in to be still forced by policy.
  EXPECT_EQ(AuthenticationService::ServiceStatus::SigninForcedByPolicy,
            authentication_service()->GetServiceStatus());

  EXPECT_OCMOCK_VERIFY(observer_delegate);
}

// Tests that a supervised user has all local data cleared on sign-in when
// there was a previous account on the device.
TEST_F(AuthenticationServiceTest, SupervisedAccountSwitch) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      signin::kEnableUnicornAccountSupport);

  SetExpectationsForSignIn();
  identity_service()->SetCapabilities(
      identity(1), @{
        @(kIsSubjectToParentalControlsCapabilityName) :
            @(static_cast<int>(ios::ChromeIdentityCapabilityResult::kTrue))
      });

  authentication_service()->SignIn(identity(0), nil);
  EXPECT_TRUE(authentication_service()->HasPrimaryIdentity(
      signin::ConsentLevel::kSignin));
  ON_CALL(*mock_sync_service()->GetMockUserSettings(), IsFirstSetupComplete())
      .WillByDefault(Return(true));

  authentication_service()->SignOut(signin_metrics::ABORT_SIGNIN,
                                    /*force_clear_browsing_data=*/false, nil);

  EXPECT_EQ(ClearBrowsingDataCount(), 0);

  // Clears browsing data when signed-in as a child.
  authentication_service()->SignIn(identity(1), nil);
  EXPECT_EQ(ClearBrowsingDataCount(), 1);
}

// Tests that supervised user's local data is correctly cleared when signing
// out.
TEST_F(AuthenticationServiceTest, SupervisedAccountSignOut) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      signin::kEnableUnicornAccountSupport);

  SetExpectationsForSignIn();
  identity_service()->SetCapabilities(
      identity(0), @{
        @(kIsSubjectToParentalControlsCapabilityName) :
            @(static_cast<int>(ios::ChromeIdentityCapabilityResult::kTrue))
      });

  authentication_service()->SignIn(identity(0), nil);
  EXPECT_TRUE(authentication_service()->HasPrimaryIdentity(
      signin::ConsentLevel::kSignin));
  ON_CALL(*mock_sync_service()->GetMockUserSettings(), IsFirstSetupComplete())
      .WillByDefault(Return(true));
  EXPECT_EQ(ClearBrowsingDataCount(), 1);

  authentication_service()->SignOut(signin_metrics::ABORT_SIGNIN,
                                    /*force_clear_browsing_data=*/false, nil);
  EXPECT_EQ(ClearBrowsingDataCount(), 2);
}
