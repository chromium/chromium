// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/commerce/price_alert_util.h"

#include "base/test/scoped_feature_list.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "components/unified_consent/pref_names.h"
#include "components/unified_consent/unified_consent_service.h"
#import "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/signin/authentication_service_factory.h"
#import "ios/chrome/browser/signin/authentication_service_fake.h"
#import "ios/chrome/browser/ui/ui_feature_flags.h"
#import "ios/public/provider/chrome/browser/signin/fake_chrome_identity.h"
#import "ios/web/public/test/web_task_environment.h"
#import "ios/web/public/test/web_test_with_web_state.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
const char kPriceTrackingWithOptimizationGuideParam[] =
    "price_tracking_with_optimization_guide";
}  // namespace

class PriceAlertUtilTest : public PlatformTest {
 public:
  void SetUp() override {
    browser_state_ = BuildChromeBrowserState();
    auth_service_ = static_cast<AuthenticationServiceFake*>(
        AuthenticationServiceFactory::GetInstance()->GetForBrowserState(
            browser_state_.get()));
    fake_identity_ = [FakeChromeIdentity identityWithEmail:@"foo1@gmail.com"
                                                    gaiaID:@"foo1ID"
                                                      name:@"Fake Foo 1"];
  }

  std::unique_ptr<TestChromeBrowserState> BuildChromeBrowserState() {
    TestChromeBrowserState::Builder builder;
    builder.AddTestingFactory(
        AuthenticationServiceFactory::GetInstance(),
        base::BindRepeating(
            &AuthenticationServiceFake::CreateAuthenticationService));
    return builder.Build();
  }

  void SetMSBB(bool enabled) {
    browser_state_->GetPrefs()->SetBoolean(
        unified_consent::prefs::kUrlKeyedAnonymizedDataCollectionEnabled,
        enabled);
  }

  void SetFeatureFlag(bool enabled) {
    if (enabled) {
      scoped_feature_list_.InitAndEnableFeatureWithParameters(
          kCommercePriceTracking,
          {{kPriceTrackingWithOptimizationGuideParam, "true"}});
    } else {
      scoped_feature_list_.InitWithFeatures({}, {});
    }
  }

  void SignIn() { auth_service_->SignIn(fake_identity_); }

  void SignOut() {
    auth_service_->SignOut(signin_metrics::SIGNOUT_TEST,
                           /*force_clear_browsing_data=*/false, nil);
  }

 protected:
  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestChromeBrowserState> browser_state_;
  AuthenticationServiceFake* auth_service_ = nullptr;
  FakeChromeIdentity* fake_identity_ = nullptr;
  base::test::ScopedFeatureList scoped_feature_list_;
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

TEST_F(PriceAlertUtilTest, TestFlagOff) {
  SetFeatureFlag(false);
  EXPECT_FALSE(IsPriceAlertsEnabled());
}

TEST_F(PriceAlertUtilTest, TestFlagOn) {
  SetFeatureFlag(true);
  EXPECT_TRUE(IsPriceAlertsEnabled());
}

TEST_F(PriceAlertUtilTest, TestPriceAlertsAllowed) {
  SignIn();
  SetFeatureFlag(true);
  SetMSBB(true);
  EXPECT_TRUE(IsPriceAlertsEligible(browser_state_.get()));
  EXPECT_TRUE(IsPriceAlertsEnabled());
}

TEST_F(PriceAlertUtilTest, TestPriceAlertsEligibleThenSignOut) {
  SignIn();
  SetFeatureFlag(true);
  SetMSBB(true);
  EXPECT_TRUE(IsPriceAlertsEligible(browser_state_.get()));
  SignOut();
  EXPECT_FALSE(IsPriceAlertsEligible(browser_state_.get()));
}

TEST_F(PriceAlertUtilTest, TestIncognito) {
  web::FakeBrowserState fake_browser_state;
  fake_browser_state.SetOffTheRecord(true);
  SignIn();
  SetFeatureFlag(true);
  SetMSBB(true);
  EXPECT_FALSE(IsPriceAlertsEligible(&fake_browser_state));
}
