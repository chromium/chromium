// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_AUTOFILL_PROFILE_EDIT_TABLE_VIEW_CONSTANTS_H_
#define IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_AUTOFILL_PROFILE_EDIT_TABLE_VIEW_CONSTANTS_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/shared/ui/list_model/list_model.h"

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

#endif  // IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_AUTOFILL_PROFILE_EDIT_TABLE_VIEW_CONSTANTS_H_
