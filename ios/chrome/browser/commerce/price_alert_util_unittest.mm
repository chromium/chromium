// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/commerce/price_alert_util.h"

#import "base/test/scoped_feature_list.h"
#import "components/commerce/core/commerce_feature_list.h"
#import "components/sync_preferences/testing_pref_service_syncable.h"
#import "components/unified_consent/pref_names.h"
#import "components/unified_consent/unified_consent_service.h"
#import "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/prefs/pref_names.h"
#import "ios/chrome/browser/signin/authentication_service.h"
#import "ios/chrome/browser/signin/authentication_service_factory.h"
#import "ios/chrome/browser/signin/fake_authentication_service_delegate.h"
#import "ios/chrome/browser/signin/fake_system_identity.h"
#import "ios/chrome/browser/ui/ui_feature_flags.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/public/provider/chrome/browser/signin/fake_chrome_identity_service.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

class PriceAlertUtilTest : public PlatformTest {
 public:
  void SetUp() override {
    browser_state_ = BuildChromeBrowserState();
    AuthenticationServiceFactory::CreateAndInitializeForBrowserState(
        browser_state_.get(),
        std::make_unique<FakeAuthenticationServiceDelegate>());
    auth_service_ = static_cast<AuthenticationService*>(
        AuthenticationServiceFactory::GetInstance()->GetForBrowserState(
            browser_state_.get()));
    fake_identity_ = [FakeSystemIdentity fakeIdentity1];
  }

  std::unique_ptr<TestChromeBrowserState> BuildChromeBrowserState() {
    TestChromeBrowserState::Builder builder;
    builder.AddTestingFactory(
        AuthenticationServiceFactory::GetInstance(),
        AuthenticationServiceFactory::GetDefaultFactory());
    return builder.Build();
  }

  void SetMSBB(bool enabled) {
    browser_state_->GetPrefs()->SetBoolean(
        unified_consent::prefs::kUrlKeyedAnonymizedDataCollectionEnabled,
        enabled);
  }

  void SetUserSetting(bool enabled) {
    browser_state_->GetPrefs()->SetBoolean(prefs::kTrackPricesOnTabsEnabled,
                                           enabled);
  }

  void SignIn() {
    ios::FakeChromeIdentityService* identity_service_ =
        ios::FakeChromeIdentityService::GetInstanceFromChromeProvider();
    identity_service_->AddIdentity(fake_identity_);
    auth_service_->SignIn(fake_identity_);
  }

  void SignOut() {
    auth_service_->SignOut(signin_metrics::SIGNOUT_TEST,
                           /*force_clear_browsing_data=*/false, nil);
  }

 protected:
  web::WebTaskEnvironment task_environment_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  std::unique_ptr<TestChromeBrowserState> browser_state_;
  AuthenticationService* auth_service_ = nullptr;
  FakeSystemIdentity* fake_identity_ = nullptr;
};

TEST_F(PriceAlertUtilTest, TestMSBBOff) {
  SetMSBB(false);
  SignIn();
  EXPECT_FALSE(IsPriceAlertsEligible(browser_state_.get()));
}

TEST_F(PriceAlertUtilTest, TestNotSignedIn) {
  SetMSBB(true);
  EXPECT_FALSE(IsPriceAlertsEligible(browser_state_.get()));
}

TEST_F(PriceAlertUtilTest, TestPriceAlertsAllowed) {
  SignIn();
  SetMSBB(true);
  EXPECT_TRUE(IsPriceAlertsEligible(browser_state_.get()));
}

TEST_F(PriceAlertUtilTest, TestPriceAlertsEligibleThenSignOut) {
  SignIn();
  SetMSBB(true);
  EXPECT_TRUE(IsPriceAlertsEligible(browser_state_.get()));
  SignOut();
  EXPECT_FALSE(IsPriceAlertsEligible(browser_state_.get()));
}

TEST_F(PriceAlertUtilTest, TestIncognito) {
  SignIn();
  SetMSBB(true);
  EXPECT_FALSE(IsPriceAlertsEligible(
      browser_state_->GetOffTheRecordChromeBrowserState()));
}

TEST_F(PriceAlertUtilTest, TestUserSettingOn) {
  SignIn();
  SetMSBB(true);
  SetUserSetting(true);
  EXPECT_TRUE(IsPriceAlertsEligible(browser_state_.get()));
}

TEST_F(PriceAlertUtilTest, TestUserSettingOff) {
  SignIn();
  SetMSBB(true);
  SetUserSetting(false);
  EXPECT_FALSE(IsPriceAlertsEligible(browser_state_.get()));
}
