// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/ui_bundled/manual_fill/manual_fill_address.h"

@implementation ManualFillAddress

- (instancetype)initWithGUID:(NSString*)GUID
                   firstName:(NSString*)firstName
         middleNameOrInitial:(NSString*)middleNameOrInitial
                    lastName:(NSString*)lastName
                     company:(NSString*)company
                       line1:(NSString*)line1
                       line2:(NSString*)line2
                         zip:(NSString*)zip
                        city:(NSString*)city
                       state:(NSString*)state
                     country:(NSString*)country
                 phoneNumber:(NSString*)phoneNumber
                emailAddress:(NSString*)emailAddress {
  self = [super init];
  if (self) {
    _GUID = GUID;
    _firstName = [firstName copy];
    _middleNameOrInitial = [middleNameOrInitial copy];
    _lastName = [lastName copy];
    _company = [company copy];
    _line1 = [line1 copy];
    _line2 = [line2 copy];
    _zip = [zip copy];
    _city = [city copy];
    _state = [state copy];
    _country = [country copy];
    _phoneNumber = [phoneNumber copy];
    _emailAddress = [emailAddress copy];
  }
  return self;
}

- (BOOL)isEqual:(id)object {
  if (!object) {
    return NO;
  }
  if (self == object) {
    return YES;
  }
  if (![object isMemberOfClass:[ManualFillAddress class]]) {
    return NO;
  }
  ManualFillAddress* otherObject = (ManualFillAddress*)object;
  if (![otherObject.GUID isEqualToString:self.GUID]) {
    return NO;
  }
  if (![otherObject.firstName isEqualToString:self.firstName]) {
    return NO;
  }
  if (![otherObject.middleNameOrInitial
          isEqualToString:self.middleNameOrInitial]) {
    return NO;
  }
  if (![otherObject.lastName isEqualToString:self.lastName]) {
    return NO;
  }
  if (![otherObject.company isEqualToString:self.company]) {
    return NO;
  }
  if (![otherObject.line1 isEqualToString:self.line1]) {
    return NO;
  }
  if (![otherObject.line2 isEqualToString:self.line2]) {
    return NO;
  }
  if (![otherObject.zip isEqualToString:self.zip]) {
    return NO;
  }
  if (![otherObject.city isEqualToString:self.city]) {
    return NO;
  }
  if (![otherObject.state isEqualToString:self.state]) {
    return NO;
  }
  if (![otherObject.country isEqualToString:self.country]) {
    return NO;
  }
  if (![otherObject.phoneNumber isEqualToString:self.phoneNumber]) {
    return NO;
  }
  if (![otherObject.emailAddress isEqualToString:self.emailAddress]) {
    return NO;
  }
  return YES;
}

- (NSUInteger)hash {
  return [self.GUID hash] ^ [self.firstName hash] ^
         [self.middleNameOrInitial hash] ^ [self.lastName hash] ^
         [self.company hash] ^ [self.line1 hash] ^ [self.line2 hash] ^
         [self.zip hash] ^ [self.city hash] ^ [self.state hash] ^
         [self.country hash] ^ [self.phoneNumber hash] ^
         [self.emailAddress hash];
}

- (NSString*)description {
  return [NSString
      stringWithFormat:
          @"<%@ (%p): GUID: %@, firstName: %@, middleNameOrInitial: %@, "
          @"lastName: %@, company: %@, line1: %@, "
          @"line2: %@, zip: %@, city: %@, state: %@, country: %@, "
          @"phoneNumber: %@, emailAddress: %@>",
          NSStringFromClass([self class]), self, self.GUID, self.firstName,
          self.middleNameOrInitial, self.lastName, self.company, self.line1,
          self.line2, self.zip, self.city, self.state, self.country,
          self.phoneNumber, self.emailAddress];
}

@end
