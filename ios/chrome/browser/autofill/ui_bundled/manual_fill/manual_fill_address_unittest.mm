// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/ui_bundled/manual_fill/manual_fill_address.h"

#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

using ManualFillAddressiOSTest = PlatformTest;

// Tests that a credential is correctly created.
TEST_F(ManualFillAddressiOSTest, Creation) {
  NSString* GUID = @"1234-5678-abcd";
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
      [[ManualFillAddress alloc] initWithGUID:GUID
                                    firstName:firstName
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
  EXPECT_NSEQ(GUID, address.GUID);
  EXPECT_NSEQ(firstName, address.firstName);
  EXPECT_NSEQ(middleNameOrInitial, address.middleNameOrInitial);
  EXPECT_NSEQ(lastName, address.lastName);
  EXPECT_NSEQ(company, address.company);
  EXPECT_NSEQ(line1, address.line1);
  EXPECT_NSEQ(line2, address.line2);
  EXPECT_NSEQ(zip, address.zip);
  EXPECT_NSEQ(city, address.city);
  EXPECT_NSEQ(state, address.state);
  EXPECT_NSEQ(country, address.country);
  EXPECT_NSEQ(phoneNumber, address.phoneNumber);
  EXPECT_NSEQ(emailAddress, address.emailAddress);
}

// Test equality between addresses (lexicographically).
TEST_F(ManualFillAddressiOSTest, Equality) {
  NSString* GUID = @"1234-5678-abcd";
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
      [[ManualFillAddress alloc] initWithGUID:GUID
                                    firstName:firstName
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
      [[ManualFillAddress alloc] initWithGUID:GUID
                                    firstName:firstName
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

  ManualFillAddress* differentAddressGUID =
      [[ManualFillAddress alloc] initWithGUID:@"1234-5678-wxyz"
                                    firstName:firstName
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
  EXPECT_FALSE([address isEqual:differentAddressGUID]);

  ManualFillAddress* differentAddressFirstName =
      [[ManualFillAddress alloc] initWithGUID:GUID
                                    firstName:@"Bilbo"
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
      [[ManualFillAddress alloc] initWithGUID:GUID
                                    firstName:firstName
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
      [[ManualFillAddress alloc] initWithGUID:GUID
                                    firstName:firstName
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
      [[ManualFillAddress alloc] initWithGUID:GUID
                                    firstName:firstName
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
      [[ManualFillAddress alloc] initWithGUID:GUID
                                    firstName:firstName
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
      [[ManualFillAddress alloc] initWithGUID:GUID
                                    firstName:firstName
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
      [[ManualFillAddress alloc] initWithGUID:GUID
                                    firstName:firstName
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
      [[ManualFillAddress alloc] initWithGUID:GUID
                                    firstName:firstName
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
      [[ManualFillAddress alloc] initWithGUID:GUID
                                    firstName:firstName
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
      [[ManualFillAddress alloc] initWithGUID:GUID
                                    firstName:firstName
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
      [[ManualFillAddress alloc] initWithGUID:GUID
                                    firstName:firstName
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
      [[ManualFillAddress alloc] initWithGUID:GUID
                                    firstName:firstName
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
