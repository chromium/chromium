// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_PROFILE_EDIT_CONSUMER_H_
#define IOS_CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_PROFILE_EDIT_CONSUMER_H_

#import <Foundation/Foundation.h>

// Sets the Autofill profile edit for consumer.
@protocol AutofillProfileEditConsumer <NSObject>

// Called when the country is selected from the dropdown.
- (void)didSelectCountry:(NSString*)country;

// Notifies the class that conforms this delegate to set that whether the
// name data is required or not.
- (void)setNameRequired:(BOOL)nameRequired;

// Notifies the class that conforms this delegate to set that whether the
// address line 1 data is required or not.
- (void)setLine1Required:(BOOL)line1Required;

// Notifies the class that conforms this delegate to set that whether the city
// data is required or not.
- (void)setCityRequired:(BOOL)cityRequired;

// Notifies the class that conforms this delegate  to set that whether the state
// data is required or not.
- (void)setStateRequired:(BOOL)stateRequired;

// Notifies the class that conforms this delegate to set that whether the zip
// code data is required or not.
- (void)setZipRequired:(BOOL)zipRequired;

// Notifies the class that conforms this delegate to set the corresponding data.
- (void)setHonorificPrefix:(NSString*)honorificPrefix;
- (void)setCompanyName:(NSString*)companyName;
- (void)setFullName:(NSString*)fullName;
- (void)setHomeAddressLine1:(NSString*)homeAddressLine1;
- (void)setHomeAddressLine2:(NSString*)homeAddressLine2;
- (void)setHomeAddressCity:(NSString*)homeAddressCity;
- (void)setHomeAddressState:(NSString*)homeAddressState;
- (void)setHomeAddressZip:(NSString*)homeAddressZip;
- (void)setHomeAddressCountry:(NSString*)homeAddressCountry;
- (void)setHomePhoneWholeNumber:(NSString*)homePhoneWholeNumber;
- (void)setEmailAddress:(NSString*)emailAddress;

// Notifies the class that conforms this delegate to set whether the profile is
// an account profile or not.
- (void)setAccountProfile:(BOOL)accountProfile;

@end

#endif  // IOS_CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_PROFILE_EDIT_CONSUMER_H_
