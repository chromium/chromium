// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/common/credential_provider/ASPasskeyCredentialIdentity+credential.h"
#import "ios/chrome/common/credential_provider/ASPasswordCredentialIdentity+credential.h"
#import "ios/chrome/common/credential_provider/archivable_credential.h"
#import "ios/chrome/common/credential_provider/archivable_credential_test_util.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

namespace {

using ASPasskeyCredentialIdentity_CredentialTest = PlatformTest;
using ASPasswordCredentialIdentity_CredentialTest = PlatformTest;

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
                           registryControlledDomain:@"example.com"
                                           username:@"user"
                                               note:@"note"
                                       lastUsedTime:0];
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
  ArchivableCredential* credential = TestPasskeyCredential();
  ASPasskeyCredentialIdentity* credentialIdentity =
      [[ASPasskeyCredentialIdentity alloc] cr_initWithCredential:credential];

  EXPECT_NSEQ(credential.username, credentialIdentity.userName);
  EXPECT_NSEQ(credential.userId, credentialIdentity.userHandle);
  EXPECT_NSEQ(credential.recordIdentifier, credentialIdentity.recordIdentifier);
  EXPECT_NSEQ(credential.rpId, credentialIdentity.relyingPartyIdentifier);
  EXPECT_NSEQ(credential.credentialId, credentialIdentity.credentialID);
}

}  // namespace
