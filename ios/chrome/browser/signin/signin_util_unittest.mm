// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/signin/signin_util.h"

#import "components/prefs/pref_registry_simple.h"
#import "components/prefs/testing_pref_service.h"
#import "google_apis/gaia/core_account_id.h"
#import "ios/chrome/browser/prefs/pref_names.h"
#import "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

class SigninUtilTest : public PlatformTest {
 public:
  explicit SigninUtilTest() {
    local_state_.registry()->RegisterDictionaryPref(
        prefs::kIosPreRestoreAccountInfo);
  }

  AccountInfo FakeAccountFull() {
    AccountInfo account;
    account.account_id = CoreAccountId::FromString("account_id");
    account.gaia = "gaia";
    account.email = "person@example.org";
    account.full_name = "Full Name";
    account.given_name = "Given Name";
    account.picture_url = "https://example.org/path";
    return account;
  }

  AccountInfo FakeAccountMinimal() {
    AccountInfo account;
    account.gaia = "gaia";
    account.email = "person@example.org";
    return account;
  }

  void ExpectEqualAccountFields(const AccountInfo& a, const AccountInfo& b) {
    EXPECT_EQ(a.account_id, b.account_id);
    EXPECT_EQ(a.gaia, b.gaia);
    EXPECT_EQ(a.email, b.email);
    EXPECT_EQ(a.full_name, b.full_name);
    EXPECT_EQ(a.given_name, b.given_name);
    EXPECT_EQ(a.picture_url, b.picture_url);
  }

  TestingPrefServiceSimple local_state_;
};

TEST_F(SigninUtilTest, StoreAndGetPreRestoreIdentityFull) {
  ClearPreRestoreIdentity(&local_state_);
  EXPECT_FALSE(GetPreRestoreIdentity(&local_state_).has_value());

  AccountInfo account = FakeAccountFull();
  StorePreRestoreIdentity(&local_state_, account);

  // Verify that the retrieved account info is the same as what was stored.
  auto retrieved_account = GetPreRestoreIdentity(&local_state_);
  EXPECT_TRUE(retrieved_account.has_value());
  ExpectEqualAccountFields(account, retrieved_account.value());
}

TEST_F(SigninUtilTest, StoreAndGetPreRestoreIdentityMinimal) {
  ClearPreRestoreIdentity(&local_state_);
  EXPECT_FALSE(GetPreRestoreIdentity(&local_state_).has_value());

  AccountInfo account = FakeAccountMinimal();
  StorePreRestoreIdentity(&local_state_, account);

  // Verify that the retrieved account info is the same as what was stored.
  auto retrieved_account = GetPreRestoreIdentity(&local_state_);
  EXPECT_TRUE(retrieved_account.has_value());
  ExpectEqualAccountFields(account, retrieved_account.value());
}

TEST_F(SigninUtilTest, ClearPreRestoreIdentity) {
  StorePreRestoreIdentity(&local_state_, FakeAccountFull());
  EXPECT_TRUE(GetPreRestoreIdentity(&local_state_).has_value());

  ClearPreRestoreIdentity(&local_state_);
  EXPECT_FALSE(GetPreRestoreIdentity(&local_state_).has_value());
}
