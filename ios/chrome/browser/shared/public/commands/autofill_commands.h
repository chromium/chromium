// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_AUTOFILL_COMMANDS_H_
#define IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_AUTOFILL_COMMANDS_H_

#import "components/plus_addresses/core/browser/plus_address_types.h"

namespace autofill {
struct AutofillErrorDialogContext;
struct FormActivityParams;
class VirtualCardEnrollUiModel;
}  // namespace autofill
namespace web {
class WebState;
}

// Commands related to the Autofill flows (passwords, addresses, payments etc).
@protocol AutofillCommands

// Shows the card unmask authentication flow.
- (void)showCardUnmaskAuthentication;

// Continue the card unmask authentication flow with OTP auth.
- (void)continueCardUnmaskWithOtpAuth;

// Continue the card unmask flow with the CVC authentication input dialog.
- (void)continueCardUnmaskWithCvcAuth;

// Shows the credential suggestion bottom sheet view controller.
- (void)showCredentialBottomSheet:(const autofill::FormActivityParams&)params;

// Shows the payments suggestion bottom sheet view controller.
- (void)showPaymentsBottomSheet:(const autofill::FormActivityParams&)params;

// Shows the plus address bottom sheet view controller.
- (void)showPlusAddressesBottomSheet;

// Commands to manage save card bottomsheet.
- (void)showSaveCardBottomSheetOnOriginWebState:(web::WebState*)originWebState;
- (void)dismissSaveCardBottomSheet;

// Sends a command to show the VCN enrollment Bottom Sheet.
- (void)showVirtualCardEnrollmentBottomSheet:
            (std::unique_ptr<autofill::VirtualCardEnrollUiModel>)model
                              originWebState:(web::WebState*)originWebState;

// Sends a command to show the bottom sheet to edit an address.
- (void)showEditAddressBottomSheet;

// Sends a command to stop showing the bottom sheet to edit an address provided
// it's shown.
- (void)dismissEditAddressBottomSheet;

// Commands to manage the Autofill error dialog.
- (void)showAutofillErrorDialog:
    (autofill::AutofillErrorDialogContext)errorContext;
- (void)dismissAutofillErrorDialog;

// Commands to manage the Autofill progress dialog.
- (void)showAutofillProgressDialog;
- (void)dismissAutofillProgressDialog;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_AUTOFILL_COMMANDS_H_
