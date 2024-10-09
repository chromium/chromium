// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/credential_provider/model/credential_provider_migrator.h"

#import "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "base/test/task_environment.h"
#import "components/password_manager/core/browser/password_form.h"
#import "components/password_manager/core/browser/password_store/mock_password_store_interface.h"
#import "components/webauthn/core/browser/test_passkey_model.h"
#import "ios/chrome/browser/credential_provider/model/archivable_credential+password_form.h"
#import "ios/chrome/common/credential_provider/archivable_credential+passkey.h"
#import "ios/chrome/common/credential_provider/user_defaults_credential_store.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "url/gurl.h"

namespace {

constexpr int64_t kJan1st2024 = 1704085200;

using base::SysNSStringToUTF8;
using base::test::ios::kWaitForFileOperationTimeout;
using base::test::ios::WaitUntilConditionOrTimeout;
using password_manager::MockPasswordStoreInterface;
using password_manager::PasswordForm;
using ::testing::_;

NSData* StringToData(std::string str) {
  return [NSData dataWithBytes:str.data() length:str.length()];
}

ArchivableCredential* TestPasswordCredential() {
  NSString* username = @"username_value";
  NSString* password = @"qwerty123";
  NSString* url = @"http://www.alpha.example.com/path/and?args=8";
  NSString* recordIdentifier = @"recordIdentifier";
  NSString* note = @"note";
  return [[ArchivableCredential alloc] initWithFavicon:nil
                                                  gaia:nil
                                              password:password
                                                  rank:1
                                      recordIdentifier:recordIdentifier
                                     serviceIdentifier:url
                                           serviceName:nil
                                              username:username
                                                  note:note];
}

ArchivableCredential* TestPasskeyCredential() {
  return
      [[ArchivableCredential alloc] initWithFavicon:nil
                                               gaia:nil
                                   recordIdentifier:@"recordIdentifier"
                                             syncId:StringToData("syncId")
                                           username:@"username"
                                    userDisplayName:@"userDisplayName"
                                             userId:StringToData("userId")
                                       credentialId:StringToData("credentialId")
                                               rpId:@"rpId"
                                         privateKey:StringToData("privateKey")
                                          encrypted:StringToData("encrypted")
                                       creationTime:kJan1st2024
                                       lastUsedTime:kJan1st2024];
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

 private:
  // Mocking time is required for password notes since they are created with the
  // creation_date metadata, which is compared in AddLogin() call expectations.
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::IO,
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
                                                 passwordStore:mock_store_
                                                  passkeyStore:nil];
  EXPECT_TRUE(migrator);

  // Start migration.
  PasswordForm expected = PasswordFormFromCredential(credential);
  EXPECT_CALL(*mock_store_, AddLogin(expected, _));
  __block BOOL blockWaitCompleted = false;
  [migrator startMigrationWithCompletion:^(BOOL success, NSError* error) {
    EXPECT_TRUE(success);
    EXPECT_TRUE(error == nil)
        << SysNSStringToUTF8([error localizedDescription]);
    blockWaitCompleted = true;
  }];
  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForFileOperationTimeout, ^bool {
    return blockWaitCompleted;
  }));

  // Reload temp store.
  store =
      [[UserDefaultsCredentialStore alloc] initWithUserDefaults:user_defaults_
                                                            key:store_key_];
  // Verify credentials are empty
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
             passwordStore:mock_store_
              passkeyStore:&test_passkey_model_];
  EXPECT_TRUE(migrator);

  // Start migration.
  sync_pb::WebauthnCredentialSpecifics expected =
      PasskeyFromCredential(credential);
  __block BOOL blockWaitCompleted = false;
  [migrator startMigrationWithCompletion:^(BOOL success, NSError* error) {
    EXPECT_TRUE(success);
    EXPECT_TRUE(error == nil)
        << SysNSStringToUTF8([error localizedDescription]);
    blockWaitCompleted = true;
  }];
  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForFileOperationTimeout, ^bool {
    return blockWaitCompleted;
  }));

  // Reload temp store.
  store =
      [[UserDefaultsCredentialStore alloc] initWithUserDefaults:user_defaults_
                                                            key:store_key_];
  // Verify credentials are empty
  EXPECT_EQ(store.credentials.count, 0u);

  // Verify that the credential is migrated.
  std::vector<sync_pb::WebauthnCredentialSpecifics> passkeys =
      test_passkey_model_.GetAllPasskeys();
  EXPECT_EQ(passkeys.size(), 1u);
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

  blockWaitCompleted = false;
  [migrator startMigrationWithCompletion:^(BOOL success, NSError* error) {
    EXPECT_TRUE(success);
    EXPECT_TRUE(error == nil)
        << SysNSStringToUTF8([error localizedDescription]);
    blockWaitCompleted = true;
  }];
  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForFileOperationTimeout, ^bool {
    return blockWaitCompleted;
  }));

  // Reload temp store.
  store =
      [[UserDefaultsCredentialStore alloc] initWithUserDefaults:user_defaults_
                                                            key:store_key_];
  // Verify credentials are empty
  EXPECT_EQ(store.credentials.count, 0u);

  // Verify that we still have only 1 passkey and that its last used time was
  // updated.
  passkeys = test_passkey_model_.GetAllPasskeys();
  EXPECT_EQ(passkeys.size(), 1u);
  EXPECT_EQ(passkeys[0].last_used_time_windows_epoch_micros(),
            credential.lastUsedTime);
}

}  // namespace
