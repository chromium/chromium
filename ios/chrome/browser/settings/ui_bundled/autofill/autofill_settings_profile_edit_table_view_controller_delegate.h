// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_AUTOFILL_AUTOFILL_SETTINGS_PROFILE_EDIT_TABLE_VIEW_CONTROLLER_DELEGATE_H_
#define IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_AUTOFILL_AUTOFILL_SETTINGS_PROFILE_EDIT_TABLE_VIEW_CONTROLLER_DELEGATE_H_

#import "components/autofill/core/browser/data_model/addresses/autofill_profile.h"

// Delegate manages editing the profile data.
@protocol AutofillSettingsProfileEditTableViewControllerDelegate

// Notifies the class that conforms this delegate that the completed or
// initiated edit of the profile.
- (void)didEditAutofillProfileFromSettings;

// Returns true if the profile satisfies minimum requirements to be migrated to
// the account.
- (BOOL)isMinimumAddress;

// Returns the record type of the stored profile.
- (autofill::AutofillProfile::RecordType)accountRecordType;

// Notifies the class that conforms this delegate that the profile needs to be
// migrated to account.
- (void)didTapMigrateToAccountButton;

@end

#endif  // IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_AUTOFILL_AUTOFILL_SETTINGS_PROFILE_EDIT_TABLE_VIEW_CONTROLLER_DELEGATE_H_
