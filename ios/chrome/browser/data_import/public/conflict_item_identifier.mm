// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/data_import/public/conflict_item_identifier.h"

#import "base/apple/foundation_util.h"

@implementation ConflictItemIdentifier

- (instancetype)initWithType:(CredentialConflictType)type
                       index:(NSInteger)index {
  self = [super init];
  if (self) {
    _type = type;
    _index = index;
  }
  return self;
}

#pragma mark - NSObject

- (BOOL)isEqual:(id)object {
  if (self == object) {
    return YES;
  }
  ConflictItemIdentifier* other =
      base::apple::ObjCCast<ConflictItemIdentifier>(object);
  return other && self.type == other.type && self.index == other.index;
}

- (NSUInteger)hash {
  return (static_cast<NSUInteger>(self.type) << 31) + self.index;
}

- (NSString*)description {
  return [NSString stringWithFormat:@"<%@: %p, type: %ld, index: %lu>",
                                    [self class], self, (long)self.type,
                                    (unsigned long)self.index];
}

@end
