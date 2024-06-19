// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_AUTOFILL_CONSTANTS_H_
#define IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_AUTOFILL_CONSTANTS_H_

#import <Foundation/Foundation.h>

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

#endif  // IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_AUTOFILL_CONSTANTS_H_
