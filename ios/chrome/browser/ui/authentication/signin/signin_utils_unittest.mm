// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/signin/signin_utils.h"

#import <UIKit/UIKit.h>

#import <memory>

#import "base/functional/callback_helpers.h"
#import "base/test/scoped_feature_list.h"
#import "base/version.h"
#import "components/pref_registry/pref_registry_syncable.h"
#import "components/signin/public/base/signin_metrics.h"
#import "components/signin/public/base/signin_pref_names.h"
#import "components/signin/public/base/signin_switches.h"
#import "components/sync/base/features.h"
#import "components/sync/base/pref_names.h"
#import "components/sync/service/sync_service.h"
#import "components/sync/service/sync_user_settings.h"
#import "components/sync_preferences/pref_service_mock_factory.h"
#import "components/sync_preferences/pref_service_syncable.h"
#import "ios/chrome/browser/policy/policy_util.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/shared/model/prefs/browser_prefs.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/signin/authentication_service.h"
#import "ios/chrome/browser/signin/authentication_service_factory.h"
#import "ios/chrome/browser/signin/chrome_account_manager_service_factory.h"
#import "ios/chrome/browser/signin/fake_authentication_service_delegate.h"
#import "ios/chrome/browser/signin/fake_system_identity.h"
#import "ios/chrome/browser/signin/fake_system_identity_manager.h"
#import "ios/chrome/browser/sync/sync_service_factory.h"
#import "ios/chrome/browser/ui/authentication/signin/user_signin/user_signin_constants.h"
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
    TestChromeBrowserState::Builder builder;
    builder.SetPrefService(CreatePrefService());
    builder.AddTestingFactory(
        AuthenticationServiceFactory::GetInstance(),
        AuthenticationServiceFactory::GetDefaultFactory());
    builder.AddTestingFactory(SyncServiceFactory::GetInstance(),
                              SyncServiceFactory::GetDefaultFactory());
    chrome_browser_state_ = builder.Build();
    AuthenticationServiceFactory::CreateAndInitializeForBrowserState(
        chrome_browser_state_.get(),
        std::make_unique<FakeAuthenticationServiceDelegate>());
    account_manager_service_ =
        ChromeAccountManagerServiceFactory::GetForBrowserState(
            chrome_browser_state_.get());
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
    RegisterBrowserStatePrefs(registry.get());
    return prefs;
  }

  PrefService* GetLocalState() { return scoped_testing_local_state_.Get(); }

  FakeSystemIdentityManager* fake_system_identity_manager() {
    return FakeSystemIdentityManager::FromSystemIdentityManager(
        GetApplicationContext()->GetSystemIdentityManager());
  }

 protected:
  web::WebTaskEnvironment task_environment_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  std::unique_ptr<TestChromeBrowserState> chrome_browser_state_;
  ChromeAccountManagerService* account_manager_service_;
};

// Should show the sign-in upgrade for the first time, after FRE.
TEST_F(SigninUtilsTest, TestWillNotDisplay) {
  fake_system_identity_manager()->AddIdentities(@[ @"foo", @"bar" ]);
  const base::Version version_1_0("1.0");
  EXPECT_FALSE(signin::ShouldPresentUserSigninUpgrade(
      chrome_browser_state_.get(), version_1_0));
}

// Should not show the sign-in upgrade twice on the same version.
TEST_F(SigninUtilsTest, TestWillNotDisplaySameVersion) {
  fake_system_identity_manager()->AddIdentities(@[ @"foo", @"bar" ]);
  const base::Version version_1_0("1.0");
  signin::RecordUpgradePromoSigninStarted(account_manager_service_,
                                          version_1_0);
  EXPECT_FALSE(signin::ShouldPresentUserSigninUpgrade(
      chrome_browser_state_.get(), version_1_0));
}

// Should not show the sign-in upgrade twice until two major version after.
TEST_F(SigninUtilsTest, TestWillNotDisplayOneMinorVersion) {
  fake_system_identity_manager()->AddIdentities(@[ @"foo", @"bar" ]);
  const base::Version version_1_0("1.0");
  const base::Version version_1_1("1.1");
  signin::RecordUpgradePromoSigninStarted(account_manager_service_,
                                          version_1_0);
  EXPECT_FALSE(signin::ShouldPresentUserSigninUpgrade(
      chrome_browser_state_.get(), version_1_1));
}

// Should not show the sign-in upgrade twice until two major version after.
TEST_F(SigninUtilsTest, TestWillNotDisplayTwoMinorVersions) {
  fake_system_identity_manager()->AddIdentities(@[ @"foo", @"bar" ]);
  const base::Version version_1_0("1.0");
  const base::Version version_1_2("1.2");
  signin::RecordUpgradePromoSigninStarted(account_manager_service_,
                                          version_1_0);
  EXPECT_FALSE(signin::ShouldPresentUserSigninUpgrade(
      chrome_browser_state_.get(), version_1_2));
}

// Should not show the sign-in upgrade twice until two major version after.
TEST_F(SigninUtilsTest, TestWillNotDisplayOneMajorVersion) {
  fake_system_identity_manager()->AddIdentities(@[ @"foo", @"bar" ]);
  const base::Version version_1_0("1.0");
  const base::Version version_2_0("2.0");
  signin::RecordUpgradePromoSigninStarted(account_manager_service_,
                                          version_1_0);
  EXPECT_FALSE(signin::ShouldPresentUserSigninUpgrade(
      chrome_browser_state_.get(), version_2_0));
}

// Should show the sign-in upgrade a second time, 2 version after.
TEST_F(SigninUtilsTest, TestWillDisplayTwoMajorVersions) {
  fake_system_identity_manager()->AddIdentities(@[ @"foo", @"bar" ]);
  const base::Version version_1_0("1.0");
  const base::Version version_3_0("3.0");
  signin::RecordUpgradePromoSigninStarted(account_manager_service_,
                                          version_1_0);
  EXPECT_TRUE(signin::ShouldPresentUserSigninUpgrade(
      chrome_browser_state_.get(), version_3_0));
}

// Show the sign-in upgrade on version 1.0.
// Show the sign-in upgrade on version 3.0.
// Move to version 5.0.
// Expected: should not show the sign-in upgrade.
TEST_F(SigninUtilsTest, TestWillShowTwoTimesOnly) {
  fake_system_identity_manager()->AddIdentities(@[ @"foo", @"bar" ]);
  const base::Version version_1_0("1.0");
  const base::Version version_3_0("3.0");
  const base::Version version_5_0("5.0");
  signin::RecordUpgradePromoSigninStarted(account_manager_service_,
                                          version_1_0);
  signin::RecordUpgradePromoSigninStarted(account_manager_service_,
                                          version_3_0);
  EXPECT_FALSE(signin::ShouldPresentUserSigninUpgrade(
      chrome_browser_state_.get(), version_5_0));
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
  fake_system_identity_manager()->AddIdentities(@[ @"foo1" ]);
  EXPECT_TRUE(signin::ShouldPresentUserSigninUpgrade(
      chrome_browser_state_.get(), version_5_0));
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
  NSString* newAccountGaiaId = @"foo1";
  fake_system_identity_manager()->AddIdentities(@[ newAccountGaiaId ]);
  signin::RecordUpgradePromoSigninStarted(account_manager_service_,
                                          version_1_0);
  signin::RecordUpgradePromoSigninStarted(account_manager_service_,
                                          version_3_0);
  NSArray<id<SystemIdentity>>* allIdentities =
      account_manager_service_->GetAllIdentities();
  id<SystemIdentity> foo1Identity = nil;
  for (id<SystemIdentity> identity in allIdentities) {
    if ([identity.userFullName isEqualToString:newAccountGaiaId]) {
      ASSERT_EQ(nil, foo1Identity);
      foo1Identity = identity;
    }
  }
  ASSERT_NE(nil, foo1Identity);
  fake_system_identity_manager()->ForgetIdentity(foo1Identity,
                                                 base::DoNothing());
  EXPECT_FALSE(signin::ShouldPresentUserSigninUpgrade(
      chrome_browser_state_.get(), version_5_0));
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
  fake_system_identity_manager()->AddIdentities(@[ @"foo1" ]);
  EXPECT_FALSE(signin::ShouldPresentUserSigninUpgrade(
      chrome_browser_state_.get(), version_4_0));
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
  fake_system_identity_manager()->AddIdentities(@[ @"foo1" ]);
  EXPECT_FALSE(signin::ShouldPresentUserSigninUpgrade(
      chrome_browser_state_.get(), version_2_0));
}

// Should not show the sign-in upgrade if sign-in is disabled by policy.
TEST_F(SigninUtilsTest, TestWillNotShowIfDisabledByPolicy) {
  fake_system_identity_manager()->AddIdentities(@[ @"foo", @"bar" ]);
  const base::Version version_1_0("1.0");
  fake_system_identity_manager()->AddIdentities(@[ @"foo1" ]);
  GetLocalState()->SetInteger(prefs::kBrowserSigninPolicy,
                              static_cast<int>(BrowserSigninMode::kDisabled));

  EXPECT_FALSE(signin::ShouldPresentUserSigninUpgrade(
      chrome_browser_state_.get(), version_1_0));
}

// Should show if the user is signed-in without history opt-in.
TEST_F(SigninUtilsTest, TestWillShowIfSignedInWithoutHistoryOptIn) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      syncer::kReplaceSyncPromosWithSignInPromos);

  FakeSystemIdentity* identity =
      [FakeSystemIdentity identityWithEmail:@"foo1@gmail.com"
                                     gaiaID:@"foo1ID"
                                       name:@"Fake Foo 1"];
  fake_system_identity_manager()->AddIdentity(identity);
  AuthenticationService* authentication_service =
      AuthenticationServiceFactory::GetForBrowserState(
          chrome_browser_state_.get());
  authentication_service->SignIn(
      identity, signin_metrics::AccessPoint::ACCESS_POINT_UNKNOWN);

  const base::Version version_1_0("1.0");
  const base::Version version_3_0("3.0");
  signin::RecordUpgradePromoSigninStarted(account_manager_service_,
                                          version_1_0);
  EXPECT_TRUE(signin::ShouldPresentUserSigninUpgrade(
      chrome_browser_state_.get(), version_3_0));
}

// Should not show if the user is signed-in with history opt-in.
TEST_F(SigninUtilsTest, TestWillNotShowIfSignedInWithHistoryOptIn) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      syncer::kReplaceSyncPromosWithSignInPromos);

  FakeSystemIdentity* identity =
      [FakeSystemIdentity identityWithEmail:@"foo1@gmail.com"
                                     gaiaID:@"foo1ID"
                                       name:@"Fake Foo 1"];
  fake_system_identity_manager()->AddIdentity(identity);
  AuthenticationService* authentication_service =
      AuthenticationServiceFactory::GetForBrowserState(
          chrome_browser_state_.get());
  authentication_service->SignIn(
      identity, signin_metrics::AccessPoint::ACCESS_POINT_UNKNOWN);

  syncer::SyncService* sync_service =
      SyncServiceFactory::GetForBrowserState(chrome_browser_state_.get());
  syncer::SyncUserSettings* sync_user_settings =
      sync_service->GetUserSettings();
  sync_user_settings->SetSelectedType(syncer::UserSelectableType::kHistory,
                                      true);
  sync_user_settings->SetSelectedType(syncer::UserSelectableType::kTabs, true);

  const base::Version version_1_0("1.0");
  const base::Version version_3_0("3.0");
  signin::RecordUpgradePromoSigninStarted(account_manager_service_,
                                          version_1_0);
  EXPECT_FALSE(signin::ShouldPresentUserSigninUpgrade(
      chrome_browser_state_.get(), version_3_0));
}

// signin::GetPrimaryIdentitySigninState for a signed-out user should
// return the signed out state.
TEST_F(SigninUtilsTest, TestGetPrimaryIdentitySigninStateSignedOut) {
  IdentitySigninState state =
      signin::GetPrimaryIdentitySigninState(chrome_browser_state_.get());
  EXPECT_EQ(IdentitySigninStateSignedOut, state);
}

// signin::GetPrimaryIdentitySigninState for a signed-in user should
// return the signed-in, sync disabled state.
TEST_F(SigninUtilsTest, TestGetPrimaryIdentitySigninStateSignedInSyncDisabled) {
  FakeSystemIdentity* identity =
      [FakeSystemIdentity identityWithEmail:@"foo1@gmail.com"
                                     gaiaID:@"foo1ID"
                                       name:@"Fake Foo 1"];
  fake_system_identity_manager()->AddIdentity(identity);
  AuthenticationService* authentication_service =
      AuthenticationServiceFactory::GetForBrowserState(
          chrome_browser_state_.get());
  authentication_service->SignIn(
      identity, signin_metrics::AccessPoint::ACCESS_POINT_UNKNOWN);

  IdentitySigninState state =
      signin::GetPrimaryIdentitySigninState(chrome_browser_state_.get());
  EXPECT_EQ(IdentitySigninStateSignedInWithSyncDisabled, state);
}

// signin::GetPrimaryIdentitySigninState for a syncing user who has
// completed the sync setup should return the signed-in, sync enabled state.
TEST_F(SigninUtilsTest,
       TestGetPrimaryIdentitySigninStateSyncGrantedSetupComplete) {
  FakeSystemIdentity* identity =
      [FakeSystemIdentity identityWithEmail:@"foo1@gmail.com"
                                     gaiaID:@"foo1ID"
                                       name:@"Fake Foo 1"];
  fake_system_identity_manager()->AddIdentity(identity);
  AuthenticationService* authentication_service =
      AuthenticationServiceFactory::GetForBrowserState(
          chrome_browser_state_.get());
  authentication_service->SignIn(
      identity, signin_metrics::AccessPoint::ACCESS_POINT_SIGNIN_PROMO);
  authentication_service->GrantSyncConsent(
      identity, signin_metrics::AccessPoint::ACCESS_POINT_SIGNIN_PROMO);
  chrome_browser_state_->GetPrefs()->SetBoolean(
      syncer::prefs::internal::kSyncInitialSyncFeatureSetupComplete, true);

  IdentitySigninState state =
      signin::GetPrimaryIdentitySigninState(chrome_browser_state_.get());
  EXPECT_EQ(IdentitySigninStateSignedInWithSyncEnabled, state);

  chrome_browser_state_->GetPrefs()->ClearPref(
      syncer::prefs::internal::kSyncInitialSyncFeatureSetupComplete);
}

// Regression test for crbug.com/1248042.
// signin::GetPrimaryIdentitySigninState for a syncing user who has not
// completed the sync setup (due to a crash while in advanced settings) should
// return the signed-in, sync disabled state.
TEST_F(SigninUtilsTest, TestGetPrimaryIdentitySigninStateSyncGranted) {
  FakeSystemIdentity* identity =
      [FakeSystemIdentity identityWithEmail:@"foo1@gmail.com"
                                     gaiaID:@"foo1ID"
                                       name:@"Fake Foo 1"];
  fake_system_identity_manager()->AddIdentity(identity);
  AuthenticationService* authentication_service =
      AuthenticationServiceFactory::GetForBrowserState(
          chrome_browser_state_.get());
  authentication_service->SignIn(
      identity, signin_metrics::AccessPoint::ACCESS_POINT_SIGNIN_PROMO);
  authentication_service->GrantSyncConsent(
      identity, signin_metrics::AccessPoint::ACCESS_POINT_SIGNIN_PROMO);

  IdentitySigninState state =
      signin::GetPrimaryIdentitySigninState(chrome_browser_state_.get());
  EXPECT_EQ(IdentitySigninStateSignedInWithSyncDisabled, state);
}

}  // namespace
