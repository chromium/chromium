// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_ADDRESS_EDITOR_AUTOFILL_EDIT_PROFILE_HANDLER_H_
#define IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_ADDRESS_EDITOR_AUTOFILL_EDIT_PROFILE_HANDLER_H_

#import <Foundation/Foundation.h>

#import "components/autofill/core/browser/data_model/addresses/autofill_profile.h"
#import "ios/chrome/browser/autofill/ui_bundled/address_editor/autofill_constants.h"

// Protocol for handling context-specific behaviours in relation to
// adding/editing an address within the
// AutofillEditProfileCoordinator. The two possible contexts being:
//  1) Infobar-triggered address edit (initiated from a webpage form fills).
//  2) Manual address entry from Settings.
@protocol AutofillEditProfileHandler <NSObject>

// Called when the sheet is cancelled by the user, either by tapping the cancel
// button or swiping the sheet down.
- (void)didCancelSheetView;

// Called when the user taps the sheet's 'Save' button.
- (void)didSaveProfile:(autofill::AutofillProfile*)profile;

// Returns if the autofill profile associated with the current sheet will be
// migrated to the Google Account.
- (BOOL)isMigrationToAccount;

// Returns the autofill profile associated with the current sheet.
- (std::unique_ptr<autofill::AutofillProfile>)autofillProfile;

// Returns the AutofillSaveProfilePromptMode associated with the current sheet.
- (AutofillSaveProfilePromptMode)saveProfilePromptMode;

// Returns the user's email address.
- (NSString*)userEmail;

// Returns if the address is being added manually.
- (BOOL)addingManualAddress;

@end

#endif  // IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_ADDRESS_EDITOR_AUTOFILL_EDIT_PROFILE_HANDLER_H_
