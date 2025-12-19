// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/signin/model/signin_util.h"

#import "base/memory/raw_ptr.h"
#import "base/run_loop.h"
#import "components/prefs/pref_registry_simple.h"
#import "components/prefs/testing_pref_service.h"
#import "google_apis/gaia/core_account_id.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/signin/model/account_capabilities_fetcher_ios.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service_factory.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/browser/signin/model/fake_system_identity_manager.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"

class SigninUtilTest : public PlatformTest {
 public:
  explicit SigninUtilTest() {
    profile_ = TestProfileIOS::Builder().Build();
    pref_service_ = profile_.get()->GetPrefs();

    account_manager_service_ =
        ChromeAccountManagerServiceFactory::GetForProfile(profile_.get());
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

  FakeSystemIdentityManager* fake_system_identity_manager() {
    return FakeSystemIdentityManager::FromSystemIdentityManager(
        GetApplicationContext()->GetSystemIdentityManager());
  }

 protected:
  web::WebTaskEnvironment task_environment_;
  raw_ptr<PrefService, DanglingUntriaged> pref_service_;
  std::unique_ptr<TestProfileIOS> profile_;
  raw_ptr<ChromeAccountManagerService> account_manager_service_;
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

TEST_F(SigninUtilTest, RunSystemCapabilitiesPrefetch) {
  FakeSystemIdentity* identity = [FakeSystemIdentity fakeIdentity1];
  fake_system_identity_manager()->AddIdentity(identity);

  AccountCapabilitiesTestMutator* mutator =
      fake_system_identity_manager()->GetPendingCapabilitiesMutator(identity);
  mutator->SetAllSupportedCapabilities(true);
  ASSERT_FALSE(fake_system_identity_manager()
                   ->GetVisibleCapabilities(identity)
                   .AreAllCapabilitiesKnown());

  RunSystemCapabilitiesPrefetch(account_manager_service_->GetAllIdentities());
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(fake_system_identity_manager()
                  ->GetVisibleCapabilities(identity)
                  .AreAllCapabilitiesKnown());
}

TEST_F(SigninUtilTest, RunSystemCapabilitiesPrefetchMultipleIdentities) {
  FakeSystemIdentity* identity1 = [FakeSystemIdentity fakeIdentity1];
  fake_system_identity_manager()->AddIdentity(identity1);
  FakeSystemIdentity* identity2 = [FakeSystemIdentity fakeIdentity2];
  fake_system_identity_manager()->AddIdentity(identity2);

  AccountCapabilitiesTestMutator* mutator1 =
      fake_system_identity_manager()->GetPendingCapabilitiesMutator(identity1);
  mutator1->SetAllSupportedCapabilities(true);
  ASSERT_FALSE(fake_system_identity_manager()
                   ->GetVisibleCapabilities(identity1)
                   .AreAllCapabilitiesKnown());

  AccountCapabilitiesTestMutator* mutator2 =
      fake_system_identity_manager()->GetPendingCapabilitiesMutator(identity2);
  mutator2->SetAllSupportedCapabilities(true);
  ASSERT_FALSE(fake_system_identity_manager()
                   ->GetVisibleCapabilities(identity2)
                   .AreAllCapabilitiesKnown());

  RunSystemCapabilitiesPrefetch(account_manager_service_->GetAllIdentities());
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(fake_system_identity_manager()
                  ->GetVisibleCapabilities(identity1)
                  .AreAllCapabilitiesKnown());
  EXPECT_TRUE(fake_system_identity_manager()
                  ->GetVisibleCapabilities(identity2)
                  .AreAllCapabilitiesKnown());
}
