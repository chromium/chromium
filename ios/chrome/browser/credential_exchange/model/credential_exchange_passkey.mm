// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/credential_exchange/model/credential_exchange_passkey.h"

@implementation CredentialExchangePasskey

- (instancetype)initWithCredentialId:(NSData*)credentialId
                                rpId:(NSString*)rpId
                            userName:(NSString*)userName
                     userDisplayName:(NSString*)userDisplayName
                              userId:(NSData*)userId
                          privateKey:(NSData*)privateKey {
  self = [super init];
  if (self) {
    _credentialId = credentialId;
    _rpId = rpId;
    _userName = userName;
    _userDisplayName = userDisplayName;
    _userId = userId;
    _privateKey = privateKey;
  }
  return self;
}

@end
