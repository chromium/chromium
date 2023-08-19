// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/signin/fake_system_identity.h"

#import "base/apple/foundation_util.h"
#import "base/strings/sys_string_conversions.h"

namespace {
NSString* const kCoderUserEmailKey = @"UserEmail";
NSString* const kCoderGaiaIDKey = @"GaiaID";
NSString* const kCoderUserFullNameKey = @"UserFullName";
NSString* const kCoderUserGivenNameKey = @"UserGivenName";
NSString* const kCoderHashedGaiaIDKey = @"HashedGaiaID";
}  // namespace

@implementation FakeSystemIdentity

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
  return [FakeSystemIdentity identityWithEmail:@"foo1@gmail.com"
                                        gaiaID:@"foo1ID"
                                          name:@"Fake Foo 1"];
}

+ (instancetype)fakeIdentity2 {
  return [FakeSystemIdentity identityWithEmail:@"foo2@gmail.com"
                                        gaiaID:@"foo2ID"
                                          name:@"Fake Foo 2"];
}

+ (instancetype)fakeIdentity3 {
  return [FakeSystemIdentity identityWithEmail:@"foo3@gmail.com"
                                        gaiaID:@"foo3ID"
                                          name:@"Fake Foo 3"];
}

+ (instancetype)fakeManagedIdentity {
  return [FakeSystemIdentity identityWithEmail:@"foo@google.com"
                                        gaiaID:@"fooManagedID"
                                          name:@"Fake Managed"];
}

+ (instancetype)identityWithEmail:(NSString*)email
                           gaiaID:(NSString*)gaiaID
                             name:(NSString*)name {
  return [[FakeSystemIdentity alloc] initWithEmail:email
                                            gaiaID:gaiaID
                                              name:name];
}

+ (instancetype)identityWithName:(NSString*)name domain:(NSString*)domain {
  DCHECK(name.length);
  DCHECK(domain.length);

  NSString* gaiaID = nil;
  NSString* email = [NSString stringWithFormat:@"%@@%@", name, domain];
  if ([domain isEqualToString:@"gmail.com"]) {
    // Consumer domain, use "%(name)ID" as Gaia ID.
    gaiaID = [NSString stringWithFormat:@"%@ID", name];
  } else if ([domain isEqualToString:@"google.com"]) {
    // Managed domain, use "%(name)ManagedID" as Gaia ID.
    gaiaID = [NSString stringWithFormat:@"%@ManagedID", name];
  } else {
    // Other domain, include the domain in the Gaia ID, replacing "." with "-".
    gaiaID = [NSString
        stringWithFormat:@"%@-%@-ID", name,
                         [domain stringByReplacingOccurrencesOfString:@"."
                                                           withString:@"-"]];
  }

  return [[FakeSystemIdentity alloc] initWithEmail:email
                                            gaiaID:gaiaID
                                              name:name];
}

- (instancetype)initWithEmail:(NSString*)email
                       gaiaID:(NSString*)gaiaID
                         name:(NSString*)name {
  if ((self = [super init])) {
    _userEmail = [email copy];
    _gaiaID = [gaiaID copy];
    _userFullName = [name copy];
    _userGivenName = [name copy];
    _hashedGaiaID = [NSString stringWithFormat:@"%@_hashID", name];
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
         [_userGivenName isEqualToString:other.userGivenName] &&
         [_hashedGaiaID isEqualToString:other.hashedGaiaID];
}

- (NSUInteger)hash {
  return _gaiaID.hash;
}

#pragma mark - NSSecureCoding

- (void)encodeWithCoder:(NSCoder*)coder {
  [coder encodeObject:_userEmail forKey:kCoderUserEmailKey];
  [coder encodeObject:_gaiaID forKey:kCoderGaiaIDKey];
  [coder encodeObject:_userFullName forKey:kCoderUserFullNameKey];
  [coder encodeObject:_userGivenName forKey:kCoderUserGivenNameKey];
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
    _userGivenName = [coder decodeObjectOfClass:[NSString class]
                                         forKey:kCoderUserGivenNameKey];
    _hashedGaiaID = [coder decodeObjectOfClass:[NSString class]
                                        forKey:kCoderHashedGaiaIDKey];
  }
  return self;
}

- (NSString*)description {
  return [NSString stringWithFormat:@"<%@: %p, GaiaID: \"%@\", name: \"%@\", "
                                    @"email: \"%@\">",
                                    self.class.description, self, self.gaiaID,
                                    self.userFullName, self.userEmail];
}

+ (BOOL)supportsSecureCoding {
  return YES;
}

@end
