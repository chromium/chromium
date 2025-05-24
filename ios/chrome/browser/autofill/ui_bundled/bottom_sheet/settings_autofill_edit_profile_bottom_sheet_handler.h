// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_BOTTOM_SHEET_SETTINGS_AUTOFILL_EDIT_PROFILE_BOTTOM_SHEET_HANDLER_H_
#define IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_BOTTOM_SHEET_SETTINGS_AUTOFILL_EDIT_PROFILE_BOTTOM_SHEET_HANDLER_H_

#import "ios/chrome/browser/autofill/ui_bundled/bottom_sheet/autofill_edit_profile_bottom_sheet_handler.h"

namespace autofill {
class AddressDataManager;
}  // namespace autofill

// Handler that provides the `AutofillEditProfileBottomSheetCoordinator` with
// the logic that is specific to adding an address manually through settings.
@interface SettingsAutofillEditProfileBottomSheetHandler
    : NSObject <AutofillEditProfileBottomSheetHandler>

// Initializes the handler with the address data manager and user email.
// `addressDataManager` must not be nil.
- (instancetype)initWithAddressDataManager:
                    (autofill::AddressDataManager*)addressDataManager
                                 userEmail:(NSString*)userEmail
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_BOTTOM_SHEET_SETTINGS_AUTOFILL_EDIT_PROFILE_BOTTOM_SHEET_HANDLER_H_
