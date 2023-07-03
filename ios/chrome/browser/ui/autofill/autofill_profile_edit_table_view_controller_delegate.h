// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_PROFILE_EDIT_TABLE_VIEW_CONTROLLER_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_PROFILE_EDIT_TABLE_VIEW_CONTROLLER_DELEGATE_H_

#import "components/autofill/core/browser/field_types.h"
#import "ios/chrome/browser/ui/autofill/autofill_ui_type.h"

// Delegate manages viewing/editing the profile data.
@protocol AutofillProfileEditTableViewControllerDelegate

// Notifies the class that conforms this delegate to open the country selection
// view.
- (void)willSelectCountryWithCurrentlySelectedCountry:(NSString*)country;

// Notifies the class that conforms this delegate to save the profile.
- (void)didSaveProfileFromModal;

// Returns true if the field value is empty.
- (BOOL)fieldValueEmptyOnProfileLoadForType:
    (autofill::ServerFieldType)serverFieldType;

// Notifies the class that conforms this delegate to update the profile
// `serverFieldType` with `value`.
- (void)updateProfileMetadataWithValue:(NSString*)value
                     forAutofillUIType:(AutofillUIType)autofillUIType;

// Notifies the class that conforms this delegate that the view has moved out of
// the view hierarchy.
- (void)viewDidDisappear;

@end

#endif  // IOS_CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_PROFILE_EDIT_TABLE_VIEW_CONTROLLER_DELEGATE_H_
