// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/autofill/manual_fill/full_card_requester.h"

#import "components/autofill/core/browser/browser_autofill_manager.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/ui/autofill/create_card_unmask_prompt_view_bridge.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

FullCardRequester::FullCardRequester(UIViewController* base_view_controller,
                                     ChromeBrowserState* browser_state)
    : base_view_controller_(base_view_controller),
      unmask_controller_(browser_state->GetPrefs()) {}

void FullCardRequester::GetFullCard(
    const autofill::CreditCard& card,
    autofill::BrowserAutofillManager* autofill_manager,
    base::WeakPtr<autofill::payments::FullCardRequest::ResultDelegate>
        result_delegate) {
  DCHECK(autofill_manager);
  DCHECK(result_delegate);

  autofill::CreditCardCvcAuthenticator* cvc_authenticator =
      autofill_manager->client()->GetCvcAuthenticator();
  cvc_authenticator->GetFullCardRequest()->GetFullCard(
      card, autofill::AutofillClient::UnmaskCardReason::kPaymentRequest,
      result_delegate, AsWeakPtr());
}

void FullCardRequester::ShowUnmaskPrompt(
    const autofill::CreditCard& card,
    const autofill::CardUnmaskPromptOptions& card_unmask_prompt_options,
    base::WeakPtr<autofill::CardUnmaskDelegate> delegate) {
  unmask_controller_.ShowPrompt(
      base::BindOnce(&autofill::CreateCardUnmaskPromptViewBridge,
                     base::Unretained(&unmask_controller_),
                     base::Unretained(base_view_controller_)),
      card, card_unmask_prompt_options, delegate);
}

void FullCardRequester::OnUnmaskVerificationResult(
    autofill::AutofillClient::PaymentsRpcResult result) {
  unmask_controller_.OnVerificationResult(result);
}
