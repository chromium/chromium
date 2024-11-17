// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_ADDRESS_EDITOR_AUTOFILL_PROFILE_EDIT_TABLE_VIEW_CONTROLLER_DELEGATE_H_
#define IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_ADDRESS_EDITOR_AUTOFILL_PROFILE_EDIT_TABLE_VIEW_CONTROLLER_DELEGATE_H_

#import "components/autofill/core/browser/field_types.h"
#import "ios/chrome/browser/autofill/ui_bundled/address_editor/autofill_profile_address_field.h"

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

// Setter to store current field value for `autofillFieldType`.
- (void)setCurrentValueForType:(NSString*)autofillFieldType
                     withValue:(NSString*)value;

// Getter of the current field value for `autofillFieldType`.
- (NSString*)currentValueForType:(NSString*)autofillFieldType;

// Responsible for showing the error if a required field does not have a
// value, or just updating the error message if multiple required fields have
// missing values or just removing the error if all the requirements are met.
// Also, updates the button status if the error is shown/removed.
- (void)validateFieldsAndUpdateButtonStatus;

// Computes the fields that were edited in the view.
- (void)computeFieldWasEdited:(NSString*)editedFieldType value:(NSString*)value;

@end

#endif  // IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_ADDRESS_EDITOR_AUTOFILL_PROFILE_EDIT_TABLE_VIEW_CONTROLLER_DELEGATE_H_
