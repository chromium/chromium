// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/atmemory/ui/at_memory_granular_fill_item.h"

#import "base/apple/foundation_util.h"

@interface AtMemoryGranularFillItem ()

// Read-write redeclaration of properties.
@property(nonatomic, copy, readwrite) NSString* attributeName;
@property(nonatomic, copy, readwrite) NSString* attributeValue;
@property(nonatomic, assign, readwrite) NSInteger index;

@end

@implementation AtMemoryGranularFillItem

- (instancetype)initWithAttributeName:(NSString*)attributeName
                       attributeValue:(NSString*)attributeValue
                                index:(NSInteger)index {
  self = [super init];
  if (self) {
    _attributeName = [attributeName copy];
    _attributeValue = [attributeValue copy];
    _index = index;
  }
  return self;
}

#pragma mark - NSObject

- (BOOL)isEqual:(id)object {
  if (self == object) {
    return YES;
  }
  if (![object isKindOfClass:[AtMemoryGranularFillItem class]]) {
    return NO;
  }
  AtMemoryGranularFillItem* other =
      base::apple::ObjCCastStrict<AtMemoryGranularFillItem>(object);
  return [self.attributeName isEqualToString:other.attributeName] &&
         [self.attributeValue isEqualToString:other.attributeValue] &&
         self.index == other.index;
}

- (NSUInteger)hash {
  return [self.attributeName hash] ^ [self.attributeValue hash] ^ self.index;
}

@end
