// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/credential_provider/model/credential_provider_service.h"

#import <memory>
#import <string>
#import <utility>
#import <vector>

#import "base/apple/foundation_util.h"
#import "base/files/scoped_temp_dir.h"
#import "base/location.h"
#import "base/memory/scoped_refptr.h"
#import "base/rand_util.h"
#import "base/strings/sys_string_conversions.h"
#import "base/task/thread_pool/thread_pool_instance.h"
#import "base/test/ios/wait_util.h"
#import "base/test/metrics/histogram_tester.h"
#import "base/test/scoped_feature_list.h"
#import "base/test/task_environment.h"
#import "components/affiliations/core/browser/fake_affiliation_service.h"
#import "components/password_manager/core/browser/password_form.h"
#import "components/password_manager/core/browser/password_store/password_form_converters.h"
#import "components/password_manager/core/browser/password_store/password_store_change.h"
#import "components/password_manager/core/browser/password_store/test_password_store.h"
#import "components/password_manager/core/common/password_manager_pref_names.h"
#import "components/prefs/pref_registry_simple.h"
#import "components/prefs/pref_service.h"
#import "components/prefs/testing_pref_service.h"
#import "components/signin/public/base/consent_level.h"
#import "components/signin/public/identity_manager/identity_test_environment.h"
#import "components/signin/public/identity_manager/tribool.h"
#import "components/sync/base/user_selectable_type.h"
#import "components/sync/test/test_sync_service.h"
#import "components/webauthn/core/browser/test_passkey_model.h"
#import "google_apis/gaia/gaia_id.h"
#import "ios/chrome/browser/credential_provider/model/credential_provider_test_util.h"
#import "ios/chrome/browser/credential_provider/model/credential_provider_util.h"
#import "ios/chrome/browser/credential_provider/model/features.h"
#import "ios/chrome/browser/favicon/model/mock_favicon_loader.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/common/app_group/app_group_constants.h"
#import "ios/chrome/common/credential_provider/constants.h"
#import "ios/chrome/common/credential_provider/credential.h"
#import "ios/chrome/common/credential_provider/memory_credential_store.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

namespace {

using ::testing::_;
using ::testing::UnorderedElementsAre;

constexpr char kRpId[] = "example.com";
constexpr char kTestUrl1[] = "http://example.com";
constexpr char kTestUrl2[] = "http://example2.com";
constexpr char kTestUrl3[] = "http://example3.com";
constexpr char kAndroidRealm[] = "android://hash@com.example.my.app";

constexpr char16_t kTestUsername1[] = u"user1";
constexpr char16_t kTestUsername2[] = u"user2";

constexpr char16_t kTestPassword1[] = u"pwd1";
constexpr char16_t kTestPassword2[] = u"pwd2";

constexpr char kEmailFoo[] = "foo@gmail.com";
constexpr char kGaiaId[] = "gaia";
constexpr char kManagedDomain[] = "managed.com";
constexpr NSUInteger kMaxFaviconsForTesting = 10;
NSString* const kDummyFaviconFileFormat = @"file%d";
NSString* const kDummyFaviconContent = @"dummy";

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
    ASSERT_TRUE(DeleteFaviconsFolder());

    ASSERT_TRUE(scoped_temp_dir_.CreateUniqueTempDir());
    NSURL* folder_url =
        base::apple::FilePathToNSURL(scoped_temp_dir_.GetPath());
    SetFaviconsFolderURLForTesting(folder_url);

    password_store_->Init(/*affiliated_match_helper=*/nullptr);
    account_password_store_->Init(/*affiliated_match_helper=*/nullptr);
    testing_pref_service_.registry()->RegisterBooleanPref(
        password_manager::prefs::kCredentialsEnableService, true);
    testing_pref_service_.registry()->RegisterBooleanPref(
        password_manager::prefs::kCredentialsEnablePasskeys, true);
    testing_pref_service_.registry()->RegisterBooleanPref(
        password_manager::prefs::kAutomaticPasskeyUpgrades, true);
  }

  void TearDown() override {
    // Delete all favicon files that were created during the test.
    EXPECT_TRUE(DeleteFaviconsFolder());
    SetFaviconsFolderURLForTesting(nil);
    ResetMaxNumberOfFaviconsForTesting();

    if (credential_provider_service_) {
      credential_provider_service_->Shutdown();
    }
    password_store_->ShutdownOnUIThread();
    account_password_store_->ShutdownOnUIThread();
    PlatformTest::TearDown();
  }

  PrefService* local_state() {
    return GetApplicationContext()->GetLocalState();
  }

  void CreateCredentialProviderService(bool with_account_store = false) {
    // Make sure to shut down the previous instance before creating a new one.
    if (credential_provider_service_) {
      credential_provider_service_->Shutdown();
    }

    const std::string profile_name = "profile_name";
    local_state()->SetString(prefs::kLastUsedProfile, profile_name);
    credential_provider_service_ = std::make_unique<CredentialProviderService>(
        profile_name, &testing_pref_service_, local_state(), password_store_,
        with_account_store ? account_password_store_ : nullptr,
        test_passkey_model_.get(), credential_store_,
        identity_test_environment_.identity_manager(), &sync_service_,
        &affiliation_service_, &favicon_loader_);
  }

  // Creates `count` dummy favicon files in the specified `folder_url`.
  void CreateDummyFavicons(int count, NSURL* folder_url) {
    for (int i = 0; i < count; ++i) {
      NSString* filename =
          [NSString stringWithFormat:kDummyFaviconFileFormat, i];
      NSURL* file_url = [folder_url URLByAppendingPathComponent:filename];
      NSError* error = nil;
      BOOL success = [kDummyFaviconContent writeToURL:file_url
                                           atomically:YES
                                             encoding:NSUTF8StringEncoding
                                                error:&error];
      ASSERT_TRUE(success) << base::SysNSStringToUTF8([error description]);
    }
  }

  bool WaitForCredentialCount(NSUInteger count) {
    // For add and remove operations to propagate to the credential store, both
    // the message loop and the NSRunLoop need to spin.
    return base::test::ios::WaitUntilConditionOrTimeout(
        base::test::ios::kWaitForActionTimeout,
        /* run_message_loop = */ true, ^{
          return credential_store_.credentials.count == count;
        });
  }

  bool WaitForCredentialUsername(NSString* username, NSUInteger index) {
    // For add and remove operations to propagate to the credential store, both
    // the message loop and the NSRunLoop need to spin.
    return base::test::ios::WaitUntilConditionOrTimeout(
        base::test::ios::kWaitForActionTimeout,
        /* run_message_loop = */ true, ^{
          return credential_store_.credentials.count > index &&
                 [credential_store_.credentials[index].username
                     isEqualToString:username];
        });
  }

  bool WaitForCredentialPassword(NSString* password, NSUInteger index) {
    // For add and remove operations to propagate to the credential store, both
    // the message loop and the NSRunLoop need to spin.
    return base::test::ios::WaitUntilConditionOrTimeout(
        base::test::ios::kWaitForActionTimeout,
        /* run_message_loop = */ true, ^{
          return credential_store_.credentials.count > index &&
                 [credential_store_.credentials[index].password
                     isEqualToString:password];
        });
  }

  bool WaitForCredentialTimestamp(const base::Time& timestamp,
                                  NSUInteger index) {
    // For add and remove operations to propagate to the credential store, both
    // the message loop and the NSRunLoop need to spin.
    const int64_t lastUsedTime =
        timestamp.ToDeltaSinceWindowsEpoch().InMicroseconds();
    return base::test::ios::WaitUntilConditionOrTimeout(
        base::test::ios::kWaitForActionTimeout,
        /* run_message_loop = */ true, ^{
          return credential_store_.credentials.count > index &&
                 credential_store_.credentials[index].lastUsedTime ==
                     lastUsedTime;
        });
  }

 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  IOSChromeScopedTestingLocalState scoped_local_state_;
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
  MockFaviconLoader favicon_loader_;
  std::unique_ptr<CredentialProviderService> credential_provider_service_;
  bool favicon_called_ = false;
  base::ScopedTempDir scoped_temp_dir_;
};

// Test that CredentialProviderService writes all the credentials the first time
// it runs.
TEST_F(CredentialProviderServiceTest, FirstSync) {
  password_manager::PasswordForm form;
  form.url = GURL(kTestUrl1);
  form.username_value = kTestUsername1;
  form.password_value = kTestPassword1;
  password_store_->AddLogin(password_manager::FromPasswordForm(form));
  base::RunLoop().RunUntilIdle();

  CreateCredentialProviderService();
  // The first write is delayed.
  task_environment_.FastForwardBy(base::Seconds(30));
  base::RunLoop().RunUntilIdle();

  ASSERT_TRUE(WaitForCredentialCount(1u));
  EXPECT_NSEQ(credential_store_.credentials[0].serviceName,
              base::SysUTF8ToNSString(GURL(kTestUrl1).host()));
  EXPECT_NSEQ(credential_store_.credentials[0].username,
              base::SysUTF16ToNSString(kTestUsername1));
  EXPECT_NSEQ(credential_store_.credentials[0].password,
              base::SysUTF16ToNSString(kTestPassword1));
}

TEST_F(CredentialProviderServiceTest, TwoStores) {
  password_manager::PasswordForm local_form;
  local_form.url = GURL(kTestUrl1);
  local_form.username_value = kTestUsername1;
  local_form.keychain_identifier = "encrypted-pwd";
  password_store_->AddLogin(password_manager::FromPasswordForm(local_form));
  password_manager::PasswordForm account_form = local_form;
  account_form.url = GURL(kTestUrl2);
  account_password_store_->AddLogin(
      password_manager::FromPasswordForm(account_form));
  CreateCredentialProviderService(/*with_account_store=*/true);
  base::RunLoop().RunUntilIdle();

  ASSERT_TRUE(WaitForCredentialCount(2u));
  EXPECT_THAT(
      GetServiceNames(credential_store_.credentials),
      UnorderedElementsAre(GURL(kTestUrl1).host(), GURL(kTestUrl2).host()));

  password_manager::PasswordForm local_and_account_form = local_form;
  local_and_account_form.url = GURL(kTestUrl3);
  password_store_->AddLogin(
      password_manager::FromPasswordForm(local_and_account_form));
  account_password_store_->AddLogin(
      password_manager::FromPasswordForm(local_and_account_form));
  base::RunLoop().RunUntilIdle();

  ASSERT_TRUE(WaitForCredentialCount(3u));
  EXPECT_THAT(
      GetServiceNames(credential_store_.credentials),
      UnorderedElementsAre(GURL(kTestUrl1).host(), GURL(kTestUrl2).host(),
                           GURL(kTestUrl3).host()));

  password_store_->RemoveLogin(
      FROM_HERE, password_manager::FromPasswordForm(local_and_account_form));
  base::RunLoop().RunUntilIdle();

  ASSERT_EQ(credential_store_.credentials.count, 3u);
  EXPECT_THAT(
      GetServiceNames(credential_store_.credentials),
      UnorderedElementsAre(GURL(kTestUrl1).host(), GURL(kTestUrl2).host(),
                           GURL(kTestUrl3).host()));

  account_password_store_->RemoveLogin(
      FROM_HERE, password_manager::FromPasswordForm(local_and_account_form));
  ASSERT_TRUE(WaitForCredentialCount(2u));

  EXPECT_THAT(
      GetServiceNames(credential_store_.credentials),
      UnorderedElementsAre(GURL(kTestUrl1).host(), GURL(kTestUrl2).host()));
}

// Test that CredentialProviderService observes changes in the password store.
TEST_F(CredentialProviderServiceTest, PasswordChanges) {
  CreateCredentialProviderService();

  EXPECT_EQ(0u, credential_store_.credentials.count);

  password_manager::PasswordForm form;
  form.url = GURL(kTestUrl1);
  form.signon_realm = kTestUrl2;
  form.action = GURL(kTestUrl2);
  form.password_element = kTestPassword1;
  form.password_value = kTestPassword1;
  password_store_->AddLogin(password_manager::FromPasswordForm(form));
  task_environment_.RunUntilIdle();

  ASSERT_TRUE(WaitForCredentialCount(1u));

  form.password_value = kTestPassword2;
  password_store_->UpdateLogin(password_manager::FromPasswordForm(form));

  // Expect that the credential in the store now has the same password.
  ASSERT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForActionTimeout,
      /* run_message_loop = */ true, ^{
        return credential_store_.credentials.count == 1 &&
               [credential_store_.credentials[0].password
                   isEqualToString:base::SysUTF16ToNSString(kTestPassword2)];
      }));

  password_store_->RemoveLogin(FROM_HERE,
                               password_manager::FromPasswordForm(form));
  task_environment_.RunUntilIdle();

  // Expect the store to be empty.
  EXPECT_TRUE(WaitForCredentialCount(0u));
}

// Test that CredentialProviderService observes changes in the primary identity.
TEST_F(CredentialProviderServiceTest, AccountChange) {
  CreateCredentialProviderService();

  EXPECT_FALSE([app_group::GetGroupUserDefaults()
      stringForKey:AppGroupUserDefaultsCredentialProviderManagedUserID()]);
  EXPECT_FALSE([app_group::GetGroupUserDefaults()
      stringForKey:AppGroupUserDefaultsCredentialProviderUserID()]);

  // Set managed account as the primary one.
  CoreAccountInfo core_account =
      identity_test_environment_.MakeAccountAvailable(kEmailFoo);
  AccountInfo account = AccountInfo::Builder(core_account)
                            .SetHostedDomain(kManagedDomain)
                            .Build();
  ASSERT_EQ(account.IsManaged(), signin::Tribool::kTrue);
  identity_test_environment_.UpdateAccountInfoForAccount(account);
  identity_test_environment_.SetPrimaryAccount(kEmailFoo,
                                               signin::ConsentLevel::kSignin);
  base::RunLoop().RunUntilIdle();

  EXPECT_NSEQ(
      [app_group::GetGroupUserDefaults()
          stringForKey:AppGroupUserDefaultsCredentialProviderManagedUserID()],
      core_account.gaia.ToNSString());
  EXPECT_NSEQ([app_group::GetGroupUserDefaults()
                  stringForKey:AppGroupUserDefaultsCredentialProviderUserID()],
              core_account.gaia.ToNSString());

  identity_test_environment_.ClearPrimaryAccount();
  base::RunLoop().RunUntilIdle();

  sync_service_.SetSignedOut();
  sync_service_.FireStateChanged();

  EXPECT_FALSE([app_group::GetGroupUserDefaults()
      stringForKey:AppGroupUserDefaultsCredentialProviderManagedUserID()]);
  EXPECT_FALSE([app_group::GetGroupUserDefaults()
      stringForKey:AppGroupUserDefaultsCredentialProviderUserID()]);
}

// Test that CredentialProviderService observes changes in the password store.
TEST_F(CredentialProviderServiceTest, AndroidCredential) {
  CreateCredentialProviderService();

  EXPECT_EQ(0u, credential_store_.credentials.count);

  password_manager::PasswordForm form;
  form.url = GURL(form.signon_realm);
  form.signon_realm = kAndroidRealm;
  form.password_element = kTestPassword1;
  form.password_value = kTestPassword2;
  password_store_->AddLogin(password_manager::FromPasswordForm(form));
  task_environment_.RunUntilIdle();

  // Expect the store to be populated with 1 credential.
  EXPECT_TRUE(WaitForCredentialCount(1u));
}

// Test that the CredentialProviderService observes changes in the preference
// that controls password creation
TEST_F(CredentialProviderServiceTest, PasswordCreationPreference) {
  CreateCredentialProviderService();

  // The test is initialized with the preference as true. Make sure the
  // NSUserDefaults value is also true.
  EXPECT_TRUE([[app_group::GetGroupUserDefaults()
      objectForKey:
          AppGroupUserDefaultsCredentialProviderSavingPasswordsEnabled()]
      boolValue]);

  // Change the pref value to false.
  testing_pref_service_.SetBoolean(
      password_manager::prefs::kCredentialsEnableService, false);

  // Make sure the NSUserDefaults value is now false.
  EXPECT_FALSE([[app_group::GetGroupUserDefaults()
      objectForKey:
          AppGroupUserDefaultsCredentialProviderSavingPasswordsEnabled()]
      boolValue]);
}

// Tests that the CredentialProviderService has the correct stored email based
// on the password sync state.
TEST_F(CredentialProviderServiceTest, PasswordSyncStoredEmail) {
  // Start by signing in and turning sync on.
  CoreAccountInfo account;
  account.email = kEmailFoo;
  account.gaia = GaiaId(kGaiaId);
  account.account_id = CoreAccountId::FromGaiaId(GaiaId(kGaiaId));
  sync_service_.SetSignedIn(signin::ConsentLevel::kSignin, account);

  CreateCredentialProviderService();

  EXPECT_NSEQ(
      base::SysUTF8ToNSString(kEmailFoo),
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
  account.email = kEmailFoo;
  account.gaia = GaiaId(kGaiaId);
  account.account_id = CoreAccountId::FromGaiaId(GaiaId(kGaiaId));
  sync_service_.SetSignedIn(signin::ConsentLevel::kSignin, account);

  CreateCredentialProviderService();

  EXPECT_NSEQ(
      [app_group::GetGroupUserDefaults()
          stringForKey:AppGroupUserDefaultsCredentialProviderUserEmail()],
      base::SysUTF8ToNSString(kEmailFoo));

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
  EXPECT_CALL(favicon_loader_, FaviconForPageUrl).Times(1);
  password_manager::PasswordForm valid_password_form;
  valid_password_form.url = GURL(kTestUrl1);
  valid_password_form.username_value = kTestUsername1;
  valid_password_form.password_value = kTestPassword1;
  password_store_->AddLogin(
      password_manager::FromPasswordForm(valid_password_form));
  task_environment_.RunUntilIdle();

  ASSERT_TRUE(WaitForCredentialCount(1u));

  // Don't add password with invalid URL to store.
  // No favicon should be fetched for invalid URLs.
  EXPECT_CALL(favicon_loader_, FaviconForPageUrl).Times(0);
  password_manager::PasswordForm invalid_password_form;
  invalid_password_form.url = GURL("");
  invalid_password_form.username_value = kTestUsername2;
  invalid_password_form.password_value = kTestPassword2;
  password_store_->AddLogin(
      password_manager::FromPasswordForm(invalid_password_form));
  task_environment_.RunUntilIdle();

  ASSERT_EQ(credential_store_.credentials.count, 1u);

  // Add password with valid Android facet URI to store.
  // No favicon should be fetched for Android URI.
  EXPECT_CALL(favicon_loader_, FaviconForPageUrl).Times(0);
  password_manager::PasswordForm android_password_form;
  android_password_form.url = GURL(android_password_form.signon_realm);
  android_password_form.signon_realm = kAndroidRealm;
  android_password_form.password_element = kTestPassword1;
  android_password_form.password_value = kTestPassword2;
  password_store_->AddLogin(
      password_manager::FromPasswordForm(android_password_form));
  task_environment_.RunUntilIdle();

  ASSERT_TRUE(WaitForCredentialCount(2u));
}

TEST_F(CredentialProviderServiceTest, AddCredentialsRefactored) {
  base::test::ScopedFeatureList local_feature_list;
  local_feature_list.InitAndEnableFeature(
      kCredentialProviderRefactoredAddCredentials);

  CreateCredentialProviderService();
  ASSERT_EQ(credential_store_.credentials.count, 0u);

  // Add password with valid URL to store.
  EXPECT_CALL(favicon_loader_, FaviconForPageUrl).Times(1);
  password_manager::PasswordForm valid_password_form;
  valid_password_form.url = GURL(kTestUrl1);
  valid_password_form.username_value = kTestUsername1;
  valid_password_form.password_value = kTestPassword1;
  password_store_->AddLogin(
      password_manager::FromPasswordForm(valid_password_form));
  task_environment_.RunUntilIdle();

  ASSERT_TRUE(WaitForCredentialCount(1u));

  // Don't add password with invalid URL to store.
  // No favicon should be fetched for invalid URLs.
  EXPECT_CALL(favicon_loader_, FaviconForPageUrl).Times(0);
  password_manager::PasswordForm invalid_password_form;
  invalid_password_form.url = GURL("");
  invalid_password_form.username_value = kTestUsername2;
  invalid_password_form.password_value = kTestPassword2;
  password_store_->AddLogin(
      password_manager::FromPasswordForm(invalid_password_form));
  task_environment_.RunUntilIdle();

  ASSERT_EQ(credential_store_.credentials.count, 1u);

  // Add password with valid Android facet URI to store.
  // No favicon should be fetched for Android URI.
  EXPECT_CALL(favicon_loader_, FaviconForPageUrl).Times(0);
  password_manager::PasswordForm android_password_form;
  android_password_form.url = GURL(android_password_form.signon_realm);
  android_password_form.signon_realm = kAndroidRealm;
  android_password_form.password_element = kTestPassword1;
  android_password_form.password_value = kTestPassword2;
  password_store_->AddLogin(
      password_manager::FromPasswordForm(android_password_form));
  task_environment_.RunUntilIdle();

  ASSERT_TRUE(WaitForCredentialCount(2u));
}

TEST_F(CredentialProviderServiceTest, AddCredentialsRefactored_CachedFavicon) {
  base::test::ScopedFeatureList local_feature_list;
  local_feature_list.InitAndEnableFeature(
      kCredentialProviderRefactoredAddCredentials);

  CreateCredentialProviderService();
  ASSERT_EQ(credential_store_.credentials.count, 0u);

  // Create a dummy favicon file to simulate a fresh cached favicon.
  GURL url(kTestUrl1);
  NSString* favicon_key = GetFaviconFileKey(url);

  NSURL* folder_url = base::apple::FilePathToNSURL(scoped_temp_dir_.GetPath());
  ASSERT_NE(nil, folder_url);

  NSURL* file_url = [folder_url URLByAppendingPathComponent:favicon_key];
  [@"dummy" writeToURL:file_url
            atomically:YES
              encoding:NSUTF8StringEncoding
                 error:nil];

  // We expect 0 calls to FaviconForPageUrl because the favicon is cached and
  // fresh.
  EXPECT_CALL(favicon_loader_, FaviconForPageUrl).Times(0);

  password_manager::PasswordForm valid_password_form;
  valid_password_form.url = url;
  valid_password_form.username_value = u"user1";
  valid_password_form.password_value = u"pwd1";
  password_store_->AddLogin(
      password_manager::FromPasswordForm(valid_password_form));

  ASSERT_TRUE(WaitForCredentialCount(1u));
}

TEST_F(CredentialProviderServiceTest, AddCredentialsRefactored_SingleFormSkip) {
  base::test::ScopedFeatureList local_feature_list;
  local_feature_list.InitAndEnableFeature(
      kCredentialProviderRefactoredAddCredentials);

  CreateCredentialProviderService();

  NSURL* folder_url = base::apple::FilePathToNSURL(scoped_temp_dir_.GetPath());
  ASSERT_NE(nil, folder_url);

  SetMaxNumberOfFaviconsForTesting(kMaxFaviconsForTesting);
  CreateDummyFavicons(kMaxFaviconsForTesting, folder_url);

  // We expect 1 call to FaviconForPageUrl because we add a single form and
  // verification should be skipped.
  EXPECT_CALL(favicon_loader_, FaviconForPageUrl).Times(1);

  password_manager::PasswordForm form;
  form.url = GURL("http://g.com");
  form.username_value = u"user1";
  form.password_value = u"pwd1";

  password_manager::PasswordStoreChangeList change_list;
  change_list.push_back(password_manager::PasswordStoreChange(
      password_manager::PasswordStoreChange::ADD,
      password_manager::FromPasswordForm(form)));

  credential_provider_service_->OnLoginsChanged(password_store_.get(),
                                                change_list);

  ASSERT_TRUE(WaitForCredentialCount(1u));
}

TEST_F(CredentialProviderServiceTest,
       AddCredentialsRefactored_DuplicateUrlsInBatch) {
  base::test::ScopedFeatureList local_feature_list;
  local_feature_list.InitAndEnableFeature(
      kCredentialProviderRefactoredAddCredentials);

  CreateCredentialProviderService();
  ASSERT_EQ(credential_store_.credentials.count, 0u);

  // We expect exactly 1 call to FaviconForPageUrl even though we add two
  // forms with the same URL in the same batch.
  EXPECT_CALL(favicon_loader_, FaviconForPageUrl)
      .WillOnce(testing::InvokeWithoutArgs([&]() { favicon_called_ = true; }));

  password_manager::PasswordForm form1;
  form1.url = GURL("http://g.com");
  form1.username_value = u"user1";
  form1.password_value = u"pwd1";

  password_manager::PasswordForm form2;
  form2.url = GURL("http://g.com");
  form2.username_value = u"user2";
  form2.password_value = u"pwd2";

  password_manager::PasswordStoreChangeList change_list;
  change_list.emplace_back(password_manager::PasswordStoreChange::ADD,
                           password_manager::FromPasswordForm(form1));
  change_list.emplace_back(password_manager::PasswordStoreChange::ADD,
                           password_manager::FromPasswordForm(form2));

  credential_provider_service_->OnLoginsChanged(password_store_.get(),
                                                change_list);

  ASSERT_TRUE(WaitForCredentialCount(2u));

  ASSERT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForActionTimeout,
      /* run_message_loop = */ true, ^{
        return favicon_called_;
      }));
}

TEST_F(CredentialProviderServiceTest,
       AddCredentialsRefactored_MaxFaviconsBound) {
  base::test::ScopedFeatureList local_feature_list;
  local_feature_list.InitAndEnableFeature(
      kCredentialProviderRefactoredAddCredentials);

  CreateCredentialProviderService();

  NSURL* folder_url = base::apple::FilePathToNSURL(scoped_temp_dir_.GetPath());
  ASSERT_NE(nil, folder_url);

  SetMaxNumberOfFaviconsForTesting(kMaxFaviconsForTesting);
  CreateDummyFavicons(kMaxFaviconsForTesting, folder_url);

  // We expect 0 calls to FaviconForPageUrl because the limit is reached and
  // we add multiple forms.
  EXPECT_CALL(favicon_loader_, FaviconForPageUrl).Times(0);

  password_manager::PasswordForm form1;
  form1.url = GURL("http://g1.com");
  form1.username_value = u"user1";
  form1.password_value = u"pwd1";

  password_manager::PasswordForm form2;
  form2.url = GURL("http://g2.com");
  form2.username_value = u"user2";
  form2.password_value = u"pwd2";

  password_manager::PasswordStoreChangeList change_list;
  change_list.push_back(password_manager::PasswordStoreChange(
      password_manager::PasswordStoreChange::ADD,
      password_manager::FromPasswordForm(form1)));
  change_list.push_back(password_manager::PasswordStoreChange(
      password_manager::PasswordStoreChange::ADD,
      password_manager::FromPasswordForm(form2)));

  credential_provider_service_->OnLoginsChanged(password_store_.get(),
                                                change_list);

  ASSERT_TRUE(WaitForCredentialCount(2u));
}

TEST_F(CredentialProviderServiceTest,
       AddCredentialsRefactored_HistorySyncState) {
  base::test::ScopedFeatureList local_feature_list;
  local_feature_list.InitAndEnableFeature(
      kCredentialProviderRefactoredAddCredentials);

  // 1. Test with history sync DISABLED (Signed out).
  sync_service_.SetSignedOut();

  CreateCredentialProviderService();

  EXPECT_CALL(favicon_loader_,
              FaviconForPageUrl(_, _, _, /*fallback=*/false, _))
      .Times(1);

  password_manager::PasswordForm form;
  form.url = GURL("http://g.com");
  form.username_value = u"user1";
  form.password_value = u"pwd1";

  password_manager::PasswordStoreChangeList change_list;
  change_list.push_back(password_manager::PasswordStoreChange(
      password_manager::PasswordStoreChange::ADD,
      password_manager::FromPasswordForm(form)));

  credential_provider_service_->OnLoginsChanged(password_store_.get(),
                                                change_list);

  ASSERT_TRUE(WaitForCredentialCount(1u));

  // 2. Test with history sync ENABLED (Signed in).
  CoreAccountInfo account;
  account.email = kEmailFoo;
  account.gaia = GaiaId(kGaiaId);
  account.account_id = CoreAccountId::FromGaiaId(GaiaId(kGaiaId));
  sync_service_.SetSignedIn(signin::ConsentLevel::kSignin, account);

  EXPECT_CALL(favicon_loader_, FaviconForPageUrl(_, _, _, /*fallback=*/true, _))
      .Times(1);

  form.username_value = u"user2";
  change_list.clear();
  change_list.push_back(password_manager::PasswordStoreChange(
      password_manager::PasswordStoreChange::ADD,
      password_manager::FromPasswordForm(form)));

  credential_provider_service_->OnLoginsChanged(password_store_.get(),
                                                change_list);

  ASSERT_TRUE(WaitForCredentialCount(2u));
}

TEST_F(CredentialProviderServiceTest, GetFaviconsFolderURL_Metric) {
  base::HistogramTester histogram_tester;

  CreateCredentialProviderService();
  base::ThreadPoolInstance::Get()->FlushForTesting();

  histogram_tester.ExpectUniqueSample(
      "IOS.CredentialExtension.FaviconFolderAvailable", true, 1);
}

TEST_F(CredentialProviderServiceTest, GetFaviconsFolderURL_Metric_Nil) {
  SetFaviconsFolderURLForTesting(nil);
  base::HistogramTester histogram_tester;

  CreateCredentialProviderService();
  base::ThreadPoolInstance::Get()->FlushForTesting();

  histogram_tester.ExpectUniqueSample(
      "IOS.CredentialExtension.FaviconFolderAvailable", false, 1);
}

TEST_F(CredentialProviderServiceTest, OnLoginsChanged_SingleOperation) {
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
      password_manager::PasswordStoreChange::ADD,
      password_manager::FromPasswordForm(test_form)));

  credential_provider_service_->OnLoginsChanged(password_store_.get(),
                                                change_list);
  task_environment_.RunUntilIdle();

  ASSERT_TRUE(WaitForCredentialCount(1u));
  EXPECT_NSEQ(credential_store_.credentials[0].username, @"username");
  EXPECT_NSEQ(credential_store_.credentials[0].password, @"12345");
  histogram_tester.ExpectTotalCount(kSyncStoreHistogramName, 1);

  // Test updating a password.
  test_form.password_value = u"54321";
  change_list.clear();
  password_manager::PasswordStoreChange change(
      password_manager::PasswordStoreChange::UPDATE,
      password_manager::FromPasswordForm(test_form),
      /*password_changed=*/true);
  change_list.emplace_back(change);

  credential_provider_service_->OnLoginsChanged(password_store_.get(),
                                                change_list);
  task_environment_.RunUntilIdle();

  ASSERT_TRUE(WaitForCredentialPassword(@"54321", /*index=*/0));

  ASSERT_EQ(credential_store_.credentials.count, 1u);
  histogram_tester.ExpectTotalCount(kSyncStoreHistogramName, 2);

  // Test deleting a password.
  change_list.clear();
  change_list.emplace_back(password_manager::PasswordStoreChange(
      password_manager::PasswordStoreChange::REMOVE,
      password_manager::FromPasswordForm(test_form)));

  credential_provider_service_->OnLoginsChanged(password_store_.get(),
                                                change_list);
  task_environment_.RunUntilIdle();

  ASSERT_TRUE(WaitForCredentialCount(0u));
  histogram_tester.ExpectTotalCount(kSyncStoreHistogramName, 3);
}

TEST_F(CredentialProviderServiceTest, AddPasskeys) {
  CreateCredentialProviderService(/*with_account_store=*/true);

  ASSERT_EQ(credential_store_.credentials.count, 0u);

  // Add passkey with valid URL to store.
  EXPECT_CALL(favicon_loader_, FaviconForPageUrl).Times(1);
  sync_pb::WebauthnCredentialSpecifics valid_passkey = CreatePasskey(
      kRpId, {1, 2, 3, 4}, "passkey_username", "passkey_display_name");
  test_passkey_model_->AddNewPasskeyForTesting(valid_passkey);
  task_environment_.RunUntilIdle();

  ASSERT_TRUE(WaitForCredentialCount(1u));

  // Add passkey with invalid URL to store.
  // No favicon should be fetched for invalid URLs.
  EXPECT_CALL(favicon_loader_, FaviconForPageUrl).Times(0);
  sync_pb::WebauthnCredentialSpecifics invalid_passkey = CreatePasskey(
      "", {1, 2, 3, 4}, "passkey_username", "passkey_display_name");
  test_passkey_model_->AddNewPasskeyForTesting(invalid_passkey);
  task_environment_.RunUntilIdle();

  ASSERT_TRUE(WaitForCredentialCount(2u));

  // Hidden passkeys should be added to the store as their properties might need
  // to be updated by the Signal API.
  EXPECT_CALL(favicon_loader_, FaviconForPageUrl).Times(1);
  sync_pb::WebauthnCredentialSpecifics hidden_passkey = CreatePasskey(
      kRpId, {1, 2, 3, 4}, "passkey_username", "passkey_display_name");
  hidden_passkey.set_hidden(true);
  test_passkey_model_->AddNewPasskeyForTesting(hidden_passkey);
  task_environment_.RunUntilIdle();

  ASSERT_TRUE(WaitForCredentialCount(3u));

  // Add 2nd passkey with valid URL to store.
  EXPECT_CALL(favicon_loader_, FaviconForPageUrl).Times(1);
  sync_pb::WebauthnCredentialSpecifics valid_passkey2 = CreatePasskey(
      kRpId, {1, 2, 3, 4}, "passkey_username2", "passkey_display_name2");
  test_passkey_model_->AddNewPasskeyForTesting(valid_passkey2);
  task_environment_.RunUntilIdle();

  EXPECT_TRUE(WaitForCredentialCount(4u));
}

TEST_F(CredentialProviderServiceTest,
       HiddenPasskeyAddedToCredentialStoreWithSignalAPI) {
  CreateCredentialProviderService(/*with_account_store=*/true);
  ASSERT_EQ(credential_store_.credentials.count, 0u);

  EXPECT_CALL(favicon_loader_, FaviconForPageUrl).Times(1);
  sync_pb::WebauthnCredentialSpecifics hidden_passkey = CreatePasskey(
      kRpId, {1, 2, 3, 4}, "passkey_username", "passkey_display_name");
  hidden_passkey.set_hidden(true);
  test_passkey_model_->AddNewPasskeyForTesting(hidden_passkey);
  task_environment_.RunUntilIdle();
  ASSERT_TRUE(WaitForCredentialCount(1u));
}

TEST_F(CredentialProviderServiceTest, DeletePasskey) {
  CreateCredentialProviderService(/*with_account_store=*/true);

  ASSERT_EQ(credential_store_.credentials.count, 0u);

  // Add passkey with valid URL to store.
  EXPECT_CALL(favicon_loader_, FaviconForPageUrl).Times(1);
  sync_pb::WebauthnCredentialSpecifics passkey = CreatePasskey(
      kRpId, {1, 2, 3, 4}, "passkey_username", "passkey_display_name");
  test_passkey_model_->AddNewPasskeyForTesting(passkey);
  task_environment_.RunUntilIdle();

  ASSERT_TRUE(WaitForCredentialCount(1u));

  test_passkey_model_->DeletePasskey(passkey.credential_id(), FROM_HERE);
  task_environment_.RunUntilIdle();

  EXPECT_TRUE(WaitForCredentialCount(0u));
}

TEST_F(CredentialProviderServiceTest, UpdatePasskey) {
  CreateCredentialProviderService(/*with_account_store=*/true);

  ASSERT_EQ(credential_store_.credentials.count, 0u);

  // Add passkey with valid URL to store.
  sync_pb::WebauthnCredentialSpecifics passkey = CreatePasskey(
      kRpId, {1, 2, 3, 4}, "passkey_username", "passkey_display_name");
  const base::Time timestamp =
      base::Time::FromDeltaSinceWindowsEpoch(base::Microseconds(10));
  passkey.set_last_used_time_windows_epoch_micros(
      timestamp.ToDeltaSinceWindowsEpoch().InMicroseconds() - 1);
  test_passkey_model_->AddNewPasskeyForTesting(passkey);
  task_environment_.RunUntilIdle();

  ASSERT_TRUE(WaitForCredentialCount(1u));

  test_passkey_model_->UpdatePasskey(
      passkey.credential_id(),
      {
          .user_name = "new_passkey_username",
          .user_display_name = "new_passkey_display_name",
      },
      /*updated_by_user=*/true);

  task_environment_.RunUntilIdle();

  ASSERT_TRUE(WaitForCredentialUsername(@"new_passkey_username", /*index=*/0));

  test_passkey_model_->UpdatePasskeyTimestamp(passkey.credential_id(),
                                              timestamp);

  task_environment_.RunUntilIdle();

  ASSERT_TRUE(WaitForCredentialTimestamp(timestamp, /*index=*/0));

  ASSERT_EQ(credential_store_.credentials.count, 1u);
  EXPECT_NSEQ(credential_store_.credentials[0].userDisplayName,
              @"new_passkey_display_name");
}

TEST_F(CredentialProviderServiceTest,
       FiltersOutShadowedPasskeysDuringInitialSync) {
  sync_pb::WebauthnCredentialSpecifics shadowed_passkey =
      CreatePasskey(kRpId, /*user_id=*/{1, 2, 3, 4}, "shadowed_username",
                    "shadowed_user_display_name");
  sync_pb::WebauthnCredentialSpecifics shadowing_passkey =
      CreatePasskey(kRpId, shadowed_passkey.user_id(), "shadowing_username",
                    "shadowing_user_display_name");
  shadowing_passkey.add_newly_shadowed_credential_ids(
      shadowed_passkey.credential_id());
  test_passkey_model_->AddNewPasskeyForTesting(shadowed_passkey);
  test_passkey_model_->AddNewPasskeyForTesting(shadowing_passkey);

  CreateCredentialProviderService(/*with_account_store=*/true);

  // The initial sync is delayed, make sure it kicks in.
  task_environment_.FastForwardBy(base::Seconds(30));

  // Check that only the shadowing passkey is present in the store.
  ASSERT_TRUE(WaitForCredentialCount(1u));
  EXPECT_NSEQ(credential_store_.credentials[0].username, @"shadowing_username");
}

TEST_F(CredentialProviderServiceTest,
       AutomaticPasskeyUpgradeDisabledsWithSavingPasswordsDisabled) {
  CreateCredentialProviderService();

  // The test is initialized with the passkey preferences as true.
  EXPECT_TRUE([[app_group::GetGroupUserDefaults()
      objectForKey:
          AppGroupUserDefaulsCredentialProviderAutomaticPasskeyUpgradeEnabled()]
      boolValue]);

  // Change the pref value to false and verify the NSUserDefaults value.
  testing_pref_service_.SetBoolean(
      password_manager::prefs::kCredentialsEnableService, false);
  EXPECT_FALSE([[app_group::GetGroupUserDefaults()
      objectForKey:
          AppGroupUserDefaulsCredentialProviderAutomaticPasskeyUpgradeEnabled()]
      boolValue]);
}

TEST_F(CredentialProviderServiceTest,
       AutomaticPasskeyUpgradesDisabledWithSavingPasskeysDisabled) {
  CreateCredentialProviderService();

  // The test is initialized with the passkey preferences as true.
  EXPECT_TRUE([[app_group::GetGroupUserDefaults()
      objectForKey:
          AppGroupUserDefaulsCredentialProviderAutomaticPasskeyUpgradeEnabled()]
      boolValue]);

  // Change the pref value to false and verify the NSUserDefaults value.
  testing_pref_service_.SetBoolean(
      password_manager::prefs::kCredentialsEnablePasskeys, false);
  EXPECT_FALSE([[app_group::GetGroupUserDefaults()
      objectForKey:
          AppGroupUserDefaulsCredentialProviderAutomaticPasskeyUpgradeEnabled()]
      boolValue]);
}

TEST_F(CredentialProviderServiceTest,
       AutomaticPasskeyUpgradesPreferenceDisabled) {
  CreateCredentialProviderService();

  // The test is initialized with the passkey preferences as true.
  EXPECT_TRUE([[app_group::GetGroupUserDefaults()
      objectForKey:
          AppGroupUserDefaulsCredentialProviderAutomaticPasskeyUpgradeEnabled()]
      boolValue]);

  // Change the pref value to false and verify the NSUserDefaults value.
  testing_pref_service_.SetBoolean(
      password_manager::prefs::kAutomaticPasskeyUpgrades, false);
  EXPECT_FALSE([[app_group::GetGroupUserDefaults()
      objectForKey:
          AppGroupUserDefaulsCredentialProviderAutomaticPasskeyUpgradeEnabled()]
      boolValue]);
}

}  // namespace
