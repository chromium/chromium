// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/common/credential_provider/mock_credential_store.h"

#import "ios/chrome/common/credential_provider/credential.h"

@implementation MockCredentialStore

@synthesize credentials = _credentials;

- (instancetype)initWithCredentials:(NSArray<id<Credential>>*)credentials {
  self = [super init];
  if (self) {
    _credentials = credentials;
  }
  return self;
}

// All the stored credentials.
- (NSArray<id<Credential>>*)credentials {
  return _credentials;
}

// Returns a credential with matching `recordIdentifier` or nil if none.
- (id<Credential>)credentialWithRecordIdentifier:(NSString*)recordIdentifier {
  NSArray<id<Credential>>* matchingCredentials = [self.credentials
      filteredArrayUsingPredicate:[NSPredicate predicateWithBlock:^BOOL(
                                                   id<Credential> credential,
                                                   NSDictionary* bindings) {
        return [credential.recordIdentifier isEqualToString:recordIdentifier];
      }]];
  return matchingCredentials.count != 0 ? matchingCredentials[0] : nil;
}

@end
