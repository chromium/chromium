// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_ADDRESS_EDITOR_AUTOFILL_CONSTANTS_H_
#define IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_ADDRESS_EDITOR_AUTOFILL_CONSTANTS_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/shared/ui/list_model/list_model.h"

// Accessibility identifier for the country selection view.
extern NSString* const kAutofillCountrySelectionTableViewId;
extern NSString* const kAutofillCountrySelectionSearchScrimId;

// Describes the type of the prompt in the save address flow.
enum class AutofillSaveProfilePromptMode {
  // The prompt is for saving a new profile.
  kNewProfile,
  // The prompt is for updating an existing profile.
  kUpdateProfile,
  // The prompt is for migrating a profile to the account.
  kMigrateProfile
};

// Defines the context in which the address is being saved/updated.
enum class SaveAddressContext {
  // Saving or updating an address captured during form submission.
  kInfobarSaveUpdateAddress,
  // Editing an existing, previously saved address.
  kEditingSavedAddress,
  // Manually adding a new address from scratch.
  kAddingManualAddress
};

// `AutofillProfileEditTableViewHelper` and
// `AutofillSettingsProfileEditTableViewController` both create the same view
// using the below section identifiers and item types. Identifier for section
// for autofill profile edit views.
typedef NS_ENUM(NSInteger, AutofillProfileDetailsSectionIdentifier) {
  AutofillProfileDetailsSectionIdentifierName = kSectionIdentifierEnumZero,
  AutofillProfileDetailsSectionIdentifierAddress,
  AutofillProfileDetailsSectionIdentifierPhoneEmail,
  AutofillProfileDetailsSectionIdentifierButton,
  AutofillProfileDetailsSectionIdentifierMigrationButton,
  AutofillProfileDetailsSectionIdentifierErrorFooter,
  AutofillProfileDetailsSectionIdentifierFooter,
  AutofillProfileDetailsSectionIdentifierEdit
};

// Identifier for item types for autofill profile edit views.
typedef NS_ENUM(NSInteger, AutofillProfileDetailsItemType) {
  AutofillProfileDetailsItemTypeTextField = kItemTypeEnumZero,
  AutofillProfileDetailsItemTypeCountrySelectionField,
  AutofillProfileDetailsItemTypeError,
  AutofillProfileDetailsItemTypeFooter,
  AutofillProfileDetailsItemTypeSaveButton,
  AutofillProfileDetailsItemTypeMigrateToAccountRecommendation,
  AutofillProfileDetailsItemTypeMigrateToAccountButton,
  AutofillProfileDetailsItemTypeEdit
};

#endif  // IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_ADDRESS_EDITOR_AUTOFILL_CONSTANTS_H_
