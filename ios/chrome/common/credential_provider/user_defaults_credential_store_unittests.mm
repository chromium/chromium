// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/common/credential_provider/user_defaults_credential_store.h"

#import "base/test/ios/wait_util.h"
#import "ios/chrome/common/credential_provider/archivable_credential.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

namespace {

using base::test::ios::WaitUntilConditionOrTimeout;
using base::test::ios::kWaitForFileOperationTimeout;

NSUserDefaults* TestUserDefaults() {
  return [NSUserDefaults standardUserDefaults];
}

static NSString* kTestUserDefaultsKey = @"UserDefaultsCredentialStoreTestKey";

class UserDefaultsCredentialStoreTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();
    [TestUserDefaults() removeObjectForKey:kTestUserDefaultsKey];
  }
  void TearDown() override {
    PlatformTest::TearDown();
    [TestUserDefaults() removeObjectForKey:kTestUserDefaultsKey];
  }
};

ArchivableCredential* TestCredential() {
  return [[ArchivableCredential alloc] initWithFavicon:@"favicon"
                                                  gaia:nil
                                              password:@"qwerty123"
                                                  rank:5
                                      recordIdentifier:@"recordIdentifier"
                                     serviceIdentifier:@"serviceIdentifier"
                                           serviceName:@"serviceName"
                                              username:@"user"
                                                  note:@"note"];
}

// Tests that an UserDefaultsCredentialStore can be created.
TEST_F(UserDefaultsCredentialStoreTest, create) {
  UserDefaultsCredentialStore* credentialStore =
      [[UserDefaultsCredentialStore alloc]
          initWithUserDefaults:TestUserDefaults()
                           key:kTestUserDefaultsKey];
  EXPECT_TRUE(credentialStore);
  EXPECT_TRUE(credentialStore.credentials);
}

// Tests that an UserDefaultsCredentialStore can add a credential.
TEST_F(UserDefaultsCredentialStoreTest, add) {
  UserDefaultsCredentialStore* credentialStore =
      [[UserDefaultsCredentialStore alloc]
          initWithUserDefaults:TestUserDefaults()
                           key:kTestUserDefaultsKey];
  EXPECT_TRUE(credentialStore);
  [credentialStore addCredential:TestCredential()];
  EXPECT_EQ(1u, credentialStore.credentials.count);
}

// Tests that an UserDefaultsCredentialStore can update a credential.
TEST_F(UserDefaultsCredentialStoreTest, update) {
  UserDefaultsCredentialStore* credentialStore =
      [[UserDefaultsCredentialStore alloc]
          initWithUserDefaults:TestUserDefaults()
                           key:kTestUserDefaultsKey];
  EXPECT_TRUE(credentialStore);
  ArchivableCredential* credential = TestCredential();
  [credentialStore addCredential:credential];
  EXPECT_EQ(1u, credentialStore.credentials.count);

  ArchivableCredential* updatedCredential =
      [[ArchivableCredential alloc] initWithFavicon:@"other_favicon"
                                               gaia:nil
                                           password:@"Qwerty123!"
                                               rank:credential.rank + 10
                                   recordIdentifier:@"recordIdentifier"
                                  serviceIdentifier:@"other_serviceIdentifier"
                                        serviceName:@"other_serviceName"
                                           username:@"other_user"
                                               note:@"other_note"];

  [credentialStore updateCredential:updatedCredential];
  EXPECT_EQ(1u, credentialStore.credentials.count);
  EXPECT_EQ(updatedCredential.rank,
            credentialStore.credentials.firstObject.rank);
}

// Tests that an UserDefaultsCredentialStore can remove a credential.
TEST_F(UserDefaultsCredentialStoreTest, remove) {
  UserDefaultsCredentialStore* credentialStore =
      [[UserDefaultsCredentialStore alloc]
          initWithUserDefaults:TestUserDefaults()
                           key:kTestUserDefaultsKey];
  EXPECT_TRUE(credentialStore);
  ArchivableCredential* credential = TestCredential();
  [credentialStore addCredential:credential];
  EXPECT_EQ(1u, credentialStore.credentials.count);

  [credentialStore
      removeCredentialWithRecordIdentifier:credential.recordIdentifier];
  EXPECT_EQ(0u, credentialStore.credentials.count);
}

// Tests that UserDefaultsCredentialStore can save and retrieve data.
TEST_F(UserDefaultsCredentialStoreTest, persist) {
  UserDefaultsCredentialStore* credentialStore =
      [[UserDefaultsCredentialStore alloc]
          initWithUserDefaults:TestUserDefaults()
                           key:kTestUserDefaultsKey];
  EXPECT_TRUE(credentialStore);

  ArchivableCredential* credential = TestCredential();
  [credentialStore addCredential:credential];
  EXPECT_EQ(1u, credentialStore.credentials.count);

  __block BOOL blockWaitCompleted = false;
  [credentialStore saveDataWithCompletion:^(NSError* error) {
    EXPECT_FALSE(error);
    blockWaitCompleted = true;
  }];
  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForFileOperationTimeout, ^bool {
    return blockWaitCompleted;
  }));

  UserDefaultsCredentialStore* freshCredentialStore =
      [[UserDefaultsCredentialStore alloc]
          initWithUserDefaults:TestUserDefaults()
                           key:kTestUserDefaultsKey];
  EXPECT_TRUE(freshCredentialStore);
  EXPECT_TRUE(freshCredentialStore.credentials);
  EXPECT_EQ(1u, freshCredentialStore.credentials.count);
  EXPECT_TRUE(
      [credential isEqual:freshCredentialStore.credentials.firstObject]);
}

}
