// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/common/credential_provider/multi_store_credential_store.h"

#import "base/test/ios/wait_util.h"
#import "ios/chrome/common/credential_provider/archivable_credential.h"
#import "ios/chrome/common/credential_provider/memory_credential_store.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

namespace {

using base::test::ios::WaitUntilConditionOrTimeout;
using base::test::ios::kWaitForFileOperationTimeout;

using MultiStoreCredentialStoreTest = PlatformTest;

ArchivableCredential* TestCredential(NSString* user) {
  return [[ArchivableCredential alloc] initWithFavicon:@"favicon"
                                                  gaia:nil
                                              password:@"qwerty123"
                                                  rank:5
                                      recordIdentifier:@"recordIdentifier"
                                     serviceIdentifier:@"serviceIdentifier"
                                           serviceName:@"serviceName"
                                              username:user
                                                  note:@"note"];
}

NSArray<id<CredentialStore>>* TestStoreArray() {
  MemoryCredentialStore* store1 = [[MemoryCredentialStore alloc] init];
  [store1 addCredential:TestCredential(@"store1user")];
  MemoryCredentialStore* store2 = [[MemoryCredentialStore alloc] init];
  [store2 addCredential:TestCredential(@"store2user")];
  return @[ store1, store2 ];
}

// Tests that an MultiStoreCredentialStore can be created.
TEST_F(MultiStoreCredentialStoreTest, Create) {
  MultiStoreCredentialStore* credentialStore =
      [[MultiStoreCredentialStore alloc] initWithStores:TestStoreArray()];
  EXPECT_TRUE(credentialStore);
  EXPECT_TRUE(credentialStore.credentials);
}

// Tests that MultiStoreCredentialStore combines data from stores.
TEST_F(MultiStoreCredentialStoreTest, CombineData) {
  MultiStoreCredentialStore* credentialStore =
      [[MultiStoreCredentialStore alloc] initWithStores:TestStoreArray()];
  EXPECT_EQ(2u, credentialStore.credentials.count);

  id<Credential> firstCredential =
      TestStoreArray().firstObject.credentials.firstObject;

  EXPECT_NSEQ(credentialStore.credentials[0], firstCredential);
  EXPECT_NSEQ(credentialStore.credentials[0].username, @"store1user");
}

// Tests that MultiStoreCredentialStore don't duplicate data from stores.
TEST_F(MultiStoreCredentialStoreTest, RetrieveCredential) {
  MultiStoreCredentialStore* credentialStore =
      [[MultiStoreCredentialStore alloc] initWithStores:TestStoreArray()];
  id<Credential> firstCredential =
      TestStoreArray().firstObject.credentials.firstObject;
  id<Credential> retrievedCredential = [credentialStore
      credentialWithRecordIdentifier:firstCredential.recordIdentifier];
  EXPECT_NSEQ(retrievedCredential, firstCredential);
  EXPECT_NSEQ(retrievedCredential.username, @"store1user");
}

}
