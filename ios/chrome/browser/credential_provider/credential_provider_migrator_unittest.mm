// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/credential_provider/credential_provider_migrator.h"

#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "base/test/task_environment.h"
#import "components/password_manager/core/browser/mock_password_store_interface.h"
#import "components/password_manager/core/browser/password_form.h"
#import "ios/chrome/browser/credential_provider/archivable_credential+password_form.h"
#import "ios/chrome/common/credential_provider/archivable_credential.h"
#import "ios/chrome/common/credential_provider/user_defaults_credential_store.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

using base::test::ios::kWaitForFileOperationTimeout;
using base::test::ios::WaitUntilConditionOrTimeout;
using password_manager::MockPasswordStoreInterface;
using password_manager::PasswordForm;
using ::testing::_;

ArchivableCredential* TestCredential() {
  NSString* username = @"username_value";
  NSString* keychainIdentifier = @"keychain_identifier_value";
  NSString* url = @"http://www.alpha.example.com/path/and?args=8";
  NSString* recordIdentifier = @"recordIdentifier";
  return [[ArchivableCredential alloc] initWithFavicon:nil
                                    keychainIdentifier:keychainIdentifier
                                                  rank:1
                                      recordIdentifier:recordIdentifier
                                     serviceIdentifier:url
                                           serviceName:nil
                                                  user:username
                                  validationIdentifier:nil
                                                  note:nil];
}

class CredentialProviderMigratorTest : public PlatformTest {
 protected:
  void SetUp() override {
    [user_defaults_ removeObjectForKey:store_key_];
  }
  void TearDown() override {
    [user_defaults_ removeObjectForKey:store_key_];
  }

  NSUserDefaults* user_defaults_ = [NSUserDefaults standardUserDefaults];
  NSString* store_key_ = @"store_key";
  scoped_refptr<MockPasswordStoreInterface> mock_store_ =
      base::MakeRefCounted<testing::NiceMock<MockPasswordStoreInterface>>();

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;
};

// Tests basic migration for 1 credential.
TEST_F(CredentialProviderMigratorTest, Migration) {
  // Create temp store and add 1 credential.
  UserDefaultsCredentialStore* store =
      [[UserDefaultsCredentialStore alloc] initWithUserDefaults:user_defaults_
                                                            key:store_key_];
  id<Credential> credential = TestCredential();
  [store addCredential:credential];
  [store saveDataWithCompletion:^(NSError* error) {
    EXPECT_TRUE(error == nil);
  }];
  EXPECT_EQ(store.credentials.count, 1u);

  // Create the migrator to be tested.
  CredentialProviderMigrator* migrator =
      [[CredentialProviderMigrator alloc] initWithUserDefaults:user_defaults_
                                                           key:store_key_
                                                 passwordStore:mock_store_];
  EXPECT_TRUE(migrator);

  // Start migration.
  PasswordForm expected = PasswordFormFromCredential(credential);
  EXPECT_CALL(*mock_store_, AddLogin(expected, _));
  __block BOOL blockWaitCompleted = false;
  [migrator startMigrationWithCompletion:^(BOOL success, NSError* error) {
    EXPECT_TRUE(success);
    EXPECT_FALSE(error);
    blockWaitCompleted = true;
  }];
  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForFileOperationTimeout, ^bool {
    return blockWaitCompleted;
  }));

  // Reload temporal store.
  store =
      [[UserDefaultsCredentialStore alloc] initWithUserDefaults:user_defaults_
                                                            key:store_key_];
  // Verify credentials are empty
  EXPECT_EQ(store.credentials.count, 0u);
}

}  // namespace
