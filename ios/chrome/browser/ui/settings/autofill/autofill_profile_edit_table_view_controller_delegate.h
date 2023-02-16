// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_AUTOFILL_AUTOFILL_PROFILE_EDIT_TABLE_VIEW_CONTROLLER_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_AUTOFILL_AUTOFILL_PROFILE_EDIT_TABLE_VIEW_CONTROLLER_DELEGATE_H_

@class AutofillProfileEditTableViewController;

namespace autofill {
class AutofillProfile;
}

// Delegate manages viewing/editing the profile data.
@protocol AutofillProfileEditTableViewControllerDelegate

// Notifies the class that conforms this delegate to open the country selection
// view.
- (void)willSelectCountryWithCurrentlySelectedCountry:(NSString*)country;

// Notifies the class that conforms this delegate to save the `profile`.
- (void)didEditAutofillProfile:(autofill::AutofillProfile*)profile;

// Notifies the class that conforms this delegate that the view has moved out of
// the view hierarchy.
- (void)viewDidDisappear;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_AUTOFILL_AUTOFILL_PROFILE_EDIT_TABLE_VIEW_CONTROLLER_DELEGATE_H_
