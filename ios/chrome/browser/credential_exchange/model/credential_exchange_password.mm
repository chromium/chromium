// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/credential_exchange/model/credential_exchange_password.h"

#import "base/apple/foundation_util.h"

@implementation CredentialExchangePassword

- (instancetype)initWithURL:(NSURL*)URL
                   username:(NSString*)username
                   password:(NSString*)password
                       note:(NSString*)note
               creationDate:(NSDate*)creationDate {
  self = [super init];
  if (self) {
    _URL = URL;
    _username = username;
    _password = password;
    _note = note;
    _creationDate = creationDate;
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
         (self.URL == other.URL ||
          [self.URL.absoluteString isEqualToString:other.URL.absoluteString]) &&
         (self.creationDate == other.creationDate ||
          [self.creationDate isEqual:other.creationDate]);
}

- (NSUInteger)hash {
  return self.username.hash ^ self.password.hash ^ self.note.hash ^
         self.URL.hash ^ self.creationDate.hash;
}

@end
