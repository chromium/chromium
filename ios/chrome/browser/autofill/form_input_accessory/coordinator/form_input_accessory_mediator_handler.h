// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_FORM_INPUT_ACCESSORY_COORDINATOR_FORM_INPUT_ACCESSORY_MEDIATOR_HANDLER_H_
#define IOS_CHROME_BROWSER_AUTOFILL_FORM_INPUT_ACCESSORY_COORDINATOR_FORM_INPUT_ACCESSORY_MEDIATOR_HANDLER_H_

#import <UIKit/UIKit.h>

namespace autofill {
class CreditCard;
class AutofillProfile;
}  // namespace autofill

namespace manual_fill {
enum class ManualFillDataType;
}

namespace password_manager {
struct CredentialUIEntry;
}

enum class SuggestionFeatureForIPH;

// Handler in charge of accessory mediator events.
@protocol FormInputAccessoryMediatorHandler <NSObject>

// The mediator detected that the keyboard input view should be reset.
- (void)resetFormInputView;

// Dismisses the popover (tablet only).
- (void)dismissPopover;

// The mediator shows autofill suggestion tip if needed.
- (void)showAutofillSuggestionIPHIfNeededFor:
    (SuggestionFeatureForIPH)featureForIPH;

// The mediator notifies that the autofill suggestion has been selected.
- (void)notifyAutofillSuggestionWithIPHSelectedFor:
    (SuggestionFeatureForIPH)featureForIPH;

// The mediator requests to open the password details in edit mode.
- (void)openPasswordDetailsInEditMode:
    (const password_manager::CredentialUIEntry&)credential;

// The mediator requests to open the credit card details in edit mode.
- (void)openCreditCardDetails:(const autofill::CreditCard&)card
                   inEditMode:(BOOL)editMode;

// The mediator requests to open the address details in edit mode.
- (void)openAddressDetailsInEditModeForSuggestion:
    (const autofill::AutofillProfile&)address;

@end

#endif  // IOS_CHROME_BROWSER_AUTOFILL_FORM_INPUT_ACCESSORY_COORDINATOR_FORM_INPUT_ACCESSORY_MEDIATOR_HANDLER_H_
