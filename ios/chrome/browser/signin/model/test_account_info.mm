// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/signin/model/test_account_info.h"

#import "base/check.h"
#import "base/strings/sys_string_conversions.h"
#import "components/signin/internal/identity_manager/account_capabilities_constants.h"

namespace {

// Keys for encoding TestAccountInfo instance.
NSString* const kCoderUserEmailKey = @"UserEmail";
NSString* const kCoderGaiaIDKey = @"GaiaID";
NSString* const kCoderUserFullNameKey = @"UserFullName";
NSString* const kCoderUserGivenNameKey = @"UserGivenName";
NSString* const kCoderUserCapabilityKey = @"CapabilityKey";

}  // namespace

@implementation TestAccountInfo {
  NSString* _gaiaID;
  NSString* _userEmail;
  NSString* _userFullName;
  NSString* _userGivenName;
  NSDictionary<NSString*, NSNumber*>* _capabilities;
}

+ (std::string)encodeTestAccountInfosToBase64:
    (NSArray<TestAccountInfo*>*)testAccountInfos {
  NSError* error = nil;
  NSData* data = [NSKeyedArchiver archivedDataWithRootObject:testAccountInfos
                                       requiringSecureCoding:NO
                                                       error:&error];
  DCHECK(!error);
  NSString* string = [data base64EncodedStringWithOptions:
                               NSDataBase64EncodingEndLineWithCarriageReturn];
  return base::SysNSStringToUTF8(string);
}

+ (NSArray<TestAccountInfo*>*)testAccountInfosFromBase64String:
    (const std::string&)string {
  NSData* data = [[NSData alloc]
      initWithBase64EncodedString:base::SysUTF8ToNSString(string)
                          options:NSDataBase64DecodingIgnoreUnknownCharacters];
  NSSet* classes =
      [NSSet setWithArray:@[ [self class], [NSArray class], [NSNumber class] ]];
  NSError* error = nil;
  NSArray* identities = [NSKeyedUnarchiver unarchivedObjectOfClasses:classes
                                                            fromData:data
                                                               error:&error];
  DCHECK(!error);
  return identities;
}

+ (NSDictionary<NSString*, NSNumber*>*)defaultCapabilityValues {
  return @{@(kIsSubjectToParentalControlsCapabilityName) : @NO};
}

+ (instancetype)testAccountInfo1 {
  return [self testAccountInfoWithUserEmail:@"foo1@gmail.com"];
}

+ (instancetype)testAccountInfo2 {
  return [self testAccountInfoWithUserEmail:@"foo2@gmail.com"];
}

+ (instancetype)testAccountInfo3 {
  return [self testAccountInfoWithUserEmail:@"foo3@gmail.com"];
}

+ (instancetype)testAccountInfo4 {
  return [self testAccountInfoWithUserEmail:@"foo4@gmail.com"];
}

+ (instancetype)managedTestAccountInfo {
  return [self testAccountInfoWithUserEmail:@"foo@google.com"];
}

+ (instancetype)testAccountInfoWithUserEmail:(NSString*)userEmail {
  return [self testAccountInfoWithUserEmail:userEmail gaiaID:nil];
}

+ (instancetype)testAccountInfoWithUserEmail:(NSString*)userEmail
                                      gaiaID:(NSString*)gaiaID {
  return [[self alloc] initWithUserEmail:userEmail
                                  gaiaID:gaiaID
                            userFullName:nil
                           userGivenName:nil
                            capabilities:nil];
}

- (instancetype)initWithUserEmail:(NSString*)userEmail
                           gaiaID:(NSString*)gaiaID
                     userFullName:(NSString*)userFullName
                    userGivenName:(NSString*)userGivenName
                     capabilities:
                         (NSDictionary<NSString*, NSNumber*>*)capabilities {
  if ((self = [super init])) {
    _userEmail = userEmail;
    if (gaiaID.length > 0) {
      _gaiaID = gaiaID;
    } else {
      // GaiaID cannot look like an email address.
      NSString* withoutAtSign =
          [userEmail stringByReplacingOccurrencesOfString:@"@" withString:@"_"];
      _gaiaID = [NSString stringWithFormat:@"%@_GAIAID", withoutAtSign];
    }
    NSArray* split = [userEmail componentsSeparatedByString:@"@"];
    DCHECK_EQ(split.count, 2ul);
    if (userFullName.length > 0) {
      _userFullName = userFullName;
    } else {
      _userFullName = split[0];
    }
    if (userGivenName.length > 0) {
      _userGivenName = userGivenName;
    } else {
      _userGivenName = split[0];
    }
    if (capabilities) {
      _capabilities = [capabilities copy];
    } else {
      _capabilities = [[[self class] defaultCapabilityValues] copy];
    }
  }
  return self;
}

#pragma mark - Properties

- (NSString*)gaiaID {
  return _gaiaID;
}

- (NSString*)userEmail {
  return _userEmail;
}

- (NSString*)userFullName {
  return _userFullName;
}

- (NSString*)userGivenName {
  return _userGivenName;
}

- (NSDictionary<NSString*, NSNumber*>*)capabilities {
  return _capabilities;
}

#pragma mark - NSSecureCoding

- (void)encodeWithCoder:(NSCoder*)coder {
  [coder encodeObject:_gaiaID forKey:kCoderGaiaIDKey];
  [coder encodeObject:self.userEmail forKey:kCoderUserEmailKey];
  [coder encodeObject:self.userFullName forKey:kCoderUserFullNameKey];
  [coder encodeObject:self.userGivenName forKey:kCoderUserGivenNameKey];
  [coder encodeObject:_capabilities forKey:kCoderUserCapabilityKey];
}

- (id)initWithCoder:(NSCoder*)coder {
  NSString* gaiaID = [coder decodeObjectOfClass:[NSString class]
                                         forKey:kCoderGaiaIDKey];
  NSString* userEmail = [coder decodeObjectOfClass:[NSString class]
                                            forKey:kCoderUserEmailKey];
  NSString* userFullName = [coder decodeObjectOfClass:[NSString class]
                                               forKey:kCoderUserFullNameKey];
  NSString* userGivenName = [coder decodeObjectOfClass:[NSString class]
                                                forKey:kCoderUserGivenNameKey];
  NSSet* capabilityClasses =
      [NSSet setWithArray:@[ [NSArray class], [NSNumber class] ]];
  NSDictionary<NSString*, NSNumber*>* capabilities =
      [coder decodeObjectOfClasses:capabilityClasses
                            forKey:kCoderUserCapabilityKey];
  return [self initWithUserEmail:gaiaID
                          gaiaID:userEmail
                    userFullName:userFullName
                   userGivenName:userGivenName
                    capabilities:capabilities];
}

+ (BOOL)supportsSecureCoding {
  return YES;
}

@end
