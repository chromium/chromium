// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/run_loop.h"
#include "base/strings/sys_string_conversions.h"
#include "components/browser_sync/profile_sync_service_mock.h"
#include "components/keyed_service/core/service_access_type.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/signin/core/browser/fake_profile_oauth2_token_service.h"
#include "components/signin/core/browser/profile_oauth2_token_service.h"
#include "components/signin/core/browser/signin_manager.h"
#include "components/signin/core/browser/signin_pref_names.h"
#include "components/signin/ios/browser/profile_oauth2_token_service_ios_delegate.h"
#include "components/sync_preferences/pref_service_mock_factory.h"
#include "components/sync_preferences/pref_service_syncable.h"
#include "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#include "ios/chrome/browser/browser_state/test_chrome_browser_state_manager.h"
#include "ios/chrome/browser/content_settings/cookie_settings_factory.h"
#include "ios/chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "ios/chrome/browser/experimental_flags.h"
#include "ios/chrome/browser/pref_names.h"
#include "ios/chrome/browser/prefs/browser_prefs.h"
#include "ios/chrome/browser/signin/account_tracker_service_factory.h"
#import "ios/chrome/browser/signin/authentication_service.h"
#import "ios/chrome/browser/signin/authentication_service_delegate_fake.h"
#import "ios/chrome/browser/signin/authentication_service_factory.h"
#include "ios/chrome/browser/signin/fake_oauth2_token_service_builder.h"
#include "ios/chrome/browser/signin/fake_signin_manager_builder.h"
#include "ios/chrome/browser/signin/ios_chrome_signin_client.h"
#include "ios/chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "ios/chrome/browser/signin/signin_client_factory.h"
#include "ios/chrome/browser/signin/signin_error_controller_factory.h"
#include "ios/chrome/browser/signin/signin_manager_factory.h"
#include "ios/chrome/browser/sync/ios_chrome_profile_sync_test_util.h"
#include "ios/chrome/browser/sync/profile_sync_service_factory.h"
#include "ios/chrome/browser/sync/sync_setup_service_factory.h"
#include "ios/chrome/browser/sync/sync_setup_service_mock.h"
#include "ios/chrome/test/ios_chrome_scoped_testing_chrome_browser_state_manager.h"
#include "ios/public/provider/chrome/browser/chrome_browser_provider.h"
#import "ios/public/provider/chrome/browser/signin/chrome_identity.h"
#import "ios/public/provider/chrome/browser/signin/fake_chrome_identity_service.h"
#include "ios/web/public/test/test_web_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"
#include "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using testing::_;
using testing::Invoke;
using testing::Return;

namespace {

class FakeSigninClient : public IOSChromeSigninClient {
 public:
  explicit FakeSigninClient(
      ios::ChromeBrowserState* browser_state,
      SigninErrorController* signin_error_controller,
      scoped_refptr<content_settings::CookieSettings> cookie_settings,
      scoped_refptr<HostContentSettingsMap> host_content_settings_map)
      : IOSChromeSigninClient(browser_state,
                              signin_error_controller,
                              cookie_settings,
                              host_content_settings_map) {}
  ~FakeSigninClient() override {}

  base::Time GetInstallDate() override { return base::Time::Now(); }
};

std::unique_ptr<KeyedService> BuildFakeTestSigninClient(
    web::BrowserState* context) {
  ios::ChromeBrowserState* chrome_browser_state =
      ios::ChromeBrowserState::FromBrowserState(context);
  return std::make_unique<FakeSigninClient>(
      chrome_browser_state,
      ios::SigninErrorControllerFactory::GetForBrowserState(
          chrome_browser_state),
      ios::CookieSettingsFactory::GetForBrowserState(chrome_browser_state),
      ios::HostContentSettingsMapFactory::GetForBrowserState(
          chrome_browser_state));
}

std::unique_ptr<KeyedService> BuildMockSyncSetupService(
    web::BrowserState* context) {
  ios::ChromeBrowserState* browser_state =
      ios::ChromeBrowserState::FromBrowserState(context);
  return std::make_unique<SyncSetupServiceMock>(
      ProfileSyncServiceFactory::GetForBrowserState(browser_state),
      browser_state->GetPrefs());
}

}  // namespace

class AuthenticationServiceTest : public PlatformTest,
                                  public OAuth2TokenService::Observer {
 protected:
  AuthenticationServiceTest()
      : scoped_browser_state_manager_(
            std::make_unique<TestChromeBrowserStateManager>(base::FilePath())),
        refresh_token_available_count_(0) {}

  void SetUp() override {
    PlatformTest::SetUp();

    identity_service_ =
        ios::FakeChromeIdentityService::GetInstanceFromChromeProvider();
    identity_service_->AddIdentities(@[ @"foo", @"foo2" ]);
    identity_ =
        [identity_service_->GetAllIdentitiesSortedForDisplay() objectAtIndex:0];
    identity2_ =
        [identity_service_->GetAllIdentitiesSortedForDisplay() objectAtIndex:1];

    TestChromeBrowserState::Builder builder;
    builder.SetPrefService(CreatePrefService());
    builder.AddTestingFactory(SigninClientFactory::GetInstance(),
                              base::BindRepeating(&BuildFakeTestSigninClient));
    builder.AddTestingFactory(
        ProfileOAuth2TokenServiceFactory::GetInstance(),
        base::BindRepeating(&BuildFakeOAuth2TokenService));
    builder.AddTestingFactory(
        ios::SigninManagerFactory::GetInstance(),
        base::BindRepeating(&ios::BuildFakeSigninManager));
    builder.AddTestingFactory(
        ProfileSyncServiceFactory::GetInstance(),
        base::BindRepeating(&BuildMockProfileSyncService));
    builder.AddTestingFactory(SyncSetupServiceFactory::GetInstance(),
                              base::BindRepeating(&BuildMockSyncSetupService));
    browser_state_ = builder.Build();

    signin_manager_ =
        ios::SigninManagerFactory::GetForBrowserState(browser_state_.get());
    profile_sync_service_mock_ =
        static_cast<browser_sync::ProfileSyncServiceMock*>(
            ProfileSyncServiceFactory::GetForBrowserState(
                browser_state_.get()));
    sync_setup_service_mock_ = static_cast<SyncSetupServiceMock*>(
        SyncSetupServiceFactory::GetForBrowserState(browser_state_.get()));
    CreateAuthenticationService();
    ProfileOAuth2TokenServiceFactory::GetForBrowserState(browser_state_.get())
        ->AddObserver(this);
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

  void TearDown() override {
    ProfileOAuth2TokenServiceFactory::GetForBrowserState(browser_state_.get())
        ->RemoveObserver(this);
    authentication_service_->Shutdown();
    authentication_service_.reset();
    browser_state_.reset();
    PlatformTest::TearDown();
  }

  void SetExpectationsForSignIn() {
    EXPECT_CALL(*profile_sync_service_mock_, RequestStart());
    EXPECT_CALL(*sync_setup_service_mock_, PrepareForFirstSyncSetup());
  }

  void CreateAuthenticationService() {
    if (authentication_service_.get()) {
      authentication_service_->Shutdown();
    }
    authentication_service_ = std::make_unique<AuthenticationService>(
        browser_state_->GetPrefs(),
        ProfileOAuth2TokenServiceFactory::GetForBrowserState(
            browser_state_.get()),
        SyncSetupServiceFactory::GetForBrowserState(browser_state_.get()),
        ios::AccountTrackerServiceFactory::GetForBrowserState(
            browser_state_.get()),
        ios::SigninManagerFactory::GetForBrowserState(browser_state_.get()),
        ProfileSyncServiceFactory::GetForBrowserState(browser_state_.get()));
    authentication_service_->Initialize(
        std::make_unique<AuthenticationServiceDelegateFake>());
  }

  void StoreAccountsInPrefs() {
    authentication_service_->StoreAccountsInPrefs();
  }

  void MigrateAccountsStoredInPrefsIfNeeded() {
    authentication_service_->MigrateAccountsStoredInPrefsIfNeeded();
  }

  std::vector<std::string> GetAccountsInPrefs() {
    return authentication_service_->GetAccountsInPrefs();
  }

  void SetAuthenticationServiceInForeground(bool is_in_foreground) {
    authentication_service_->is_in_foreground_ = is_in_foreground;
  }

  void FireApplicationEnterForeground() {
    authentication_service_->OnApplicationEnterForeground();
  }

  void FireApplicationEnterBackground() {
    authentication_service_->OnApplicationEnterBackground();
  }

  void FireAccessTokenRefreshFailed(ChromeIdentity* identity,
                                    NSDictionary* user_info) {
    authentication_service_->OnAccessTokenRefreshFailed(identity, user_info);
  }

  void FireIdentityListChanged() {
    authentication_service_->OnIdentityListChanged();
  }

  void SetCachedMDMInfo(ChromeIdentity* identity, NSDictionary* user_info) {
    authentication_service_
        ->cached_mdm_infos_[base::SysNSStringToUTF8([identity gaiaID])] =
        user_info;
  }

  bool HasCachedMDMInfo(ChromeIdentity* identity) {
    return authentication_service_->cached_mdm_infos_.count(
               base::SysNSStringToUTF8([identity gaiaID])) > 0;
  }

  void OnRefreshTokenAvailable(const std::string& account_id) override {
    refresh_token_available_count_++;
  }

  web::TestWebThreadBundle thread_bundle_;
  IOSChromeScopedTestingChromeBrowserStateManager scoped_browser_state_manager_;
  std::unique_ptr<TestChromeBrowserState> browser_state_;
  ios::FakeChromeIdentityService* identity_service_;
  browser_sync::ProfileSyncServiceMock* profile_sync_service_mock_;
  SyncSetupServiceMock* sync_setup_service_mock_;
  SigninManager* signin_manager_;
  std::unique_ptr<AuthenticationService> authentication_service_;
  ChromeIdentity* identity_;
  ChromeIdentity* identity2_;
  int refresh_token_available_count_;
};

TEST_F(AuthenticationServiceTest, TestDefaultGetAuthenticatedIdentity) {
  EXPECT_FALSE(authentication_service_->GetAuthenticatedIdentity());
}

TEST_F(AuthenticationServiceTest, TestSignInAndGetAuthenticatedIdentity) {
  // Sign in.
  SetExpectationsForSignIn();
  authentication_service_->SignIn(identity_, std::string());

  EXPECT_NSEQ(identity_, authentication_service_->GetAuthenticatedIdentity());

  ProfileOAuth2TokenService* token_service =
      ProfileOAuth2TokenServiceFactory::GetForBrowserState(
          browser_state_.get());
  AccountTrackerService* account_tracker =
      ios::AccountTrackerServiceFactory::GetForBrowserState(
          browser_state_.get());

  std::string user_email = base::SysNSStringToUTF8([identity_ userEmail]);
  AccountInfo account_info =
      account_tracker->FindAccountInfoByEmail(user_email);
  EXPECT_EQ(user_email, account_info.email);
  EXPECT_EQ(base::SysNSStringToUTF8([identity_ gaiaID]), account_info.gaia);
  EXPECT_TRUE(token_service->RefreshTokenIsAvailable(account_info.account_id));
}

TEST_F(AuthenticationServiceTest, TestSetPromptForSignIn) {
  // Verify that the default value of this flag is off.
  EXPECT_FALSE(authentication_service_->ShouldPromptForSignIn());
  // Verify that prompt-flag setter and getter functions are working correctly.
  authentication_service_->SetPromptForSignIn(true);
  EXPECT_TRUE(authentication_service_->ShouldPromptForSignIn());
  authentication_service_->SetPromptForSignIn(false);
  EXPECT_FALSE(authentication_service_->ShouldPromptForSignIn());
}

TEST_F(AuthenticationServiceTest, OnAppEnterForegroundWithSyncSetupCompleted) {
  // Sign in.
  SetExpectationsForSignIn();
  authentication_service_->SignIn(identity_, std::string());

  EXPECT_CALL(*sync_setup_service_mock_, HasFinishedInitialSetup())
      .WillOnce(Return(true));

  CreateAuthenticationService();

  EXPECT_EQ(base::SysNSStringToUTF8([identity_ userEmail]),
            signin_manager_->GetAuthenticatedAccountInfo().email);
  EXPECT_NSEQ([identity_ userEmail],
              authentication_service_->GetAuthenticatedUserEmail());
  EXPECT_EQ(identity_, authentication_service_->GetAuthenticatedIdentity());
}

TEST_F(AuthenticationServiceTest, OnAppEnterForegroundWithSyncDisabled) {
  // Sign in.
  SetExpectationsForSignIn();
  authentication_service_->SignIn(identity_, std::string());

  EXPECT_CALL(*sync_setup_service_mock_, HasFinishedInitialSetup())
      .WillOnce(Invoke(
          sync_setup_service_mock_,
          &SyncSetupServiceMock::SyncSetupServiceHasFinishedInitialSetup));
  EXPECT_CALL(*profile_sync_service_mock_, GetDisableReasons())
      .WillOnce(Return(syncer::SyncService::DISABLE_REASON_USER_CHOICE));

  CreateAuthenticationService();

  EXPECT_EQ(base::SysNSStringToUTF8([identity_ userEmail]),
            signin_manager_->GetAuthenticatedAccountInfo().email);
  EXPECT_NSEQ([identity_ userEmail],
              authentication_service_->GetAuthenticatedUserEmail());
  EXPECT_EQ(identity_, authentication_service_->GetAuthenticatedIdentity());
}

TEST_F(AuthenticationServiceTest, OnAppEnterForegroundWithSyncNotConfigured) {
  // Sign in.
  SetExpectationsForSignIn();
  authentication_service_->SignIn(identity_, std::string());

  // User is signed out if sync initial setup isn't completed.
  EXPECT_CALL(*sync_setup_service_mock_, HasFinishedInitialSetup())
      .WillOnce(Return(false));
  // Expect a call to disable sync as part of the sign out process.
  EXPECT_CALL(*profile_sync_service_mock_,
              RequestStop(syncer::SyncService::CLEAR_DATA));

  CreateAuthenticationService();

  EXPECT_EQ("", signin_manager_->GetAuthenticatedAccountInfo().email);
  EXPECT_NSEQ(nil, authentication_service_->GetAuthenticatedUserEmail());
  EXPECT_FALSE(authentication_service_->GetAuthenticatedIdentity());
}

TEST_F(AuthenticationServiceTest, TestHandleForgottenIdentityNoPromptSignIn) {
  // Sign in.
  SetExpectationsForSignIn();
  authentication_service_->SignIn(identity_, std::string());

  // Set the authentication service as "In Foreground", remove identity and run
  // the loop.
  SetAuthenticationServiceInForeground(false);
  FireApplicationEnterForeground();
  identity_service_->ForgetIdentity(identity_, nil);
  base::RunLoop().RunUntilIdle();

  // User is signed out (no corresponding identity), but not prompted for sign
  // in (as the action was user initiated).
  EXPECT_EQ("", signin_manager_->GetAuthenticatedAccountInfo().email);
  EXPECT_FALSE(authentication_service_->GetAuthenticatedIdentity());
  EXPECT_FALSE(authentication_service_->ShouldPromptForSignIn());
}

TEST_F(AuthenticationServiceTest, TestHandleForgottenIdentityPromptSignIn) {
  // Sign in.
  SetExpectationsForSignIn();
  authentication_service_->SignIn(identity_, std::string());

  // Set the authentication service as "In Background", remove identity and run
  // the loop.
  FireApplicationEnterBackground();
  identity_service_->ForgetIdentity(identity_, nil);
  base::RunLoop().RunUntilIdle();

  // User is signed out (no corresponding identity), but not prompted for sign
  // in (as the action was user initiated).
  EXPECT_EQ("", signin_manager_->GetAuthenticatedAccountInfo().email);
  EXPECT_FALSE(authentication_service_->GetAuthenticatedIdentity());
  EXPECT_TRUE(authentication_service_->ShouldPromptForSignIn());
}

TEST_F(AuthenticationServiceTest, StoreAndGetAccountsInPrefs) {
  // Profile starts empty.
  std::vector<std::string> accounts = GetAccountsInPrefs();
  EXPECT_TRUE(accounts.empty());

  // Sign in.
  SetExpectationsForSignIn();
  authentication_service_->SignIn(identity_, std::string());

  // Store the accounts and get them back from the prefs. They should be the
  // same as the token service accounts.
  StoreAccountsInPrefs();
  accounts = GetAccountsInPrefs();
  ASSERT_EQ(2u, accounts.size());
  AccountTrackerService* account_tracker =
      ios::AccountTrackerServiceFactory::GetForBrowserState(
          browser_state_.get());
  switch (account_tracker->GetMigrationState()) {
    case AccountTrackerService::MIGRATION_NOT_STARTED:
      EXPECT_EQ("foo2@foo.com", accounts[0]);
      EXPECT_EQ("foo@foo.com", accounts[1]);
      break;
    case AccountTrackerService::MIGRATION_IN_PROGRESS:
    case AccountTrackerService::MIGRATION_DONE:
      EXPECT_EQ("foo2ID", accounts[0]);
      EXPECT_EQ("fooID", accounts[1]);
      break;
    case AccountTrackerService::NUM_MIGRATION_STATES:
      FAIL() << "NUM_MIGRATION_STATES is not a real migration state.";
      break;
  }
}

TEST_F(AuthenticationServiceTest,
       OnApplicationEnterForegroundReloadCredentials) {
  // Sign in.
  SetExpectationsForSignIn();
  authentication_service_->SignIn(identity_, std::string());

  identity_service_->AddIdentities(@[ @"foo3" ]);
  ProfileOAuth2TokenService* token_service =
      ProfileOAuth2TokenServiceFactory::GetForBrowserState(
          browser_state_.get());
  std::vector<std::string> accounts = token_service->GetAccounts();
  std::sort(accounts.begin(), accounts.end());
  ASSERT_EQ(2u, accounts.size());
  AccountTrackerService* account_tracker =
      ios::AccountTrackerServiceFactory::GetForBrowserState(
          browser_state_.get());
  switch (account_tracker->GetMigrationState()) {
    case AccountTrackerService::MIGRATION_NOT_STARTED:
      EXPECT_EQ("foo2@foo.com", accounts[0]);
      EXPECT_EQ("foo@foo.com", accounts[1]);
      break;
    case AccountTrackerService::MIGRATION_IN_PROGRESS:
    case AccountTrackerService::MIGRATION_DONE:
      EXPECT_EQ("foo2ID", accounts[0]);
      EXPECT_EQ("fooID", accounts[1]);
      break;
    case AccountTrackerService::NUM_MIGRATION_STATES:
      FAIL() << "NUM_MIGRATION_STATES is not a real migration state.";
      break;
  }

  // Simulate a switching to background and back to foreground, triggering a
  // credentials reload.
  FireApplicationEnterBackground();
  FireApplicationEnterForeground();

  // Accounts are reloaded, "foo3@foo.com" is added as it is now in
  // ChromeIdentityService.
  accounts = token_service->GetAccounts();
  std::sort(accounts.begin(), accounts.end());
  ASSERT_EQ(3u, accounts.size());
  switch (account_tracker->GetMigrationState()) {
    case AccountTrackerService::MIGRATION_NOT_STARTED:
      EXPECT_EQ("foo2@foo.com", accounts[0]);
      EXPECT_EQ("foo3@foo.com", accounts[1]);
      EXPECT_EQ("foo@foo.com", accounts[2]);
      break;
    case AccountTrackerService::MIGRATION_IN_PROGRESS:
    case AccountTrackerService::MIGRATION_DONE:
      EXPECT_EQ("foo2ID", accounts[0]);
      EXPECT_EQ("foo3ID", accounts[1]);
      EXPECT_EQ("fooID", accounts[2]);
      break;
    case AccountTrackerService::NUM_MIGRATION_STATES:
      FAIL() << "NUM_MIGRATION_STATES is not a real migration state.";
      break;
  }
}

TEST_F(AuthenticationServiceTest, HaveAccountsNotChangedDefault) {
  EXPECT_FALSE(authentication_service_->HaveAccountsChanged());
}

TEST_F(AuthenticationServiceTest, HaveAccountsNotChanged) {
  SetExpectationsForSignIn();
  authentication_service_->SignIn(identity_, std::string());

  identity_service_->AddIdentities(@[ @"foo3" ]);
  FireIdentityListChanged();
  base::RunLoop().RunUntilIdle();

  // Simulate a switching to background and back to foreground.
  FireApplicationEnterBackground();
  FireApplicationEnterForeground();

  EXPECT_FALSE(authentication_service_->HaveAccountsChanged());
}

TEST_F(AuthenticationServiceTest, HaveAccountsChanged) {
  SetExpectationsForSignIn();
  authentication_service_->SignIn(identity_, std::string());

  identity_service_->AddIdentities(@[ @"foo3" ]);
  FireIdentityListChanged();
  base::RunLoop().RunUntilIdle();

  // Simulate a switching to background and back to foreground, changing the
  // accounts while in background.
  FireApplicationEnterBackground();
  identity_service_->AddIdentities(@[ @"foo4" ]);
  FireApplicationEnterForeground();

  EXPECT_TRUE(authentication_service_->HaveAccountsChanged());
}

TEST_F(AuthenticationServiceTest, HaveAccountsChangedBackground) {
  SetExpectationsForSignIn();
  authentication_service_->SignIn(identity_, std::string());

  identity_service_->AddIdentities(@[ @"foo3" ]);
  FireIdentityListChanged();
  base::RunLoop().RunUntilIdle();

  // Simulate a switching to background, changing the accounts while in
  // background.
  FireApplicationEnterBackground();
  identity_service_->AddIdentities(@[ @"foo4" ]);

  EXPECT_TRUE(authentication_service_->HaveAccountsChanged());
}

TEST_F(AuthenticationServiceTest, IsAuthenticatedBackground) {
  // Sign in.
  SetExpectationsForSignIn();
  authentication_service_->SignIn(identity_, std::string());
  EXPECT_TRUE(authentication_service_->IsAuthenticated());

  // Remove the signed in identity while in background, and check that
  // IsAuthenticated is up-to-date.
  FireApplicationEnterBackground();
  identity_service_->ForgetIdentity(identity_, nil);
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(authentication_service_->IsAuthenticated());
}

TEST_F(AuthenticationServiceTest, MigrateAccountsStoredInPref) {
  AccountTrackerService* account_tracker =
      ios::AccountTrackerServiceFactory::GetForBrowserState(
          browser_state_.get());
  if (account_tracker->GetMigrationState() ==
      AccountTrackerService::MIGRATION_NOT_STARTED) {
    // The account tracker is not migratable. Skip the test as the accounts
    // cannot be migrated.
    return;
  }

  // Force the migration state to MIGRATION_NOT_STARTED before signing in.
  browser_state_->GetPrefs()->SetInteger(
      prefs::kAccountIdMigrationState,
      AccountTrackerService::MIGRATION_NOT_STARTED);
  browser_state_->GetPrefs()->SetBoolean(prefs::kSigninLastAccountsMigrated,
                                         false);

  // Sign in user emails as account ids.
  SetExpectationsForSignIn();
  authentication_service_->SignIn(identity_, std::string());
  std::vector<std::string> accounts_in_prefs = GetAccountsInPrefs();
  ASSERT_EQ(2U, accounts_in_prefs.size());
  EXPECT_EQ("foo2@foo.com", accounts_in_prefs[0]);
  EXPECT_EQ("foo@foo.com", accounts_in_prefs[1]);

  // Migrate the accounts (this actually requires a shutdown and re-initialize
  // of the account tracker).
  account_tracker->Shutdown();
  account_tracker->Initialize(browser_state_->GetPrefs(), base::FilePath());
  account_tracker->SetMigrationDone();

  // Actually migrate the accounts in prefs.
  MigrateAccountsStoredInPrefsIfNeeded();
  std::vector<std::string> migrated_accounts_in_prefs = GetAccountsInPrefs();
  ASSERT_EQ(2U, migrated_accounts_in_prefs.size());
  EXPECT_EQ("foo2ID", migrated_accounts_in_prefs[0]);
  EXPECT_EQ("fooID", migrated_accounts_in_prefs[1]);
  EXPECT_TRUE(browser_state_->GetPrefs()->GetBoolean(
      prefs::kSigninLastAccountsMigrated));

  // Calling migrate after the migration is done is a no-op.
  MigrateAccountsStoredInPrefsIfNeeded();
  EXPECT_EQ(migrated_accounts_in_prefs, GetAccountsInPrefs());
}

// Tests that MDM errors are correctly cleared on foregrounding, sending
// refresh token available notifications.
TEST_F(AuthenticationServiceTest, MDMErrorsClearedOnForeground) {
  SetExpectationsForSignIn();
  authentication_service_->SignIn(identity_, std::string());
  EXPECT_EQ(2, refresh_token_available_count_);

  NSDictionary* user_info = [NSDictionary dictionary];
  SetCachedMDMInfo(identity_, user_info);
  GoogleServiceAuthError error(
      GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS);
  ProfileOAuth2TokenServiceFactory::GetForBrowserState(browser_state_.get())
      ->GetDelegate()
      ->UpdateAuthError(base::SysNSStringToUTF8([identity_ gaiaID]), error);
  EXPECT_EQ(2, refresh_token_available_count_);

  // MDM error for |identity_| is being cleared, refresh token available
  // notification will be sent.
  FireApplicationEnterBackground();
  FireApplicationEnterForeground();
  EXPECT_EQ(3, refresh_token_available_count_);

  // MDM error has already been cleared, no notification will be sent.
  FireApplicationEnterBackground();
  FireApplicationEnterForeground();
  EXPECT_EQ(3, refresh_token_available_count_);
}

// Tests that MDM errors are correctly cleared when signing out, without sending
// refresh token available notifications.
TEST_F(AuthenticationServiceTest, MDMErrorsClearedOnSignout) {
  SetExpectationsForSignIn();
  authentication_service_->SignIn(identity_, std::string());

  NSDictionary* user_info = [NSDictionary dictionary];
  SetCachedMDMInfo(identity_, user_info);
  int refresh_token_available_count_before_signout =
      refresh_token_available_count_;

  authentication_service_->SignOut(signin_metrics::ABORT_SIGNIN, nil);
  EXPECT_FALSE(HasCachedMDMInfo(identity_));
  EXPECT_EQ(refresh_token_available_count_before_signout,
            refresh_token_available_count_);
}

// Tests that potential MDM notifications are correctly handled and dispatched
// to MDM service when necessary.
TEST_F(AuthenticationServiceTest, HandleMDMNotification) {
  SetExpectationsForSignIn();
  authentication_service_->SignIn(identity_, std::string());
  GoogleServiceAuthError error(
      GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS);
  ProfileOAuth2TokenServiceFactory::GetForBrowserState(browser_state_.get())
      ->GetDelegate()
      ->UpdateAuthError(base::SysNSStringToUTF8([identity_ gaiaID]), error);

  NSDictionary* user_info1 = @{ @"foo" : @1 };
  ON_CALL(*identity_service_, GetMDMDeviceStatus(user_info1))
      .WillByDefault(Return(1));
  NSDictionary* user_info2 = @{ @"foo" : @2 };
  ON_CALL(*identity_service_, GetMDMDeviceStatus(user_info2))
      .WillByDefault(Return(2));

  // Notification will show the MDM dialog the first time.
  EXPECT_CALL(*identity_service_,
              HandleMDMNotification(identity_, user_info1, _))
      .WillOnce(Return(true));
  FireAccessTokenRefreshFailed(identity_, user_info1);

  // Same notification won't show the MDM dialog the second time.
  EXPECT_CALL(*identity_service_,
              HandleMDMNotification(identity_, user_info1, _))
      .Times(0);
  FireAccessTokenRefreshFailed(identity_, user_info1);

  // New notification will show the MDM dialog on the same identity.
  EXPECT_CALL(*identity_service_,
              HandleMDMNotification(identity_, user_info2, _))
      .WillOnce(Return(true));
  FireAccessTokenRefreshFailed(identity_, user_info2);
}

// Tests that MDM blocked notifications are correctly signing out the user if
// the primary account is blocked.
TEST_F(AuthenticationServiceTest, HandleMDMBlockedNotification) {
  SetExpectationsForSignIn();
  authentication_service_->SignIn(identity_, std::string());
  GoogleServiceAuthError error(
      GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS);
  ProfileOAuth2TokenServiceFactory::GetForBrowserState(browser_state_.get())
      ->GetDelegate()
      ->UpdateAuthError(base::SysNSStringToUTF8([identity_ gaiaID]), error);

  NSDictionary* user_info1 = @{ @"foo" : @1 };
  ON_CALL(*identity_service_, GetMDMDeviceStatus(user_info1))
      .WillByDefault(Return(1));

  auto handle_mdm_notification_callback = [](ChromeIdentity*, NSDictionary*,
                                             ios::MDMStatusCallback callback) {
    callback(true /* is_blocked */);
    return true;
  };

  // User not signed out as |identity2_| isn't the primary account.
  EXPECT_CALL(*identity_service_,
              HandleMDMNotification(identity2_, user_info1, _))
      .WillOnce(Invoke(handle_mdm_notification_callback));
  FireAccessTokenRefreshFailed(identity2_, user_info1);
  EXPECT_TRUE(authentication_service_->IsAuthenticated());

  // User signed out as |identity_| is the primary account.
  EXPECT_CALL(*identity_service_,
              HandleMDMNotification(identity_, user_info1, _))
      .WillOnce(Invoke(handle_mdm_notification_callback));
  FireAccessTokenRefreshFailed(identity_, user_info1);
  EXPECT_FALSE(authentication_service_->IsAuthenticated());
}

// Tests that MDM dialog isn't shown when there is no cached MDM error.
TEST_F(AuthenticationServiceTest, ShowMDMErrorDialogNoCachedError) {
  EXPECT_CALL(*identity_service_, HandleMDMNotification(identity_, _, _))
      .Times(0);

  EXPECT_FALSE(
      authentication_service_->ShowMDMErrorDialogForIdentity(identity_));
}

// Tests that MDM dialog isn't shown when there is a cached MDM error but no
// corresponding error for the account.
TEST_F(AuthenticationServiceTest, ShowMDMErrorDialogInvalidCachedError) {
  NSDictionary* user_info = [NSDictionary dictionary];
  SetCachedMDMInfo(identity_, user_info);

  EXPECT_CALL(*identity_service_,
              HandleMDMNotification(identity_, user_info, _))
      .Times(0);

  EXPECT_FALSE(
      authentication_service_->ShowMDMErrorDialogForIdentity(identity_));
}

// Tests that MDM dialog is shown when there is a cached error and a
// corresponding error for the account.
TEST_F(AuthenticationServiceTest, ShowMDMErrorDialog) {
  SetExpectationsForSignIn();
  authentication_service_->SignIn(identity_, std::string());
  GoogleServiceAuthError error(
      GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS);
  ProfileOAuth2TokenServiceFactory::GetForBrowserState(browser_state_.get())
      ->GetDelegate()
      ->UpdateAuthError(base::SysNSStringToUTF8([identity_ gaiaID]), error);

  NSDictionary* user_info = [NSDictionary dictionary];
  SetCachedMDMInfo(identity_, user_info);

  EXPECT_CALL(*identity_service_,
              HandleMDMNotification(identity_, user_info, _))
      .WillOnce(Return(true));

  EXPECT_TRUE(
      authentication_service_->ShowMDMErrorDialogForIdentity(identity_));
}
