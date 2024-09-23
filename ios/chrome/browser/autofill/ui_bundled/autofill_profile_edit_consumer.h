// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_AUTOFILL_PROFILE_EDIT_CONSUMER_H_
#define IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_AUTOFILL_PROFILE_EDIT_CONSUMER_H_

#import <Foundation/Foundation.h>

// Sets the Autofill profile edit for consumer.
@protocol AutofillProfileEditConsumer <NSObject>

// Called when the country is selected from the dropdown.
- (void)didSelectCountry:(NSString*)country;

// Notifies the class that conforms this delegate to set the corresponding data.
- (void)setFieldValuesMap:
    (NSMutableDictionary<NSString*, NSString*>*)fieldValueMap;

// Notifies the class that conforms this delegate to set whether the profile is
// an account profile or not.
- (void)setAccountProfile:(BOOL)accountProfile;

@end

#endif  // IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_AUTOFILL_PROFILE_EDIT_CONSUMER_H_
