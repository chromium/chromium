// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/credential_provider/model/credential_provider_migrator.h"

#import "base/functional/callback_helpers.h"
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
#import "ios/chrome/common/credential_provider/archivable_credential_test_util.h"
#import "ios/chrome/common/credential_provider/user_defaults_credential_store.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "url/gurl.h"

namespace {

NSString* const kMismatchingGaia = @"654321";

using ::base::SysNSStringToUTF8;
using ::password_manager::MockPasswordStoreInterface;
using ::password_manager::PasswordForm;
using ::testing::_;
using ::testing::IsEmpty;
using ::testing::SizeIs;
using AnyRp = ::webauthn::PasskeyModel::AnyRp;
using ShadowedCredentials = ::webauthn::PasskeyModel::ShadowedCredentials;

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
  base::test::TestFuture<CredentialMigrationResult> future;
  [migrator startMigrationWithCompletion:future.GetCallback()];
  CredentialMigrationResult result = future.Take();
  ASSERT_TRUE(result.has_value())
      << SysNSStringToUTF8(result.error().localizedDescription);
  EXPECT_THAT(*result, IsEmpty());

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
  id<Credential> credential = TestPasskeyCredential();
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
                      gaia:kDefaultGaia
             passwordStore:mock_store_
              passkeyStore:&test_passkey_model_];
  ASSERT_TRUE(migrator);

  histogram_tester_.ExpectBucketCount(
      "Passkeys.IOSMigration", PasskeysMigrationStatus::kPasskeyCreated, 0);

  // Start migration.
  sync_pb::WebauthnCredentialSpecifics expected =
      PasskeyFromCredential(credential);
  base::test::TestFuture<CredentialMigrationResult> future;
  [migrator startMigrationWithCompletion:future.GetCallback()];
  CredentialMigrationResult result = future.Take();
  ASSERT_TRUE(result.has_value())
      << SysNSStringToUTF8(result.error().localizedDescription);
  ASSERT_THAT(*result, SizeIs(1));
  EXPECT_EQ((*result)[0].sync_id(), expected.sync_id());
  EXPECT_EQ((*result)[0].credential_id(), expected.credential_id());
  EXPECT_EQ((*result)[0].rp_id(), expected.rp_id());
  EXPECT_EQ((*result)[0].user_name(), expected.user_name());

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
  credential.lastUsedTime = kDefaultLastUsedTime + 10;
  [store updateCredential:credential];
  [store saveDataWithCompletion:^(NSError* error) {
    EXPECT_TRUE(error == nil)
        << SysNSStringToUTF8([error localizedDescription]);
  }];
  EXPECT_EQ(store.credentials.count, 1u);

  histogram_tester_.ExpectBucketCount(
      "Passkeys.IOSMigration", PasskeysMigrationStatus::kPasskeyUpdated, 0);

  base::test::TestFuture<CredentialMigrationResult> update_future;

  migrator = [[CredentialProviderMigrator alloc]
      initWithUserDefaults:user_defaults_
                       key:store_key_
                      gaia:kDefaultGaia
             passwordStore:mock_store_
              passkeyStore:&test_passkey_model_];

  [migrator startMigrationWithCompletion:update_future.GetCallback()];
  CredentialMigrationResult update_result = update_future.Take();
  ASSERT_TRUE(update_result.has_value())
      << SysNSStringToUTF8(update_result.error().localizedDescription);
  EXPECT_THAT(*update_result, IsEmpty());

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
      TestPasskeyCredential(TestPasskeyCredentialOverrides{.rp_id = nil});

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
                      gaia:kDefaultGaia
             passwordStore:mock_store_
              passkeyStore:&test_passkey_model_];
  ASSERT_TRUE(migrator);

  histogram_tester_.ExpectBucketCount(
      "Passkeys.IOSMigration", PasskeysMigrationStatus::kInvalidPasskey, 0);

  // Start migration.
  base::test::TestFuture<CredentialMigrationResult> future;
  [migrator startMigrationWithCompletion:future.GetCallback()];
  CredentialMigrationResult result = future.Take();
  ASSERT_TRUE(result.has_value())
      << SysNSStringToUTF8(result.error().localizedDescription);
  EXPECT_THAT(*result, IsEmpty());

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
  ArchivableCredential* credential = TestPasskeyCredential();
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
                      gaia:kDefaultGaia
             passwordStore:mock_store_
              passkeyStore:&test_passkey_model_];
  ASSERT_TRUE(migrator);

  histogram_tester_.ExpectBucketCount(
      "Passkeys.IOSMigration.SignalAction",
      PasskeysSignalMigrationAction::kPasskeyHidden, 0);

  // Start initial migration.
  base::test::TestFuture<CredentialMigrationResult> future;
  [migrator startMigrationWithCompletion:future.GetCallback()];
  CredentialMigrationResult result = future.Take();
  ASSERT_TRUE(result.has_value())
      << SysNSStringToUTF8(result.error().localizedDescription);
  EXPECT_THAT(*result, SizeIs(1));

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
  credential.hiddenTime = kDefaultLastUsedTime;
  [store addCredential:credential];
  [store saveDataWithCompletion:^(NSError* error) {
    EXPECT_TRUE(error == nil)
        << SysNSStringToUTF8([error localizedDescription]);
  }];
  EXPECT_EQ(store.credentials.count, 1u);

  // Start migration again.
  base::test::TestFuture<CredentialMigrationResult> update_future;

  migrator = [[CredentialProviderMigrator alloc]
      initWithUserDefaults:user_defaults_
                       key:store_key_
                      gaia:kDefaultGaia
             passwordStore:mock_store_
              passkeyStore:&test_passkey_model_];

  [migrator startMigrationWithCompletion:update_future.GetCallback()];
  CredentialMigrationResult update_result = update_future.Take();
  ASSERT_TRUE(update_result.has_value())
      << SysNSStringToUTF8(update_result.error().localizedDescription);
  EXPECT_THAT(*update_result, IsEmpty());

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
  EXPECT_EQ(passkeys[0].hidden_time(), kDefaultLastUsedTime);

  histogram_tester_.ExpectBucketCount(
      "Passkeys.IOSMigration.SignalAction",
      PasskeysSignalMigrationAction::kPasskeyHidden, 1);
}

// Tests that migrating a non-hidden credential restores an existing hidden
// passkey in the model.
TEST_F(CredentialProviderMigratorTest, PasskeyMigrationRestoresHiddenPasskey) {
  ArchivableCredential* credential = TestPasskeyCredential();
  credential.hidden = YES;
  sync_pb::WebauthnCredentialSpecifics passkey =
      PasskeyFromCredential(credential);
  test_passkey_model_.AddNewPasskeyForTesting(std::move(passkey));

  // Verify the passkey is initially hidden.
  std::vector<sync_pb::WebauthnCredentialSpecifics> passkeys =
      test_passkey_model_.GetPasskeys(AnyRp(), ShadowedCredentials::kInclude);
  ASSERT_THAT(passkeys, SizeIs(1));
  ASSERT_TRUE(passkeys[0].hidden());

  // Add the credential to the store with `hidden: NO`.
  UserDefaultsCredentialStore* store =
      [[UserDefaultsCredentialStore alloc] initWithUserDefaults:user_defaults_
                                                            key:store_key_];
  credential.hidden = NO;
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
                      gaia:kDefaultGaia
             passwordStore:mock_store_
              passkeyStore:&test_passkey_model_];
  ASSERT_TRUE(migrator);

  histogram_tester_.ExpectBucketCount(
      "Passkeys.IOSMigration.SignalAction",
      PasskeysSignalMigrationAction::kPasskeyRestored, 0);

  // Start migration.
  base::test::TestFuture<CredentialMigrationResult> future;
  [migrator startMigrationWithCompletion:future.GetCallback()];
  CredentialMigrationResult result = future.Take();
  ASSERT_TRUE(result.has_value())
      << SysNSStringToUTF8(result.error().localizedDescription);
  EXPECT_THAT(*result, IsEmpty());

  // Verify temporal store is empty.
  store =
      [[UserDefaultsCredentialStore alloc] initWithUserDefaults:user_defaults_
                                                            key:store_key_];
  EXPECT_EQ(store.credentials.count, 0u);

  // Verify passkey is restored (not hidden).
  passkeys =
      test_passkey_model_.GetPasskeys(AnyRp(), ShadowedCredentials::kInclude);
  EXPECT_THAT(passkeys, SizeIs(1));
  EXPECT_FALSE(passkeys[0].hidden());

  histogram_tester_.ExpectBucketCount(
      "Passkeys.IOSMigration.SignalAction",
      PasskeysSignalMigrationAction::kPasskeyRestored, 1);
}

TEST_F(CredentialProviderMigratorTest, PasskeyMigrationUpdatesUsername) {
  UserDefaultsCredentialStore* store =
      [[UserDefaultsCredentialStore alloc] initWithUserDefaults:user_defaults_
                                                            key:store_key_];
  ArchivableCredential* credential = TestPasskeyCredential();
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
                      gaia:kDefaultGaia
             passwordStore:mock_store_
              passkeyStore:&test_passkey_model_];
  ASSERT_TRUE(migrator);

  histogram_tester_.ExpectBucketCount(
      "Passkeys.IOSMigration.SignalAction",
      PasskeysSignalMigrationAction::kPasskeyDetailsUpdated, 0);

  base::test::TestFuture<CredentialMigrationResult> future;
  [migrator startMigrationWithCompletion:future.GetCallback()];
  CredentialMigrationResult result = future.Take();
  ASSERT_TRUE(result.has_value())
      << SysNSStringToUTF8(result.error().localizedDescription);
  EXPECT_THAT(*result, SizeIs(1));

  // Verify the passkey was migrated and has the initial names.
  std::vector<sync_pb::WebauthnCredentialSpecifics> passkeys =
      test_passkey_model_.GetPasskeys(AnyRp(), ShadowedCredentials::kInclude);
  EXPECT_THAT(passkeys, SizeIs(1));
  EXPECT_EQ(passkeys[0].user_name(), SysNSStringToUTF8(kDefaultUsername));
  EXPECT_EQ(passkeys[0].user_display_name(),
            SysNSStringToUTF8(kDefaultUserDisplayName));

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
  base::test::TestFuture<CredentialMigrationResult> update_future;

  migrator = [[CredentialProviderMigrator alloc]
      initWithUserDefaults:user_defaults_
                       key:store_key_
                      gaia:kDefaultGaia
             passwordStore:mock_store_
              passkeyStore:&test_passkey_model_];

  [migrator startMigrationWithCompletion:update_future.GetCallback()];
  CredentialMigrationResult update_result = update_future.Take();
  ASSERT_TRUE(update_result.has_value())
      << SysNSStringToUTF8(update_result.error().localizedDescription);
  EXPECT_THAT(*update_result, IsEmpty());

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
  EXPECT_EQ(passkeys[0].user_display_name(),
            SysNSStringToUTF8(kDefaultUserDisplayName));

  histogram_tester_.ExpectBucketCount(
      "Passkeys.IOSMigration.SignalAction",
      PasskeysSignalMigrationAction::kPasskeyDetailsUpdated, 1);
}

// Tests that credentials with a mismatching GAIA ID are ignored.
TEST_F(CredentialProviderMigratorTest, MigrationFiltersByGaiaID) {
  // Create temp store and add 2 credentials: 1 matching, 1 mismatching.
  UserDefaultsCredentialStore* store =
      [[UserDefaultsCredentialStore alloc] initWithUserDefaults:user_defaults_
                                                            key:store_key_];
  id<Credential> matchingCredential = TestPasswordCredential(kDefaultGaia);
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
                                                          gaia:kDefaultGaia
                                                 passwordStore:mock_store_
                                                  passkeyStore:nil];

  // Start migration. Only the matching one should be migrated.
  PasswordForm expected = PasswordFormFromCredential(matchingCredential);
  EXPECT_CALL(*mock_store_, AddLogin(EqStoredCredential(expected), _));
  base::test::TestFuture<CredentialMigrationResult> future;
  [migrator startMigrationWithCompletion:future.GetCallback()];
  CredentialMigrationResult result = future.Take();
  ASSERT_TRUE(result.has_value());
  EXPECT_THAT(*result, IsEmpty());

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
  base::test::TestFuture<CredentialMigrationResult> future;
  [migrator startMigrationWithCompletion:future.GetCallback()];
  CredentialMigrationResult result = future.Take();
  ASSERT_TRUE(result.has_value());
  EXPECT_THAT(*result, IsEmpty());

  store =
      [[UserDefaultsCredentialStore alloc] initWithUserDefaults:user_defaults_
                                                            key:store_key_];
  EXPECT_EQ(store.credentials.count, 0u);
}

// Tests that starting a second migration while one is already in progress
// returns `kCredentialProviderMigratorErrorAlreadyRunning`.
TEST_F(CredentialProviderMigratorTest, AlreadyRunningReturnsError) {
  CredentialProviderMigrator* migrator =
      [[CredentialProviderMigrator alloc] initWithUserDefaults:user_defaults_
                                                           key:store_key_
                                                          gaia:nil
                                                 passwordStore:mock_store_
                                                  passkeyStore:nil];

  base::test::TestFuture<CredentialMigrationResult> first_future;
  [migrator startMigrationWithCompletion:first_future.GetCallback()];

  base::test::TestFuture<CredentialMigrationResult> second_future;
  [migrator startMigrationWithCompletion:second_future.GetCallback()];
  CredentialMigrationResult second_result = second_future.Take();
  ASSERT_FALSE(second_result.has_value());
  EXPECT_NSEQ(second_result.error().domain,
              kCredentialProviderMigratorErrorDomain);
  EXPECT_EQ(second_result.error().code,
            kCredentialProviderMigratorErrorAlreadyRunning);

  CredentialMigrationResult first_result = first_future.Take();
  ASSERT_TRUE(first_result.has_value());
  EXPECT_THAT(*first_result, IsEmpty());
}

// Tests that migrating a mixed batch containing newly created passkeys, an
// update to an existing passkey, an invalid passkey, and a password returns
// only the newly created valid passkeys in `CredentialMigrationResult`.
TEST_F(CredentialProviderMigratorTest,
       MixedBatchReturnsOnlyNewlyCreatedPasskeys) {
  ArchivableCredential* existing_passkey = TestPasskeyCredential({
      .record_identifier = @"existing_record",
      .sync_id = TestDataFromString("syncIdExisting16"),
      .username = @"existing_user",
      .credential_id = TestDataFromString("credIdExisting16"),
  });
  test_passkey_model_.AddNewPasskeyForTesting(
      PasskeyFromCredential(existing_passkey));
  existing_passkey.lastUsedTime = kDefaultLastUsedTime + 50;

  ArchivableCredential* created_passkey_1 = TestPasskeyCredential({
      .record_identifier = @"created_record_1",
      .sync_id = TestDataFromString("syncIdCreated116"),
      .username = @"created_user_1",
      .credential_id = TestDataFromString("credIdCreated116"),
  });
  ArchivableCredential* created_passkey_2 = TestPasskeyCredential({
      .record_identifier = @"created_record_2",
      .sync_id = TestDataFromString("syncIdCreated216"),
      .username = @"created_user_2",
      .credential_id = TestDataFromString("credIdCreated216"),
  });
  ArchivableCredential* invalid_passkey = TestPasskeyCredential({
      .record_identifier = @"invalid_record",
      .sync_id = TestDataFromString("syncIdInvalid016"),
      .credential_id = TestDataFromString("credIdInvalid016"),
      .rp_id = nil,
  });
  ArchivableCredential* password_credential =
      TestPasswordCredential(kDefaultGaia);

  UserDefaultsCredentialStore* store =
      [[UserDefaultsCredentialStore alloc] initWithUserDefaults:user_defaults_
                                                            key:store_key_];
  [store addCredential:existing_passkey];
  [store addCredential:created_passkey_1];
  [store addCredential:invalid_passkey];
  [store addCredential:password_credential];
  [store addCredential:created_passkey_2];
  base::test::TestFuture<NSError*> save_future;
  [store
      saveDataWithCompletion:base::CallbackToBlock(save_future.GetCallback())];
  ASSERT_FALSE(save_future.Get());

  PasswordForm expected_password =
      PasswordFormFromCredential(password_credential);
  EXPECT_CALL(*mock_store_, AddLogin(EqStoredCredential(expected_password), _));

  CredentialProviderMigrator* migrator = [[CredentialProviderMigrator alloc]
      initWithUserDefaults:user_defaults_
                       key:store_key_
                      gaia:kDefaultGaia
             passwordStore:mock_store_
              passkeyStore:&test_passkey_model_];
  base::test::TestFuture<CredentialMigrationResult> migration_future;
  [migrator startMigrationWithCompletion:migration_future.GetCallback()];
  CredentialMigrationResult result = migration_future.Take();
  ASSERT_TRUE(result.has_value())
      << SysNSStringToUTF8(result.error().localizedDescription);
  ASSERT_THAT(*result, SizeIs(2));
  EXPECT_EQ("created_user_1", (*result)[0].user_name());
  EXPECT_EQ("created_user_2", (*result)[1].user_name());
}

}  // namespace
