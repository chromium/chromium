// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_AUTOFILL_AUTOFILL_PROFILE_EDIT_CONSUMER_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_AUTOFILL_AUTOFILL_PROFILE_EDIT_CONSUMER_H_

#import <Foundation/Foundation.h>

// Sets the Autofill profile edit for consumer.
@protocol AutofillProfileEditConsumer <NSObject>

// Called when the country is selected from the dropdown.
- (void)didSelectCountry:(NSString*)country;

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

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_AUTOFILL_AUTOFILL_PROFILE_EDIT_CONSUMER_H_
