// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/authentication/account_switching_promo/model/account_switching_promo_prefs.h"

#import "base/memory/raw_ptr.h"
#import "components/prefs/pref_service.h"
#import "google_apis/gaia/gaia_id.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_manager_ios.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace signin {

class AccountSwitchingPromoPrefsTest : public PlatformTest {
 public:
  AccountSwitchingPromoPrefsTest() {
    personal_profile_ =
        profile_manager_.AddProfileWithBuilder(TestProfileIOS::Builder());
  }

 protected:
  // Returns the `PrefService` of `personal_profile_`.
  PrefService& pref_service() { return *personal_profile_->GetPrefs(); }

  web::WebTaskEnvironment task_environment_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  TestProfileManagerIOS profile_manager_;
  // The personal profile.
  raw_ptr<TestProfileIOS> personal_profile_;
  const GaiaId account_a_ = GaiaId("gaia_a");
  const GaiaId account_b_ = GaiaId("gaia_b");
  const GaiaId account_c_ = GaiaId("gaia_c");
};

TEST_F(AccountSwitchingPromoPrefsTest, InitiallyAllowed) {
  EXPECT_TRUE(
      CanShowAccountSwitchingPromo(pref_service(), account_a_, account_b_));
}

TEST_F(AccountSwitchingPromoPrefsTest,
       SwitchingAccountsPermanentlySuppressesReversePair) {
  // User switches from Account B to Account A.
  SuppressAccountSwitchingReversePair(pref_service(), account_b_, account_a_);

  // Reverse pair (A -> B) is permanently suppressed, whereas (A -> C) is
  // allowed.
  EXPECT_FALSE(
      CanShowAccountSwitchingPromo(pref_service(), account_a_, account_b_));
  EXPECT_TRUE(
      CanShowAccountSwitchingPromo(pref_service(), account_a_, account_c_));
}

// Tests that explicitly dismissing the account switching promo twice for a
// specific (`account_a_`, `account_b_`) pair suppresses that pair from being
// shown, while other pairs and the reverse direction (`account_b_`,
// `account_a_`) remain allowed.
TEST_F(AccountSwitchingPromoPrefsTest,
       ExplicitDismissalSuppressesPairAtTwoDismissals) {
  RecordAccountSwitchingPromoExplicitlyDismissed(pref_service(), account_a_,
                                                 account_b_);
  EXPECT_TRUE(
      CanShowAccountSwitchingPromo(pref_service(), account_a_, account_b_));

  RecordAccountSwitchingPromoExplicitlyDismissed(pref_service(), account_a_,
                                                 account_b_);
  EXPECT_FALSE(
      CanShowAccountSwitchingPromo(pref_service(), account_a_, account_b_));
  // Another pair (A -> C) remains eligible in per-pair prefs.
  EXPECT_TRUE(
      CanShowAccountSwitchingPromo(pref_service(), account_a_, account_c_));
  // The reverse direction (B -> A) remains eligible as well.
  EXPECT_TRUE(
      CanShowAccountSwitchingPromo(pref_service(), account_b_, account_a_));
}

}  // namespace signin
