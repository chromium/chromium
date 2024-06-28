// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/signin/model/signin_util.h"

#import "base/run_loop.h"
#import "components/prefs/pref_registry_simple.h"
#import "components/prefs/testing_pref_service.h"
#import "google_apis/gaia/core_account_id.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
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
    local_state_.registry()->RegisterDictionaryPref(
        prefs::kIosPreRestoreAccountInfo);

    chrome_browser_state_ = TestChromeBrowserState::Builder().Build();

    account_manager_service_ =
        ChromeAccountManagerServiceFactory::GetForBrowserState(
            chrome_browser_state_.get());
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

  FakeSystemIdentityManager* fake_system_identity_manager() {
    return FakeSystemIdentityManager::FromSystemIdentityManager(
        GetApplicationContext()->GetSystemIdentityManager());
  }

 protected:
  web::WebTaskEnvironment task_environment_;
  TestingPrefServiceSimple local_state_;
  std::unique_ptr<TestChromeBrowserState> chrome_browser_state_;
  raw_ptr<ChromeAccountManagerService> account_manager_service_;
};

TEST_F(SigninUtilTest, StoreAndGetPreRestoreIdentityFull) {
  ClearPreRestoreIdentity(&local_state_);
  EXPECT_FALSE(GetPreRestoreIdentity(&local_state_).has_value());

  AccountInfo account = FakeAccountFull();
  StorePreRestoreIdentity(&local_state_, account,
                          /*history_sync_enabled=*/false);

  // Verify that the retrieved account info is the same as what was stored.
  auto retrieved_account = GetPreRestoreIdentity(&local_state_);
  EXPECT_TRUE(retrieved_account.has_value());
  ExpectEqualAccountFields(account, retrieved_account.value());
}

TEST_F(SigninUtilTest, StoreAndGetPreRestoreIdentityMinimal) {
  ClearPreRestoreIdentity(&local_state_);
  EXPECT_FALSE(GetPreRestoreIdentity(&local_state_).has_value());

  AccountInfo account = FakeAccountMinimal();
  StorePreRestoreIdentity(&local_state_, account,
                          /*history_sync_enabled=*/false);

  // Verify that the retrieved account info is the same as what was stored.
  auto retrieved_account = GetPreRestoreIdentity(&local_state_);
  EXPECT_TRUE(retrieved_account.has_value());
  ExpectEqualAccountFields(account, retrieved_account.value());
}

TEST_F(SigninUtilTest, ClearPreRestoreIdentity) {
  StorePreRestoreIdentity(&local_state_, FakeAccountFull(),
                          /*history_sync_enabled=*/true);
  EXPECT_TRUE(GetPreRestoreIdentity(&local_state_).has_value());
  EXPECT_TRUE(GetPreRestoreHistorySyncEnabled(&local_state_));

  ClearPreRestoreIdentity(&local_state_);
  EXPECT_FALSE(GetPreRestoreIdentity(&local_state_).has_value());
  EXPECT_FALSE(GetPreRestoreHistorySyncEnabled(&local_state_));
}

TEST_F(SigninUtilTest, RunSystemCapabilitiesPrefetch) {
  FakeSystemIdentity* identity =
      [FakeSystemIdentity identityWithEmail:@"foo1@gmail.com"
                                     gaiaID:@"foo1ID"
                                       name:@"Fake Foo 1"];
  fake_system_identity_manager()->AddIdentity(identity);

  AccountCapabilitiesTestMutator* mutator =
      fake_system_identity_manager()->GetCapabilitiesMutator(identity);
  mutator->SetAllSupportedCapabilities(true);

  base::RunLoop run_loop;
  auto quit_closure = run_loop.QuitClosure();
  auto callback = base::BindOnce(
      ^(std::map<std::string, SystemIdentityCapabilityResult> result) {
        ASSERT_EQ(result.size(), GetAccountCapabilityNamesForPrefetch().size());
        for (const auto& pair : result) {
          EXPECT_EQ(pair.second, SystemIdentityCapabilityResult::kTrue);
        }
        std::move(quit_closure).Run();
      });

  RunSystemCapabilitiesPrefetch(account_manager_service_->GetAllIdentities(),
                                std::move(callback));
  run_loop.Run();
}
