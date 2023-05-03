// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/credential_provider/credential_provider_service.h"

#import <memory>
#import <string>
#import <utility>
#import <vector>

#import "base/memory/scoped_refptr.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/scoped_feature_list.h"
#import "base/test/task_environment.h"
#import "components/favicon/core/large_icon_service.h"
#import "components/password_manager/core/browser/affiliation/fake_affiliation_service.h"
#import "components/password_manager/core/browser/password_form.h"
#import "components/password_manager/core/browser/test_password_store.h"
#import "components/password_manager/core/common/password_manager_features.h"
#import "components/password_manager/core/common/password_manager_pref_names.h"
#import "components/prefs/pref_registry_simple.h"
#import "components/prefs/pref_service.h"
#import "components/prefs/testing_pref_service.h"
#import "components/signin/public/identity_manager/identity_test_environment.h"
#import "components/sync/base/user_selectable_type.h"
#import "components/sync/test/test_sync_service.h"
#import "ios/chrome/browser/favicon/favicon_loader.h"
#import "ios/chrome/common/app_group/app_group_constants.h"
#import "ios/chrome/common/credential_provider/constants.h"
#import "ios/chrome/common/credential_provider/credential.h"
#import "ios/chrome/common/credential_provider/memory_credential_store.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

using testing::UnorderedElementsAre;

// Extracts the service names of `credentials` to an std::vector, so tests can
// use a gmock matcher on it.
std::vector<std::string> GetServiceNames(NSArray<id<Credential>>* credentials) {
  std::vector<std::string> service_names;
  for (id<Credential> credential : credentials) {
    service_names.push_back(base::SysNSStringToUTF8(credential.serviceName));
  }
  return service_names;
}

// Needed since FaviconLoader has no fake currently.
class MockLargeIconService : public favicon::LargeIconService {
 public:
  MOCK_METHOD(base::CancelableTaskTracker::TaskId,
              GetLargeIconRawBitmapOrFallbackStyleForPageUrl,
              (const GURL&,
               int,
               int,
               favicon_base::LargeIconCallback,
               base::CancelableTaskTracker*),
              (override));
  MOCK_METHOD(base::CancelableTaskTracker::TaskId,
              GetLargeIconImageOrFallbackStyleForPageUrl,
              (const GURL&,
               int,
               int,
               favicon_base::LargeIconImageCallback,
               base::CancelableTaskTracker*),
              (override));
  MOCK_METHOD(base::CancelableTaskTracker::TaskId,
              GetLargeIconRawBitmapOrFallbackStyleForIconUrl,
              (const GURL&,
               int,
               int,
               favicon_base::LargeIconCallback,
               base::CancelableTaskTracker*),
              (override));
  MOCK_METHOD(base::CancelableTaskTracker::TaskId,
              GetIconRawBitmapOrFallbackStyleForPageUrl,
              (const GURL&,
               int,
               favicon_base::LargeIconCallback,
               base::CancelableTaskTracker*),
              (override));
  MOCK_METHOD(void,
              GetLargeIconOrFallbackStyleFromGoogleServerSkippingLocalCache,
              (const GURL&,
               bool,
               bool,
               const net::NetworkTrafficAnnotationTag&,
               favicon_base::GoogleFaviconServerCallback),
              (override));
  MOCK_METHOD(void, TouchIconFromGoogleServer, (const GURL&), (override));
};

class CredentialProviderServiceTest : public PlatformTest {
 public:
  CredentialProviderServiceTest() = default;
  ~CredentialProviderServiceTest() override = default;

  CredentialProviderServiceTest(const CredentialProviderServiceTest&) = delete;
  CredentialProviderServiceTest& operator=(
      const CredentialProviderServiceTest&) = delete;

  void SetUp() override {
    PlatformTest::SetUp();
    password_store_->Init(&testing_pref_service_,
                          /*affiliated_match_helper=*/nullptr);
    account_password_store_->Init(&testing_pref_service_,
                                  /*affiliated_match_helper=*/nullptr);
    testing_pref_service_.registry()->RegisterBooleanPref(
        password_manager::prefs::kCredentialsEnableService, true);
  }

  void TearDown() override {
    credential_provider_service_->Shutdown();
    password_store_->ShutdownOnUIThread();
    account_password_store_->ShutdownOnUIThread();
    PlatformTest::TearDown();
  }

  void CreateCredentialProviderService(bool with_account_store = false) {
    credential_provider_service_ = std::make_unique<CredentialProviderService>(
        &testing_pref_service_, password_store_,
        with_account_store ? account_password_store_ : nullptr,
        credential_store_, identity_test_environment_.identity_manager(),
        &sync_service_, &affiliation_service_, &favicon_loader_);
  }

 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  TestingPrefServiceSimple testing_pref_service_;
  scoped_refptr<password_manager::TestPasswordStore> password_store_ =
      base::MakeRefCounted<password_manager::TestPasswordStore>();
  scoped_refptr<password_manager::TestPasswordStore> account_password_store_ =
      base::MakeRefCounted<password_manager::TestPasswordStore>(
          password_manager::IsAccountStore(true));
  MemoryCredentialStore* credential_store_ =
      [[MemoryCredentialStore alloc] init];
  signin::IdentityTestEnvironment identity_test_environment_;
  syncer::TestSyncService sync_service_;
  password_manager::FakeAffiliationService affiliation_service_;
  MockLargeIconService large_icon_service_;
  FaviconLoader favicon_loader_ = FaviconLoader(&large_icon_service_);
  std::unique_ptr<CredentialProviderService> credential_provider_service_;
};

// Test that CredentialProviderService writes all the credentials the first time
// it runs.
TEST_F(CredentialProviderServiceTest, FirstSync) {
  password_manager::PasswordForm form;
  form.url = GURL("http://g.com");
  form.username_value = u"user";
  form.encrypted_password = "encrypted-pwd";
  password_store_->AddLogin(form);
  base::RunLoop().RunUntilIdle();

  CreateCredentialProviderService();
  // The first write is delayed.
  task_environment_.FastForwardBy(base::Seconds(30));
  base::RunLoop().RunUntilIdle();

  ASSERT_EQ(credential_store_.credentials.count, 1u);
  EXPECT_NSEQ(credential_store_.credentials[0].serviceName, @"g.com");
  EXPECT_NSEQ(credential_store_.credentials[0].user, @"user");
  EXPECT_NSEQ(credential_store_.credentials[0].keychainIdentifier,
              @"encrypted-pwd");
}

TEST_F(CredentialProviderServiceTest, TwoStores) {
  password_manager::PasswordForm local_form;
  local_form.url = GURL("http://local.com");
  local_form.username_value = u"user";
  local_form.encrypted_password = "encrypted-pwd";
  password_store_->AddLogin(local_form);
  password_manager::PasswordForm account_form = local_form;
  account_form.url = GURL("http://account.com");
  account_password_store_->AddLogin(account_form);
  CreateCredentialProviderService(/*with_account_store=*/true);
  base::RunLoop().RunUntilIdle();

  ASSERT_EQ(credential_store_.credentials.count, 2u);
  EXPECT_THAT(GetServiceNames(credential_store_.credentials),
              UnorderedElementsAre("local.com", "account.com"));

  password_manager::PasswordForm local_and_account_form = local_form;
  local_and_account_form.url = GURL("http://local-and-account.com");
  password_store_->AddLogin(local_and_account_form);
  account_password_store_->AddLogin(local_and_account_form);
  base::RunLoop().RunUntilIdle();

  ASSERT_EQ(credential_store_.credentials.count, 3u);
  EXPECT_THAT(GetServiceNames(credential_store_.credentials),
              UnorderedElementsAre("local.com", "account.com",
                                   "local-and-account.com"));

  password_store_->RemoveLogin(local_and_account_form);
  base::RunLoop().RunUntilIdle();

  ASSERT_EQ(credential_store_.credentials.count, 3u);
  EXPECT_THAT(GetServiceNames(credential_store_.credentials),
              UnorderedElementsAre("local.com", "account.com",
                                   "local-and-account.com"));

  account_password_store_->RemoveLogin(local_and_account_form);
  base::RunLoop().RunUntilIdle();

  ASSERT_EQ(credential_store_.credentials.count, 2u);
  EXPECT_THAT(GetServiceNames(credential_store_.credentials),
              UnorderedElementsAre("local.com", "account.com"));
}

// Test that CredentialProviderService observes changes in the password store.
TEST_F(CredentialProviderServiceTest, PasswordChanges) {
  CreateCredentialProviderService();

  EXPECT_EQ(0u, credential_store_.credentials.count);

  password_manager::PasswordForm form;
  form.url = GURL("http://0.com");
  form.signon_realm = "http://www.example.com/";
  form.action = GURL("http://www.example.com/action");
  form.password_element = u"pwd";
  form.encrypted_password = "example";
  password_store_->AddLogin(form);
  task_environment_.RunUntilIdle();

  // Expect the store to be populated with 1 credential.
  ASSERT_EQ(1u, credential_store_.credentials.count);
  NSString* keychainIdentifier =
      credential_store_.credentials[0].keychainIdentifier;

  form.encrypted_password = "secret";
  password_store_->UpdateLogin(form);
  task_environment_.RunUntilIdle();

  // Expect that the credential in the store now has a different keychain
  // identifier.
  ASSERT_EQ(1u, credential_store_.credentials.count);
  EXPECT_NSNE(keychainIdentifier,
              credential_store_.credentials[0].keychainIdentifier);

  password_store_->RemoveLogin(form);
  task_environment_.RunUntilIdle();

  // Expect the store to be empty.
  EXPECT_EQ(0u, credential_store_.credentials.count);
}

// Test that CredentialProviderService observes changes in the primary identity.
TEST_F(CredentialProviderServiceTest, AccountChange) {
  CreateCredentialProviderService();

  password_manager::PasswordForm form;
  form.url = GURL("http://0.com");
  form.signon_realm = "http://www.example.com/";
  form.action = GURL("http://www.example.com/action");
  form.password_element = u"pwd";
  form.password_value = u"example";
  password_store_->AddLogin(form);
  task_environment_.RunUntilIdle();

  ASSERT_EQ(credential_store_.credentials.count, 1u);
  EXPECT_FALSE(credential_store_.credentials[0].validationIdentifier);

  // Enable sync for managed account.
  CoreAccountInfo core_account =
      identity_test_environment_.MakeAccountAvailable("foo@gmail.com");
  AccountInfo account;
  account.account_id = core_account.account_id;
  account.gaia = core_account.gaia;
  account.email = core_account.email;
  account.hosted_domain = "managed.com";
  ASSERT_TRUE(account.IsManaged());
  identity_test_environment_.UpdateAccountInfoForAccount(account);
  identity_test_environment_.SetPrimaryAccount("foo@gmail.com",
                                               signin::ConsentLevel::kSync);
  base::RunLoop().RunUntilIdle();

  ASSERT_EQ(credential_store_.credentials.count, 1u);
  EXPECT_NSEQ(credential_store_.credentials[0].validationIdentifier,
              base::SysUTF8ToNSString(core_account.gaia));

  identity_test_environment_.ClearPrimaryAccount();
  base::RunLoop().RunUntilIdle();

  ASSERT_EQ(credential_store_.credentials.count, 1u);
  EXPECT_FALSE(credential_store_.credentials[0].validationIdentifier);
}

// Test that CredentialProviderService observes changes in the password store.
TEST_F(CredentialProviderServiceTest, AndroidCredential) {
  CreateCredentialProviderService();

  EXPECT_EQ(0u, credential_store_.credentials.count);

  password_manager::PasswordForm form;
  form.url = GURL(form.signon_realm);
  form.signon_realm = "android://hash@com.example.my.app";
  form.password_element = u"pwd";
  form.password_value = u"example";
  password_store_->AddLogin(form);
  task_environment_.RunUntilIdle();

  // Expect the store to be populated with 1 credential.
  EXPECT_EQ(1u, credential_store_.credentials.count);
}

// Test that the CredentialProviderService observes changes in the preference
// that controls password creation
TEST_F(CredentialProviderServiceTest, PasswordCreationPreference) {
  CreateCredentialProviderService();

  // The test is initialized with the preference as true. Make sure the
  // NSUserDefaults value is also true.
  EXPECT_TRUE([[app_group::GetGroupUserDefaults()
      objectForKey:
          AppGroupUserDefaulsCredentialProviderSavingPasswordsEnabled()]
      boolValue]);

  // Change the pref value to false.
  testing_pref_service_.SetBoolean(
      password_manager::prefs::kCredentialsEnableService, false);

  // Make sure the NSUserDefaults value is now false.
  EXPECT_FALSE([[app_group::GetGroupUserDefaults()
      objectForKey:
          AppGroupUserDefaulsCredentialProviderSavingPasswordsEnabled()]
      boolValue]);
}

// Tests that the CredentialProviderService has the correct stored email based
// on the password sync state.
TEST_F(CredentialProviderServiceTest, PasswordSyncStoredEmail) {
  // Start by signing in and turning sync on.
  CoreAccountInfo account;
  account.email = "foo@gmail.com";
  account.gaia = "gaia";
  account.account_id = CoreAccountId::FromGaiaId("gaia");
  sync_service_.SetAccountInfo(account);
  sync_service_.SetHasSyncConsent(true);

  CreateCredentialProviderService();

  EXPECT_NSEQ(
      @"foo@gmail.com",
      [app_group::GetGroupUserDefaults()
          stringForKey:AppGroupUserDefaultsCredentialProviderUserEmail()]);

  // Turn off password sync.
  syncer::UserSelectableTypeSet user_selectable_type_set =
      sync_service_.GetUserSettings()->GetSelectedTypes();
  user_selectable_type_set.Remove(syncer::UserSelectableType::kPasswords);
  sync_service_.GetUserSettings()->SetSelectedTypes(
      /*sync_everything=*/false,
      /*types=*/user_selectable_type_set);
  sync_service_.FireStateChanged();

  EXPECT_FALSE([app_group::GetGroupUserDefaults()
      stringForKey:AppGroupUserDefaultsCredentialProviderUserEmail()]);
}

// Tests that the CredentialProviderService has the correct stored email based
// on the account storage state.
TEST_F(CredentialProviderServiceTest, SignedInUserStoredEmail) {
  // Set up a signed in user with the flag enabled.
  base::test::ScopedFeatureList features(
      password_manager::features::kEnablePasswordsAccountStorage);
  CoreAccountInfo account;
  account.email = "foo@gmail.com";
  account.gaia = "gaia";
  account.account_id = CoreAccountId::FromGaiaId("gaia");
  sync_service_.SetAccountInfo(account);
  sync_service_.SetHasSyncConsent(false);

  CreateCredentialProviderService();

  EXPECT_NSEQ(
      [app_group::GetGroupUserDefaults()
          stringForKey:AppGroupUserDefaultsCredentialProviderUserEmail()],
      @"foo@gmail.com");

  // Disable account storage.
  syncer::UserSelectableTypeSet user_selectable_type_set =
      sync_service_.GetUserSettings()->GetSelectedTypes();
  user_selectable_type_set.Remove(syncer::UserSelectableType::kPasswords);
  sync_service_.GetUserSettings()->SetSelectedTypes(
      /*sync_everything=*/false,
      /*types=*/user_selectable_type_set);
  sync_service_.FireStateChanged();

  EXPECT_FALSE([app_group::GetGroupUserDefaults()
      stringForKey:AppGroupUserDefaultsCredentialProviderUserEmail()]);
}

// Similar to SignedInUserStoredEmail but disable the account storage flag.
TEST_F(CredentialProviderServiceTest,
       SignedInUserStoredEmailWithFeatureDisabled) {
  // Set up a signed in user with the flag disabled.
  base::test::ScopedFeatureList features;
  features.InitAndDisableFeature(
      password_manager::features::kEnablePasswordsAccountStorage);
  CoreAccountInfo account;
  account.email = "foo@gmail.com";
  account.gaia = "gaia";
  account.account_id = CoreAccountId::FromGaiaId("gaia");
  sync_service_.SetAccountInfo(account);
  sync_service_.SetHasSyncConsent(false);
  sync_service_.FireStateChanged();

  CreateCredentialProviderService();

  EXPECT_FALSE([app_group::GetGroupUserDefaults()
      stringForKey:AppGroupUserDefaultsCredentialProviderUserEmail()]);
}

}  // namespace
