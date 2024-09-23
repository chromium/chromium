// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/common/credential_provider/archivable_credential_store.h"

#import "base/test/ios/wait_util.h"
#import "ios/chrome/common/credential_provider/archivable_credential.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

namespace {

using base::test::ios::WaitUntilConditionOrTimeout;
using base::test::ios::kWaitForFileOperationTimeout;

NSURL* testStorageFileURL() {
  NSURL* temporaryDirectory = [NSURL fileURLWithPath:NSTemporaryDirectory()];
  NSURL* URL = [temporaryDirectory URLByAppendingPathComponent:@"credentials"];
  return URL;
}

class ArchivableCredentialStoreTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();
    [[NSFileManager defaultManager] removeItemAtURL:testStorageFileURL()
                                              error:nil];
  }
  void TearDown() override {
    PlatformTest::TearDown();
    [[NSFileManager defaultManager] removeItemAtURL:testStorageFileURL()
                                              error:nil];
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

// Tests that an ArchivableCredentialStore can be created.
TEST_F(ArchivableCredentialStoreTest, create) {
  ArchivableCredentialStore* credentialStore =
      [[ArchivableCredentialStore alloc] initWithFileURL:testStorageFileURL()];
  EXPECT_TRUE(credentialStore);
  EXPECT_TRUE(credentialStore.credentials);
}

// Tests that an ArchivableCredentialStore can add a credential.
TEST_F(ArchivableCredentialStoreTest, add) {
  ArchivableCredentialStore* credentialStore =
      [[ArchivableCredentialStore alloc] initWithFileURL:testStorageFileURL()];
  EXPECT_TRUE(credentialStore);
  [credentialStore addCredential:TestCredential()];
  EXPECT_EQ(1u, credentialStore.credentials.count);
}

// Tests that an ArchivableCredentialStore can update a credential.
TEST_F(ArchivableCredentialStoreTest, update) {
  ArchivableCredentialStore* credentialStore =
      [[ArchivableCredentialStore alloc] initWithFileURL:testStorageFileURL()];
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

// Tests that an ArchivableCredentialStore can remove a credential.
TEST_F(ArchivableCredentialStoreTest, remove) {
  ArchivableCredentialStore* credentialStore =
      [[ArchivableCredentialStore alloc] initWithFileURL:testStorageFileURL()];
  EXPECT_TRUE(credentialStore);
  ArchivableCredential* credential = TestCredential();
  [credentialStore addCredential:credential];
  EXPECT_EQ(1u, credentialStore.credentials.count);

  [credentialStore
      removeCredentialWithRecordIdentifier:credential.recordIdentifier];
  EXPECT_EQ(0u, credentialStore.credentials.count);
}

// Tests that ArchivableCredentialStore can save and retrieve from URLs.
TEST_F(ArchivableCredentialStoreTest, persist) {
  ArchivableCredentialStore* credentialStore =
      [[ArchivableCredentialStore alloc] initWithFileURL:testStorageFileURL()];
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

  ArchivableCredentialStore* freshCredentialStore =
      [[ArchivableCredentialStore alloc] initWithFileURL:testStorageFileURL()];
  EXPECT_TRUE(freshCredentialStore);
  EXPECT_TRUE(freshCredentialStore.credentials);
  EXPECT_EQ(1u, freshCredentialStore.credentials.count);
  EXPECT_TRUE(
      [credential isEqual:freshCredentialStore.credentials.firstObject]);
}

// Tests that ArchivableCredentialStore can save in a folder that doesn't exist.
TEST_F(ArchivableCredentialStoreTest, createFolder) {
  NSURL* deepFolderURL = [testStorageFileURL()
      URLByAppendingPathComponent:@"a/deep/path/component"];
  ArchivableCredentialStore* credentialStore =
      [[ArchivableCredentialStore alloc] initWithFileURL:deepFolderURL];
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
  NSError* error = nil;
  [deepFolderURL checkResourceIsReachableAndReturnError:&error];
  EXPECT_FALSE(error);
}
}
