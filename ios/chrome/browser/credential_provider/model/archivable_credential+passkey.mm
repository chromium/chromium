// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/strings/string_number_conversions.h"
#import "base/strings/sys_string_conversions.h"
#import "components/sync/protocol/webauthn_credential_specifics.pb.h"
#import "ios/chrome/browser/credential_provider/model/archivable_credential+passkey.h"
#import "ios/chrome/browser/credential_provider/model/credential_provider_util.h"

using base::HexEncode;
using base::SysUTF8ToNSString;

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
