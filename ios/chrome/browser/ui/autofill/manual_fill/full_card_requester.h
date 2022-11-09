// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTOFILL_MANUAL_FILL_FULL_CARD_REQUESTER_H_
#define IOS_CHROME_BROWSER_UI_AUTOFILL_MANUAL_FILL_FULL_CARD_REQUESTER_H_


#import <UIKit/UIKit.h>

#include "base/memory/weak_ptr.h"
#include "components/autofill/core/browser/autofill_client.h"
#include "components/autofill/core/browser/payments/full_card_request.h"
#include "components/autofill/core/browser/ui/payments/card_unmask_prompt_controller_impl.h"

class ChromeBrowserState;

namespace autofill {
class BrowserAutofillManager;
class CreditCard;
struct CardUnmaskPromptOptions;
}  // namespace autofill

// Receives the full credit card details. Also displays the unmask prompt UI.
class FullCardRequester
    : public autofill::payments::FullCardRequest::UIDelegate,
      public base::SupportsWeakPtr<FullCardRequester> {
 public:
  FullCardRequester(UIViewController* base_view_controller,
                    ChromeBrowserState* browser_state);

  FullCardRequester(const FullCardRequester&) = delete;
  FullCardRequester& operator=(const FullCardRequester&) = delete;

  void GetFullCard(
      const autofill::CreditCard& card,
      autofill::BrowserAutofillManager* autofill_manager,
      base::WeakPtr<autofill::payments::FullCardRequest::ResultDelegate>
          result_delegate);

  // payments::FullCardRequest::UIDelegate:
  void ShowUnmaskPrompt(
      const autofill::CreditCard& card,
      const autofill::CardUnmaskPromptOptions& card_unmask_prompt_options,
      base::WeakPtr<autofill::CardUnmaskDelegate> delegate) override;
  void OnUnmaskVerificationResult(
      autofill::AutofillClient::PaymentsRpcResult result) override;

 private:
  __weak UIViewController* base_view_controller_;
  autofill::CardUnmaskPromptControllerImpl unmask_controller_;
};

#endif  // IOS_CHROME_BROWSER_UI_AUTOFILL_MANUAL_FILL_FULL_CARD_REQUESTER_H_
