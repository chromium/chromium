// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/public/provider/chrome/browser/signin/fake_chrome_identity.h"

#include "base/mac/foundation_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
NSString* const kCoderUserEmailKey = @"UserEmail";
NSString* const kCoderGaiaIDKey = @"GaiaID";
NSString* const kCoderUserFullNameKey = @"UserFullName";
NSString* const kCoderHashedGaiaIDKey = @"HashedGaiaID";
}  // namespace

@implementation FakeChromeIdentity {
  NSString* _userEmail;
  NSString* _gaiaID;
  NSString* _userFullName;
  NSString* _hashedGaiaID;
}

+ (FakeChromeIdentity*)identityWithEmail:(NSString*)email
                                  gaiaID:(NSString*)gaiaID
                                    name:(NSString*)name {
  return [[FakeChromeIdentity alloc] initWithEmail:email
                                            gaiaID:gaiaID
                                              name:name];
}

- (instancetype)initWithEmail:(NSString*)email
                       gaiaID:(NSString*)gaiaID
                         name:(NSString*)name {
  self = [super init];
  if (self) {
    _userEmail = [email copy];
    _gaiaID = [gaiaID copy];
    _userFullName = [name copy];
    _hashedGaiaID = [NSString stringWithFormat:@"%@_hashID", name];
  }
  return self;
}

- (NSString*)userEmail {
  return _userEmail;
}

- (NSString*)gaiaID {
  return _gaiaID;
}

- (NSString*)userFullName {
  return _userFullName;
}

- (NSString*)hashedGaiaID {
  return _hashedGaiaID;
}

// Overrides the method to return YES so that the object will be passed by value
// in EDO. This requires the object confirm to NSSecureCoding protocol.
- (BOOL)edo_isEDOValueType {
  return YES;
}

// Overrides |isEqual| and |hash| methods to compare objects by values. This is
// useful when the object is passed by value between processes in EG2.
- (BOOL)isEqual:(id)object {
  if (self == object) {
    return YES;
  }
  if ([object isKindOfClass:self.class]) {
    FakeChromeIdentity* other =
        base::mac::ObjCCastStrict<FakeChromeIdentity>(object);
    return [_userEmail isEqualToString:other.userEmail] &&
           [_gaiaID isEqualToString:other.gaiaID] &&
           [_userFullName isEqualToString:other.userFullName] &&
           [_hashedGaiaID isEqualToString:other.hashedGaiaID];
  }
  return NO;
}

- (NSUInteger)hash {
  return _gaiaID.hash;
}

#pragma mark - NSSecureCoding

- (void)encodeWithCoder:(NSCoder*)coder {
  [coder encodeObject:_userEmail forKey:kCoderUserEmailKey];
  [coder encodeObject:_gaiaID forKey:kCoderGaiaIDKey];
  [coder encodeObject:_userFullName forKey:kCoderUserFullNameKey];
  [coder encodeObject:_hashedGaiaID forKey:kCoderHashedGaiaIDKey];
}

- (id)initWithCoder:(NSCoder*)coder {
  if ((self = [super init])) {
    _userEmail = [coder decodeObjectOfClass:[NSString class]
                                     forKey:kCoderUserEmailKey];
    _gaiaID = [coder decodeObjectOfClass:[NSString class]
                                  forKey:kCoderGaiaIDKey];
    _userFullName = [coder decodeObjectOfClass:[NSString class]
                                        forKey:kCoderUserFullNameKey];
    _hashedGaiaID = [coder decodeObjectOfClass:[NSString class]
                                        forKey:kCoderHashedGaiaIDKey];
  }
  return self;
}

+ (BOOL)supportsSecureCoding {
  return YES;
}

@end
