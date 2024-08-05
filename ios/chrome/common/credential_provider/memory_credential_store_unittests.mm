// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/common/credential_provider/memory_credential_store.h"

#import "ios/chrome/common/credential_provider/archivable_credential.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

namespace {

using MemoryCredentialStoreTest = PlatformTest;

ArchivableCredential* TestCredential() {
  return [[ArchivableCredential alloc] initWithFavicon:@"favicon"
                                                  gaia:nil
                                              password:@"qwerty!"
                                                  rank:5
                                      recordIdentifier:@"recordIdentifier"
                                     serviceIdentifier:@"serviceIdentifier"
                                           serviceName:@"serviceName"
                                              username:@"user"
                                                  note:@"note"];
}

// Tests that an MemoryCredentialStore can be created.
TEST_F(MemoryCredentialStoreTest, create) {
  MemoryCredentialStore* credentialStore = [[MemoryCredentialStore alloc] init];
  EXPECT_TRUE(credentialStore);
  EXPECT_TRUE(credentialStore.credentials);
}

// Tests that an MemoryCredentialStore can add a credential.
TEST_F(MemoryCredentialStoreTest, add) {
  MemoryCredentialStore* credentialStore = [[MemoryCredentialStore alloc] init];
  EXPECT_TRUE(credentialStore);
  [credentialStore addCredential:TestCredential()];
  EXPECT_EQ(1u, credentialStore.credentials.count);
}

// Tests that an MemoryCredentialStore can update a credential.
TEST_F(MemoryCredentialStoreTest, update) {
  MemoryCredentialStore* credentialStore = [[MemoryCredentialStore alloc] init];
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

// Tests that an MemoryCredentialStore can remove a credential.
TEST_F(MemoryCredentialStoreTest, remove) {
  MemoryCredentialStore* credentialStore = [[MemoryCredentialStore alloc] init];
  EXPECT_TRUE(credentialStore);
  ArchivableCredential* credential = TestCredential();
  [credentialStore addCredential:credential];
  EXPECT_EQ(1u, credentialStore.credentials.count);

  [credentialStore
      removeCredentialWithRecordIdentifier:credential.recordIdentifier];
  EXPECT_EQ(0u, credentialStore.credentials.count);
}

}
