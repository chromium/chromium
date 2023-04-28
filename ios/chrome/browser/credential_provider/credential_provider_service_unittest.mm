// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/credential_provider/credential_provider_service.h"

#import <memory>
#import <utility>

#import "base/memory/scoped_refptr.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/task_environment.h"
#import "components/favicon/core/large_icon_service.h"
#import "components/password_manager/core/browser/affiliation/fake_affiliation_service.h"
#import "components/password_manager/core/browser/password_form.h"
#import "components/password_manager/core/browser/test_password_store.h"
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
    NSUserDefaults* user_defaults = [NSUserDefaults standardUserDefaults];
    [user_defaults removeObjectForKey:
                       kUserDefaultsCredentialProviderFirstTimeSyncCompleted];
    password_store_->Init(&testing_pref_service_,
                          /*affiliated_match_helper=*/nullptr);
    testing_pref_service_.registry()->RegisterBooleanPref(
        password_manager::prefs::kCredentialsEnableService, true);
    credential_provider_service_ = std::make_unique<CredentialProviderService>(
        &testing_pref_service_, password_store_.get(), credential_store_,
        identity_test_environment_.identity_manager(), &sync_service_,
        &affiliation_service_, &favicon_loader_);

    // Fire sync service state changed to simulate sync setup finishing.
    sync_service_.FireStateChanged();
  }

  void TearDown() override {
    credential_provider_service_->Shutdown();
    password_store_->ShutdownOnUIThread();
    NSUserDefaults* user_defaults = [NSUserDefaults standardUserDefaults];
    [user_defaults removeObjectForKey:
                       kUserDefaultsCredentialProviderFirstTimeSyncCompleted];
    PlatformTest::TearDown();
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  TestingPrefServiceSimple testing_pref_service_;
  scoped_refptr<password_manager::TestPasswordStore> password_store_ =
      base::MakeRefCounted<password_manager::TestPasswordStore>();
  MemoryCredentialStore* credential_store_ =
      [[MemoryCredentialStore alloc] init];
  signin::IdentityTestEnvironment identity_test_environment_;
  syncer::TestSyncService sync_service_;
  password_manager::FakeAffiliationService affiliation_service_;
  MockLargeIconService large_icon_service_;
  FaviconLoader favicon_loader_ = FaviconLoader(&large_icon_service_);
  std::unique_ptr<CredentialProviderService> credential_provider_service_;
};

// Test that CredentialProviderService syncs all the credentials the first time
// it runs.
TEST_F(CredentialProviderServiceTest, FirstSync) {
  base::RunLoop().RunUntilIdle();

  NSUserDefaults* user_defaults = [NSUserDefaults standardUserDefaults];
  EXPECT_TRUE([user_defaults
      boolForKey:kUserDefaultsCredentialProviderFirstTimeSyncCompleted]);
}

// Test that CredentialProviderService observes changes in the password store.
TEST_F(CredentialProviderServiceTest, PasswordChanges) {
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
  CoreAccountInfo account = identity_test_environment_.SetPrimaryAccount(
      "foo@gmail.com", signin::ConsentLevel::kSync);
  sync_service_.FireStateChanged();

  EXPECT_NSEQ(
      base::SysUTF8ToNSString(account.email),
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

}  // namespace
