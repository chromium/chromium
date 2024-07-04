// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/strings/string_number_conversions.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/common/credential_provider/ASPasskeyCredentialIdentity+credential.h"
#import "ios/chrome/common/credential_provider/ASPasswordCredentialIdentity+credential.h"
#import "ios/chrome/common/credential_provider/archivable_credential.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

using base::HexEncode;
using base::SysUTF8ToNSString;

namespace {

constexpr int64_t kJan1st2024 = 1704085200;

using ASPasskeyCredentialIdentity_CredentialTest = PlatformTest;
using ASPasswordCredentialIdentity_CredentialTest = PlatformTest;

// Tests that ASPasswordCredentialIdentity can be created from Credential.
TEST_F(ASPasswordCredentialIdentity_CredentialTest, create) {
  ArchivableCredential* credential =
      [[ArchivableCredential alloc] initWithFavicon:@"favicon"
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
        recordIdentifier:@"recordIdentifier"
                  syncId:SysUTF8ToNSString(HexEncode("syncId"))
                username:@"username"
         userDisplayName:@"userDisplayName"
                  userId:SysUTF8ToNSString(HexEncode("userId"))
            credentialId:SysUTF8ToNSString(HexEncode("credentialId"))
                    rpId:@"rpId"
              privateKey:SysUTF8ToNSString(HexEncode("privateKey"))
               encrypted:SysUTF8ToNSString(HexEncode("encrypted"))
            creationTime:kJan1st2024];
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
