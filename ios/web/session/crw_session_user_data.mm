// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/session/crw_session_user_data.h"

#import "base/apple/foundation_util.h"

@implementation CRWSessionUserData {
  NSMutableDictionary<NSString*, id<NSCoding>>* _data;
}

#pragma mark - Public methods

- (void)setObject:(id<NSCoding>)object forKey:(NSString*)key {
  [_data setObject:object forKey:key];
}

- (id<NSCoding>)objectForKey:(NSString*)key {
  return [_data objectForKey:key];
}

- (void)removeObjectForKey:(NSString*)key {
  [_data removeObjectForKey:key];
}

#pragma mark - NSCoding

- (instancetype)init {
  if ((self = [super init])) {
    _data = [[NSMutableDictionary alloc] init];
  }
  return self;
}

- (instancetype)initWithCoder:(NSCoder*)decoder {
  if ((self = [super init])) {
    _data = [[decoder decodeObject] mutableCopy];
    if (!_data) {
      _data = [[NSMutableDictionary alloc] init];
    }
  }
  return self;
}

- (void)encodeWithCoder:(NSCoder*)coder {
  [coder encodeObject:[_data copy]];
}

#pragma mark - NSObject

- (BOOL)isEqual:(id)object {
  if (![object isKindOfClass:[self class]])
    return NO;

  CRWSessionUserData* other =
      base::apple::ObjCCastStrict<CRWSessionUserData>(object);

  return [_data isEqual:other->_data];
}

@end
