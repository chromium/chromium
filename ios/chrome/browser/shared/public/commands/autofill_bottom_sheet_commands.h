// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_AUTOFILL_BOTTOM_SHEET_COMMANDS_H_
#define IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_AUTOFILL_BOTTOM_SHEET_COMMANDS_H_

#import "components/plus_addresses/plus_address_types.h"

namespace autofill {
struct FormActivityParams;
}  // namespace autofill

// Commands related to the passwords bottom sheet.
@protocol AutofillBottomSheetCommands

// Shows the password suggestion view controller.
- (void)showPasswordBottomSheet:(const autofill::FormActivityParams&)params;

// Shows the payments suggestion view controller.
- (void)showPaymentsBottomSheet:(const autofill::FormActivityParams&)params;

// Shows the plus address bottom sheet view controller.
- (void)showPlusAddressesBottomSheet;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_AUTOFILL_BOTTOM_SHEET_COMMANDS_H_
