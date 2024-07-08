// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/strings/string_number_conversions.h"
#import "base/strings/sys_string_conversions.h"
#import "components/sync/protocol/webauthn_credential_specifics.pb.h"
#import "ios/chrome/browser/credential_provider/model/archivable_credential+passkey.h"
#import "ios/chrome/browser/credential_provider/model/credential_provider_util.h"

using base::HexEncode;
using base::HexStringToString;
using base::SysNSStringToUTF8;
using base::SysUTF8ToNSString;

namespace {

std::string HexNSStringToString(NSString* str) {
  std::string decoded;
  if (!HexStringToString(SysNSStringToUTF8(str), &decoded)) {
    return nil;
  }

  return decoded;
}

}  // namespace

sync_pb::WebauthnCredentialSpecifics PasskeyFromCredential(
    id<Credential> credential) {
  sync_pb::WebauthnCredentialSpecifics credential_specifics;

  credential_specifics.set_sync_id(HexNSStringToString(credential.syncId));
  credential_specifics.set_credential_id(
      HexNSStringToString(credential.credentialId));
  credential_specifics.set_user_name(SysNSStringToUTF8(credential.username));
  credential_specifics.set_user_display_name(
      SysNSStringToUTF8(credential.userDisplayName));
  credential_specifics.set_rp_id(SysNSStringToUTF8(credential.rpId));
  credential_specifics.set_user_id(HexNSStringToString(credential.userId));
  credential_specifics.set_creation_time(credential.creationTime);

  if ([credential.privateKey length] > 0) {
    credential_specifics.set_private_key(
        HexNSStringToString(credential.privateKey));
  } else {
    credential_specifics.set_encrypted(
        HexNSStringToString(credential.encrypted));
  }

  return credential_specifics;
}

@implementation ArchivableCredential (WebauthnCredentialSpecifics)

// Convenience initializer from a WebauthnCredentialSpecifics.
- (instancetype)initWithFavicon:(NSString*)favicon
                        passkey:(const sync_pb::WebauthnCredentialSpecifics&)
                                    passkey {
  // Any passkey member which contains a string of bytes with potentially non
  // ASCII characters is hex encoded first before being transformed into an
  // NSString object.
  return [self
       initWithFavicon:favicon
      recordIdentifier:RecordIdentifierForPasskey(passkey)
                syncId:SysUTF8ToNSString(HexEncode(passkey.sync_id()))
              username:SysUTF8ToNSString(passkey.user_name())
       userDisplayName:SysUTF8ToNSString(passkey.user_display_name())
                userId:SysUTF8ToNSString(HexEncode(passkey.user_id()))
          credentialId:SysUTF8ToNSString(HexEncode(passkey.credential_id()))
                  rpId:SysUTF8ToNSString(passkey.rp_id())
            privateKey:SysUTF8ToNSString(HexEncode(passkey.private_key()))
             encrypted:SysUTF8ToNSString(HexEncode(passkey.encrypted()))
          creationTime:passkey.creation_time()];
}

@end
