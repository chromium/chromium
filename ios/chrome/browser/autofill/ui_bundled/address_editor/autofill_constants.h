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

// `AutofillProfileEditTableViewController` and
// `AutofillSettingsProfileEditTableViewController` both create the same view
// using the below section identifiers and item types. Identifier for section
// for autofill profile edit views.
typedef NS_ENUM(NSInteger, AutofillProfileDetailsSectionIdentifier) {
  AutofillProfileDetailsSectionIdentifierFields = kSectionIdentifierEnumZero,
  AutofillProfileDetailsSectionIdentifierName,
  AutofillProfileDetailsSectionIdentifierAddress,
  AutofillProfileDetailsSectionIdentifierPhoneEmail,
  AutofillProfileDetailsSectionIdentifierButton,
  AutofillProfileDetailsSectionIdentifierMigrationButton,
  AutofillProfileDetailsSectionIdentifierErrorFooter,
  AutofillProfileDetailsSectionIdentifierFooter,
};

// Identifier for item types for autofill profile edit views.
typedef NS_ENUM(NSInteger, AutofillProfileDetailsItemType) {
  AutofillProfileDetailsItemTypeTextField = kItemTypeEnumZero,
  AutofillProfileDetailsItemTypeCountrySelectionField,
  AutofillProfileDetailsItemTypeError,
  AutofillProfileDetailsItemTypeFooter,
  AutofillProfileDetailsItemTypeSaveButton,
  AutofillProfileDetailsItemTypeMigrateToAccountRecommendation,
  AutofillProfileDetailsItemTypeMigrateToAccountButton
};

#endif  // IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_ADDRESS_EDITOR_AUTOFILL_CONSTANTS_H_
