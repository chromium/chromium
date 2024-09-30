// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/signin/signin_utils.h"

#import <UIKit/UIKit.h>

#import <memory>

#import "base/functional/callback_helpers.h"
#import "base/memory/raw_ptr.h"
#import "base/version.h"
#import "components/pref_registry/pref_registry_syncable.h"
#import "components/signin/public/base/signin_metrics.h"
#import "components/signin/public/base/signin_pref_names.h"
#import "components/signin/public/base/signin_switches.h"
#import "components/sync/base/pref_names.h"
#import "components/sync/service/sync_service.h"
#import "components/sync/service/sync_user_settings.h"
#import "components/sync_preferences/pref_service_mock_factory.h"
#import "components/sync_preferences/pref_service_syncable.h"
#import "ios/chrome/browser/policy/model/policy_util.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/prefs/browser_prefs.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service_factory.h"
#import "ios/chrome/browser/signin/model/fake_authentication_service_delegate.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/browser/signin/model/fake_system_identity_manager.h"
#import "ios/chrome/browser/signin/model/signin_util.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace {

class SigninUtilsTest : public PlatformTest {
 public:
  SigninUtilsTest() = default;

  void SetUp() override {
    PlatformTest::SetUp();
    TestProfileIOS::Builder builder;
    builder.SetPrefService(CreatePrefService());
    builder.AddTestingFactory(
        AuthenticationServiceFactory::GetInstance(),
        AuthenticationServiceFactory::GetDefaultFactory());
    builder.AddTestingFactory(SyncServiceFactory::GetInstance(),
                              SyncServiceFactory::GetDefaultFactory());
    profile_ = std::move(builder).Build();
    AuthenticationServiceFactory::CreateAndInitializeForProfile(
        profile_.get(), std::make_unique<FakeAuthenticationServiceDelegate>());
    account_manager_service_ =
        ChromeAccountManagerServiceFactory::GetForProfile(profile_.get());
  }

  void TearDown() override {
    NSUserDefaults* standardDefaults = [NSUserDefaults standardUserDefaults];
    [standardDefaults removeObjectForKey:kDisplayedSSORecallForMajorVersionKey];
    [standardDefaults removeObjectForKey:kLastShownAccountGaiaIdVersionKey];
    [standardDefaults removeObjectForKey:kSigninPromoViewDisplayCountKey];
    [standardDefaults synchronize];
    PlatformTest::TearDown();
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

  PrefService* GetLocalState() {
    return GetApplicationContext()->GetLocalState();
  }

  PrefService* GetProfilePrefs() { return profile_.get()->GetPrefs(); }

  FakeSystemIdentityManager* fake_system_identity_manager() {
    return FakeSystemIdentityManager::FromSystemIdentityManager(
        GetApplicationContext()->GetSystemIdentityManager());
  }

 protected:
  web::WebTaskEnvironment task_environment_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  std::unique_ptr<TestProfileIOS> profile_;
  raw_ptr<ChromeAccountManagerService> account_manager_service_;
};

// Should show the sign-in upgrade for the first time, after FRE.
TEST_F(SigninUtilsTest, TestWillNotDisplay) {
  FakeSystemIdentity* fake_identity1 = [FakeSystemIdentity fakeIdentity1];
  fake_system_identity_manager()->AddIdentity(fake_identity1);
  FakeSystemIdentity* fake_identity2 = [FakeSystemIdentity fakeIdentity2];
  fake_system_identity_manager()->AddIdentity(fake_identity2);
  const base::Version version_1_0("1.0");
  EXPECT_FALSE(
      signin::ShouldPresentUserSigninUpgrade(profile_.get(), version_1_0));
}

// Should not show the sign-in upgrade twice on the same version.
TEST_F(SigninUtilsTest, TestWillNotDisplaySameVersion) {
  FakeSystemIdentity* fake_identity1 = [FakeSystemIdentity fakeIdentity1];
  fake_system_identity_manager()->AddIdentity(fake_identity1);
  FakeSystemIdentity* fake_identity2 = [FakeSystemIdentity fakeIdentity2];
  fake_system_identity_manager()->AddIdentity(fake_identity2);
  const base::Version version_1_0("1.0");
  signin::RecordUpgradePromoSigninStarted(account_manager_service_,
                                          version_1_0);
  EXPECT_FALSE(
      signin::ShouldPresentUserSigninUpgrade(profile_.get(), version_1_0));
}

// Should not show the sign-in upgrade twice until two major version after.
TEST_F(SigninUtilsTest, TestWillNotDisplayOneMinorVersion) {
  FakeSystemIdentity* fake_identity1 = [FakeSystemIdentity fakeIdentity1];
  fake_system_identity_manager()->AddIdentity(fake_identity1);
  FakeSystemIdentity* fake_identity2 = [FakeSystemIdentity fakeIdentity2];
  fake_system_identity_manager()->AddIdentity(fake_identity2);
  const base::Version version_1_0("1.0");
  const base::Version version_1_1("1.1");
  signin::RecordUpgradePromoSigninStarted(account_manager_service_,
                                          version_1_0);
  EXPECT_FALSE(
      signin::ShouldPresentUserSigninUpgrade(profile_.get(), version_1_1));
}

// Should not show the sign-in upgrade twice until two major version after.
TEST_F(SigninUtilsTest, TestWillNotDisplayTwoMinorVersions) {
  FakeSystemIdentity* fake_identity1 = [FakeSystemIdentity fakeIdentity1];
  fake_system_identity_manager()->AddIdentity(fake_identity1);
  FakeSystemIdentity* fake_identity2 = [FakeSystemIdentity fakeIdentity2];
  fake_system_identity_manager()->AddIdentity(fake_identity2);
  const base::Version version_1_0("1.0");
  const base::Version version_1_2("1.2");
  signin::RecordUpgradePromoSigninStarted(account_manager_service_,
                                          version_1_0);
  EXPECT_FALSE(
      signin::ShouldPresentUserSigninUpgrade(profile_.get(), version_1_2));
}

// Should not show the sign-in upgrade twice until two major version after.
TEST_F(SigninUtilsTest, TestWillNotDisplayOneMajorVersion) {
  FakeSystemIdentity* fake_identity1 = [FakeSystemIdentity fakeIdentity1];
  fake_system_identity_manager()->AddIdentity(fake_identity1);
  FakeSystemIdentity* fake_identity2 = [FakeSystemIdentity fakeIdentity2];
  fake_system_identity_manager()->AddIdentity(fake_identity2);
  const base::Version version_1_0("1.0");
  const base::Version version_2_0("2.0");
  signin::RecordUpgradePromoSigninStarted(account_manager_service_,
                                          version_1_0);
  EXPECT_FALSE(
      signin::ShouldPresentUserSigninUpgrade(profile_.get(), version_2_0));
}

// Should show the sign-in upgrade a second time, 2 version after.
TEST_F(SigninUtilsTest, TestWillDisplayTwoMajorVersions) {
  FakeSystemIdentity* fake_identity1 = [FakeSystemIdentity fakeIdentity1];
  fake_system_identity_manager()->AddIdentity(fake_identity1);
  FakeSystemIdentity* fake_identity2 = [FakeSystemIdentity fakeIdentity2];
  fake_system_identity_manager()->AddIdentity(fake_identity2);
  const base::Version version_1_0("1.0");
  const base::Version version_3_0("3.0");
  signin::RecordUpgradePromoSigninStarted(account_manager_service_,
                                          version_1_0);
  EXPECT_TRUE(
      signin::ShouldPresentUserSigninUpgrade(profile_.get(), version_3_0));
}

// Show the sign-in upgrade on version 1.0.
// Show the sign-in upgrade on version 3.0.
// Move to version 5.0.
// Expected: should not show the sign-in upgrade.
TEST_F(SigninUtilsTest, TestWillShowTwoTimesOnly) {
  FakeSystemIdentity* fake_identity1 = [FakeSystemIdentity fakeIdentity1];
  fake_system_identity_manager()->AddIdentity(fake_identity1);
  FakeSystemIdentity* fake_identity2 = [FakeSystemIdentity fakeIdentity2];
  fake_system_identity_manager()->AddIdentity(fake_identity2);
  const base::Version version_1_0("1.0");
  const base::Version version_3_0("3.0");
  const base::Version version_5_0("5.0");
  signin::RecordUpgradePromoSigninStarted(account_manager_service_,
                                          version_1_0);
  signin::RecordUpgradePromoSigninStarted(account_manager_service_,
                                          version_3_0);
  EXPECT_FALSE(
      signin::ShouldPresentUserSigninUpgrade(profile_.get(), version_5_0));
}

// Show the sign-in upgrade on version 1.0.
// Show the sign-in upgrade on version 3.0.
// Move to version 5.0.
// Add new account.
// Expected: should show the sign-in upgrade.
TEST_F(SigninUtilsTest, TestWillShowForNewAccountAdded) {
  const base::Version version_1_0("1.0");
  const base::Version version_3_0("3.0");
  const base::Version version_5_0("5.0");
  signin::RecordUpgradePromoSigninStarted(account_manager_service_,
                                          version_1_0);
  signin::RecordUpgradePromoSigninStarted(account_manager_service_,
                                          version_3_0);
  FakeSystemIdentity* fake_identity = [FakeSystemIdentity fakeIdentity1];
  fake_system_identity_manager()->AddIdentity(fake_identity);
  EXPECT_TRUE(
      signin::ShouldPresentUserSigninUpgrade(profile_.get(), version_5_0));
}

// Add new account.
// Show the sign-in upgrade on version 1.0.
// Show the sign-in upgrade on version 3.0.
// Move to version 5.0.
// Remove previous account.
// Expected: should not show the sign-in upgrade.
TEST_F(SigninUtilsTest, TestWillNotShowWithAccountRemoved) {
  const base::Version version_1_0("1.0");
  const base::Version version_3_0("3.0");
  const base::Version version_5_0("5.0");
  FakeSystemIdentity* fake_identity = [FakeSystemIdentity fakeIdentity1];
  fake_system_identity_manager()->AddIdentity(fake_identity);
  signin::RecordUpgradePromoSigninStarted(account_manager_service_,
                                          version_1_0);
  signin::RecordUpgradePromoSigninStarted(account_manager_service_,
                                          version_3_0);
  fake_system_identity_manager()->ForgetIdentity(fake_identity,
                                                 base::DoNothing());
  EXPECT_FALSE(
      signin::ShouldPresentUserSigninUpgrade(profile_.get(), version_5_0));
}

// Show the sign-in upgrade on version 1.0.
// Show the sign-in upgrade on version 3.0.
// Move to version 4.0.
// Add an account.
// Expected: should not show the sign-in upgrade.
TEST_F(SigninUtilsTest, TestWillNotShowNewAccountUntilTwoVersion) {
  const base::Version version_1_0("1.0");
  const base::Version version_3_0("3.0");
  const base::Version version_4_0("4.0");
  signin::RecordUpgradePromoSigninStarted(account_manager_service_,
                                          version_1_0);
  signin::RecordUpgradePromoSigninStarted(account_manager_service_,
                                          version_3_0);
  FakeSystemIdentity* fake_identity = [FakeSystemIdentity fakeIdentity1];
  fake_system_identity_manager()->AddIdentity(fake_identity);
  EXPECT_FALSE(
      signin::ShouldPresentUserSigninUpgrade(profile_.get(), version_4_0));
}

// Show the sign-in upgrade on version 1.0.
// Move to version 2.0.
// Add an account.
// Expected: should not show the sign-in upgrade (only display every 2
// versions).
TEST_F(SigninUtilsTest, TestWillNotShowNewAccountUntilTwoVersionBis) {
  const base::Version version_1_0("1.0");
  const base::Version version_2_0("2.0");
  signin::RecordUpgradePromoSigninStarted(account_manager_service_,
                                          version_1_0);
  FakeSystemIdentity* fake_identity = [FakeSystemIdentity fakeIdentity1];
  fake_system_identity_manager()->AddIdentity(fake_identity);
  EXPECT_FALSE(
      signin::ShouldPresentUserSigninUpgrade(profile_.get(), version_2_0));
}

// Should not show the sign-in upgrade for first run after post restore.
TEST_F(SigninUtilsTest, TestWillNotShowIfFirstRunAfterPostRestore) {
  const base::Version version_1_0("1.0");
  const base::Version version_3_0("3.0");
  signin::RecordUpgradePromoSigninStarted(account_manager_service_,
                                          version_1_0);
  FakeSystemIdentity* fake_identity = [FakeSystemIdentity fakeIdentity1];
  fake_system_identity_manager()->AddIdentity(fake_identity);
  ASSERT_TRUE(
      signin::ShouldPresentUserSigninUpgrade(profile_.get(), version_3_0));

  AccountInfo accountInfo;
  accountInfo.email = "foo@bar.com";
  StorePreRestoreIdentity(GetProfilePrefs(), accountInfo,
                          /*history_sync_enabled=*/false);
  EXPECT_FALSE(
      signin::ShouldPresentUserSigninUpgrade(profile_.get(), version_3_0));
}

// Should not show the sign-in upgrade if sign-in is disabled by policy.
TEST_F(SigninUtilsTest, TestWillNotShowIfDisabledByPolicy) {
  const base::Version version_1_0("1.0");
  const base::Version version_3_0("3.0");
  signin::RecordUpgradePromoSigninStarted(account_manager_service_,
                                          version_1_0);
  FakeSystemIdentity* fake_identity = [FakeSystemIdentity fakeIdentity1];
  fake_system_identity_manager()->AddIdentity(fake_identity);
  GetLocalState()->SetInteger(prefs::kBrowserSigninPolicy,
                              static_cast<int>(BrowserSigninMode::kDisabled));

  EXPECT_FALSE(
      signin::ShouldPresentUserSigninUpgrade(profile_.get(), version_3_0));
}

// Should show if the user is signed-in without history opt-in.
TEST_F(SigninUtilsTest, TestWillShowIfSignedInWithoutHistoryOptIn) {
  FakeSystemIdentity* identity = [FakeSystemIdentity fakeIdentity1];
  fake_system_identity_manager()->AddIdentity(identity);
  AuthenticationService* authentication_service =
      AuthenticationServiceFactory::GetForProfile(profile_.get());
  authentication_service->SignIn(
      identity, signin_metrics::AccessPoint::ACCESS_POINT_UNKNOWN);

  const base::Version version_1_0("1.0");
  const base::Version version_3_0("3.0");
  signin::RecordUpgradePromoSigninStarted(account_manager_service_,
                                          version_1_0);
  EXPECT_TRUE(
      signin::ShouldPresentUserSigninUpgrade(profile_.get(), version_3_0));
}

// Should not show if the user is signed-in with history opt-in.
TEST_F(SigninUtilsTest, TestWillNotShowIfSignedInWithHistoryOptIn) {
  FakeSystemIdentity* identity = [FakeSystemIdentity fakeIdentity1];
  fake_system_identity_manager()->AddIdentity(identity);
  AuthenticationService* authentication_service =
      AuthenticationServiceFactory::GetForProfile(profile_.get());
  authentication_service->SignIn(
      identity, signin_metrics::AccessPoint::ACCESS_POINT_UNKNOWN);

  syncer::SyncService* sync_service =
      SyncServiceFactory::GetForProfile(profile_.get());
  syncer::SyncUserSettings* sync_user_settings =
      sync_service->GetUserSettings();
  sync_user_settings->SetSelectedType(syncer::UserSelectableType::kHistory,
                                      true);
  sync_user_settings->SetSelectedType(syncer::UserSelectableType::kTabs, true);

  const base::Version version_1_0("1.0");
  const base::Version version_3_0("3.0");
  signin::RecordUpgradePromoSigninStarted(account_manager_service_,
                                          version_1_0);
  EXPECT_FALSE(
      signin::ShouldPresentUserSigninUpgrade(profile_.get(), version_3_0));
}

// signin::GetPrimaryIdentitySigninState for a signed-out user should
// return the signed out state.
TEST_F(SigninUtilsTest, TestGetPrimaryIdentitySigninStateSignedOut) {
  IdentitySigninState state =
      signin::GetPrimaryIdentitySigninState(profile_.get());
  EXPECT_EQ(IdentitySigninStateSignedOut, state);
}

// signin::GetPrimaryIdentitySigninState for a signed-in user should
// return the signed-in, sync disabled state.
TEST_F(SigninUtilsTest, TestGetPrimaryIdentitySigninStateSignedInSyncDisabled) {
  FakeSystemIdentity* identity = [FakeSystemIdentity fakeIdentity1];
  fake_system_identity_manager()->AddIdentity(identity);
  AuthenticationService* authentication_service =
      AuthenticationServiceFactory::GetForProfile(profile_.get());
  authentication_service->SignIn(
      identity, signin_metrics::AccessPoint::ACCESS_POINT_UNKNOWN);

  IdentitySigninState state =
      signin::GetPrimaryIdentitySigninState(profile_.get());
  EXPECT_EQ(IdentitySigninStateSignedInWithSyncDisabled, state);
}

// signin::GetPrimaryIdentitySigninState for a syncing user who has
// completed the sync setup should return the signed-in, sync enabled state.
TEST_F(SigninUtilsTest,
       TestGetPrimaryIdentitySigninStateSyncGrantedSetupComplete) {
  FakeSystemIdentity* identity = [FakeSystemIdentity fakeIdentity1];
  fake_system_identity_manager()->AddIdentity(identity);
  AuthenticationService* authentication_service =
      AuthenticationServiceFactory::GetForProfile(profile_.get());
  authentication_service->SignIn(
      identity, signin_metrics::AccessPoint::ACCESS_POINT_SIGNIN_PROMO);
  authentication_service->GrantSyncConsent(
      identity, signin_metrics::AccessPoint::ACCESS_POINT_SIGNIN_PROMO);
  profile_->GetPrefs()->SetBoolean(
      syncer::prefs::internal::kSyncInitialSyncFeatureSetupComplete, true);

  IdentitySigninState state =
      signin::GetPrimaryIdentitySigninState(profile_.get());
  EXPECT_EQ(IdentitySigninStateSignedInWithSyncEnabled, state);

  profile_->GetPrefs()->ClearPref(
      syncer::prefs::internal::kSyncInitialSyncFeatureSetupComplete);
}

}  // namespace
