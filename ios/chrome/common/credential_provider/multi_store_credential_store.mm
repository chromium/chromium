// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/common/credential_provider/multi_store_credential_store.h"

#import "base/check.h"
#import "base/notreached.h"
#import "ios/chrome/common/credential_provider/credential.h"

@interface MultiStoreCredentialStore ()

@property(nonatomic, strong) NSArray<id<CredentialStore>>* stores;

@end

@implementation MultiStoreCredentialStore

- (instancetype)initWithStores:(NSArray<id<CredentialStore>>*)stores {
  DCHECK(stores);
  self = [super init];
  if (self) {
    _stores = stores;
  }
  return self;
}

#pragma mark - CredentialStore

- (NSArray<id<Credential>>*)credentials {
  NSMutableSet<id<Credential>>* uniqueCredentials = [[NSMutableSet alloc] init];
  for (id<CredentialStore> store in self.stores) {
    for (id<Credential> credential in store.credentials) {
      [uniqueCredentials addObject:credential];
    }
  }
  return uniqueCredentials.allObjects;
}

- (id<Credential>)credentialWithRecordIdentifier:(NSString*)recordIdentifier {
  DCHECK(recordIdentifier.length);
  for (id<CredentialStore> store in self.stores) {
    id<Credential> credential =
        [store credentialWithRecordIdentifier:recordIdentifier];
    if (credential) {
      return credential;
    }
  }
  return nil;
}

@end
