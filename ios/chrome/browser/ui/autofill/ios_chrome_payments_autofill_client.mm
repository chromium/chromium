// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/autofill/ios_chrome_payments_autofill_client.h"

#import "base/check_deref.h"
#import "base/functional/callback.h"
#import "base/memory/raw_ref.h"
#import "base/memory/weak_ptr.h"
#import "base/strings/sys_string_conversions.h"
#import "components/autofill/core/browser/autofill_progress_dialog_type.h"
#import "components/autofill/core/browser/payments/autofill_error_dialog_context.h"
#import "components/autofill/core/browser/payments/card_unmask_challenge_option.h"
#import "components/autofill/core/browser/payments/credit_card_cvc_authenticator.h"
#import "components/autofill/core/browser/payments/credit_card_otp_authenticator.h"
#import "components/autofill/core/browser/payments/credit_card_risk_based_authenticator.h"
#import "components/autofill/core/browser/payments/otp_unmask_delegate.h"
#import "components/autofill/core/browser/payments/otp_unmask_result.h"
#import "components/autofill/core/browser/payments/payments_network_interface.h"
#import "components/autofill/core/browser/payments/virtual_card_enrollment_manager.h"
#import "components/autofill/core/browser/ui/payments/autofill_progress_dialog_controller_impl.h"
#import "components/autofill/core/browser/ui/payments/card_unmask_authentication_selection_dialog_controller_impl.h"
#import "components/autofill/core/browser/ui/payments/card_unmask_prompt_view.h"
#import "ios/chrome/browser/autofill/model/bottom_sheet/autofill_bottom_sheet_tab_helper.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/shared/public/commands/autofill_commands.h"
#import "ios/chrome/browser/ui/autofill/card_unmask_prompt_view_bridge.h"
#import "ios/chrome/browser/ui/autofill/chrome_autofill_client_ios.h"
#import "ios/public/provider/chrome/browser/risk_data/risk_data_api.h"
#import "ios/web/public/web_state.h"
#import "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"

namespace autofill::payments {

IOSChromePaymentsAutofillClient::IOSChromePaymentsAutofillClient(
    autofill::ChromeAutofillClientIOS* client,
    ChromeBrowserState* browser_state,
    web::WebState* web_state)
    : client_(CHECK_DEREF(client)),
      payments_network_interface_(
          std::make_unique<payments::PaymentsNetworkInterface>(
              base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
                  browser_state->GetURLLoaderFactory()),
              client->GetIdentityManager(),
              &client->GetPersonalDataManager()->payments_data_manager(),
              browser_state->IsOffTheRecord())),
      browser_state_(browser_state),
      web_state_(web_state) {}

IOSChromePaymentsAutofillClient::~IOSChromePaymentsAutofillClient() = default;

void IOSChromePaymentsAutofillClient::LoadRiskData(
    base::OnceCallback<void(const std::string&)> callback) {
  std::move(callback).Run(
      base::SysNSStringToUTF8(ios::provider::GetRiskData()));
}

void IOSChromePaymentsAutofillClient::CreditCardUploadCompleted(
    bool card_saved) {
  NOTIMPLEMENTED();
}

void IOSChromePaymentsAutofillClient::ShowAutofillProgressDialog(
    AutofillProgressDialogType autofill_progress_dialog_type,
    base::OnceClosure cancel_callback) {
  progress_dialog_controller_ =
      std::make_unique<AutofillProgressDialogControllerImpl>(
          autofill_progress_dialog_type, std::move(cancel_callback));
  progress_dialog_controller_weak_ =
      progress_dialog_controller_->GetImplWeakPtr();
  [client_->commands_handler() showAutofillProgressDialog];
}

void IOSChromePaymentsAutofillClient::CloseAutofillProgressDialog(
    bool show_confirmation_before_closing,
    base::OnceClosure no_interactive_authentication_callback) {
  if (progress_dialog_controller_weak_) {
    progress_dialog_controller_weak_->DismissDialog(
        show_confirmation_before_closing,
        std::move(no_interactive_authentication_callback));
  }
}

void IOSChromePaymentsAutofillClient::ShowCardUnmaskOtpInputDialog(
    const CardUnmaskChallengeOption& challenge_option,
    base::WeakPtr<OtpUnmaskDelegate> delegate) {
  otp_input_dialog_controller_ =
      std::make_unique<CardUnmaskOtpInputDialogControllerImpl>(challenge_option,
                                                               delegate);
  otp_input_dialog_controller_weak_ =
      otp_input_dialog_controller_->GetImplWeakPtr();
  [client_->commands_handler() continueCardUnmaskWithOtpAuth];
}

void IOSChromePaymentsAutofillClient::OnUnmaskOtpVerificationResult(
    OtpUnmaskResult unmask_result) {
  if (otp_input_dialog_controller_weak_) {
    otp_input_dialog_controller_weak_->OnOtpVerificationResult(unmask_result);
  }
}

void IOSChromePaymentsAutofillClient::ShowAutofillErrorDialog(
    AutofillErrorDialogContext error_context) {
  [client_->commands_handler()
      showAutofillErrorDialog:std::move(error_context)];
}

PaymentsNetworkInterface*
IOSChromePaymentsAutofillClient::GetPaymentsNetworkInterface() {
  return payments_network_interface_.get();
}

void IOSChromePaymentsAutofillClient::ShowUnmaskPrompt(
    const CreditCard& card,
    const CardUnmaskPromptOptions& card_unmask_prompt_options,
    base::WeakPtr<CardUnmaskDelegate> delegate) {
  unmask_controller_ = std::make_unique<CardUnmaskPromptControllerImpl>(
      browser_state_->GetPrefs(), card, card_unmask_prompt_options, delegate);
  [client_->commands_handler() continueCardUnmaskWithCvcAuth];
}

void IOSChromePaymentsAutofillClient::ShowUnmaskAuthenticatorSelectionDialog(
    const std::vector<CardUnmaskChallengeOption>& challenge_options,
    base::OnceCallback<void(const std::string&)>
        confirm_unmask_challenge_option_callback,
    base::OnceClosure cancel_unmasking_closure) {
  AutofillBottomSheetTabHelper* bottom_sheet_tab_helper =
      AutofillBottomSheetTabHelper::FromWebState(web_state_);
  auto controller = std::make_unique<
      autofill::CardUnmaskAuthenticationSelectionDialogControllerImpl>(
      challenge_options, std::move(confirm_unmask_challenge_option_callback),
      std::move(cancel_unmasking_closure));
  card_unmask_authentication_selection_controller_ = controller->GetWeakPtr();
  bottom_sheet_tab_helper->ShowCardUnmaskAuthenticationSelection(
      std::move(controller));
}

void IOSChromePaymentsAutofillClient::DismissUnmaskAuthenticatorSelectionDialog(
    bool server_success) {
  if (card_unmask_authentication_selection_controller_) {
    card_unmask_authentication_selection_controller_
        ->DismissDialogUponServerProcessedAuthenticationMethodRequest(
            server_success);
  }
}

void IOSChromePaymentsAutofillClient::OnUnmaskVerificationResult(
    AutofillClient::PaymentsRpcResult result) {
  if (unmask_controller_) {
    unmask_controller_->OnVerificationResult(result);
  }

  // For VCN-related errors, on iOS we show a new error dialog instead of
  // updating the CVC unmask prompt with the error message.
  switch (result) {
    case AutofillClient::PaymentsRpcResult::kVcnRetrievalPermanentFailure:
      ShowAutofillErrorDialog(
          AutofillErrorDialogContext::WithVirtualCardPermanentOrTemporaryError(
              /*is_permanent_error=*/true));
      break;
    case AutofillClient::PaymentsRpcResult::kVcnRetrievalTryAgainFailure:
      ShowAutofillErrorDialog(
          AutofillErrorDialogContext::WithVirtualCardPermanentOrTemporaryError(
              /*is_permanent_error=*/false));
      break;
    case AutofillClient::PaymentsRpcResult::kSuccess:
    case AutofillClient::PaymentsRpcResult::kTryAgainFailure:
    case AutofillClient::PaymentsRpcResult::kPermanentFailure:
    case AutofillClient::PaymentsRpcResult::kNetworkError:
      // Do nothing
      break;
    case AutofillClient::PaymentsRpcResult::kNone:
      NOTREACHED();
      return;
  }
}

VirtualCardEnrollmentManager*
IOSChromePaymentsAutofillClient::GetVirtualCardEnrollmentManager() {
  if (!virtual_card_enrollment_manager_) {
    virtual_card_enrollment_manager_ =
        std::make_unique<VirtualCardEnrollmentManager>(
            client_->GetPersonalDataManager(), GetPaymentsNetworkInterface(),
            &client_.get());
  }

  return virtual_card_enrollment_manager_.get();
}

CreditCardCvcAuthenticator&
IOSChromePaymentsAutofillClient::GetCvcAuthenticator() {
  if (!cvc_authenticator_) {
    cvc_authenticator_ =
        std::make_unique<CreditCardCvcAuthenticator>(&client_.get());
  }
  return *cvc_authenticator_;
}

CreditCardOtpAuthenticator*
IOSChromePaymentsAutofillClient::GetOtpAuthenticator() {
  if (!otp_authenticator_) {
    otp_authenticator_ =
        std::make_unique<CreditCardOtpAuthenticator>(&client_.get());
  }
  return otp_authenticator_.get();
}

CreditCardRiskBasedAuthenticator*
IOSChromePaymentsAutofillClient::GetRiskBasedAuthenticator() {
  if (!risk_based_authenticator_) {
    risk_based_authenticator_ =
        std::make_unique<CreditCardRiskBasedAuthenticator>(&client_.get());
  }
  return risk_based_authenticator_.get();
}

}  // namespace autofill::payments
