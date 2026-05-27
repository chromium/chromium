// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/credential_exchange/model/credential_exchange_passkey.h"

#import "base/apple/foundation_util.h"

@implementation CredentialExchangePasskey

- (instancetype)initWithCredentialId:(NSData*)credentialId
                                rpId:(NSString*)rpId
                            userName:(NSString*)userName
                     userDisplayName:(NSString*)userDisplayName
                              userId:(NSData*)userId
                          privateKey:(NSData*)privateKey
                        creationDate:(NSDate*)creationDate {
  self = [super init];
  if (self) {
    _credentialId = credentialId;
    _rpId = rpId;
    _userName = userName;
    _userDisplayName = userDisplayName;
    _userId = userId;
    _privateKey = privateKey;
    _creationDate = creationDate;
  }
  return self;
}

- (BOOL)isEqual:(id)object {
  if (self == object) {
    return YES;
  }
  CredentialExchangePasskey* other =
      base::apple::ObjCCast<CredentialExchangePasskey>(object);
  return other && [self.userName isEqualToString:other.userName] &&
         (self.userDisplayName == other.userDisplayName ||
          [self.userDisplayName isEqualToString:other.userDisplayName]) &&
         [self.rpId isEqualToString:other.rpId] &&
         [self.credentialId isEqualToData:other.credentialId] &&
         [self.userId isEqualToData:other.userId] &&
         [self.privateKey isEqualToData:other.privateKey] &&
         (self.creationDate == other.creationDate ||
          [self.creationDate isEqual:other.creationDate]);
}

- (NSUInteger)hash {
  return self.userName.hash ^ self.userDisplayName.hash ^ self.rpId.hash ^
         self.credentialId.hash ^ self.userId.hash ^ self.privateKey.hash ^
         self.creationDate.hash;
}

@end
