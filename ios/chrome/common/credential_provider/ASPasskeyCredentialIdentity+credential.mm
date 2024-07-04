// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/strings/string_number_conversions.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/common/credential_provider/ASPasskeyCredentialIdentity+credential.h"
#import "ios/chrome/common/credential_provider/credential.h"

namespace {

NSData* HexStringToData(NSString* str) {
  std::string decoded;
  if (!base::HexStringToString(base::SysNSStringToUTF8(str), &decoded)) {
    return nil;
  }

  return [NSData dataWithBytes:decoded.data() length:decoded.length()];
}

}  // namespace

@implementation ASPasskeyCredentialIdentity (Credential)

- (instancetype)cr_initWithCredential:(id<Credential>)credential {
  return [self
      initWithRelyingPartyIdentifier:credential.rpId
                            userName:credential.username
                        credentialID:HexStringToData(credential.credentialId)
                          userHandle:HexStringToData(credential.userId)
                    recordIdentifier:credential.recordIdentifier];
}

@end
