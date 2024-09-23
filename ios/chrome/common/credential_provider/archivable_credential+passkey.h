// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_COMMON_CREDENTIAL_PROVIDER_ARCHIVABLE_CREDENTIAL_PASSKEY_H_
#define IOS_CHROME_COMMON_CREDENTIAL_PROVIDER_ARCHIVABLE_CREDENTIAL_PASSKEY_H_

#import "ios/chrome/common/credential_provider/archivable_credential.h"

namespace sync_pb {
class WebauthnCredentialSpecifics;
}  // namespace sync_pb

// Returns the equivalent of a unique record identifier. Built from the unique
// columns in the logins database.
NSString* RecordIdentifierForPasskey(
    const sync_pb::WebauthnCredentialSpecifics& passkey);

// Convenience method to create a WebauthnCredentialSpecifics from a Credential.
sync_pb::WebauthnCredentialSpecifics PasskeyFromCredential(
    id<Credential> credential);

// Category for adding convenience logic related to WebauthnCredentialSpecifics.
@interface ArchivableCredential (Passkey)

// Convenience initializer from a WebauthnCredentialSpecifics.
- (instancetype)initWithFavicon:(NSString*)favicon
                           gaia:(NSString*)gaia
                        passkey:(const sync_pb::WebauthnCredentialSpecifics&)
                                    passkey;

@end

#endif  // IOS_CHROME_COMMON_CREDENTIAL_PROVIDER_ARCHIVABLE_CREDENTIAL_PASSKEY_H_
