// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_AUTOFILL_PROFILE_EDIT_TABLE_VIEW_CONTROLLER_DELEGATE_H_
#define IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_AUTOFILL_PROFILE_EDIT_TABLE_VIEW_CONTROLLER_DELEGATE_H_

#import "components/autofill/core/browser/field_types.h"
#import "ios/chrome/browser/autofill/ui_bundled/autofill_profile_address_field.h"

// Delegate manages viewing/editing the profile data.
@protocol AutofillProfileEditTableViewControllerDelegate

// Notifies the class that conforms this delegate to open the country selection
// view.
- (void)willSelectCountryWithCurrentlySelectedCountry:(NSString*)country;

// Notifies the class that conforms this delegate to save the profile.
- (void)didSaveProfileFromModal;

// Notifies the class that conforms this delegate to update the profile
// `serverFieldType` with `value`.
- (void)updateProfileMetadataWithValue:(NSString*)value
                  forAutofillFieldType:(NSString*)autofillFieldType;

// For `autofillFieldType`, computes whether the field contains a valid value or
// not. If not,
- (BOOL)fieldContainsValidValue:(NSString*)autofillFieldType
                  hasEmptyValue:(BOOL)hasEmptyValue
      moveToAccountFromSettings:(BOOL)moveToAccountFromSettings;

// Notifies the class that conforms this delegate that the view has moved out of
// the view hierarchy.
- (void)viewDidDisappear;

// Returns the type name in "NSString*" for the `autofillType`.
- (NSString*)fieldTypeToTypeName:(autofill::FieldType)autofillType;

// Returns the count of the fields that are required and contain no value.
- (int)requiredFieldsWithEmptyValuesCount;

// Resets the container that stores the required fields with empty values.
- (void)resetRequiredFieldsWithEmptyValuesCount;

// Returns the list of the address fields.
- (NSArray<AutofillProfileAddressField*>*)inputAddressFields;

@end

#endif  // IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_AUTOFILL_PROFILE_EDIT_TABLE_VIEW_CONTROLLER_DELEGATE_H_
