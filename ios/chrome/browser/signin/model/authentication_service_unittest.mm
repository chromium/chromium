// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/signin/model/authentication_service.h"

#import <memory>

#import "base/apple/foundation_util.h"
#import "base/functional/bind.h"
#import "base/functional/callback_helpers.h"
#import "base/memory/raw_ptr.h"
#import "base/run_loop.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/bind.h"
#import "base/test/gtest_util.h"
#import "base/test/metrics/histogram_tester.h"
#import "base/test/scoped_feature_list.h"
#import "components/keyed_service/core/service_access_type.h"
#import "components/policy/core/common/policy_loader_ios_constants.h"
#import "components/pref_registry/pref_registry_syncable.h"
#import "components/prefs/pref_registry_simple.h"
#import "components/signin/ios/browser/features.h"
#import "components/signin/public/base/signin_pref_names.h"
#import "components/signin/public/base/signin_switches.h"
#import "components/signin/public/identity_manager/device_accounts_synchronizer.h"
#import "components/signin/public/identity_manager/identity_manager.h"
#import "components/signin/public/identity_manager/identity_test_environment.h"
#import "components/signin/public/identity_manager/identity_test_utils.h"
#import "components/signin/public/identity_manager/test_identity_manager_observer.h"
#import "components/sync/test/mock_sync_service.h"
#import "components/sync_preferences/pref_service_mock_factory.h"
#import "components/sync_preferences/pref_service_syncable.h"
#import "ios/chrome/browser/content_settings/model/cookie_settings_factory.h"
#import "ios/chrome/browser/content_settings/model/host_content_settings_map_factory.h"
#import "ios/chrome/browser/policy/model/policy_util.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/prefs/browser_prefs.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/public/features/system_flags.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/authentication_service_observer.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service_factory.h"
#import "ios/chrome/browser/signin/model/fake_authentication_service_delegate.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/browser/signin/model/fake_system_identity_manager.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/signin/model/refresh_access_token_error.h"
#import "ios/chrome/browser/signin/model/system_identity.h"
#import "ios/chrome/browser/sync/model/mock_sync_service_utils.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

using testing::_;
using testing::Invoke;
using testing::Return;

using HandleMDMCallback = FakeSystemIdentityManager::HandleMDMCallback;
using HandleMDMNotificationCallback =
    FakeSystemIdentityManager::HandleMDMNotificationCallback;

namespace {

CoreAccountId GetAccountId(id<SystemIdentity> identity) {
  return CoreAccountId::FromGaiaId(base::SysNSStringToUTF8([identity gaiaID]));
}

}  // namespace

class AuthenticationServiceObserverTest : public AuthenticationServiceObserver {
 public:
  void OnPrimaryAccountRestricted() override {
    ++on_primary_account_restricted_counter_;
  }

  int GetOnPrimaryAccountRestrictedCounter() {
    return on_primary_account_restricted_counter_;
  }

  void OnServiceStatusChanged() override {
    ++on_service_status_changed_counter_;
  }

  int GetOnServiceStatusChangedCounter() {
    return on_service_status_changed_counter_;
  }

 private:
  int on_primary_account_restricted_counter_ = 0;
  int on_service_status_changed_counter_ = 0;
};

class AuthenticationServiceTest : public PlatformTest {
 protected:
  AuthenticationServiceTest() : identity_test_env_() {
    fake_system_identity1_ = [FakeSystemIdentity fakeIdentity1];
    fake_system_identity_manager()->AddIdentity(fake_system_identity1_);
    fake_system_identity2_ = [FakeSystemIdentity fakeIdentity2];
    fake_system_identity_manager()->AddIdentity(fake_system_identity2_);

    TestProfileIOS::Builder builder;
    builder.SetPrefService(CreatePrefService());
    builder.AddTestingFactory(SyncServiceFactory::GetInstance(),
                              base::BindRepeating(&CreateMockSyncService));
    builder.AddTestingFactory(
        AuthenticationServiceFactory::GetInstance(),
        AuthenticationServiceFactory::GetDefaultFactory());

    profile_ = std::move(builder).Build();

    account_manager_ =
        ChromeAccountManagerServiceFactory::GetForProfile(profile_.get());

    AuthenticationServiceFactory::CreateAndInitializeForProfile(
        profile_.get(), std::make_unique<FakeAuthenticationServiceDelegate>());
  }

  std::unique_ptr<sync_preferences::PrefServiceSyncable> CreatePrefService() {
    sync_preferences::PrefServiceMockFactory factory;
    scoped_refptr<user_prefs::PrefRegistrySyncable> registry(
        new user_prefs::PrefRegistrySyncable);
    std::unique_ptr<sync_preferences::PrefServiceSyncable> prefs =
        factory.CreateSyncable(registry.get());
    RegisterProfilePrefs(registry.get());
    return prefs;
  }

  void VerifyLastSigninTimestamp() {
    EXPECT_EQ(profile_.get()->GetPrefs()->GetTime(prefs::kLastSigninTimestamp),
              base::Time::Now());
  }

  void FireApplicationWillEnterForeground() {
    authentication_service()->OnApplicationWillEnterForeground();
  }

  void FireAccessTokenRefreshFailed(id<SystemIdentity> identity,
                                    id<RefreshAccessTokenError> error) {
    authentication_service()->OnAccessTokenRefreshFailed(identity, error);
  }

  // Simulates that fetching access token for `identity` fails with a given
  // error identifier. Returns the MDM error information.
  //
  // `invocation_counter` will be incremented each time `HandleMDMNotification`
  // is invoked with the returned error object (unless a new error is created).
  // The pointer mush outlive the use of the returned error object. Using a
  // stack allocated value in a test case should be enough.
  //
  // The callback passed to `HandleMDMNotification` will be invoked with the
  // value of `is_identity_blocked`.
  id<RefreshAccessTokenError> CreateRefreshAccessTokenError(
      id<SystemIdentity> identity,
      uint32_t* invocation_counter = nullptr,
      bool is_identity_blocked = false) {
    return fake_system_identity_manager()->CreateRefreshAccessTokenFailure(
        identity,
        base::BindRepeating(
            [](uint32_t* counter, bool is_blocked, HandleMDMCallback callback) {
              if (counter) {
                ++*counter;
              }
              std::move(callback).Run(is_blocked);
            },
            invocation_counter, is_identity_blocked));
  }

  void SetCachedMDMInfo(id<SystemIdentity> identity,
                        id<RefreshAccessTokenError> mdm_error) {
    auto& cached_mdm_errors = authentication_service()->cached_mdm_errors_;
    cached_mdm_errors[GetAccountId(identity)] = mdm_error;
  }

  id<RefreshAccessTokenError> GetCachedMDMInfo(id<SystemIdentity> identity) {
    auto& cached_mdm_errors = authentication_service()->cached_mdm_errors_;
    auto iterator = cached_mdm_errors.find(GetAccountId(identity));
    return iterator == cached_mdm_errors.end() ? nil : iterator->second;
  }

  bool HasCachedMDMInfo(id<SystemIdentity> identity) {
    return GetCachedMDMInfo(identity) != nil;
  }

  int ClearBrowsingDataCount() {
    return authentication_service()->delegate_->clear_browsing_data_counter_;
  }

  int ClearBrowsingDataFromSigninCount() {
    return authentication_service()
        ->delegate_->clear_browsing_data_from_signin_counter_;
  }

  AuthenticationService* authentication_service() {
    return AuthenticationServiceFactory::GetForProfile(profile_.get());
  }

  signin::IdentityManager* identity_manager() {
    return IdentityManagerFactory::GetForProfile(profile_.get());
  }

  FakeSystemIdentityManager* fake_system_identity_manager() {
    return FakeSystemIdentityManager::FromSystemIdentityManager(
        GetApplicationContext()->GetSystemIdentityManager());
  }

  syncer::MockSyncService* mock_sync_service() {
    return static_cast<syncer::MockSyncService*>(
        SyncServiceFactory::GetForProfile(profile_.get()));
  }

  id<SystemIdentity> identity(NSUInteger index) {
    return [account_manager_->GetAllIdentities() objectAtIndex:index];
  }

  // Sets a restricted pattern.
  void SetPattern(const std::string& pattern) {
    base::Value::List allowed_patterns;
    allowed_patterns.Append(pattern);
    GetApplicationContext()->GetLocalState()->SetList(
        prefs::kRestrictAccountsToPatterns, std::move(allowed_patterns));
  }

  PrefService* local_state() {
    return GetApplicationContext()->GetLocalState();
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  web::WebTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  raw_ptr<ChromeAccountManagerService> account_manager_;
  signin::IdentityTestEnvironment identity_test_env_;
  std::unique_ptr<TestProfileIOS> profile_;
  // Used to verify histogram logging.
  base::HistogramTester histogram_tester_;
  FakeSystemIdentity* fake_system_identity1_;
  FakeSystemIdentity* fake_system_identity2_;
};

TEST_F(AuthenticationServiceTest, TestDefaultGetPrimaryIdentity) {
  EXPECT_FALSE(authentication_service()->GetPrimaryIdentity(
      signin::ConsentLevel::kSignin));
  EXPECT_FALSE(authentication_service()->HasPrimaryIdentity(
      signin::ConsentLevel::kSignin));
}

TEST_F(AuthenticationServiceTest, TestSignInAndGetPrimaryIdentity) {
  // Sign in.
  authentication_service()->SignIn(
      identity(0), signin_metrics::AccessPoint::ACCESS_POINT_SIGNIN_PROMO);
  VerifyLastSigninTimestamp();

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
  histogram_tester_.ExpectUniqueSample(
      "Signin.SignIn.Completed",
      signin_metrics::AccessPoint::ACCESS_POINT_SIGNIN_PROMO, 1);
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
TEST_F(AuthenticationServiceTest,
       TestHandleForgottenIdentityNoPromptSignIn_SyncingUser) {
  // Sign in.
  authentication_service()->SignIn(
      identity(0), signin_metrics::AccessPoint::ACCESS_POINT_UNKNOWN);
  authentication_service()->GrantSyncConsent(
      identity(0), signin_metrics::AccessPoint::ACCESS_POINT_UNKNOWN);
  VerifyLastSigninTimestamp();

  // Set the authentication service as "In Foreground", remove identity and run
  // the loop.
  FireApplicationWillEnterForeground();
  fake_system_identity_manager()->ForgetIdentity(identity(0),
                                                 base::DoNothing());
  base::RunLoop().RunUntilIdle();

  // User is signed out (no corresponding identity), but not prompted for sign
  // in (as the action was user initiated).
  EXPECT_FALSE(authentication_service()->HasPrimaryIdentity(
      signin::ConsentLevel::kSignin));
  EXPECT_FALSE(authentication_service()->ShouldReauthPromptForSignInAndSync());
}

// Tests that reauth prompt is set if the primary identity is remove from
// an other app when the user was signed and syncing.
TEST_F(AuthenticationServiceTest, TestHandleForgottenIdentityPromptSignIn) {
  // Sign in.
  authentication_service()->SignIn(
      identity(0), signin_metrics::AccessPoint::ACCESS_POINT_UNKNOWN);
  authentication_service()->GrantSyncConsent(
      identity(0), signin_metrics::AccessPoint::ACCESS_POINT_UNKNOWN);
  VerifyLastSigninTimestamp();

  // Set the authentication service as "In Background", remove identity and run
  // the loop.
  fake_system_identity_manager()->ForgetIdentityFromOtherApplication(
      identity(0));
  base::RunLoop().RunUntilIdle();

  // User is signed out (no corresponding identity), and reauth prompt is set.
  EXPECT_FALSE(authentication_service()->HasPrimaryIdentity(
      signin::ConsentLevel::kSignin));
  EXPECT_TRUE(authentication_service()->ShouldReauthPromptForSignInAndSync());
}

// The reauth prompt should be shown if the primary identity is remove from an
// other app when the user was signed in.
TEST_F(AuthenticationServiceTest,
       TestHandleForgottenIdentityNoPromptSignIn_NonSyncingUser) {
  // Sign in.
  authentication_service()->SignIn(
      identity(0), signin_metrics::AccessPoint::ACCESS_POINT_UNKNOWN);
  VerifyLastSigninTimestamp();

  // Set the authentication service as "In Background", remove identity and run
  // the loop.
  fake_system_identity_manager()->ForgetIdentityFromOtherApplication(
      identity(0));
  base::RunLoop().RunUntilIdle();

  // User is signed out (no corresponding identity), and reauth prompt is set.
  EXPECT_FALSE(authentication_service()->HasPrimaryIdentity(
      signin::ConsentLevel::kSignin));
  EXPECT_TRUE(authentication_service()->ShouldReauthPromptForSignInAndSync());
}

TEST_F(AuthenticationServiceTest,
       OnApplicationEnterForegroundReloadCredentials) {
  // Sign in.
  authentication_service()->SignIn(
      identity(0), signin_metrics::AccessPoint::ACCESS_POINT_UNKNOWN);
  VerifyLastSigninTimestamp();

  FakeSystemIdentity* fake_system_identity3 =
      [FakeSystemIdentity fakeIdentity3];
  fake_system_identity_manager()->AddIdentity(fake_system_identity3);

  auto account_compare_func = [](const CoreAccountInfo& first,
                                 const CoreAccountInfo& second) {
    return first.account_id < second.account_id;
  };
  std::vector<CoreAccountInfo> accounts =
      identity_manager()->GetAccountsWithRefreshTokens();
  std::sort(accounts.begin(), accounts.end(), account_compare_func);
  ASSERT_EQ(2u, accounts.size());
  CoreAccountId gaiad_id_1 = CoreAccountId::FromGaiaId(
      base::SysNSStringToUTF8(fake_system_identity1_.gaiaID));
  EXPECT_EQ(gaiad_id_1, accounts[0].account_id);
  CoreAccountId gaiad_id_2 = CoreAccountId::FromGaiaId(
      base::SysNSStringToUTF8(fake_system_identity2_.gaiaID));
  EXPECT_EQ(gaiad_id_2, accounts[1].account_id);

  // Simulate a switching to background and back to foreground, triggering a
  // credentials reload.
  fake_system_identity_manager()->FireSystemIdentityReloaded();
  base::RunLoop().RunUntilIdle();

  // Accounts are reloaded, "foo3@foo.com" is added as it is now in
  // ChromeIdentityService.
  accounts = identity_manager()->GetAccountsWithRefreshTokens();
  std::sort(accounts.begin(), accounts.end(), account_compare_func);
  ASSERT_EQ(3u, accounts.size());
  EXPECT_EQ(gaiad_id_1, accounts[0].account_id);
  EXPECT_EQ(gaiad_id_2, accounts[1].account_id);
  CoreAccountId gaiad_id_3 = CoreAccountId::FromGaiaId(
      base::SysNSStringToUTF8(fake_system_identity3.gaiaID));
  EXPECT_EQ(gaiad_id_3, accounts[2].account_id);
}

TEST_F(AuthenticationServiceTest, HasPrimaryIdentityBackground) {
  // Sign in.
  authentication_service()->SignIn(
      identity(0), signin_metrics::AccessPoint::ACCESS_POINT_UNKNOWN);
  EXPECT_TRUE(authentication_service()->HasPrimaryIdentity(
      signin::ConsentLevel::kSignin));
  VerifyLastSigninTimestamp();

  // Remove the signed in identity while in background, and check that
  // HasPrimaryIdentity is up-to-date.
  fake_system_identity_manager()->ForgetIdentity(identity(0),
                                                 base::DoNothing());
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(authentication_service()->HasPrimaryIdentity(
      signin::ConsentLevel::kSignin));
}

// Tests that MDM errors are correctly cleared on foregrounding, sending
// notifications that the state of error has changed.
TEST_F(AuthenticationServiceTest, MDMErrorsClearedOnForeground) {
  authentication_service()->SignIn(
      identity(0), signin_metrics::AccessPoint::ACCESS_POINT_UNKNOWN);
  EXPECT_EQ(identity_manager()->GetAccountsWithRefreshTokens().size(), 2UL);
  VerifyLastSigninTimestamp();

  SetCachedMDMInfo(identity(0), CreateRefreshAccessTokenError(identity(0)));

  GoogleServiceAuthError error(
      GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS);
  signin::UpdatePersistentErrorOfRefreshTokenForAccount(
      identity_manager(), GetAccountId(identity(0)), error);

  // MDM error for `identity_` is being cleared and the error state of refresh
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
  authentication_service()->SignIn(
      identity(0), signin_metrics::AccessPoint::ACCESS_POINT_UNKNOWN);
  EXPECT_EQ(identity_manager()->GetAccountsWithRefreshTokens().size(), 2UL);
  VerifyLastSigninTimestamp();

  SetCachedMDMInfo(identity(0), CreateRefreshAccessTokenError(identity(0)));
  authentication_service()->SignOut(
      signin_metrics::ProfileSignout::kAbortSignin,
      /*force_clear_browsing_data=*/false, nil);
  EXPECT_FALSE(HasCachedMDMInfo(identity(0)));
  EXPECT_EQ(identity_manager()->GetAccountsWithRefreshTokens().size(), 0UL);
  EXPECT_EQ(ClearBrowsingDataCount(), 0);
}

// Tests that MDM errors are correctly cleared when signing out with clearing
// browsing data.
TEST_F(AuthenticationServiceTest,
       MDMErrorsClearedOnSignoutAndClearBrowsingData) {
  authentication_service()->SignIn(
      identity(0), signin_metrics::AccessPoint::ACCESS_POINT_UNKNOWN);
  EXPECT_EQ(identity_manager()->GetAccountsWithRefreshTokens().size(), 2UL);
  VerifyLastSigninTimestamp();

  SetCachedMDMInfo(identity(0), CreateRefreshAccessTokenError(identity(0)));
  authentication_service()->SignOut(
      signin_metrics::ProfileSignout::kAbortSignin,
      /*force_clear_browsing_data=*/true, nil);
  EXPECT_FALSE(HasCachedMDMInfo(identity(0)));
  EXPECT_EQ(identity_manager()->GetAccountsWithRefreshTokens().size(), 0UL);
  EXPECT_EQ(ClearBrowsingDataCount(), 1);
}

// Tests that local data are not cleared when signing out of a non-syncing
// managed account.
TEST_F(AuthenticationServiceTest, SignedInManagedAccountSignOut) {
  FakeSystemIdentity* fake_system_identity =
      [FakeSystemIdentity fakeManagedIdentity];
  fake_system_identity_manager()->AddIdentity(fake_system_identity);

  authentication_service()->SignIn(
      identity(2), signin_metrics::AccessPoint::ACCESS_POINT_UNKNOWN);
  EXPECT_EQ(identity_manager()->GetAccountsWithRefreshTokens().size(), 3UL);
  EXPECT_TRUE(authentication_service()->HasPrimaryIdentityManaged(
      signin::ConsentLevel::kSignin));
  VerifyLastSigninTimestamp();

  SetCachedMDMInfo(identity(2), CreateRefreshAccessTokenError(identity(0)));
  authentication_service()->SignOut(
      signin_metrics::ProfileSignout::kAbortSignin,
      /*force_clear_browsing_data=*/false, nil);
  EXPECT_FALSE(HasCachedMDMInfo(identity(2)));
  EXPECT_EQ(identity_manager()->GetAccountsWithRefreshTokens().size(), 0UL);
  EXPECT_EQ(ClearBrowsingDataCount(), 0);
}

// Tests that local data is cleared on signout when
// `kClearDeviceDataOnSignOutForManagedUsers` is enabled for a managed account.
TEST_F(
    AuthenticationServiceTest,
    SignedInManagedAccountSignOutWithClearDataFeatureEnabled_UnmanagedBrowser) {
  scoped_feature_list_.InitWithFeatures(
      {kClearDeviceDataOnSignOutForManagedUsers}, {});
  FakeSystemIdentity* fake_system_identity =
      [FakeSystemIdentity fakeManagedIdentity];
  fake_system_identity_manager()->AddIdentity(fake_system_identity);

  authentication_service()->SignIn(
      identity(2), signin_metrics::AccessPoint::ACCESS_POINT_UNKNOWN);
  EXPECT_EQ(identity_manager()->GetAccountsWithRefreshTokens().size(), 3UL);
  EXPECT_TRUE(authentication_service()->HasPrimaryIdentityManaged(
      signin::ConsentLevel::kSignin));
  VerifyLastSigninTimestamp();

  SetCachedMDMInfo(identity(2), CreateRefreshAccessTokenError(identity(0)));
  // Data will be cleared regardless of `force_clear_browsing_data` value
  // passed. This is intended because signout is not always triggered from UI
  // sources that set this value to true.
  authentication_service()->SignOut(
      signin_metrics::ProfileSignout::kUserClickedSignoutSettings,
      /*force_clear_browsing_data=*/false, nil);
  EXPECT_FALSE(HasCachedMDMInfo(identity(2)));
  EXPECT_EQ(identity_manager()->GetAccountsWithRefreshTokens().size(), 0UL);
  EXPECT_EQ(ClearBrowsingDataFromSigninCount(), 1);
}

// Tests that local data is not cleared on managed user's signout when
// `kClearDeviceDataOnSignOutForManagedUsers` is enabled for a managed account
// and the browser is managed.
TEST_F(
    AuthenticationServiceTest,
    SignedInManagedAccountSignOutWithClearDataFeatureEnabled_ManagedBrowser) {
  scoped_feature_list_.InitWithFeatures(
      {kClearDeviceDataOnSignOutForManagedUsers}, {});
  // Add managed configuration so the browser is managed.
  NSUserDefaults* userDefaults = [NSUserDefaults standardUserDefaults];
  NSDictionary* dict = @{@"key" : @"value"};
  [userDefaults setObject:dict forKey:kPolicyLoaderIOSConfigurationKey];
  FakeSystemIdentity* fake_system_identity =
      [FakeSystemIdentity fakeManagedIdentity];
  fake_system_identity_manager()->AddIdentity(fake_system_identity);

  authentication_service()->SignIn(
      identity(2), signin_metrics::AccessPoint::ACCESS_POINT_UNKNOWN);
  ASSERT_EQ(identity_manager()->GetAccountsWithRefreshTokens().size(), 3UL);
  ASSERT_TRUE(authentication_service()->HasPrimaryIdentityManaged(
      signin::ConsentLevel::kSignin));
  VerifyLastSigninTimestamp();

  SetCachedMDMInfo(identity(2), CreateRefreshAccessTokenError(identity(0)));
  // Data should not be cleared if the browser is managed.
  authentication_service()->SignOut(
      signin_metrics::ProfileSignout::kAbortSignin,
      /*force_clear_browsing_data=*/false, nil);
  ASSERT_FALSE(HasCachedMDMInfo(identity(2)));
  ASSERT_EQ(identity_manager()->GetAccountsWithRefreshTokens().size(), 0UL);
  EXPECT_EQ(ClearBrowsingDataCount(), 0);
  EXPECT_EQ(ClearBrowsingDataFromSigninCount(), 0);
  [userDefaults removeObjectForKey:kPolicyLoaderIOSConfigurationKey];
}

// TODO(crbug.com/40066949): Remove this test after kSync users are migrated in
// phase 3. See ConsentLevel::kSync documentation for details.
// Tests that all local data is cleared (not just the data from sign-in time)
// when the user has the sync consent.
TEST_F(AuthenticationServiceTest,
       SignedInManagedAccountSignOutWithClearDataFeatureEnabled_SyncMigration) {
  scoped_feature_list_.InitWithFeatures(
      {kClearDeviceDataOnSignOutForManagedUsers}, {});
  FakeSystemIdentity* fake_system_identity =
      [FakeSystemIdentity fakeManagedIdentity];
  fake_system_identity_manager()->AddIdentity(fake_system_identity);

  authentication_service()->SignIn(
      identity(2), signin_metrics::AccessPoint::ACCESS_POINT_UNKNOWN);
  ASSERT_EQ(identity_manager()->GetAccountsWithRefreshTokens().size(), 3UL);
  ASSERT_TRUE(authentication_service()->HasPrimaryIdentityManaged(
      signin::ConsentLevel::kSignin));
  VerifyLastSigninTimestamp();

  // Grant Sync consent.
  EXPECT_CALL(*mock_sync_service(), SetSyncFeatureRequested());
  authentication_service()->GrantSyncConsent(
      identity(2), signin_metrics::AccessPoint::ACCESS_POINT_UNKNOWN);

  EXPECT_NSEQ(identity(2), authentication_service()->GetPrimaryIdentity(
                               signin::ConsentLevel::kSync));
  EXPECT_TRUE(
      identity_manager()->HasPrimaryAccount(signin::ConsentLevel::kSync));

  // Data should be fully clear if the browser is managed and still has sync
  // consent.
  ON_CALL(*mock_sync_service()->GetMockUserSettings(),
          IsInitialSyncFeatureSetupComplete())
      .WillByDefault(Return(true));
  authentication_service()->SignOut(
      signin_metrics::ProfileSignout::kAbortSignin,
      /*force_clear_browsing_data=*/false, nil);
  ASSERT_FALSE(HasCachedMDMInfo(identity(2)));
  ASSERT_EQ(identity_manager()->GetAccountsWithRefreshTokens().size(), 0UL);
  EXPECT_EQ(ClearBrowsingDataCount(), 1);
  EXPECT_EQ(ClearBrowsingDataFromSigninCount(), 0);
}

// Tests that MDM errors do not lead to seeding empty account ids.
//
// Regression test for root cause of crbug/1482236
TEST_F(AuthenticationServiceTest, MDMErrorsDontSeedEmptyAccountIds) {
  authentication_service()->SignIn(
      identity(0), signin_metrics::AccessPoint::ACCESS_POINT_UNKNOWN);
  EXPECT_EQ(identity_manager()->GetAccountsWithRefreshTokens().size(), 2UL);
  VerifyLastSigninTimestamp();

  SetCachedMDMInfo(identity(0), CreateRefreshAccessTokenError(identity(0)));

  // Fake an mdm error for an identity that is not loaded in IdentityManager.
  FakeSystemIdentity* fake_system_identity3 =
      [FakeSystemIdentity fakeIdentity3];
  fake_system_identity_manager()->AddIdentity(fake_system_identity3);
  SetCachedMDMInfo(identity(2), CreateRefreshAccessTokenError(identity(2)));

  GoogleServiceAuthError error(
      GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS);
  signin::UpdatePersistentErrorOfRefreshTokenForAccount(
      identity_manager(), GetAccountId(identity(0)), error);
  FireApplicationWillEnterForeground();

  ASSERT_EQ(identity_manager()->GetAccountsWithRefreshTokens().size(), 2u);
  EXPECT_FALSE(
      identity_manager()->GetAccountsWithRefreshTokens()[0].account_id.empty());
  EXPECT_FALSE(
      identity_manager()->GetAccountsWithRefreshTokens()[1].account_id.empty());
}

// Tests that MDM errors are correctly cleared when signing out of a managed
// account.
TEST_F(AuthenticationServiceTest, ManagedAccountSignOut) {
  FakeSystemIdentity* fake_system_identity =
      [FakeSystemIdentity fakeManagedIdentity];
  fake_system_identity_manager()->AddIdentity(fake_system_identity);

  authentication_service()->SignIn(
      identity(2), signin_metrics::AccessPoint::ACCESS_POINT_UNKNOWN);
  EXPECT_EQ(identity_manager()->GetAccountsWithRefreshTokens().size(), 3UL);
  EXPECT_TRUE(authentication_service()->HasPrimaryIdentityManaged(
      signin::ConsentLevel::kSignin));
  ON_CALL(*mock_sync_service()->GetMockUserSettings(),
          IsInitialSyncFeatureSetupComplete())
      .WillByDefault(Return(true));
  VerifyLastSigninTimestamp();

  authentication_service()->SignOut(
      signin_metrics::ProfileSignout::kAbortSignin,
      /*force_clear_browsing_data=*/false, nil);
  EXPECT_FALSE(HasCachedMDMInfo(identity(2)));
  EXPECT_EQ(identity_manager()->GetAccountsWithRefreshTokens().size(), 0UL);
  EXPECT_EQ(ClearBrowsingDataCount(), 1);
}

// Tests that MDM errors are correctly cleared when signing out with clearing
// browsing data of a managed account.
TEST_F(AuthenticationServiceTest, ManagedAccountSignOutAndClearBrowsingData) {
  FakeSystemIdentity* fake_system_identity =
      [FakeSystemIdentity fakeManagedIdentity];
  fake_system_identity_manager()->AddIdentity(fake_system_identity);

  authentication_service()->SignIn(
      identity(2), signin_metrics::AccessPoint::ACCESS_POINT_UNKNOWN);
  VerifyLastSigninTimestamp();

  EXPECT_EQ(identity_manager()->GetAccountsWithRefreshTokens().size(), 3UL);
  EXPECT_TRUE(authentication_service()->HasPrimaryIdentityManaged(
      signin::ConsentLevel::kSignin));
  VerifyLastSigninTimestamp();

  authentication_service()->SignOut(
      signin_metrics::ProfileSignout::kAbortSignin,
      /*force_clear_browsing_data=*/true, nil);
  EXPECT_FALSE(HasCachedMDMInfo(identity(2)));
  EXPECT_EQ(identity_manager()->GetAccountsWithRefreshTokens().size(), 0UL);
  EXPECT_EQ(ClearBrowsingDataCount(), 1);
}

// Tests that potential MDM notifications are correctly handled and dispatched
// to MDM service when necessary.
TEST_F(AuthenticationServiceTest, HandleMDMNotification) {
  authentication_service()->SignIn(
      identity(0), signin_metrics::AccessPoint::ACCESS_POINT_UNKNOWN);
  VerifyLastSigninTimestamp();

  GoogleServiceAuthError error(
      GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS);
  signin::UpdatePersistentErrorOfRefreshTokenForAccount(
      identity_manager(), GetAccountId(identity(0)), error);

  uint32_t invocation_counter1 = 0;
  id<RefreshAccessTokenError> mdm_error1 =
      CreateRefreshAccessTokenError(identity(0), &invocation_counter1);
  ASSERT_TRUE(mdm_error1);

  // Notification will show the MDM dialog the first time.
  FireAccessTokenRefreshFailed(identity(0), mdm_error1);
  fake_system_identity_manager()->WaitForServiceCallbacksToComplete();
  EXPECT_EQ(invocation_counter1, 1u);

  // Same notification won't show the MDM dialog the second time.
  FireAccessTokenRefreshFailed(identity(0), mdm_error1);
  fake_system_identity_manager()->WaitForServiceCallbacksToComplete();
  EXPECT_EQ(invocation_counter1, 1u);

  uint32_t invocation_counter2 = 0;
  id<RefreshAccessTokenError> mdm_error2 =
      CreateRefreshAccessTokenError(identity(0), &invocation_counter2);
  ASSERT_TRUE(mdm_error2);

  // New notification will show the MDM dialog on the same identity.
  FireAccessTokenRefreshFailed(identity(0), mdm_error2);
  fake_system_identity_manager()->WaitForServiceCallbacksToComplete();
  EXPECT_EQ(invocation_counter1, 1u);
  EXPECT_EQ(invocation_counter2, 1u);
}

// Tests that MDM blocked notifications are correctly signing out the user if
// the primary account is blocked.
TEST_F(AuthenticationServiceTest, HandleMDMBlockedNotification) {
  authentication_service()->SignIn(
      identity(0), signin_metrics::AccessPoint::ACCESS_POINT_UNKNOWN);
  GoogleServiceAuthError error(
      GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS);
  signin::UpdatePersistentErrorOfRefreshTokenForAccount(
      identity_manager(), GetAccountId(identity(0)), error);
  VerifyLastSigninTimestamp();

  uint32_t invocation_counter = 0;
  id<RefreshAccessTokenError> mdm_error = CreateRefreshAccessTokenError(
      identity(0), &invocation_counter, /*is_identity_blocked*/ true);

  // User not signed out as `identity(1)` isn't the primary account.
  FireAccessTokenRefreshFailed(identity(1), mdm_error);
  fake_system_identity_manager()->WaitForServiceCallbacksToComplete();
  EXPECT_TRUE(authentication_service()->HasPrimaryIdentity(
      signin::ConsentLevel::kSignin));
  EXPECT_EQ(invocation_counter, 0u);

  // User signed out as `identity_` is the primary account.
  FireAccessTokenRefreshFailed(identity(0), mdm_error);
  fake_system_identity_manager()->WaitForServiceCallbacksToComplete();
  EXPECT_FALSE(authentication_service()->HasPrimaryIdentity(
      signin::ConsentLevel::kSignin));
  EXPECT_EQ(invocation_counter, 1u);
}

// Tests that MDM dialog isn't shown when there is no cached MDM error.
TEST_F(AuthenticationServiceTest, ShowMDMErrorDialogNoCachedError) {
  EXPECT_FALSE(
      authentication_service()->ShowMDMErrorDialogForIdentity(identity(0)));
}

// Tests that MDM dialog isn't shown when there is a cached MDM error but no
// corresponding error for the account.
TEST_F(AuthenticationServiceTest, ShowMDMErrorDialogInvalidCachedError) {
  uint32_t invocation_counter = 0;
  SetCachedMDMInfo(identity(0), CreateRefreshAccessTokenError(
                                    identity(0), &invocation_counter));

  EXPECT_FALSE(
      authentication_service()->ShowMDMErrorDialogForIdentity(identity(0)));
  fake_system_identity_manager()->WaitForServiceCallbacksToComplete();
  EXPECT_EQ(invocation_counter, 0u);
}

// Tests that MDM dialog is shown when there is a cached error and a
// corresponding error for the account.
TEST_F(AuthenticationServiceTest, ShowMDMErrorDialog) {
  authentication_service()->SignIn(
      identity(0), signin_metrics::AccessPoint::ACCESS_POINT_UNKNOWN);
  GoogleServiceAuthError error(
      GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS);
  signin::UpdatePersistentErrorOfRefreshTokenForAccount(
      identity_manager(), GetAccountId(identity(0)), error);
  VerifyLastSigninTimestamp();

  uint32_t invocation_counter = 0;
  SetCachedMDMInfo(identity(0), CreateRefreshAccessTokenError(
                                    identity(0), &invocation_counter));

  EXPECT_TRUE(
      authentication_service()->ShowMDMErrorDialogForIdentity(identity(0)));
  fake_system_identity_manager()->WaitForServiceCallbacksToComplete();
  EXPECT_EQ(invocation_counter, 1u);
}

// TODO(crbug.com/40066949): Remove this test after kSync users are migrated in
// phase 3. See ConsentLevel::kSync documentation for details.
TEST_F(AuthenticationServiceTest, SigninAndSyncDecoupled) {
  // Sign in.
  authentication_service()->SignIn(
      identity(0), signin_metrics::AccessPoint::ACCESS_POINT_UNKNOWN);
  VerifyLastSigninTimestamp();

  EXPECT_NSEQ(identity(0), authentication_service()->GetPrimaryIdentity(
                               signin::ConsentLevel::kSignin));
  EXPECT_TRUE(
      identity_manager()->HasPrimaryAccount(signin::ConsentLevel::kSignin));
  EXPECT_FALSE(
      identity_manager()->HasPrimaryAccount(signin::ConsentLevel::kSync));
  EXPECT_TRUE(authentication_service()->HasPrimaryIdentity(
      signin::ConsentLevel::kSignin));

  // Grant Sync consent.
  EXPECT_CALL(*mock_sync_service(), SetSyncFeatureRequested());
  authentication_service()->GrantSyncConsent(
      identity(0), signin_metrics::AccessPoint::ACCESS_POINT_UNKNOWN);

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
  profile_->GetPrefs()->SetBoolean(prefs::kSigninAllowed, false);

  // Attempt to sign in, and verify there is a crash.
  EXPECT_CHECK_DEATH(authentication_service()->SignIn(
      identity(0), signin_metrics::AccessPoint::ACCESS_POINT_UNKNOWN));
}

// Tests that reauth prompt is not set if the primary identity is restricted and
// `OnPrimaryAccountRestricted` is forwarded.
TEST_F(AuthenticationServiceTest, TestHandleRestrictedIdentityPromptSignIn) {
  AuthenticationServiceObserverTest observer_test;
  authentication_service()->AddObserver(&observer_test);
  // Sign in.
  authentication_service()->SignIn(
      identity(0), signin_metrics::AccessPoint::ACCESS_POINT_UNKNOWN);
  authentication_service()->GrantSyncConsent(
      identity(0), signin_metrics::AccessPoint::ACCESS_POINT_UNKNOWN);
  VerifyLastSigninTimestamp();

  // Set the account restriction.
  SetPattern("foo");
  EXPECT_FALSE(account_manager_->HasIdentities());

  // Set the authentication service as "In Background" and run the loop.
  base::RunLoop().RunUntilIdle();

  // User is signed out (no corresponding identity), and reauth prompt is set.
  EXPECT_FALSE(authentication_service()->HasPrimaryIdentity(
      signin::ConsentLevel::kSignin));
  EXPECT_EQ(1, observer_test.GetOnPrimaryAccountRestrictedCounter());
  authentication_service()->RemoveObserver(&observer_test);
}

// Tests AuthenticationService::GetServiceStatus() using
// prefs::kBrowserSigninPolicy.
TEST_F(AuthenticationServiceTest, TestGetServiceStatus) {
  AuthenticationServiceObserverTest observer_test;
  authentication_service()->AddObserver(&observer_test);

  // Expect sign-in allowed by default.
  EXPECT_EQ(AuthenticationService::ServiceStatus::SigninAllowed,
            authentication_service()->GetServiceStatus());

  profile_->GetPrefs()->SetBoolean(prefs::kSigninAllowed, false);
  // Expect sign-in disabled by user.
  EXPECT_EQ(AuthenticationService::ServiceStatus::SigninDisabledByUser,
            authentication_service()->GetServiceStatus());
  // Expect onServiceStatus notification called.
  EXPECT_EQ(1, observer_test.GetOnServiceStatusChangedCounter());

  // Set sign-in disabled by policy.
  local_state()->SetInteger(prefs::kBrowserSigninPolicy,
                            static_cast<int>(BrowserSigninMode::kDisabled));
  // Expect sign-in to be disabled by policy.
  EXPECT_EQ(AuthenticationService::ServiceStatus::SigninDisabledByPolicy,
            authentication_service()->GetServiceStatus());
  // Expect onServiceStatus notification called.
  EXPECT_EQ(2, observer_test.GetOnServiceStatusChangedCounter());

  // Set sign-in forced by policy.
  local_state()->SetInteger(prefs::kBrowserSigninPolicy,
                            static_cast<int>(BrowserSigninMode::kForced));
  // Expect sign-in to be forced by policy.
  EXPECT_EQ(AuthenticationService::ServiceStatus::SigninForcedByPolicy,
            authentication_service()->GetServiceStatus());
  // Expect onServiceStatus notification called.
  EXPECT_EQ(3, observer_test.GetOnServiceStatusChangedCounter());

  profile_->GetPrefs()->SetBoolean(prefs::kSigninAllowed, true);
  // Expect sign-in to be still forced by policy.
  EXPECT_EQ(AuthenticationService::ServiceStatus::SigninForcedByPolicy,
            authentication_service()->GetServiceStatus());
  // Expect onServiceStatus notification called.
  EXPECT_EQ(4, observer_test.GetOnServiceStatusChangedCounter());
  authentication_service()->RemoveObserver(&observer_test);
}

// Tests that identity manager loads identities while being signed out.
// And also tests that an identity being added is loaded by identity manager.
// kAlwaysLoadDeviceAccounts flag is enabled.
TEST_F(AuthenticationServiceTest,
       TestAccountsLoadedByIdentityManagerWhenSignedOut) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(switches::kAlwaysLoadDeviceAccounts);
  // `fakeIdentity1` and `fakeIdentity2` are already loaded.
  std::vector<AccountInfo> account_info_vector =
      identity_manager()->GetExtendedAccountInfoForAccountsWithRefreshToken();
  EXPECT_EQ(2ul, account_info_vector.size());
  // Let's load `fakeIdentity3`.
  id<SystemIdentity> fake_identity3 = [FakeSystemIdentity fakeIdentity3];
  fake_system_identity_manager()->AddIdentity(fake_identity3);
  base::RunLoop().RunUntilIdle();
  account_info_vector =
      identity_manager()->GetExtendedAccountInfoForAccountsWithRefreshToken();
  EXPECT_EQ(3ul, account_info_vector.size());
}

// Tests that identity manager loads identities while being signed out.
// And also tests that an identity being removed is forgotten by identity
// manager.
// kAlwaysLoadDeviceAccounts flag is enabled.
TEST_F(AuthenticationServiceTest, TestAccountsForgetIdentityWhenSignedOut) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(switches::kAlwaysLoadDeviceAccounts);
  std::vector<AccountInfo> account_info_vector =
      identity_manager()->GetExtendedAccountInfoForAccountsWithRefreshToken();
  // `fakeIdentity1` and `fakeIdentity2` are already loaded.
  EXPECT_EQ(2ul, account_info_vector.size());
  // Let's forget `fakeIdentity2`.
  id<SystemIdentity> fake_identity2 = [FakeSystemIdentity fakeIdentity2];
  fake_system_identity_manager()->ForgetIdentity(fake_identity2,
                                                 base::DoNothing());
  base::RunLoop().RunUntilIdle();
  account_info_vector =
      identity_manager()->GetExtendedAccountInfoForAccountsWithRefreshToken();
  EXPECT_EQ(1ul, account_info_vector.size());
}
