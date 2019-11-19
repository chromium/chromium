// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_PAYMENTS_FULL_CARD_REQUESTER_H_
#define IOS_CHROME_BROWSER_UI_PAYMENTS_FULL_CARD_REQUESTER_H_

#include <string>

#import <UIKit/UIKit.h>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "components/autofill/core/browser/autofill_client.h"
#include "components/autofill/core/browser/payments/full_card_request.h"
#include "components/autofill/core/browser/ui/payments/card_unmask_prompt_controller_impl.h"

namespace autofill {
class AutofillManager;
class CreditCard;
}  // namespace autofill

namespace ios {
class ChromeBrowserState;
}  // namespace ios

// Receives the full credit card details. Also displays the unmask prompt UI.
class FullCardRequester
    : public autofill::payments::FullCardRequest::UIDelegate,
      public base::SupportsWeakPtr<FullCardRequester> {
 public:
  FullCardRequester(UIViewController* base_view_controller,
                    ios::ChromeBrowserState* browser_state);

  void GetFullCard(
      const autofill::CreditCard& card,
      autofill::AutofillManager* autofill_manager,
      base::WeakPtr<autofill::payments::FullCardRequest::ResultDelegate>
          result_delegate);

  // payments::FullCardRequest::UIDelegate:
  void ShowUnmaskPrompt(
      const autofill::CreditCard& card,
      autofill::AutofillClient::UnmaskCardReason reason,
      base::WeakPtr<autofill::CardUnmaskDelegate> delegate) override;
  void OnUnmaskVerificationResult(
      autofill::AutofillClient::PaymentsRpcResult result) override;

 private:
  __weak UIViewController* base_view_controller_;
  autofill::CardUnmaskPromptControllerImpl unmask_controller_;

  DISALLOW_COPY_AND_ASSIGN(FullCardRequester);
};

#endif  // IOS_CHROME_BROWSER_UI_PAYMENTS_FULL_CARD_REQUESTER_H_
