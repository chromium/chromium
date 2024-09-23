// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/common/credential_provider/ASPasskeyCredentialIdentity+credential.h"
#import "ios/chrome/common/credential_provider/credential.h"

@implementation ASPasskeyCredentialIdentity (Credential)

- (instancetype)cr_initWithCredential:(id<Credential>)credential {
  return [self initWithRelyingPartyIdentifier:credential.rpId
                                     userName:credential.username
                                 credentialID:credential.credentialId
                                   userHandle:credential.userId
                             recordIdentifier:credential.recordIdentifier];
}

@end
