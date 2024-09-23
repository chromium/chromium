// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/signin/model/fake_system_identity.h"

#import "base/apple/foundation_util.h"
#import "base/strings/sys_string_conversions.h"

namespace {
NSString* const kCoderUserEmailKey = @"UserEmail";
NSString* const kCoderGaiaIDKey = @"GaiaID";
NSString* const kCoderUserFullNameKey = @"UserFullName";
NSString* const kCoderUserGivenNameKey = @"UserGivenName";
}  // namespace

@implementation FakeSystemIdentity {
  NSString* _gaiaID;
}

+ (std::string)encodeIdentitiesToBase64:
    (NSArray<FakeSystemIdentity*>*)identities {
  NSError* error = nil;
  NSData* data = [NSKeyedArchiver archivedDataWithRootObject:identities
                                       requiringSecureCoding:NO
                                                       error:&error];
  DCHECK(!error);
  NSString* string = [data base64EncodedStringWithOptions:
                               NSDataBase64EncodingEndLineWithCarriageReturn];
  return base::SysNSStringToUTF8(string);
}

+ (NSArray<FakeSystemIdentity*>*)identitiesFromBase64String:
    (const std::string&)string {
  NSData* data = [[NSData alloc]
      initWithBase64EncodedString:base::SysUTF8ToNSString(string)
                          options:NSDataBase64DecodingIgnoreUnknownCharacters];
  NSSet* classes =
      [NSSet setWithArray:@[ [NSArray class], [FakeSystemIdentity class] ]];
  NSError* error = nil;
  NSArray* identities = [NSKeyedUnarchiver unarchivedObjectOfClasses:classes
                                                            fromData:data
                                                               error:&error];
  return identities;
}

+ (instancetype)fakeIdentity1 {
  return [FakeSystemIdentity identityWithEmail:@"foo1@gmail.com"];
}

+ (instancetype)fakeIdentity2 {
  return [FakeSystemIdentity identityWithEmail:@"foo2@gmail.com"];
}

+ (instancetype)fakeIdentity3 {
  return [FakeSystemIdentity identityWithEmail:@"foo3@gmail.com"];
}

+ (instancetype)fakeIdentity4 {
  return [FakeSystemIdentity identityWithEmail:@"foo4@gmail.com"];
}

+ (instancetype)fakeManagedIdentity {
  return [FakeSystemIdentity identityWithEmail:@"foo@google.com"];
}

+ (instancetype)identityWithEmail:(NSString*)email {
  // GaiaID cannot look like an email address.
  NSString* withoutAtSign = [email stringByReplacingOccurrencesOfString:@"@"
                                                             withString:@"_"];
  NSString* gaiaID = [NSString stringWithFormat:@"%@_GAIAID", withoutAtSign];
  return [[FakeSystemIdentity alloc] initWithEmail:email gaiaID:gaiaID];
}

+ (instancetype)identityWithEmail:(NSString*)email gaiaID:(NSString*)gaiaID {
  return [[FakeSystemIdentity alloc] initWithEmail:email gaiaID:gaiaID];
}

- (instancetype)initWithEmail:(NSString*)email gaiaID:(NSString*)gaiaID {
  if ((self = [super init])) {
    _gaiaID = gaiaID;
    _userEmail = [email copy];
    NSArray* split = [email componentsSeparatedByString:@"@"];
    DCHECK_EQ(split.count, 2ul);
    _userFullName = split[0];
    _userGivenName = split[0];
  }
  return self;
}

// Overrides the method to return YES so that the object will be passed by value
// in EDO. This requires the object confirm to NSSecureCoding protocol.
- (BOOL)edo_isEDOValueType {
  return YES;
}

// Overrides `isEqual` and `hash` methods to compare objects by values. This is
// useful when the object is passed by value between processes in EG2.
- (BOOL)isEqual:(id)object {
  if (self == object) {
    return YES;
  }

  FakeSystemIdentity* other = base::apple::ObjCCast<FakeSystemIdentity>(object);
  if (!other) {
    return NO;
  }

  return [_userEmail isEqualToString:other.userEmail] &&
         [_gaiaID isEqualToString:other.gaiaID] &&
         [_userFullName isEqualToString:other.userFullName] &&
         [_userGivenName isEqualToString:other.userGivenName];
}

- (NSUInteger)hash {
  return _gaiaID.hash;
}

#pragma mark - Properties

- (NSString*)gaiaID {
  return _gaiaID;
}

- (NSString*)hashedGaiaID {
  return [NSString stringWithFormat:@"%@_hash", _gaiaID];
}

#pragma mark - NSSecureCoding

- (void)encodeWithCoder:(NSCoder*)coder {
  [coder encodeObject:_userEmail forKey:kCoderUserEmailKey];
  [coder encodeObject:_gaiaID forKey:kCoderGaiaIDKey];
  [coder encodeObject:_userFullName forKey:kCoderUserFullNameKey];
  [coder encodeObject:_userGivenName forKey:kCoderUserGivenNameKey];
}

- (id)initWithCoder:(NSCoder*)coder {
  if ((self = [super init])) {
    _userEmail = [coder decodeObjectOfClass:[NSString class]
                                     forKey:kCoderUserEmailKey];
    _gaiaID = [coder decodeObjectOfClass:[NSString class]
                                  forKey:kCoderGaiaIDKey];
    _userFullName = [coder decodeObjectOfClass:[NSString class]
                                        forKey:kCoderUserFullNameKey];
    _userGivenName = [coder decodeObjectOfClass:[NSString class]
                                         forKey:kCoderUserGivenNameKey];
  }
  return self;
}

+ (BOOL)supportsSecureCoding {
  return YES;
}

#pragma mark - Debug

- (NSString*)description {
  return [NSString stringWithFormat:@"<%@: %p, GaiaID: \"%@\", name: \"%@\", "
                                    @"email: \"%@\">",
                                    self.class.description, self, self.gaiaID,
                                    self.userFullName, self.userEmail];
}

@end
