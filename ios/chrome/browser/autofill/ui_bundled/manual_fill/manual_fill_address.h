// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_MANUAL_FILL_MANUAL_FILL_ADDRESS_H_
#define IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_MANUAL_FILL_MANUAL_FILL_ADDRESS_H_

#import <Foundation/Foundation.h>

// This represents an address to use with manual fill.
@interface ManualFillAddress : NSObject

// The address' GUID.
@property(nonatomic, readonly) NSString* GUID;

// The addressee's first name.
@property(nonatomic, readonly) NSString* firstName;

// The addressee's middle name or middle initial, or empty.
@property(nonatomic, readonly) NSString* middleNameOrInitial;

// The addressee's last name.
@property(nonatomic, readonly) NSString* lastName;

// The company name.
@property(nonatomic, readonly) NSString* company;

// The first line of this address.
@property(nonatomic, readonly) NSString* line1;

// The second, optional, line of this address.
@property(nonatomic, readonly) NSString* line2;

// The zip code of the address.
@property(nonatomic, readonly) NSString* zip;

// The city of the address.
@property(nonatomic, readonly) NSString* city;

// The state or province of the address.
@property(nonatomic, readonly) NSString* state;

// The country of the address.
@property(nonatomic, readonly) NSString* country;

// The home phone number.
@property(nonatomic, readonly) NSString* phoneNumber;

// The profile email address.
@property(nonatomic, readonly) NSString* emailAddress;

// Default init.
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
                emailAddress:(NSString*)emailAddress NS_DESIGNATED_INITIALIZER;

// Unavailable. Please use `initWithFirstName:middleNameOrInitial:lastName:
// line1:line2:zip:city:state:country:`.
- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_MANUAL_FILL_MANUAL_FILL_ADDRESS_H_
