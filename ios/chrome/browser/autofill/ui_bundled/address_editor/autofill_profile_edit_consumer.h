// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_ADDRESS_EDITOR_AUTOFILL_PROFILE_EDIT_CONSUMER_H_
#define IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_ADDRESS_EDITOR_AUTOFILL_PROFILE_EDIT_CONSUMER_H_

#import <Foundation/Foundation.h>

// Sets the Autofill profile edit for consumer.
@protocol AutofillProfileEditConsumer <NSObject>

// Called when the country is selected from the dropdown.
- (void)didSelectCountry:(NSString*)country;

// Notifies the class that conforms this delegate to set whether the profile is
// an account profile or not.
- (void)setAccountProfile:(BOOL)accountProfile;

// Notifies the consumer to present/remove the error state based on
// `shouldShowError`.
- (void)updateErrorStatus:(BOOL)shouldShowError;

// Notifies the consumer to update the error message if required.
- (void)updateErrorMessageIfRequired;

// Notifies the consumer to update the button status.
- (void)updateButtonStatus:(BOOL)enabled;

// Notifies the consumer to update the profile data.
- (void)updateProfileData;

@end

#endif  // IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_ADDRESS_EDITOR_AUTOFILL_PROFILE_EDIT_CONSUMER_H_
