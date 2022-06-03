// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/common/credential_provider/as_password_credential_identity+credential.h"

#import "base/test/ios/wait_util.h"
#import "ios/chrome/common/credential_provider/archivable_credential.h"
#include "testing/gtest_mac.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

using ASPasswordCredentialIdentity_CredentialTest = PlatformTest;

// Tests that ASPasswordCredentialIdentity can be created from Credential.
TEST_F(ASPasswordCredentialIdentity_CredentialTest, create) {
  ArchivableCredential* credential =
      [[ArchivableCredential alloc] initWithFavicon:@"favicon"
                                 keychainIdentifier:@"keychainIdentifier"
                                               rank:5
                                   recordIdentifier:@"recordIdentifier"
                                  serviceIdentifier:@"serviceIdentifier"
                                        serviceName:@"serviceName"
                                               user:@"user"
                               validationIdentifier:@"validationIdentifier"];
  ASPasswordCredentialIdentity* credentialIdentity =
      [[ASPasswordCredentialIdentity alloc] initWithCredential:credential];

  EXPECT_NSEQ(credential.user, credentialIdentity.user);
  EXPECT_NSEQ(credential.recordIdentifier, credentialIdentity.recordIdentifier);
  EXPECT_EQ(ASCredentialServiceIdentifierTypeURL,
            credentialIdentity.serviceIdentifier.type);
  EXPECT_NSEQ(credential.serviceIdentifier,
              credentialIdentity.serviceIdentifier.identifier);
}
}  // namespace
