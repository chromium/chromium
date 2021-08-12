// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/signin/signin_utils.h"

#import <UIKit/UIKit.h>

#include <memory>

#include "base/test/scoped_feature_list.h"
#import "base/version.h"
#import "components/pref_registry/pref_registry_syncable.h"
#import "components/signin/public/base/signin_pref_names.h"
#import "components/signin/public/base/signin_switches.h"
#import "components/sync_preferences/pref_service_mock_factory.h"
#import "components/sync_preferences/pref_service_syncable.h"
#import "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/prefs/browser_prefs.h"
#import "ios/chrome/browser/signin/authentication_service_factory.h"
#import "ios/chrome/browser/signin/authentication_service_fake.h"
#import "ios/chrome/browser/signin/chrome_account_manager_service_factory.h"
#import "ios/chrome/browser/ui/authentication/signin/user_signin/user_signin_constants.h"
#import "ios/public/provider/chrome/browser/signin/fake_chrome_identity_service.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

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
        base::BindRepeating(
            &AuthenticationServiceFake::CreateAuthenticationService));
    chrome_browser_state_ = builder.Build();
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

 protected:
  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestChromeBrowserState> chrome_browser_state_;
  ChromeAccountManagerService* account_manager_service_;
};

// Should show the sign-in upgrade for the first time.
TEST_F(SigninUtilsTest, TestWillDisplay) {
  ios::FakeChromeIdentityService::GetInstanceFromChromeProvider()
      ->AddIdentities(@[ @"foo", @"bar" ]);
  const base::Version version_1_0("1.0");
  EXPECT_TRUE(signin::ShouldPresentUserSigninUpgrade(
      chrome_browser_state_.get(), version_1_0));
}

// Should not show the sign-in upgrade twice on the same version.
TEST_F(SigninUtilsTest, TestWillNotDisplaySameVersion) {
  ios::FakeChromeIdentityService::GetInstanceFromChromeProvider()
      ->AddIdentities(@[ @"foo", @"bar" ]);
  const base::Version version_1_0("1.0");
  signin::RecordVersionSeen(account_manager_service_, version_1_0);
  EXPECT_FALSE(signin::ShouldPresentUserSigninUpgrade(
      chrome_browser_state_.get(), version_1_0));
}

// Should not show the sign-in upgrade twice until two major version after.
TEST_F(SigninUtilsTest, TestWillNotDisplayOneMinorVersion) {
  ios::FakeChromeIdentityService::GetInstanceFromChromeProvider()
      ->AddIdentities(@[ @"foo", @"bar" ]);
  const base::Version version_1_0("1.0");
  const base::Version version_1_1("1.1");
  signin::RecordVersionSeen(account_manager_service_, version_1_0);
  EXPECT_FALSE(signin::ShouldPresentUserSigninUpgrade(
      chrome_browser_state_.get(), version_1_1));
}

// Should not show the sign-in upgrade twice until two major version after.
TEST_F(SigninUtilsTest, TestWillNotDisplayTwoMinorVersions) {
  ios::FakeChromeIdentityService::GetInstanceFromChromeProvider()
      ->AddIdentities(@[ @"foo", @"bar" ]);
  const base::Version version_1_0("1.0");
  const base::Version version_1_2("1.2");
  signin::RecordVersionSeen(account_manager_service_, version_1_0);
  EXPECT_FALSE(signin::ShouldPresentUserSigninUpgrade(
      chrome_browser_state_.get(), version_1_2));
}

// Should not show the sign-in upgrade twice until two major version after.
TEST_F(SigninUtilsTest, TestWillNotDisplayOneMajorVersion) {
  ios::FakeChromeIdentityService::GetInstanceFromChromeProvider()
      ->AddIdentities(@[ @"foo", @"bar" ]);
  const base::Version version_1_0("1.0");
  const base::Version version_2_0("2.0");
  signin::RecordVersionSeen(account_manager_service_, version_1_0);
  EXPECT_FALSE(signin::ShouldPresentUserSigninUpgrade(
      chrome_browser_state_.get(), version_2_0));
}

// Should show the sign-in upgrade a second time, 2 version after.
TEST_F(SigninUtilsTest, TestWillDisplayTwoMajorVersions) {
  ios::FakeChromeIdentityService::GetInstanceFromChromeProvider()
      ->AddIdentities(@[ @"foo", @"bar" ]);
  const base::Version version_1_0("1.0");
  const base::Version version_3_0("3.0");
  signin::RecordVersionSeen(account_manager_service_, version_1_0);
  EXPECT_TRUE(signin::ShouldPresentUserSigninUpgrade(
      chrome_browser_state_.get(), version_3_0));
}

// Show the sign-in upgrade on version 1.0.
// Show the sign-in upgrade on version 3.0.
// Move to version 5.0.
// Expected: should not show the sign-in upgrade.
TEST_F(SigninUtilsTest, TestWillShowTwoTimesOnly) {
  ios::FakeChromeIdentityService::GetInstanceFromChromeProvider()
      ->AddIdentities(@[ @"foo", @"bar" ]);
  const base::Version version_1_0("1.0");
  const base::Version version_3_0("3.0");
  const base::Version version_5_0("5.0");
  signin::RecordVersionSeen(account_manager_service_, version_1_0);
  signin::RecordVersionSeen(account_manager_service_, version_3_0);
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
  signin::RecordVersionSeen(account_manager_service_, version_1_0);
  signin::RecordVersionSeen(account_manager_service_, version_3_0);
  ios::FakeChromeIdentityService::GetInstanceFromChromeProvider()
      ->AddIdentities(@[ @"foo1" ]);
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
  ios::FakeChromeIdentityService::GetInstanceFromChromeProvider()
      ->AddIdentities(@[ newAccountGaiaId ]);
  signin::RecordVersionSeen(account_manager_service_, version_1_0);
  signin::RecordVersionSeen(account_manager_service_, version_3_0);
  NSArray* allIdentities = account_manager_service_->GetAllIdentities();
  ChromeIdentity* foo1Identity = nil;
  for (ChromeIdentity* identity in allIdentities) {
    if ([identity.userFullName isEqualToString:newAccountGaiaId]) {
      ASSERT_EQ(nil, foo1Identity);
      foo1Identity = identity;
    }
  }
  ASSERT_NE(nil, foo1Identity);
  ios::FakeChromeIdentityService::GetInstanceFromChromeProvider()
      ->ForgetIdentity(foo1Identity, nil);
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
  signin::RecordVersionSeen(account_manager_service_, version_1_0);
  signin::RecordVersionSeen(account_manager_service_, version_3_0);
  ios::FakeChromeIdentityService::GetInstanceFromChromeProvider()
      ->AddIdentities(@[ @"foo1" ]);
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
  signin::RecordVersionSeen(account_manager_service_, version_1_0);
  ios::FakeChromeIdentityService::GetInstanceFromChromeProvider()
      ->AddIdentities(@[ @"foo1" ]);
  EXPECT_FALSE(signin::ShouldPresentUserSigninUpgrade(
      chrome_browser_state_.get(), version_2_0));
}

// Should not show the sign-in upgrade if sign-in is disabled by policy.
TEST_F(SigninUtilsTest, TestWillNotShowIfDisabledByPolicy) {
  ios::FakeChromeIdentityService::GetInstanceFromChromeProvider()
      ->AddIdentities(@[ @"foo", @"bar" ]);
  const base::Version version_1_0("1.0");
  ios::FakeChromeIdentityService::GetInstanceFromChromeProvider()
      ->AddIdentities(@[ @"foo1" ]);
  chrome_browser_state_->GetPrefs()->SetBoolean(prefs::kSigninAllowedByPolicy,
                                                false);

  EXPECT_FALSE(signin::ShouldPresentUserSigninUpgrade(
      chrome_browser_state_.get(), version_1_0));
}

// signin::IsSigninAllowed should respect the kSigninAllowed pref with MICE
// enabled.
TEST_F(SigninUtilsTest, TestSigninAllowedPref) {
  base::test::ScopedFeatureList scoped_feature;
  scoped_feature.InitAndEnableFeature(signin::kMobileIdentityConsistency);

  ios::FakeChromeIdentityService::GetInstanceFromChromeProvider()
      ->AddIdentities(@[ @"foo", @"bar" ]);
  // Sign-in is allowed by default.
  EXPECT_TRUE(signin::IsSigninAllowed(chrome_browser_state_.get()->GetPrefs()));

  // When sign-in is disabled, the accessor should return false.
  chrome_browser_state_->GetPrefs()->SetBoolean(prefs::kSigninAllowed, false);
  EXPECT_FALSE(
      signin::IsSigninAllowed(chrome_browser_state_.get()->GetPrefs()));
}

// signin::IsSigninAllowedByPolicy should respect the kSigninAllowedByPolicy
// pref.
TEST_F(SigninUtilsTest, TestSigninAllowedByPolicyPref) {
  ios::FakeChromeIdentityService::GetInstanceFromChromeProvider()
      ->AddIdentities(@[ @"foo", @"bar" ]);
  // Sign-in is allowed by default.
  EXPECT_TRUE(
      signin::IsSigninAllowedByPolicy(chrome_browser_state_.get()->GetPrefs()));

  // When sign-in is disabled by policy, the accessor should return false.
  chrome_browser_state_->GetPrefs()->SetBoolean(prefs::kSigninAllowedByPolicy,
                                                false);
  EXPECT_FALSE(
      signin::IsSigninAllowedByPolicy(chrome_browser_state_.get()->GetPrefs()));
  EXPECT_FALSE(
      signin::IsSigninAllowed(chrome_browser_state_.get()->GetPrefs()));

  // When sign-in is explicitly enabled by the user, but the policy has not
  // changed the accessor should return false.
  chrome_browser_state_->GetPrefs()->SetBoolean(prefs::kSigninAllowed, true);

  EXPECT_FALSE(
      signin::IsSigninAllowedByPolicy(chrome_browser_state_.get()->GetPrefs()));
  EXPECT_FALSE(
      signin::IsSigninAllowed(chrome_browser_state_.get()->GetPrefs()));
}

// Should not show the sign-in upgrade when extended sync promos are disabled.
TEST_F(SigninUtilsTest, TestWillNotShowWhenPromosDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      switches::kForceDisableExtendedSyncPromos);

  ios::FakeChromeIdentityService::GetInstanceFromChromeProvider()
      ->AddIdentities(@[ @"foo", @"bar" ]);
  const base::Version version_1_0("1.0");

  EXPECT_FALSE(signin::ShouldPresentUserSigninUpgrade(
      chrome_browser_state_.get(), version_1_0));
}

// signin::IsSigninAllowed should not use kSigninAllowed pref when MICE is
// disabled.
TEST_F(SigninUtilsTest, TestSigninAllowedPrefPreMICE) {
  base::test::ScopedFeatureList scoped_feature;
  scoped_feature.InitAndDisableFeature(signin::kMobileIdentityConsistency);

  ios::FakeChromeIdentityService::GetInstanceFromChromeProvider()
      ->AddIdentities(@[ @"foo", @"bar" ]);
  // Sign-in is allowed by default.
  EXPECT_TRUE(signin::IsSigninAllowed(chrome_browser_state_.get()->GetPrefs()));
}

}  // namespace
