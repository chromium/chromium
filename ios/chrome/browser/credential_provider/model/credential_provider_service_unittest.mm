// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/credential_provider/model/credential_provider_service.h"

#import <memory>
#import <string>
#import <utility>
#import <vector>

#import "base/location.h"
#import "base/memory/scoped_refptr.h"
#import "base/rand_util.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/metrics/histogram_tester.h"
#import "base/test/scoped_feature_list.h"
#import "base/test/task_environment.h"
#import "components/affiliations/core/browser/fake_affiliation_service.h"
#import "components/favicon/core/large_icon_service.h"
#import "components/password_manager/core/browser/password_form.h"
#import "components/password_manager/core/browser/password_store/password_store_change.h"
#import "components/password_manager/core/browser/password_store/test_password_store.h"
#import "components/password_manager/core/common/password_manager_features.h"
#import "components/password_manager/core/common/password_manager_pref_names.h"
#import "components/prefs/pref_registry_simple.h"
#import "components/prefs/pref_service.h"
#import "components/prefs/testing_pref_service.h"
#import "components/signin/public/identity_manager/identity_test_environment.h"
#import "components/sync/base/user_selectable_type.h"
#import "components/sync/test/test_sync_service.h"
#import "components/webauthn/core/browser/test_passkey_model.h"
#import "ios/chrome/browser/credential_provider/model/credential_provider_util.h"
#import "ios/chrome/browser/credential_provider/model/features.h"
#import "ios/chrome/browser/favicon/model/favicon_loader.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/common/app_group/app_group_constants.h"
#import "ios/chrome/common/credential_provider/constants.h"
#import "ios/chrome/common/credential_provider/credential.h"
#import "ios/chrome/common/credential_provider/memory_credential_store.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

using testing::_;

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

sync_pb::WebauthnCredentialSpecifics CreatePasskey(
    const std::string& rp_id,
    const std::string& user_id,
    const std::string& user_name,
    const std::string& user_display_name) {
  sync_pb::WebauthnCredentialSpecifics passkey;
  passkey.set_sync_id(base::RandBytesAsString(16));
  passkey.set_credential_id(base::RandBytesAsString(16));
  passkey.set_rp_id(rp_id);
  passkey.set_user_id(user_id);
  passkey.set_user_name(user_name);
  passkey.set_user_display_name(user_display_name);
  return passkey;
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
              GetLargeIconRawBitmapForPageUrl,
              (const GURL&,
               int,
               std::optional<int>,
               LargeIconService::NoBigEnoughIconBehavior,
               favicon_base::LargeIconCallback,
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
               const net::NetworkTrafficAnnotationTag&,
               favicon_base::GoogleFaviconServerCallback),
              (override));
  MOCK_METHOD(void,
              GetLargeIconFromCacheFallbackToGoogleServer,
              (const GURL& page_url,
               StandardIconSize min_source_size_in_pixel,
               std::optional<StandardIconSize> size_in_pixel_to_resize_to,
               NoBigEnoughIconBehavior no_big_enough_icon_behavior,
               bool should_trim_page_url_path,
               const net::NetworkTrafficAnnotationTag& traffic_annotation,
               favicon_base::LargeIconCallback callback,
               base::CancelableTaskTracker* tracker),
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
    // Make sure there are no favicons left from some other tests.
    EXPECT_TRUE(DeleteFaviconsFolder());
    password_store_->Init(&testing_pref_service_,
                          /*affiliated_match_helper=*/nullptr);
    account_password_store_->Init(&testing_pref_service_,
                                  /*affiliated_match_helper=*/nullptr);
    testing_pref_service_.registry()->RegisterBooleanPref(
        password_manager::prefs::kCredentialsEnableService, true);
  }

  void TearDown() override {
    // Delete all favicon files that were created during the test.
    EXPECT_TRUE(DeleteFaviconsFolder());
    credential_provider_service_->Shutdown();
    password_store_->ShutdownOnUIThread();
    account_password_store_->ShutdownOnUIThread();
    PlatformTest::TearDown();
  }

  void CreateCredentialProviderService(bool with_account_store = false) {
    credential_provider_service_ = std::make_unique<CredentialProviderService>(
        &testing_pref_service_, password_store_,
        with_account_store ? account_password_store_ : nullptr,
        test_passkey_model_.get(), credential_store_,
        identity_test_environment_.identity_manager(), &sync_service_,
        &affiliation_service_, &favicon_loader_);
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
  std::unique_ptr<webauthn::TestPasskeyModel> test_passkey_model_ =
      std::make_unique<webauthn::TestPasskeyModel>();
  MemoryCredentialStore* credential_store_ =
      [[MemoryCredentialStore alloc] init];
  signin::IdentityTestEnvironment identity_test_environment_;
  syncer::TestSyncService sync_service_;
  affiliations::FakeAffiliationService affiliation_service_;
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
  form.password_value = u"qwerty123";
  password_store_->AddLogin(form);
  base::RunLoop().RunUntilIdle();

  CreateCredentialProviderService();
  // The first write is delayed.
  task_environment_.FastForwardBy(base::Seconds(30));
  base::RunLoop().RunUntilIdle();

  ASSERT_EQ(credential_store_.credentials.count, 1u);
  EXPECT_NSEQ(credential_store_.credentials[0].serviceName, @"g.com");
  EXPECT_NSEQ(credential_store_.credentials[0].username, @"user");
  EXPECT_NSEQ(credential_store_.credentials[0].password, @"qwerty123");
}

TEST_F(CredentialProviderServiceTest, TwoStores) {
  password_manager::PasswordForm local_form;
  local_form.url = GURL("http://local.com");
  local_form.username_value = u"user";
  local_form.keychain_identifier = "encrypted-pwd";
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

  password_store_->RemoveLogin(FROM_HERE, local_and_account_form);
  base::RunLoop().RunUntilIdle();

  ASSERT_EQ(credential_store_.credentials.count, 3u);
  EXPECT_THAT(GetServiceNames(credential_store_.credentials),
              UnorderedElementsAre("local.com", "account.com",
                                   "local-and-account.com"));

  account_password_store_->RemoveLogin(FROM_HERE, local_and_account_form);
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
  form.password_value = u"qwerty123";
  password_store_->AddLogin(form);
  task_environment_.RunUntilIdle();

  // Expect the store to be populated with 1 credential.
  ASSERT_EQ(1u, credential_store_.credentials.count);

  form.password_value = u"Qwerty123!";
  password_store_->UpdateLogin(form);
  task_environment_.RunUntilIdle();

  // Expect that the credential in the store now has the same password.
  ASSERT_EQ(1u, credential_store_.credentials.count);
  EXPECT_NSEQ(credential_store_.credentials[0].password, @"Qwerty123!");

  password_store_->RemoveLogin(FROM_HERE, form);
  task_environment_.RunUntilIdle();

  // Expect the store to be empty.
  EXPECT_EQ(0u, credential_store_.credentials.count);
}

// Test that CredentialProviderService observes changes in the primary identity.
TEST_F(CredentialProviderServiceTest, AccountChange) {
  CreateCredentialProviderService();

  EXPECT_FALSE([app_group::GetGroupUserDefaults()
      stringForKey:AppGroupUserDefaultsCredentialProviderUserID()]);

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

  EXPECT_NSEQ([app_group::GetGroupUserDefaults()
                  stringForKey:AppGroupUserDefaultsCredentialProviderUserID()],
              base::SysUTF8ToNSString(core_account.gaia));

  identity_test_environment_.ClearPrimaryAccount();
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE([app_group::GetGroupUserDefaults()
      stringForKey:AppGroupUserDefaultsCredentialProviderUserID()]);
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
  sync_service_.SetSignedIn(signin::ConsentLevel::kSync, account);

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
  CoreAccountInfo account;
  account.email = "foo@gmail.com";
  account.gaia = "gaia";
  account.account_id = CoreAccountId::FromGaiaId("gaia");
  sync_service_.SetSignedIn(signin::ConsentLevel::kSignin, account);

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

TEST_F(CredentialProviderServiceTest, AddCredentialsWithValidURL) {
  CreateCredentialProviderService();

  ASSERT_EQ(credential_store_.credentials.count, 0u);

  // Add password with valid URL to store.
  EXPECT_CALL(large_icon_service_,
              GetLargeIconRawBitmapOrFallbackStyleForPageUrl(_, _, _, _, _))
      .Times(1);
  password_manager::PasswordForm valid_password_form;
  valid_password_form.url = GURL("http://g.com");
  valid_password_form.username_value = u"user1";
  valid_password_form.password_value = u"pwd1";
  password_store_->AddLogin(valid_password_form);
  task_environment_.RunUntilIdle();

  ASSERT_EQ(credential_store_.credentials.count, 1u);

  // Don't add password with invalid URL to store.
  // No favicon should be fetched for invalid URLs.
  EXPECT_CALL(large_icon_service_,
              GetLargeIconRawBitmapOrFallbackStyleForPageUrl(_, _, _, _, _))
      .Times(0);
  password_manager::PasswordForm invalid_password_form;
  invalid_password_form.url = GURL("");
  invalid_password_form.username_value = u"user2";
  invalid_password_form.password_value = u"pwd2";
  password_store_->AddLogin(invalid_password_form);
  task_environment_.RunUntilIdle();

  ASSERT_EQ(credential_store_.credentials.count, 1u);

  // Add password with valid Android facet URI to store.
  // No favicon should be fetched for Android URI.
  EXPECT_CALL(large_icon_service_,
              GetLargeIconRawBitmapOrFallbackStyleForPageUrl(_, _, _, _, _))
      .Times(0);
  password_manager::PasswordForm android_password_form;
  android_password_form.url = GURL(android_password_form.signon_realm);
  android_password_form.signon_realm = "android://hash@com.example.my.app";
  android_password_form.password_element = u"pwd";
  android_password_form.password_value = u"example";
  password_store_->AddLogin(android_password_form);
  task_environment_.RunUntilIdle();

  ASSERT_EQ(credential_store_.credentials.count, 2u);
}

TEST_F(CredentialProviderServiceTest, AddCredentialsRefactored) {
  base::test::ScopedFeatureList scoped_feature_list_;
  scoped_feature_list_.InitWithFeatureState(
      kCredentialProviderPerformanceImprovements, true);

  CreateCredentialProviderService();
  ASSERT_EQ(credential_store_.credentials.count, 0u);

  // Add password with valid URL to store.
  EXPECT_CALL(large_icon_service_,
              GetLargeIconRawBitmapOrFallbackStyleForPageUrl(_, _, _, _, _))
      .Times(1);
  password_manager::PasswordForm valid_password_form;
  valid_password_form.url = GURL("http://g.com");
  valid_password_form.username_value = u"user1";
  valid_password_form.password_value = u"pwd1";
  password_store_->AddLogin(valid_password_form);
  task_environment_.RunUntilIdle();

  ASSERT_EQ(credential_store_.credentials.count, 1u);

  // Don't add password with invalid URL to store.
  // No favicon should be fetched for invalid URLs.
  EXPECT_CALL(large_icon_service_,
              GetLargeIconRawBitmapOrFallbackStyleForPageUrl(_, _, _, _, _))
      .Times(0);
  password_manager::PasswordForm invalid_password_form;
  invalid_password_form.url = GURL("");
  invalid_password_form.username_value = u"user2";
  invalid_password_form.password_value = u"pwd2";
  password_store_->AddLogin(invalid_password_form);
  task_environment_.RunUntilIdle();

  ASSERT_EQ(credential_store_.credentials.count, 1u);

  // Add password with valid Android facet URI to store.
  // No favicon should be fetched for Android URI.
  EXPECT_CALL(large_icon_service_,
              GetLargeIconRawBitmapOrFallbackStyleForPageUrl(_, _, _, _, _))
      .Times(0);
  password_manager::PasswordForm android_password_form;
  android_password_form.url = GURL(android_password_form.signon_realm);
  android_password_form.signon_realm = "android://hash@com.example.my.app";
  android_password_form.password_element = u"pwd";
  android_password_form.password_value = u"example";
  password_store_->AddLogin(android_password_form);
  task_environment_.RunUntilIdle();

  ASSERT_EQ(credential_store_.credentials.count, 2u);
}

TEST_F(CredentialProviderServiceTest,
       OnLoginsChanged_WithPerformanceImprovements_SingleOperation) {
  base::test::ScopedFeatureList scoped_feature_list_;
  scoped_feature_list_.InitWithFeatureState(
      kCredentialProviderPerformanceImprovements, true);

  CreateCredentialProviderService();
  ASSERT_EQ(credential_store_.credentials.count, 0u);

  base::HistogramTester histogram_tester;

  // Test adding a password.
  password_manager::PasswordForm test_form;
  test_form.url = GURL("http://example.com/login");
  test_form.username_value = u"username";
  test_form.password_value = u"12345";

  password_manager::PasswordStoreChangeList change_list;
  change_list.emplace_back(password_manager::PasswordStoreChange(
      password_manager::PasswordStoreChange::ADD, test_form));

  credential_provider_service_->OnLoginsChanged(password_store_.get(),
                                                change_list);
  task_environment_.RunUntilIdle();

  ASSERT_EQ(credential_store_.credentials.count, 1u);
  EXPECT_NSEQ(credential_store_.credentials[0].username, @"username");
  EXPECT_NSEQ(credential_store_.credentials[0].password, @"12345");
  histogram_tester.ExpectTotalCount(kSyncStoreHistogramName, 1);

  // Test updating a password.
  test_form.password_value = u"54321";
  change_list.clear();
  password_manager::PasswordStoreChange change(
      password_manager::PasswordStoreChange::UPDATE, test_form,
      /*password_changed=*/true);
  change_list.emplace_back(change);

  credential_provider_service_->OnLoginsChanged(password_store_.get(),
                                                change_list);
  task_environment_.RunUntilIdle();

  ASSERT_EQ(credential_store_.credentials.count, 1u);
  EXPECT_NSEQ(credential_store_.credentials[0].password, @"54321");
  histogram_tester.ExpectTotalCount(kSyncStoreHistogramName, 2);

  // Test deleting a password.
  change_list.clear();
  change_list.emplace_back(password_manager::PasswordStoreChange(
      password_manager::PasswordStoreChange::REMOVE, test_form));

  credential_provider_service_->OnLoginsChanged(password_store_.get(),
                                                change_list);
  task_environment_.RunUntilIdle();

  ASSERT_EQ(credential_store_.credentials.count, 0u);
  histogram_tester.ExpectTotalCount(kSyncStoreHistogramName, 3);
}

TEST_F(CredentialProviderServiceTest,
       OnLoginsChanged_WithPerformanceImprovements_MultipleOperations) {
  base::test::ScopedFeatureList scoped_feature_list_;
  scoped_feature_list_.InitWithFeatureState(
      kCredentialProviderPerformanceImprovements, true);

  CreateCredentialProviderService();
  ASSERT_EQ(credential_store_.credentials.count, 0u);

  // Setup
  password_manager::PasswordForm test_form;
  test_form.url = GURL("http://example.com/login");
  test_form.username_value = u"username";
  test_form.password_value = u"12345";

  password_manager::PasswordForm test_form2;
  test_form2.url = GURL("http://homersimpson.com/login");
  test_form2.username_value = u"homer";
  test_form2.password_value = u"simpson";

  password_manager::PasswordStoreChangeList change_list;
  change_list.emplace_back(password_manager::PasswordStoreChange(
      password_manager::PasswordStoreChange::ADD, test_form));
  change_list.emplace_back(password_manager::PasswordStoreChange(
      password_manager::PasswordStoreChange::ADD, test_form2));

  credential_provider_service_->OnLoginsChanged(password_store_.get(),
                                                change_list);
  task_environment_.RunUntilIdle();

  ASSERT_EQ(credential_store_.credentials.count, 2u);

  // Prepare simultaneous ADD, UPDATE and REMOVE operations.
  password_manager::PasswordForm test_form3;
  test_form3.url = GURL("http://margesimpson.com/login");
  test_form3.username_value = u"marge";
  test_form3.password_value = u"bouvier";

  test_form2.password_value = u"JSimpson";

  change_list.clear();
  change_list.emplace_back(password_manager::PasswordStoreChange(
      password_manager::PasswordStoreChange::ADD, test_form3));
  change_list.emplace_back(password_manager::PasswordStoreChange(
      password_manager::PasswordStoreChange::UPDATE, test_form2,
      /*password_changed=*/true));
  change_list.emplace_back(password_manager::PasswordStoreChange(
      password_manager::PasswordStoreChange::REMOVE, test_form));

  base::HistogramTester histogram_tester;

  // Test results.
  credential_provider_service_->OnLoginsChanged(password_store_.get(),
                                                change_list);
  task_environment_.RunUntilIdle();

  ASSERT_EQ(credential_store_.credentials.count, 2u);
  EXPECT_NSEQ(credential_store_.credentials[0].username, @"homer");
  EXPECT_NSEQ(credential_store_.credentials[0].password, @"JSimpson");
  EXPECT_NSEQ(credential_store_.credentials[1].username, @"marge");
  EXPECT_NSEQ(credential_store_.credentials[1].password, @"bouvier");

  // There should have been only one write to disk.
  histogram_tester.ExpectTotalCount(kSyncStoreHistogramName, 1);
}

// Tests that a PasswordStoreChange Update that doesn't change the password
// doesn't result in a write to disk.
TEST_F(
    CredentialProviderServiceTest,
    OnLoginsChanged_WithPerformanceImprovements_UpdateWithoutPasswordChangeNoDiskSave) {
  base::test::ScopedFeatureList scoped_feature_list_;
  scoped_feature_list_.InitWithFeatureState(
      kCredentialProviderPerformanceImprovements, true);

  CreateCredentialProviderService();
  ASSERT_EQ(credential_store_.credentials.count, 0u);

  // Setup
  password_manager::PasswordForm test_form;
  test_form.url = GURL("http://example.com/login");
  test_form.username_value = u"username";
  test_form.password_value = u"12345";

  password_manager::PasswordStoreChangeList change_list;
  change_list.emplace_back(password_manager::PasswordStoreChange(
      password_manager::PasswordStoreChange::ADD, test_form));

  credential_provider_service_->OnLoginsChanged(password_store_.get(),
                                                change_list);
  task_environment_.RunUntilIdle();

  ASSERT_EQ(credential_store_.credentials.count, 1u);

  // Update the PasswordForm without changing its username, url or password.
  // This mimicks a password usage.
  test_form.date_last_used = base::Time::Now();

  change_list.clear();
  change_list.emplace_back(password_manager::PasswordStoreChange(
      password_manager::PasswordStoreChange::UPDATE, test_form,
      /*password_changed=*/false));

  base::HistogramTester histogram_tester;

  // Test results.
  credential_provider_service_->OnLoginsChanged(password_store_.get(),
                                                change_list);
  task_environment_.RunUntilIdle();

  ASSERT_EQ(credential_store_.credentials.count, 1u);
  EXPECT_NSEQ(credential_store_.credentials[0].username, @"username");
  EXPECT_NSEQ(credential_store_.credentials[0].password, @"12345");

  // There should have been only one write to disk.
  histogram_tester.ExpectTotalCount(kSyncStoreHistogramName, 0);
}

TEST_F(CredentialProviderServiceTest, AddPasskeys) {
  CreateCredentialProviderService(/*with_account_store=*/true);

  ASSERT_EQ(credential_store_.credentials.count, 0u);

  // Add passkey with valid URL to store.
  EXPECT_CALL(large_icon_service_,
              GetLargeIconRawBitmapOrFallbackStyleForPageUrl(_, _, _, _, _))
      .Times(1);
  sync_pb::WebauthnCredentialSpecifics valid_passkey = CreatePasskey(
      "g.com", {1, 2, 3, 4}, "passkey_username", "passkey_display_name");
  test_passkey_model_->AddNewPasskeyForTesting(valid_passkey);
  task_environment_.RunUntilIdle();

  ASSERT_EQ(credential_store_.credentials.count, 1u);

  // Don't add passkey with invalid URL to store.
  // No favicon should be fetched for invalid URLs.
  EXPECT_CALL(large_icon_service_,
              GetLargeIconRawBitmapOrFallbackStyleForPageUrl(_, _, _, _, _))
      .Times(0);
  sync_pb::WebauthnCredentialSpecifics invalid_passkey = CreatePasskey(
      "", {1, 2, 3, 4}, "passkey_username", "passkey_display_name");
  test_passkey_model_->AddNewPasskeyForTesting(invalid_passkey);
  task_environment_.RunUntilIdle();

  ASSERT_EQ(credential_store_.credentials.count, 1u);

  // Add 2nd passkey with valid URL to store.
  EXPECT_CALL(large_icon_service_,
              GetLargeIconRawBitmapOrFallbackStyleForPageUrl(_, _, _, _, _))
      .Times(1);
  sync_pb::WebauthnCredentialSpecifics valid_passkey2 = CreatePasskey(
      "g.com", {1, 2, 3, 4}, "passkey_username2", "passkey_display_name2");
  test_passkey_model_->AddNewPasskeyForTesting(valid_passkey2);
  task_environment_.RunUntilIdle();

  ASSERT_EQ(credential_store_.credentials.count, 2u);
}

TEST_F(CredentialProviderServiceTest, DeletePasskey) {
  CreateCredentialProviderService(/*with_account_store=*/true);

  ASSERT_EQ(credential_store_.credentials.count, 0u);

  // Add passkey with valid URL to store.
  EXPECT_CALL(large_icon_service_,
              GetLargeIconRawBitmapOrFallbackStyleForPageUrl(_, _, _, _, _))
      .Times(1);
  sync_pb::WebauthnCredentialSpecifics passkey = CreatePasskey(
      "g.com", {1, 2, 3, 4}, "passkey_username", "passkey_display_name");
  test_passkey_model_->AddNewPasskeyForTesting(passkey);
  task_environment_.RunUntilIdle();

  ASSERT_EQ(credential_store_.credentials.count, 1u);

  test_passkey_model_->DeletePasskey(passkey.credential_id(), FROM_HERE);
  task_environment_.RunUntilIdle();

  ASSERT_EQ(credential_store_.credentials.count, 0u);
}

TEST_F(CredentialProviderServiceTest, UpdatePasskey) {
  CreateCredentialProviderService(/*with_account_store=*/true);

  ASSERT_EQ(credential_store_.credentials.count, 0u);

  // Add passkey with valid URL to store.
  sync_pb::WebauthnCredentialSpecifics passkey = CreatePasskey(
      "g.com", {1, 2, 3, 4}, "passkey_username", "passkey_display_name");
  const base::Time timestamp =
      base::Time::FromDeltaSinceWindowsEpoch(base::Microseconds(10));
  passkey.set_last_used_time_windows_epoch_micros(
      timestamp.ToDeltaSinceWindowsEpoch().InMicroseconds() - 1);
  test_passkey_model_->AddNewPasskeyForTesting(passkey);
  task_environment_.RunUntilIdle();

  ASSERT_EQ(credential_store_.credentials.count, 1u);

  test_passkey_model_->UpdatePasskey(
      passkey.credential_id(),
      {
          .user_name = "new_passkey_username",
          .user_display_name = "new_passkey_display_name",
      },
      /*updated_by_user=*/true);

  test_passkey_model_->UpdatePasskeyTimestamp(passkey.credential_id(),
                                              timestamp);

  task_environment_.RunUntilIdle();

  ASSERT_EQ(credential_store_.credentials.count, 1u);
  EXPECT_NSEQ(credential_store_.credentials[0].username,
              @"new_passkey_username");
  EXPECT_NSEQ(credential_store_.credentials[0].userDisplayName,
              @"new_passkey_display_name");
  ASSERT_EQ(credential_store_.credentials[0].lastUsedTime,
            timestamp.ToDeltaSinceWindowsEpoch().InMicroseconds());
}

}  // namespace
