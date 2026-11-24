// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/public/cwv_trusted_vault_key.h"

#import "base/check.h"

@implementation CWVTrustedVaultKey

@synthesize keyMaterial = _keyMaterial;
@synthesize version = _version;

- (instancetype)initWithKeyMaterial:(NSData*)keyMaterial
                            version:(NSInteger)version {
  self = [super init];
  if (self) {
    CHECK(keyMaterial);
    _keyMaterial = [keyMaterial copy];
    _version = version;
  }
  return self;
}

#pragma mark - NSObject

- (BOOL)isEqual:(id)other {
  if (self == other) {
    return YES;
  }
  if (![other isKindOfClass:[CWVTrustedVaultKey class]]) {
    return NO;
  }
  CWVTrustedVaultKey* otherKey = (CWVTrustedVaultKey*)other;
  return self.version == otherKey.version &&
         [self.keyMaterial isEqualToData:otherKey.keyMaterial];
}

- (NSUInteger)hash {
  return self.keyMaterial.hash ^ static_cast<NSUInteger>(self.version);
}

- (NSString*)debugDescription {
  return [NSString
      stringWithFormat:@"%@ version: %ld, keyMaterialLength: %lu",
                       super.debugDescription, static_cast<long>(self.version),
                       static_cast<unsigned long>(self.keyMaterial.length)];
}

@end
