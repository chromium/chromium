// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_AUTOFILL_PROFILE_EDIT_MEDIATOR_DELEGATE_H_
#define IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_AUTOFILL_PROFILE_EDIT_MEDIATOR_DELEGATE_H_

@class AutofillProfileEditMediator;
@class CountryItem;

// This delegate is notified of the result of saving the edited profile
@protocol AutofillProfileEditMediatorDelegate

// Notifies that the profile is valid or the user cancel the view
// controller.
- (void)autofillEditProfileMediatorDidFinish:
    (AutofillProfileEditMediator*)mediator;

// Notifies the class that conforms this delegate to open the country selection
// view.
- (void)willSelectCountryWithCurrentlySelectedCountry:(NSString*)country
                                          countryList:(NSArray<CountryItem*>*)
                                                          allCountries;

// Notifies the class that conforms this delegate to save the profile.
- (void)didSaveProfile;

@end

#endif  // IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_AUTOFILL_PROFILE_EDIT_MEDIATOR_DELEGATE_H_
