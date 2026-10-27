// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/authentication/account_switching_promo/model/account_switching_promo_prefs.h"

#import "components/prefs/pref_registry_simple.h"
#import "components/prefs/testing_pref_service.h"
#import "google_apis/gaia/gaia_id.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace signin {

class AccountSwitchingPromoPrefsTest : public PlatformTest {
 public:
  AccountSwitchingPromoPrefsTest() {
    RegisterAccountSwitchingPromoProfilePrefs(pref_service_.registry());
  }

 protected:
  TestingPrefServiceSimple pref_service_;
  const GaiaId account_a_ = GaiaId("gaia_a");
  const GaiaId account_b_ = GaiaId("gaia_b");
  const GaiaId account_c_ = GaiaId("gaia_c");
};

TEST_F(AccountSwitchingPromoPrefsTest, InitiallyAllowed) {
  EXPECT_TRUE(
      CanShowAccountSwitchingPromo(pref_service_, account_a_, account_b_));
}

TEST_F(AccountSwitchingPromoPrefsTest,
       SwitchingAccountsPermanentlySuppressesReversePair) {
  // User switches from Account B to Account A.
  SuppressAccountSwitchingReversePair(pref_service_, account_b_, account_a_);

  // Reverse pair (A -> B) is permanently suppressed, whereas (A -> C) is
  // allowed.
  EXPECT_FALSE(
      CanShowAccountSwitchingPromo(pref_service_, account_a_, account_b_));
  EXPECT_TRUE(
      CanShowAccountSwitchingPromo(pref_service_, account_a_, account_c_));
}

}  // namespace signin
