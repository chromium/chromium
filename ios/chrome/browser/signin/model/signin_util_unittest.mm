// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/signin/model/signin_util.h"

#import "base/memory/raw_ptr.h"
#import "base/test/scoped_feature_list.h"
#import "components/prefs/pref_registry_simple.h"
#import "components/prefs/testing_pref_service.h"
#import "google_apis/gaia/core_account_id.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_manager_ios.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"

class SigninUtilTest : public PlatformTest {
 public:
  explicit SigninUtilTest() {
    profile_ =
        profile_manager_.AddProfileWithBuilder(TestProfileIOS::Builder());
    pref_service_ = profile_->GetPrefs();
  }

  void TearDown() override {
    pref_service_ = nullptr;
    profile_ = nullptr;
    PlatformTest::TearDown();
  }

  AccountInfo FakeAccountFull() {
    return AccountInfo::Builder(GaiaId("gaia"), "person@example.org")
        .SetAccountId(CoreAccountId::FromString("account_id"))
        .SetFullName("Full Name")
        .SetGivenName("Given Name")
        .SetAvatarUrl("https://example.org/path")
        .Build();
  }

  AccountInfo FakeAccountMinimal() {
    return AccountInfo::Builder(GaiaId("gaia"), "person@example.org").Build();
  }

  void ExpectEqualAccountFields(const AccountInfo& a, const AccountInfo& b) {
    EXPECT_EQ(a.GetAccountId(), b.GetAccountId());
    EXPECT_EQ(a.GetGaiaId(), b.GetGaiaId());
    EXPECT_EQ(a.GetEmail(), b.GetEmail());
    EXPECT_EQ(a.GetFullName(), b.GetFullName());
    EXPECT_EQ(a.GetGivenName(), b.GetGivenName());
    EXPECT_EQ(a.GetAvatarUrl(), b.GetAvatarUrl());
  }

 protected:
  web::WebTaskEnvironment task_environment_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  TestProfileManagerIOS profile_manager_;
  raw_ptr<TestProfileIOS> profile_ = nullptr;
  raw_ptr<PrefService> pref_service_ = nullptr;
};

TEST_F(SigninUtilTest, StoreAndGetPreRestoreIdentityFull) {
  ClearPreRestoreIdentity(pref_service_);
  EXPECT_FALSE(GetPreRestoreIdentity(pref_service_).has_value());

  AccountInfo account = FakeAccountFull();
  StorePreRestoreIdentity(pref_service_, account,
                          /*history_sync_enabled=*/false);

  // Verify that the retrieved account info is the same as what was stored.
  auto retrieved_account = GetPreRestoreIdentity(pref_service_);
  EXPECT_TRUE(retrieved_account.has_value());
  ExpectEqualAccountFields(account, retrieved_account.value());
}

TEST_F(SigninUtilTest, StoreAndGetPreRestoreIdentityMinimal) {
  ClearPreRestoreIdentity(pref_service_);
  EXPECT_FALSE(GetPreRestoreIdentity(pref_service_).has_value());

  AccountInfo account = FakeAccountMinimal();
  StorePreRestoreIdentity(pref_service_, account,
                          /*history_sync_enabled=*/false);

  // Verify that the retrieved account info is the same as what was stored.
  auto retrieved_account = GetPreRestoreIdentity(pref_service_);
  EXPECT_TRUE(retrieved_account.has_value());
  ExpectEqualAccountFields(account, retrieved_account.value());
}

TEST_F(SigninUtilTest, ClearPreRestoreIdentity) {
  StorePreRestoreIdentity(pref_service_, FakeAccountFull(),
                          /*history_sync_enabled=*/true);
  EXPECT_TRUE(GetPreRestoreIdentity(pref_service_).has_value());
  EXPECT_TRUE(GetPreRestoreHistorySyncEnabled(pref_service_));

  ClearPreRestoreIdentity(pref_service_);
  EXPECT_FALSE(GetPreRestoreIdentity(pref_service_).has_value());
  EXPECT_FALSE(GetPreRestoreHistorySyncEnabled(pref_service_));
}

TEST_F(SigninUtilTest, GetSizeForIdentityAvatarSize) {
  // The avatar should be its default size.
  {
    base::test::ScopedFeatureList scoped_feature_list;
    scoped_feature_list.InitAndDisableFeature(kAiSubscriptionAvatarRingIOS);
    EXPECT_EQ(GetSizeForIdentityAvatarSize(IdentityAvatarSize::Large,
                                           AITierRingSize::kNoRing)
                  .width,
              48.0);
  }
  // The avatar should be its default size as the ring is around it.
  {
    base::test::ScopedFeatureList scoped_feature_list;
    scoped_feature_list.InitAndEnableFeature(kAiSubscriptionAvatarRingIOS);
    EXPECT_EQ(GetSizeForIdentityAvatarSize(IdentityAvatarSize::Large,
                                           AITierRingSize::kImageSize)
                  .width,
              48.0);
  }
  // The avatar should be smaller so that the ring takes the usual avatar size.
  {
    base::test::ScopedFeatureList scoped_feature_list;
    scoped_feature_list.InitAndEnableFeature(kAiSubscriptionAvatarRingIOS);
    EXPECT_EQ(GetSizeForIdentityAvatarSize(IdentityAvatarSize::Large,
                                           AITierRingSize::kViewSize)
                  .width,
              38.0);
  }
}
