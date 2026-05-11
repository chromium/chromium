// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/credential_provider/model/credential_provider_migrator.h"

#import "base/strings/sys_string_conversions.h"
#import "base/test/metrics/histogram_tester.h"
#import "base/test/task_environment.h"
#import "base/test/test_future.h"
#import "components/password_manager/core/browser/password_form.h"
#import "components/password_manager/core/browser/password_manager_test_utils.h"
#import "components/password_manager/core/browser/password_store/mock_password_store_interface.h"
#import "components/password_manager/core/browser/password_store/password_form_converters.h"
#import "components/webauthn/core/browser/test_passkey_model.h"
#import "ios/chrome/browser/credential_provider/model/archivable_credential+password_form.h"
#import "ios/chrome/common/credential_provider/archivable_credential+passkey.h"
#import "ios/chrome/common/credential_provider/user_defaults_credential_store.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "url/gurl.h"

namespace {

constexpr int64_t kJan1st2024 = 1704085200;

NSString* const kMatchingGaia = @"123456";
NSString* const kMismatchingGaia = @"654321";

using ::base::SysNSStringToUTF8;
using ::password_manager::MockPasswordStoreInterface;
using ::password_manager::PasswordForm;
using ::testing::_;
using ::testing::IsEmpty;
using ::testing::SizeIs;
using AnyRp = ::webauthn::PasskeyModel::AnyRp;
using ShadowedCredentials = ::webauthn::PasskeyModel::ShadowedCredentials;

NSData* StringToData(std::string str) {
  return [NSData dataWithBytes:str.data() length:str.length()];
}

ArchivableCredential* TestPasswordCredential(NSString* gaia = nil) {
  NSString* username = @"username_value";
  NSString* password = @"qwerty123";
  NSString* url = @"http://www.alpha.example.com/path/and?args=8";
  NSString* recordIdentifier = @"recordIdentifier";
  NSString* note = @"note";
  return [[ArchivableCredential alloc] initWithFavicon:nil
                                                  gaia:gaia
                                              password:password
                                                  rank:1
                                      recordIdentifier:recordIdentifier
                                     serviceIdentifier:url
                                           serviceName:nil
                              registryControlledDomain:nil
                                              username:username
                                                  note:note
                                          lastUsedTime:0];
}

ArchivableCredential* TestPasskeyCredential(NSString* rpId, NSString* gaia) {
  return [[ArchivableCredential alloc]
       initWithFavicon:nil
                  gaia:gaia
      recordIdentifier:@"recordIdentifier"
                syncId:StringToData("syncIdOfLength16")
              username:@"username"
       userDisplayName:@"userDisplayName"
                userId:StringToData("userId")
          credentialId:StringToData("credentialId_16_")
                  rpId:rpId
            privateKey:StringToData("privateKey")
             encrypted:StringToData("encrypted")
          creationTime:kJan1st2024
          lastUsedTime:kJan1st2024
                hidden:NO
            hiddenTime:kJan1st2024
          editedByUser:NO];
}

class CredentialProviderMigratorTest : public PlatformTest {
 protected:
  void SetUp() override { [user_defaults_ removeObjectForKey:store_key_]; }
  void TearDown() override { [user_defaults_ removeObjectForKey:store_key_]; }

  NSUserDefaults* user_defaults_ = [NSUserDefaults standardUserDefaults];
  NSString* store_key_ = @"store_key";
  scoped_refptr<MockPasswordStoreInterface> mock_store_ =
      base::MakeRefCounted<testing::NiceMock<MockPasswordStoreInterface>>();
  webauthn::TestPasskeyModel test_passkey_model_;
  const base::HistogramTester histogram_tester_;

 private:
  // Mocking time is required for password notes since they are created with the
  // creation_date metadata, which is compared in AddLogin() call expectations.
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::UI,
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
};

// Tests basic migration for 1 password credential.
TEST_F(CredentialProviderMigratorTest, Migration) {
  // Create temp store and add 1 credential.
  UserDefaultsCredentialStore* store =
      [[UserDefaultsCredentialStore alloc] initWithUserDefaults:user_defaults_
                                                            key:store_key_];
  id<Credential> credential = TestPasswordCredential();
  [store addCredential:credential];
  [store saveDataWithCompletion:^(NSError* error) {
    EXPECT_TRUE(error == nil)
        << SysNSStringToUTF8([error localizedDescription]);
  }];
  EXPECT_EQ(store.credentials.count, 1u);

  // Create the migrator to be tested.
  CredentialProviderMigrator* migrator =
      [[CredentialProviderMigrator alloc] initWithUserDefaults:user_defaults_
                                                           key:store_key_
                                                          gaia:nil
                                                 passwordStore:mock_store_
                                                  passkeyStore:nil];
  ASSERT_TRUE(migrator);

  // Start migration.
  PasswordForm expected = PasswordFormFromCredential(credential);
  EXPECT_CALL(*mock_store_, AddLogin(EqStoredCredential(expected), _));
  base::test::TestFuture<BOOL, NSError*> future;
  auto* future_ptr = &future;
  [migrator startMigrationWithCompletion:^(BOOL success, NSError* error) {
    future_ptr->SetValue(success, error);
  }];
  auto [migration_success, migration_error] = future.Take();
  EXPECT_TRUE(migration_success);
  EXPECT_TRUE(migration_error == nil)
      << SysNSStringToUTF8([migration_error localizedDescription]);

  // Reload temp store.
  store =
      [[UserDefaultsCredentialStore alloc] initWithUserDefaults:user_defaults_
                                                            key:store_key_];
  // Verify credentials are empty.
  EXPECT_EQ(store.credentials.count, 0u);
}

// Tests basic migration for 1 passkey credential.
TEST_F(CredentialProviderMigratorTest, PasskeyMigration) {
  // Create temp store and add 1 credential.
  UserDefaultsCredentialStore* store =
      [[UserDefaultsCredentialStore alloc] initWithUserDefaults:user_defaults_
                                                            key:store_key_];
  id<Credential> credential = TestPasskeyCredential(@"rpId", kMatchingGaia);
  [store addCredential:credential];
  [store saveDataWithCompletion:^(NSError* error) {
    EXPECT_TRUE(error == nil)
        << SysNSStringToUTF8([error localizedDescription]);
  }];
  EXPECT_EQ(store.credentials.count, 1u);

  // Create the migrator to be tested.
  CredentialProviderMigrator* migrator = [[CredentialProviderMigrator alloc]
      initWithUserDefaults:user_defaults_
                       key:store_key_
                      gaia:kMatchingGaia
             passwordStore:mock_store_
              passkeyStore:&test_passkey_model_];
  ASSERT_TRUE(migrator);

  histogram_tester_.ExpectBucketCount(
      "Passkeys.IOSMigration", PasskeysMigrationStatus::kPasskeyCreated, 0);

  // Start migration.
  sync_pb::WebauthnCredentialSpecifics expected =
      PasskeyFromCredential(credential);
  base::test::TestFuture<BOOL, NSError*> future;
  auto* future_ptr = &future;
  [migrator startMigrationWithCompletion:^(BOOL success, NSError* error) {
    future_ptr->SetValue(success, error);
  }];
  auto [migration_success, migration_error] = future.Take();
  EXPECT_TRUE(migration_success);
  EXPECT_TRUE(migration_error == nil)
      << SysNSStringToUTF8([migration_error localizedDescription]);

  histogram_tester_.ExpectBucketCount(
      "Passkeys.IOSMigration", PasskeysMigrationStatus::kPasskeyCreated, 1);

  // Reload temp store.
  store =
      [[UserDefaultsCredentialStore alloc] initWithUserDefaults:user_defaults_
                                                            key:store_key_];
  // Verify credentials are empty.
  EXPECT_EQ(store.credentials.count, 0u);

  // Verify that the credential is migrated.
  std::vector<sync_pb::WebauthnCredentialSpecifics> passkeys =
      test_passkey_model_.GetPasskeys(AnyRp(), ShadowedCredentials::kInclude);
  EXPECT_THAT(passkeys, SizeIs(1));
  EXPECT_EQ(passkeys[0].sync_id(), expected.sync_id());
  EXPECT_EQ(passkeys[0].credential_id(), expected.credential_id());
  EXPECT_EQ(passkeys[0].rp_id(), expected.rp_id());
  EXPECT_EQ(passkeys[0].user_id(), expected.user_id());
  EXPECT_EQ(passkeys[0].user_name(), expected.user_name());
  EXPECT_EQ(passkeys[0].user_display_name(), expected.user_display_name());
  EXPECT_EQ(passkeys[0].creation_time(), expected.creation_time());
  EXPECT_EQ(passkeys[0].last_used_time_windows_epoch_micros(),
            expected.last_used_time_windows_epoch_micros());

  // Try to only update the last used time.
  credential.lastUsedTime = kJan1st2024 + 10;
  [store updateCredential:credential];
  [store saveDataWithCompletion:^(NSError* error) {
    EXPECT_TRUE(error == nil)
        << SysNSStringToUTF8([error localizedDescription]);
  }];
  EXPECT_EQ(store.credentials.count, 1u);

  histogram_tester_.ExpectBucketCount(
      "Passkeys.IOSMigration", PasskeysMigrationStatus::kPasskeyUpdated, 0);

  base::test::TestFuture<BOOL, NSError*> update_future;
  auto* update_future_ptr = &update_future;

  migrator = [[CredentialProviderMigrator alloc]
      initWithUserDefaults:user_defaults_
                       key:store_key_
                      gaia:kMatchingGaia
             passwordStore:mock_store_
              passkeyStore:&test_passkey_model_];

  [migrator startMigrationWithCompletion:^(BOOL success, NSError* error) {
    update_future_ptr->SetValue(success, error);
  }];
  auto [update_success, update_error] = update_future.Take();
  EXPECT_TRUE(update_success);
  EXPECT_TRUE(update_error == nil)
      << SysNSStringToUTF8([update_error localizedDescription]);

  histogram_tester_.ExpectBucketCount(
      "Passkeys.IOSMigration", PasskeysMigrationStatus::kPasskeyUpdated, 1);

  // Reload temp store.
  store =
      [[UserDefaultsCredentialStore alloc] initWithUserDefaults:user_defaults_
                                                            key:store_key_];
  // Verify credentials are empty.
  EXPECT_EQ(store.credentials.count, 0u);

  // Verify that we still have only 1 passkey and that its last used time was
  // updated.
  passkeys =
      test_passkey_model_.GetPasskeys(AnyRp(), ShadowedCredentials::kInclude);
  EXPECT_THAT(passkeys, SizeIs(1));
  EXPECT_EQ(passkeys[0].last_used_time_windows_epoch_micros(),
            credential.lastUsedTime);
}

// Tests basic migration for 1 passkey credential.
TEST_F(CredentialProviderMigratorTest, InvalidPasskeyMigration) {
  // Create temp store and add 1 credential.
  UserDefaultsCredentialStore* store =
      [[UserDefaultsCredentialStore alloc] initWithUserDefaults:user_defaults_
                                                            key:store_key_];
  id<Credential> invalidCredential =
      TestPasskeyCredential(/*rpId=*/nil, kMatchingGaia);

  [store addCredential:invalidCredential];
  [store saveDataWithCompletion:^(NSError* error) {
    EXPECT_TRUE(error == nil)
        << SysNSStringToUTF8([error localizedDescription]);
  }];
  EXPECT_EQ(store.credentials.count, 1u);

  // Create the migrator to be tested.
  CredentialProviderMigrator* migrator = [[CredentialProviderMigrator alloc]
      initWithUserDefaults:user_defaults_
                       key:store_key_
                      gaia:kMatchingGaia
             passwordStore:mock_store_
              passkeyStore:&test_passkey_model_];
  ASSERT_TRUE(migrator);

  histogram_tester_.ExpectBucketCount(
      "Passkeys.IOSMigration", PasskeysMigrationStatus::kInvalidPasskey, 0);

  // Start migration.
  base::test::TestFuture<BOOL, NSError*> future;
  auto* future_ptr = &future;
  [migrator startMigrationWithCompletion:^(BOOL success, NSError* error) {
    future_ptr->SetValue(success, error);
  }];
  auto [migration_success, migration_error] = future.Take();
  EXPECT_TRUE(migration_success);
  EXPECT_TRUE(migration_error == nil)
      << SysNSStringToUTF8([migration_error localizedDescription]);

  histogram_tester_.ExpectBucketCount(
      "Passkeys.IOSMigration", PasskeysMigrationStatus::kInvalidPasskey, 1);

  // Reload temp store.
  store =
      [[UserDefaultsCredentialStore alloc] initWithUserDefaults:user_defaults_
                                                            key:store_key_];
  // Verify credentials are empty.
  EXPECT_EQ(store.credentials.count, 0u);

  // Verify that the credential is not migrated.
  EXPECT_THAT(
      test_passkey_model_.GetPasskeys(AnyRp(), ShadowedCredentials::kInclude),
      IsEmpty());
}

TEST_F(CredentialProviderMigratorTest, PasskeyMigrationUpdatesHidden) {
  UserDefaultsCredentialStore* store =
      [[UserDefaultsCredentialStore alloc] initWithUserDefaults:user_defaults_
                                                            key:store_key_];

  // `TestPasskeyCredential()` is created with hidden = NO.
  ArchivableCredential* credential =
      TestPasskeyCredential(@"rpId", kMatchingGaia);
  [store addCredential:credential];
  [store saveDataWithCompletion:^(NSError* error) {
    EXPECT_TRUE(error == nil)
        << SysNSStringToUTF8([error localizedDescription]);
  }];
  EXPECT_EQ(store.credentials.count, 1u);

  // Create the migrator.
  CredentialProviderMigrator* migrator = [[CredentialProviderMigrator alloc]
      initWithUserDefaults:user_defaults_
                       key:store_key_
                      gaia:kMatchingGaia
             passwordStore:mock_store_
              passkeyStore:&test_passkey_model_];
  ASSERT_TRUE(migrator);

  // Start initial migration.
  base::test::TestFuture<BOOL, NSError*> future;
  auto* future_ptr = &future;
  [migrator startMigrationWithCompletion:^(BOOL success, NSError* error) {
    future_ptr->SetValue(success, error);
  }];
  auto [migration_success, migration_error] = future.Take();
  EXPECT_TRUE(migration_success);
  EXPECT_TRUE(migration_error == nil)
      << SysNSStringToUTF8([migration_error localizedDescription]);

  // Verify the passkey was migrated and is not hidden.
  std::vector<sync_pb::WebauthnCredentialSpecifics> passkeys =
      test_passkey_model_.GetPasskeys(AnyRp(), ShadowedCredentials::kInclude);
  EXPECT_THAT(passkeys, SizeIs(1));
  EXPECT_FALSE(passkeys[0].hidden());
  EXPECT_FALSE(passkeys[0].has_hidden_time());

  // Verify temporal store is empty.
  store =
      [[UserDefaultsCredentialStore alloc] initWithUserDefaults:user_defaults_
                                                            key:store_key_];
  EXPECT_EQ(store.credentials.count, 0u);

  // Add the same credential back to the store, but this time set `hidden: YES`.
  credential.hidden = YES;
  [store addCredential:credential];
  [store saveDataWithCompletion:^(NSError* error) {
    EXPECT_TRUE(error == nil)
        << SysNSStringToUTF8([error localizedDescription]);
  }];
  EXPECT_EQ(store.credentials.count, 1u);

  // Start migration again.
  base::test::TestFuture<BOOL, NSError*> update_future;
  auto* update_future_ptr = &update_future;

  migrator = [[CredentialProviderMigrator alloc]
      initWithUserDefaults:user_defaults_
                       key:store_key_
                      gaia:kMatchingGaia
             passwordStore:mock_store_
              passkeyStore:&test_passkey_model_];

  [migrator startMigrationWithCompletion:^(BOOL success, NSError* error) {
    update_future_ptr->SetValue(success, error);
  }];
  auto [update_success, update_error] = update_future.Take();
  EXPECT_TRUE(update_success);
  EXPECT_TRUE(update_error == nil)
      << SysNSStringToUTF8([update_error localizedDescription]);

  // Verify temporal store is empty again.
  store =
      [[UserDefaultsCredentialStore alloc] initWithUserDefaults:user_defaults_
                                                            key:store_key_];
  EXPECT_EQ(store.credentials.count, 0u);

  // Verify we still have only 1 passkey, but it's hidden now.
  passkeys =
      test_passkey_model_.GetPasskeys(AnyRp(), ShadowedCredentials::kInclude);
  EXPECT_THAT(passkeys, SizeIs(1));
  EXPECT_TRUE(passkeys[0].hidden());
  EXPECT_EQ(passkeys[0].hidden_time(), kJan1st2024);
}

TEST_F(CredentialProviderMigratorTest, PasskeyMigrationUpdatesUsername) {
  UserDefaultsCredentialStore* store =
      [[UserDefaultsCredentialStore alloc] initWithUserDefaults:user_defaults_
                                                            key:store_key_];
  ArchivableCredential* credential =
      TestPasskeyCredential(@"rpId", kMatchingGaia);
  [store addCredential:credential];
  [store saveDataWithCompletion:^(NSError* error) {
    EXPECT_TRUE(error == nil)
        << SysNSStringToUTF8([error localizedDescription]);
  }];
  EXPECT_EQ(store.credentials.count, 1u);

  // Create the migrator and start the initial migration.
  CredentialProviderMigrator* migrator = [[CredentialProviderMigrator alloc]
      initWithUserDefaults:user_defaults_
                       key:store_key_
                      gaia:kMatchingGaia
             passwordStore:mock_store_
              passkeyStore:&test_passkey_model_];
  ASSERT_TRUE(migrator);
  base::test::TestFuture<BOOL, NSError*> future;
  auto* future_ptr = &future;
  [migrator startMigrationWithCompletion:^(BOOL success, NSError* error) {
    future_ptr->SetValue(success, error);
  }];
  auto [migration_success, migration_error] = future.Take();
  EXPECT_TRUE(migration_success);
  EXPECT_TRUE(migration_error == nil)
      << SysNSStringToUTF8([migration_error localizedDescription]);

  // Verify the passkey was migrated and has the initial names.
  std::vector<sync_pb::WebauthnCredentialSpecifics> passkeys =
      test_passkey_model_.GetPasskeys(AnyRp(), ShadowedCredentials::kInclude);
  EXPECT_THAT(passkeys, SizeIs(1));
  EXPECT_EQ(passkeys[0].user_name(), "username");
  EXPECT_EQ(passkeys[0].user_display_name(), "userDisplayName");

  // Verify temporal store is empty.
  store =
      [[UserDefaultsCredentialStore alloc] initWithUserDefaults:user_defaults_
                                                            key:store_key_];
  EXPECT_EQ(store.credentials.count, 0u);

  // Add the same credential with a new username.
  credential.username = @"newUsername";
  [store addCredential:credential];
  [store saveDataWithCompletion:^(NSError* error) {
    EXPECT_TRUE(error == nil)
        << SysNSStringToUTF8([error localizedDescription]);
  }];
  EXPECT_EQ(store.credentials.count, 1u);

  // Start migration again.
  base::test::TestFuture<BOOL, NSError*> update_future;
  auto* update_future_ptr = &update_future;

  migrator = [[CredentialProviderMigrator alloc]
      initWithUserDefaults:user_defaults_
                       key:store_key_
                      gaia:kMatchingGaia
             passwordStore:mock_store_
              passkeyStore:&test_passkey_model_];

  [migrator startMigrationWithCompletion:^(BOOL success, NSError* error) {
    update_future_ptr->SetValue(success, error);
  }];
  auto [update_success, update_error] = update_future.Take();
  EXPECT_TRUE(update_success);
  EXPECT_TRUE(update_error == nil)
      << SysNSStringToUTF8([update_error localizedDescription]);

  // Verify temporal store is empty again.
  store =
      [[UserDefaultsCredentialStore alloc] initWithUserDefaults:user_defaults_
                                                            key:store_key_];
  EXPECT_EQ(store.credentials.count, 0u);

  // Verify there is still 1 passkey, but with an updated username and the same
  // user display name.
  passkeys =
      test_passkey_model_.GetPasskeys(AnyRp(), ShadowedCredentials::kInclude);
  EXPECT_THAT(passkeys, SizeIs(1));
  EXPECT_EQ(passkeys[0].user_name(), "newUsername");
  EXPECT_EQ(passkeys[0].user_display_name(), "userDisplayName");
}

// Tests that credentials with a mismatching GAIA ID are ignored.
TEST_F(CredentialProviderMigratorTest, MigrationFiltersByGaiaID) {
  // Create temp store and add 2 credentials: 1 matching, 1 mismatching.
  UserDefaultsCredentialStore* store =
      [[UserDefaultsCredentialStore alloc] initWithUserDefaults:user_defaults_
                                                            key:store_key_];
  id<Credential> matchingCredential = TestPasswordCredential(kMatchingGaia);
  // Use unique identifiers for the mismatching credential.
  id<Credential> mismatchingCredential = [[ArchivableCredential alloc]
               initWithFavicon:nil
                          gaia:kMismatchingGaia
                      password:@"password"
                          rank:1
              recordIdentifier:@"recordIdentifier2"
             serviceIdentifier:@"http://www.beta.example.com"
                   serviceName:nil
      registryControlledDomain:nil
                      username:@"username2"
                          note:nil
                  lastUsedTime:0];
  [store addCredential:matchingCredential];
  [store addCredential:mismatchingCredential];
  [store saveDataWithCompletion:^(NSError* error) {
    EXPECT_TRUE(error == nil);
  }];
  EXPECT_EQ(store.credentials.count, 2u);

  // Create the migrator with the matching GAIA ID.
  CredentialProviderMigrator* migrator =
      [[CredentialProviderMigrator alloc] initWithUserDefaults:user_defaults_
                                                           key:store_key_
                                                          gaia:kMatchingGaia
                                                 passwordStore:mock_store_
                                                  passkeyStore:nil];

  // Start migration. Only the matching one should be migrated.
  PasswordForm expected = PasswordFormFromCredential(matchingCredential);
  EXPECT_CALL(*mock_store_, AddLogin(EqStoredCredential(expected), _));
  base::test::TestFuture<BOOL, NSError*> future;
  auto* future_ptr = &future;
  [migrator startMigrationWithCompletion:^(BOOL success, NSError* error) {
    future_ptr->SetValue(success, error);
  }];
  auto [migration_success, migration_error] = future.Take();
  EXPECT_TRUE(migration_success);

  // Reload temp store.
  store =
      [[UserDefaultsCredentialStore alloc] initWithUserDefaults:user_defaults_
                                                            key:store_key_];
  // Verify only 1 credential remains (the mismatching one).
  EXPECT_EQ(store.credentials.count, 1u);
  EXPECT_NSEQ(store.credentials[0].gaia, kMismatchingGaia);
}

// Tests that credentials with a nil GAIA ID are migrated if the migrator's
// GAIA ID is also nil.
TEST_F(CredentialProviderMigratorTest, MigrationAllowsNilGaiaID) {
  UserDefaultsCredentialStore* store =
      [[UserDefaultsCredentialStore alloc] initWithUserDefaults:user_defaults_
                                                            key:store_key_];
  id<Credential> credential = TestPasswordCredential(nil);
  [store addCredential:credential];
  [store saveDataWithCompletion:^(NSError* error) {
    EXPECT_TRUE(error == nil);
  }];
  EXPECT_EQ(store.credentials.count, 1u);

  CredentialProviderMigrator* migrator =
      [[CredentialProviderMigrator alloc] initWithUserDefaults:user_defaults_
                                                           key:store_key_
                                                          gaia:nil
                                                 passwordStore:mock_store_
                                                  passkeyStore:nil];

  PasswordForm expected = PasswordFormFromCredential(credential);
  EXPECT_CALL(*mock_store_, AddLogin(EqStoredCredential(expected), _));
  base::test::TestFuture<BOOL, NSError*> future;
  auto* future_ptr = &future;
  [migrator startMigrationWithCompletion:^(BOOL success, NSError* error) {
    future_ptr->SetValue(success, error);
  }];
  auto [migration_success, migration_error] = future.Take();
  EXPECT_TRUE(migration_success);

  store =
      [[UserDefaultsCredentialStore alloc] initWithUserDefaults:user_defaults_
                                                            key:store_key_];
  EXPECT_EQ(store.credentials.count, 0u);
}

}  // namespace
