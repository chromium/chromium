// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/common/credential_provider/as_password_credential_identity+credential.h"

#import "ios/chrome/common/credential_provider/credential.h"

@implementation ASPasswordCredentialIdentity (Credential)

- (instancetype)initWithCredential:(id<Credential>)credential {
  ASCredentialServiceIdentifier* serviceIdentifier =
      [[ASCredentialServiceIdentifier alloc]
          initWithIdentifier:credential.serviceIdentifier
                        type:ASCredentialServiceIdentifierTypeURL];
  return [self initWithServiceIdentifier:serviceIdentifier
                                    user:credential.user
                        recordIdentifier:credential.recordIdentifier];
}

@end
