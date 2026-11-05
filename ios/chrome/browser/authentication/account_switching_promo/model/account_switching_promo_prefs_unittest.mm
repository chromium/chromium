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
  const GaiaId account_d_ = GaiaId("gaia_d");
};

TEST_F(AccountSwitchingPromoPrefsTest, InitiallyAllowed) {
  EXPECT_TRUE(
      CanShowAccountSwitchingPromo(pref_service(), account_a_, account_b_));
}

TEST_F(AccountSwitchingPromoPrefsTest, SwitchingAccountsSuppressesReversePair) {
  // User switches from Account B to Account A.
  SuppressAccountSwitchingReversePair(pref_service(), account_b_, account_a_);

  // Reverse pair (A -> B) is suppressed, whereas (A -> C) is allowed.
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

// Tests that calling `DeleteAccountSwitchingPromoStateForRemovedAccounts()`
// deletes stored explicit dismissal state when the target account is removed,
// while preserving dismissal state for pairs where both accounts remain on the
// device.
TEST_F(AccountSwitchingPromoPrefsTest,
       RemovingTargetAccountDeletesPairDismissalStateFromPrefs) {
  // Dismiss (A -> B) twice and (A -> C) twice.
  RecordAccountSwitchingPromoExplicitlyDismissed(pref_service(), account_a_,
                                                 account_b_);
  RecordAccountSwitchingPromoExplicitlyDismissed(pref_service(), account_a_,
                                                 account_b_);
  RecordAccountSwitchingPromoExplicitlyDismissed(pref_service(), account_a_,
                                                 account_c_);
  RecordAccountSwitchingPromoExplicitlyDismissed(pref_service(), account_a_,
                                                 account_c_);

  // Also dismiss (C -> D) twice.
  RecordAccountSwitchingPromoExplicitlyDismissed(pref_service(), account_c_,
                                                 account_d_);
  RecordAccountSwitchingPromoExplicitlyDismissed(pref_service(), account_c_,
                                                 account_d_);

  ASSERT_FALSE(
      CanShowAccountSwitchingPromo(pref_service(), account_a_, account_b_));
  ASSERT_FALSE(
      CanShowAccountSwitchingPromo(pref_service(), account_a_, account_c_));
  ASSERT_FALSE(
      CanShowAccountSwitchingPromo(pref_service(), account_c_, account_d_));

  // Remove Account B from the device; A, C, and D remain active.
  DeleteAccountSwitchingPromoStateForRemovedAccounts(
      pref_service(), {account_a_, account_c_, account_d_});

  // Since Account B was removed, the (A -> B) dismissal history is deleted.
  EXPECT_TRUE(
      CanShowAccountSwitchingPromo(pref_service(), account_a_, account_b_));
  // Since both A and C remain active, (A -> C) remains suppressed.
  EXPECT_FALSE(
      CanShowAccountSwitchingPromo(pref_service(), account_a_, account_c_));
  // Unrelated pair (C -> D) also remains suppressed.
  EXPECT_FALSE(
      CanShowAccountSwitchingPromo(pref_service(), account_c_, account_d_));
}

// Tests that calling `DeleteAccountSwitchingPromoStateForRemovedAccounts()`
// deletes stored explicit dismissal state when the primary account is removed.
TEST_F(AccountSwitchingPromoPrefsTest,
       RemovingPrimaryAccountDeletesPairDismissalStateFromPrefs) {
  RecordAccountSwitchingPromoExplicitlyDismissed(pref_service(), account_a_,
                                                 account_b_);
  RecordAccountSwitchingPromoExplicitlyDismissed(pref_service(), account_a_,
                                                 account_b_);
  ASSERT_FALSE(
      CanShowAccountSwitchingPromo(pref_service(), account_a_, account_b_));

  // Remove primary Account A from the device; B remains active.
  DeleteAccountSwitchingPromoStateForRemovedAccounts(pref_service(),
                                                     {account_b_});

  EXPECT_TRUE(
      CanShowAccountSwitchingPromo(pref_service(), account_a_, account_b_));
}

// Tests that removing the primary account of a pair deletes its reverse switch
// suppression, while preserving suppression of pairs where both accounts
// remain on the device.
TEST_F(AccountSwitchingPromoPrefsTest,
       RemovingPrimaryAccountDeletesReverseSwitchSuppression) {
  // User switches from Account B to Account A, suppressing (A -> B).
  SuppressAccountSwitchingReversePair(pref_service(), account_b_, account_a_);
  // User switches from Account D to Account C, suppressing (C -> D).
  SuppressAccountSwitchingReversePair(pref_service(), account_d_, account_c_);

  ASSERT_FALSE(
      CanShowAccountSwitchingPromo(pref_service(), account_a_, account_b_));
  ASSERT_FALSE(
      CanShowAccountSwitchingPromo(pref_service(), account_c_, account_d_));

  // Remove Account A from the device; B, C, and D remain active.
  DeleteAccountSwitchingPromoStateForRemovedAccounts(
      pref_service(), {account_b_, account_c_, account_d_});

  // Reverse pair (A -> B) is cleared since primary Account A was removed.
  EXPECT_TRUE(
      CanShowAccountSwitchingPromo(pref_service(), account_a_, account_b_));
  // Unaffected reverse pair (C -> D) remains suppressed.
  EXPECT_FALSE(
      CanShowAccountSwitchingPromo(pref_service(), account_c_, account_d_));
}

// Tests that removing the target account of a reverse suppressed pair deletes
// its suppression.
TEST_F(AccountSwitchingPromoPrefsTest,
       RemovingTargetAccountDeletesReverseSwitchSuppression) {
  // User switches from Account B to Account A, suppressing (A -> B).
  SuppressAccountSwitchingReversePair(pref_service(), account_b_, account_a_);
  ASSERT_FALSE(
      CanShowAccountSwitchingPromo(pref_service(), account_a_, account_b_));

  // Remove target Account B from the device; A remains active.
  DeleteAccountSwitchingPromoStateForRemovedAccounts(pref_service(),
                                                     {account_a_});

  EXPECT_TRUE(
      CanShowAccountSwitchingPromo(pref_service(), account_a_, account_b_));
}

// Tests that calling `DeleteAccountSwitchingPromoStateForRemovedAccounts()`
// is safe when no preferences have been recorded.
TEST_F(AccountSwitchingPromoPrefsTest, DeleteOnEmptyPrefsIsNoOp) {
  // Calling on an uninitialized preference does not crash or corrupt state.
  DeleteAccountSwitchingPromoStateForRemovedAccounts(pref_service(), {});
  EXPECT_TRUE(
      CanShowAccountSwitchingPromo(pref_service(), account_a_, account_b_));
}

// Tests that calling `DeleteAccountSwitchingPromoStateForRemovedAccounts()`
// is safe when no accounts are active.
TEST_F(AccountSwitchingPromoPrefsTest, EmptyActiveAccountsDeletesAllState) {
  // Record a dismissal, then delete with an empty active list.
  RecordAccountSwitchingPromoExplicitlyDismissed(pref_service(), account_a_,
                                                 account_b_);
  RecordAccountSwitchingPromoExplicitlyDismissed(pref_service(), account_a_,
                                                 account_b_);
  ASSERT_FALSE(
      CanShowAccountSwitchingPromo(pref_service(), account_a_, account_b_));

  DeleteAccountSwitchingPromoStateForRemovedAccounts(pref_service(), {});
  EXPECT_TRUE(
      CanShowAccountSwitchingPromo(pref_service(), account_a_, account_b_));
}
}  // namespace signin
