// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/credential_exchange/model/credential_exchange_password.h"

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

@end
