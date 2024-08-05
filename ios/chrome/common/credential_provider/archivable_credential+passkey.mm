// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/common/credential_provider/archivable_credential+passkey.h"

#import "base/strings/string_number_conversions.h"
#import "base/strings/sys_string_conversions.h"
#import "components/sync/protocol/webauthn_credential_specifics.pb.h"

using base::HexEncode;
using base::SysNSStringToUTF8;
using base::SysUTF8ToNSString;

namespace {

std::string DataToString(NSData* data) {
  return std::string(static_cast<const char*>(data.bytes), data.length);
}

NSData* StringToData(std::string str) {
  return [NSData dataWithBytes:str.data() length:str.length()];
}

}  // namespace

NSString* RecordIdentifierForPasskey(
    const sync_pb::WebauthnCredentialSpecifics& passkey) {
  // These are the UNIQUE keys in the login database.
  return [NSString
      stringWithFormat:@"%@|%@", SysUTF8ToNSString(passkey.rp_id()),
                       SysUTF8ToNSString(
                           HexEncode(passkey.credential_id()))];
}

sync_pb::WebauthnCredentialSpecifics PasskeyFromCredential(
    id<Credential> credential) {
  sync_pb::WebauthnCredentialSpecifics credential_specifics;

  credential_specifics.set_sync_id(DataToString(credential.syncId));
  credential_specifics.set_credential_id(DataToString(credential.credentialId));
  credential_specifics.set_user_name(SysNSStringToUTF8(credential.username));
  credential_specifics.set_user_display_name(
      SysNSStringToUTF8(credential.userDisplayName));
  credential_specifics.set_rp_id(SysNSStringToUTF8(credential.rpId));
  credential_specifics.set_user_id(DataToString(credential.userId));
  credential_specifics.set_creation_time(credential.creationTime);
  credential_specifics.set_last_used_time_windows_epoch_micros(
      credential.lastUsedTime);

  if ([credential.privateKey length] > 0) {
    credential_specifics.set_private_key(DataToString(credential.privateKey));
  } else {
    credential_specifics.set_encrypted(DataToString(credential.encrypted));
  }

  return credential_specifics;
}

@implementation ArchivableCredential (WebauthnCredentialSpecifics)

// Convenience initializer from a WebauthnCredentialSpecifics.
- (instancetype)initWithFavicon:(NSString*)favicon
                           gaia:(NSString*)gaia
                        passkey:(const sync_pb::WebauthnCredentialSpecifics&)
                                    passkey {
  // Any passkey member which contains a string of bytes with potentially non
  // ASCII characters is hex encoded first before being transformed into an
  // NSString object.
  return [self initWithFavicon:favicon
                          gaia:gaia
              recordIdentifier:RecordIdentifierForPasskey(passkey)
                        syncId:StringToData(passkey.sync_id())
                      username:SysUTF8ToNSString(passkey.user_name())
               userDisplayName:SysUTF8ToNSString(passkey.user_display_name())
                        userId:StringToData(passkey.user_id())
                  credentialId:StringToData(passkey.credential_id())
                          rpId:SysUTF8ToNSString(passkey.rp_id())
                    privateKey:StringToData(passkey.private_key())
                     encrypted:StringToData(passkey.encrypted())
                  creationTime:passkey.creation_time()
                  lastUsedTime:passkey.last_used_time_windows_epoch_micros()];
}

@end
