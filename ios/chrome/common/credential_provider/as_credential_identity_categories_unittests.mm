// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/common/credential_provider/ASPasskeyCredentialIdentity+credential.h"
#import "ios/chrome/common/credential_provider/ASPasswordCredentialIdentity+credential.h"
#import "ios/chrome/common/credential_provider/archivable_credential.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

namespace {

constexpr int64_t kJan1st2024 = 1704085200;

using ASPasskeyCredentialIdentity_CredentialTest = PlatformTest;
using ASPasswordCredentialIdentity_CredentialTest = PlatformTest;

NSData* StringToData(std::string str) {
  return [NSData dataWithBytes:str.data() length:str.length()];
}

// Tests that ASPasswordCredentialIdentity can be created from Credential.
TEST_F(ASPasswordCredentialIdentity_CredentialTest, create) {
  ArchivableCredential* credential =
      [[ArchivableCredential alloc] initWithFavicon:@"favicon"
                                               gaia:nil
                                           password:@"qwerty!"
                                               rank:5
                                   recordIdentifier:@"recordIdentifier"
                                  serviceIdentifier:@"serviceIdentifier"
                                        serviceName:@"serviceName"
                                           username:@"user"
                                               note:@"note"];
  ASPasswordCredentialIdentity* credentialIdentity =
      [[ASPasswordCredentialIdentity alloc] cr_initWithCredential:credential];

  EXPECT_NSEQ(credential.username, credentialIdentity.user);
  EXPECT_NSEQ(credential.recordIdentifier, credentialIdentity.recordIdentifier);
  EXPECT_EQ(ASCredentialServiceIdentifierTypeURL,
            credentialIdentity.serviceIdentifier.type);
  EXPECT_NSEQ(credential.serviceIdentifier,
              credentialIdentity.serviceIdentifier.identifier);
}

// Tests that ASPasskeyCredentialIdentity can be created from Credential.
TEST_F(ASPasskeyCredentialIdentity_CredentialTest, create) {
  if (@available(iOS 17, *)) {
    ArchivableCredential* credential = [[ArchivableCredential alloc]
         initWithFavicon:@"favicon"
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
    ASPasskeyCredentialIdentity* credentialIdentity =
        [[ASPasskeyCredentialIdentity alloc] cr_initWithCredential:credential];

    EXPECT_NSEQ(credential.username, credentialIdentity.userName);
    EXPECT_NSEQ([@"userId" dataUsingEncoding:NSUTF8StringEncoding],
                credentialIdentity.userHandle);
    EXPECT_NSEQ(credential.recordIdentifier,
                credentialIdentity.recordIdentifier);
    EXPECT_NSEQ(credential.rpId, credentialIdentity.relyingPartyIdentifier);
    EXPECT_NSEQ([@"credentialId" dataUsingEncoding:NSUTF8StringEncoding],
                credentialIdentity.credentialID);
  }
}

}  // namespace
