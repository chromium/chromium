// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/common/credential_provider/archivable_credential.h"

#import "base/test/ios/wait_util.h"
#include "testing/gtest_mac.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

using ArchivableCredentialTest = PlatformTest;

ArchivableCredential* TestCredential() {
  return [[ArchivableCredential alloc] initWithFavicon:@"favicon"
                                    keychainIdentifier:@"keychainIdentifier"
                                                  rank:5
                                      recordIdentifier:@"recordIdentifier"
                                     serviceIdentifier:@"serviceIdentifier"
                                           serviceName:@"serviceName"
                                                  user:@"user"
                                  validationIdentifier:@"validationIdentifier"];
}

// Tests that an ArchivableCredential can be created.
TEST_F(ArchivableCredentialTest, create) {
  ArchivableCredential* credential =
      [[ArchivableCredential alloc] initWithFavicon:@"favicon"
                                 keychainIdentifier:@"keychainIdentifier"
                                               rank:5
                                   recordIdentifier:@"recordIdentifier"
                                  serviceIdentifier:@"serviceIdentifier"
                                        serviceName:@"serviceName"
                                               user:@"user"
                               validationIdentifier:@"validationIdentifier"];
  EXPECT_TRUE(credential);
}

// Tests that an ArchivableCredential can be converted to NSData.
TEST_F(ArchivableCredentialTest, createData) {
  ArchivableCredential* credential = TestCredential();
  EXPECT_TRUE(credential);
  NSError* error = nil;
  NSData* data = [NSKeyedArchiver archivedDataWithRootObject:credential
                                       requiringSecureCoding:YES
                                                       error:&error];
  EXPECT_TRUE(data);
  EXPECT_FALSE(error);
}

// Tests that an ArchivableCredential can be retrieved from NSData.
TEST_F(ArchivableCredentialTest, retrieveData) {
  ArchivableCredential* credential = TestCredential();
  NSError* error = nil;
  NSData* data = [NSKeyedArchiver archivedDataWithRootObject:credential
                                       requiringSecureCoding:YES
                                                       error:&error];
  EXPECT_TRUE(data);
  EXPECT_FALSE(error);

  ArchivableCredential* unarchivedCredential =
      [NSKeyedUnarchiver unarchivedObjectOfClass:[ArchivableCredential class]
                                        fromData:data
                                           error:&error];
  EXPECT_TRUE(unarchivedCredential);
  EXPECT_TRUE(
      [unarchivedCredential isKindOfClass:[ArchivableCredential class]]);

  EXPECT_NSEQ(credential.favicon, unarchivedCredential.favicon);
  EXPECT_NSEQ(credential.keychainIdentifier,
              unarchivedCredential.keychainIdentifier);
  EXPECT_EQ(credential.rank, unarchivedCredential.rank);
  EXPECT_NSEQ(credential.recordIdentifier,
              unarchivedCredential.recordIdentifier);
  EXPECT_NSEQ(credential.serviceIdentifier,
              unarchivedCredential.serviceIdentifier);
  EXPECT_NSEQ(credential.serviceName, unarchivedCredential.serviceName);
  EXPECT_NSEQ(credential.user, unarchivedCredential.user);
  EXPECT_NSEQ(credential.validationIdentifier,
              unarchivedCredential.validationIdentifier);
}

// Tests ArchivableCredential equality.
TEST_F(ArchivableCredentialTest, equality) {
  ArchivableCredential* credential = TestCredential();
  ArchivableCredential* credentialIdentical = TestCredential();
  EXPECT_NSEQ(credential, credentialIdentical);
  EXPECT_EQ(credential.hash, credentialIdentical.hash);

  ArchivableCredential* credentialSameIdentifier = [[ArchivableCredential alloc]
           initWithFavicon:@"other_favicon"
        keychainIdentifier:@"other_keychainIdentifier"
                      rank:credential.rank + 10
          recordIdentifier:@"recordIdentifier"
         serviceIdentifier:@"other_serviceIdentifier"
               serviceName:@"other_serviceName"
                      user:@"other_user"
      validationIdentifier:@"other_validationIdentifier"];
  EXPECT_NSNE(credential, credentialSameIdentifier);

  ArchivableCredential* credentialDiferentIdentifier =
      [[ArchivableCredential alloc] initWithFavicon:@"favicon"
                                 keychainIdentifier:@"keychainIdentifier"
                                               rank:credential.rank
                                   recordIdentifier:@"other_recordIdentifier"
                                  serviceIdentifier:@"serviceIdentifier"
                                        serviceName:@"serviceName"
                                               user:@"user"
                               validationIdentifier:@"validationIdentifier"];
  EXPECT_NSNE(credential, credentialDiferentIdentifier);

  EXPECT_NSNE(credential, nil);
}
}
