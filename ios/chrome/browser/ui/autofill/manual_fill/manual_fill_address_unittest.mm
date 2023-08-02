// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/autofill/manual_fill/manual_fill_address.h"

#import "testing/platform_test.h"

using ManualFillAddressiOSTest = PlatformTest;

// Tests that a credential is correctly created.
TEST_F(ManualFillAddressiOSTest, Creation) {
  NSString* firstName = @"First";
  NSString* middleNameOrInitial = @"M";
  NSString* lastName = @"Last";
  NSString* company = @"Google";
  NSString* line1 = @"10 Main Street";
  NSString* line2 = @"Appt 16";
  NSString* zip = @"12345";
  NSString* city = @"Springfield";
  NSString* state = @"State";
  NSString* country = @"Country";
  NSString* phoneNumber = @"123-456-789";
  NSString* emailAddress = @"john@doe";
  ManualFillAddress* address =
      [[ManualFillAddress alloc] initWithFirstName:firstName
                               middleNameOrInitial:middleNameOrInitial
                                          lastName:lastName
                                           company:company
                                             line1:line1
                                             line2:line2
                                               zip:zip
                                              city:city
                                             state:state
                                           country:country
                                       phoneNumber:phoneNumber
                                      emailAddress:emailAddress];
  EXPECT_TRUE(address);
  EXPECT_TRUE([firstName isEqualToString:address.firstName]);
  EXPECT_TRUE(
      [middleNameOrInitial isEqualToString:address.middleNameOrInitial]);
  EXPECT_TRUE([lastName isEqualToString:address.lastName]);
  EXPECT_TRUE([company isEqualToString:address.company]);
  EXPECT_TRUE([line1 isEqualToString:address.line1]);
  EXPECT_TRUE([line2 isEqualToString:address.line2]);
  EXPECT_TRUE([zip isEqualToString:address.zip]);
  EXPECT_TRUE([city isEqualToString:address.city]);
  EXPECT_TRUE([state isEqualToString:address.state]);
  EXPECT_TRUE([country isEqualToString:address.country]);
  EXPECT_TRUE([phoneNumber isEqualToString:address.phoneNumber]);
  EXPECT_TRUE([emailAddress isEqualToString:address.emailAddress]);
}

// Test equality between addresses (lexicographically).
TEST_F(ManualFillAddressiOSTest, Equality) {
  NSString* firstName = @"First";
  NSString* middleNameOrInitial = @"M";
  NSString* lastName = @"Last";
  NSString* company = @"Google";
  NSString* line1 = @"10 Main Street";
  NSString* line2 = @"Appt 16";
  NSString* zip = @"12345";
  NSString* city = @"Springfield";
  NSString* state = @"State";
  NSString* country = @"Country";
  NSString* phoneNumber = @"123-456-789";
  NSString* emailAddress = @"john@doe";
  ManualFillAddress* address =
      [[ManualFillAddress alloc] initWithFirstName:firstName
                               middleNameOrInitial:middleNameOrInitial
                                          lastName:lastName
                                           company:company
                                             line1:line1
                                             line2:line2
                                               zip:zip
                                              city:city
                                             state:state
                                           country:country
                                       phoneNumber:phoneNumber
                                      emailAddress:emailAddress];

  ManualFillAddress* equalAddress =
      [[ManualFillAddress alloc] initWithFirstName:firstName
                               middleNameOrInitial:middleNameOrInitial
                                          lastName:lastName
                                           company:company
                                             line1:line1
                                             line2:line2
                                               zip:zip
                                              city:city
                                             state:state
                                           country:country
                                       phoneNumber:phoneNumber
                                      emailAddress:emailAddress];
  EXPECT_TRUE([address isEqual:equalAddress]);

  ManualFillAddress* differentAddressFirstName =
      [[ManualFillAddress alloc] initWithFirstName:@"Bilbo"
                               middleNameOrInitial:middleNameOrInitial
                                          lastName:lastName
                                           company:company
                                             line1:line1
                                             line2:line2
                                               zip:zip
                                              city:city
                                             state:state
                                           country:country
                                       phoneNumber:phoneNumber
                                      emailAddress:emailAddress];
  EXPECT_FALSE([address isEqual:differentAddressFirstName]);

  ManualFillAddress* differentAddressMiddleNameOrInitial =
      [[ManualFillAddress alloc] initWithFirstName:firstName
                               middleNameOrInitial:@"R"
                                          lastName:lastName
                                           company:company
                                             line1:line1
                                             line2:line2
                                               zip:zip
                                              city:city
                                             state:state
                                           country:country
                                       phoneNumber:phoneNumber
                                      emailAddress:emailAddress];
  EXPECT_FALSE([address isEqual:differentAddressMiddleNameOrInitial]);

  ManualFillAddress* differentAddressLastName =
      [[ManualFillAddress alloc] initWithFirstName:firstName
                               middleNameOrInitial:middleNameOrInitial
                                          lastName:@"Hobbit"
                                           company:company
                                             line1:line1
                                             line2:line2
                                               zip:zip
                                              city:city
                                             state:state
                                           country:country
                                       phoneNumber:phoneNumber
                                      emailAddress:emailAddress];
  EXPECT_FALSE([address isEqual:differentAddressLastName]);

  ManualFillAddress* differentAddressCompany =
      [[ManualFillAddress alloc] initWithFirstName:firstName
                               middleNameOrInitial:middleNameOrInitial
                                          lastName:lastName
                                           company:@"Tokien"
                                             line1:line1
                                             line2:line2
                                               zip:zip
                                              city:city
                                             state:state
                                           country:country
                                       phoneNumber:phoneNumber
                                      emailAddress:emailAddress];
  EXPECT_FALSE([address isEqual:differentAddressCompany]);

  ManualFillAddress* differentAddressLine1 =
      [[ManualFillAddress alloc] initWithFirstName:firstName
                               middleNameOrInitial:middleNameOrInitial
                                          lastName:lastName
                                           company:company
                                             line1:@"A House"
                                             line2:line2
                                               zip:zip
                                              city:city
                                             state:state
                                           country:country
                                       phoneNumber:phoneNumber
                                      emailAddress:emailAddress];
  EXPECT_FALSE([address isEqual:differentAddressLine1]);

  ManualFillAddress* differentAddressLine2 =
      [[ManualFillAddress alloc] initWithFirstName:firstName
                               middleNameOrInitial:middleNameOrInitial
                                          lastName:lastName
                                           company:company
                                             line1:line1
                                             line2:@""
                                               zip:zip
                                              city:city
                                             state:state
                                           country:country
                                       phoneNumber:phoneNumber
                                      emailAddress:emailAddress];
  EXPECT_FALSE([address isEqual:differentAddressLine2]);

  ManualFillAddress* differentAddressZip =
      [[ManualFillAddress alloc] initWithFirstName:firstName
                               middleNameOrInitial:middleNameOrInitial
                                          lastName:lastName
                                           company:company
                                             line1:line1
                                             line2:line2
                                               zip:@"1937"
                                              city:city
                                             state:state
                                           country:country
                                       phoneNumber:phoneNumber
                                      emailAddress:emailAddress];
  EXPECT_FALSE([address isEqual:differentAddressZip]);

  ManualFillAddress* differentAddressCity =
      [[ManualFillAddress alloc] initWithFirstName:firstName
                               middleNameOrInitial:middleNameOrInitial
                                          lastName:lastName
                                           company:company
                                             line1:line1
                                             line2:line2
                                               zip:zip
                                              city:@"Shire"
                                             state:state
                                           country:country
                                       phoneNumber:phoneNumber
                                      emailAddress:emailAddress];
  EXPECT_FALSE([address isEqual:differentAddressCity]);

  ManualFillAddress* differentAddressState =
      [[ManualFillAddress alloc] initWithFirstName:firstName
                               middleNameOrInitial:middleNameOrInitial
                                          lastName:lastName
                                           company:company
                                             line1:line1
                                             line2:line2
                                               zip:zip
                                              city:city
                                             state:@"Eriador"
                                           country:country
                                       phoneNumber:phoneNumber
                                      emailAddress:emailAddress];
  EXPECT_FALSE([address isEqual:differentAddressState]);

  ManualFillAddress* differentAddressCountry =
      [[ManualFillAddress alloc] initWithFirstName:firstName
                               middleNameOrInitial:middleNameOrInitial
                                          lastName:lastName
                                           company:company
                                             line1:line1
                                             line2:line2
                                               zip:zip
                                              city:city
                                             state:state
                                           country:@"Arnor"
                                       phoneNumber:phoneNumber
                                      emailAddress:emailAddress];
  EXPECT_FALSE([address isEqual:differentAddressCountry]);

  ManualFillAddress* differentPhoneNumber =
      [[ManualFillAddress alloc] initWithFirstName:firstName
                               middleNameOrInitial:middleNameOrInitial
                                          lastName:lastName
                                           company:company
                                             line1:line1
                                             line2:line2
                                               zip:zip
                                              city:city
                                             state:state
                                           country:country
                                       phoneNumber:@"999-999-999"
                                      emailAddress:emailAddress];
  EXPECT_FALSE([address isEqual:differentPhoneNumber]);

  ManualFillAddress* differentEmailAddress =
      [[ManualFillAddress alloc] initWithFirstName:firstName
                               middleNameOrInitial:middleNameOrInitial
                                          lastName:lastName
                                           company:company
                                             line1:line1
                                             line2:line2
                                               zip:zip
                                              city:city
                                             state:state
                                           country:country
                                       phoneNumber:phoneNumber
                                      emailAddress:@"jane@doe"];
  EXPECT_FALSE([address isEqual:differentEmailAddress]);
}
