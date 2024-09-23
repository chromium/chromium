// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/commerce/model/price_alert_util.h"

#import "base/memory/raw_ptr.h"
#import "components/commerce/core/commerce_feature_list.h"
#import "components/sync_preferences/testing_pref_service_syncable.h"
#import "components/unified_consent/pref_names.h"
#import "components/unified_consent/unified_consent_service.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/fake_authentication_service_delegate.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/browser/signin/model/fake_system_identity_manager.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"

class PriceAlertUtilTest : public PlatformTest {
 public:
  void SetUp() override {
    profile_ = BuildProfileIOS();
    AuthenticationServiceFactory::CreateAndInitializeForBrowserState(
        profile_.get(), std::make_unique<FakeAuthenticationServiceDelegate>());
    auth_service_ = static_cast<AuthenticationService*>(
        AuthenticationServiceFactory::GetInstance()->GetForProfile(
            profile_.get()));
    fake_identity_ = [FakeSystemIdentity fakeIdentity1];
  }

  std::unique_ptr<TestProfileIOS> BuildProfileIOS() {
    TestProfileIOS::Builder builder;
    builder.AddTestingFactory(
        AuthenticationServiceFactory::GetInstance(),
        AuthenticationServiceFactory::GetDefaultFactory());
    return std::move(builder).Build();
  }

  void SetMSBB(bool enabled) {
    profile_->GetPrefs()->SetBoolean(
        unified_consent::prefs::kUrlKeyedAnonymizedDataCollectionEnabled,
        enabled);
  }

  void SetUserSetting(bool enabled) {
    profile_->GetPrefs()->SetBoolean(prefs::kTrackPricesOnTabsEnabled, enabled);
  }

  void SignIn() {
    FakeSystemIdentityManager* system_identity_manager =
        FakeSystemIdentityManager::FromSystemIdentityManager(
            GetApplicationContext()->GetSystemIdentityManager());
    system_identity_manager->AddIdentity(fake_identity_);
    auth_service_->SignIn(fake_identity_,
                          signin_metrics::AccessPoint::ACCESS_POINT_UNKNOWN);
  }

  void SignOut() {
    auth_service_->SignOut(signin_metrics::ProfileSignout::kTest,
                           /*force_clear_browsing_data=*/false, nil);
  }

 protected:
  web::WebTaskEnvironment task_environment_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  std::unique_ptr<TestProfileIOS> profile_;
  raw_ptr<AuthenticationService> auth_service_ = nullptr;
  FakeSystemIdentity* fake_identity_ = nullptr;
};

TEST_F(PriceAlertUtilTest, TestMSBBOff) {
  SetMSBB(false);
  SignIn();
  EXPECT_FALSE(IsPriceAlertsEligible(profile_.get()));
}

TEST_F(PriceAlertUtilTest, TestNotSignedIn) {
  SetMSBB(true);
  EXPECT_FALSE(IsPriceAlertsEligible(profile_.get()));
}

TEST_F(PriceAlertUtilTest, TestPriceAlertsAllowed) {
  SignIn();
  SetMSBB(true);
  EXPECT_TRUE(IsPriceAlertsEligible(profile_.get()));
}

TEST_F(PriceAlertUtilTest, TestPriceAlertsEligibleThenSignOut) {
  SignIn();
  SetMSBB(true);
  EXPECT_TRUE(IsPriceAlertsEligible(profile_.get()));
  SignOut();
  EXPECT_FALSE(IsPriceAlertsEligible(profile_.get()));
}

TEST_F(PriceAlertUtilTest, TestIncognito) {
  SignIn();
  SetMSBB(true);
  EXPECT_FALSE(IsPriceAlertsEligible(profile_->GetOffTheRecordProfile()));
}

TEST_F(PriceAlertUtilTest, TestUserSettingOn) {
  SignIn();
  SetMSBB(true);
  SetUserSetting(true);
  EXPECT_TRUE(IsPriceAlertsEligible(profile_.get()));
}

TEST_F(PriceAlertUtilTest, TestUserSettingOff) {
  SignIn();
  SetMSBB(true);
  SetUserSetting(false);
  EXPECT_FALSE(IsPriceAlertsEligible(profile_.get()));
}
