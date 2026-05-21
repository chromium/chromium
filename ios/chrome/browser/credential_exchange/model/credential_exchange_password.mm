// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/credential_exchange/model/credential_exchange_password.h"

#import "base/apple/foundation_util.h"

@implementation CredentialExchangePassword

- (instancetype)initWithURL:(NSURL*)URL
                   username:(NSString*)username
                   password:(NSString*)password
                       note:(NSString*)note {
  self = [super init];
  if (self) {
    _URL = URL;
    _username = username;
    _password = password;
    _note = note;
  }
  return self;
}

- (BOOL)isEqual:(id)object {
  if (self == object) {
    return YES;
  }
  CredentialExchangePassword* other =
      base::apple::ObjCCast<CredentialExchangePassword>(object);
  return other && [self.username isEqualToString:other.username] &&
         [self.password isEqualToString:other.password] &&
         [self.note isEqualToString:other.note] &&
         [self.URL.absoluteString isEqualToString:other.URL.absoluteString];
}

- (NSUInteger)hash {
  return self.username.hash ^ self.password.hash ^ self.note.hash ^
         self.URL.hash;
}

@end
