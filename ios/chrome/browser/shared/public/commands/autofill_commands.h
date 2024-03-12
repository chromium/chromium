// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_AUTOFILL_COMMANDS_H_
#define IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_AUTOFILL_COMMANDS_H_

#import "components/plus_addresses/plus_address_types.h"

namespace autofill {
struct AutofillErrorDialogContext;
struct FormActivityParams;
struct VirtualCardEnrollUiModel;
}  // namespace autofill

// Commands related to the Autofill flows (passwords, addresses, payments etc).
@protocol AutofillCommands

// Shows the card unmask authentication flow.
- (void)showCardUnmaskAuthentication;

// Shows the password suggestion view controller.
- (void)showPasswordBottomSheet:(const autofill::FormActivityParams&)params;

// Shows the payments suggestion view controller.
- (void)showPaymentsBottomSheet:(const autofill::FormActivityParams&)params;

// Shows the plus address bottom sheet view controller.
- (void)showPlusAddressesBottomSheet;

// Shows a command to show the VCN enrollment Bottom Sheet.
- (void)showVirtualCardEnrollmentBottomSheet:
    (const autofill::VirtualCardEnrollUiModel&)model;

// Commands to manage the Autofill error dialog.
- (void)showAutofillErrorDialog:
    (autofill::AutofillErrorDialogContext)errorContext;
- (void)dismissAutofillErrorDialog;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_AUTOFILL_COMMANDS_H_
