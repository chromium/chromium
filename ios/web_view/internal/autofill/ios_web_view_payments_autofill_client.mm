// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/internal/autofill/ios_web_view_payments_autofill_client.h"

#import <optional>

#import "base/check_deref.h"
#import "base/containers/span.h"
#import "base/functional/callback.h"
#import "base/functional/callback_helpers.h"
#import "base/notimplemented.h"
#import "components/autofill/core/browser/autofill_progress_dialog_type.h"
#import "components/autofill/core/browser/data_manager/payments/payments_data_manager.h"
#import "components/autofill/core/browser/data_model/payments/bnpl_issuer.h"
#import "components/autofill/core/browser/payments/bnpl_util.h"
#import "components/autofill/core/browser/payments/card_unmask_challenge_option.h"
#import "components/autofill/core/browser/payments/credit_card_cvc_authenticator.h"
#import "components/autofill/core/browser/payments/credit_card_otp_authenticator.h"
#import "components/autofill/core/browser/payments/credit_card_risk_based_authenticator.h"
#import "components/autofill/core/browser/payments/mandatory_reauth_manager.h"
#import "components/autofill/core/browser/payments/payments_autofill_client.h"
#import "components/autofill/core/browser/payments/payments_network_interface.h"
#import "components/autofill/core/browser/payments/virtual_card_enrollment_manager.h"
#import "components/autofill/core/browser/ui/payments/autofill_progress_dialog_controller.h"
#import "components/autofill/core/browser/ui/payments/bnpl_tos_controller.h"
#import "components/autofill/core/browser/ui/payments/card_unmask_otp_input_dialog_controller.h"
#import "components/autofill/core/browser/ui/payments/card_unmask_prompt_controller.h"
#import "components/autofill/core/common/autofill_prefs.h"
#import "components/prefs/pref_service.h"
#import "ios/web/public/web_state.h"
#import "ios/web_view/internal/autofill/cwv_autofill_prefs.h"
#import "ios/web_view/internal/autofill/web_view_autofill_client_ios.h"
#import "ios/web_view/internal/web_view_browser_state.h"
#import "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#import "url/gurl.h"

namespace autofill::payments {

IOSWebViewPaymentsAutofillClient::IOSWebViewPaymentsAutofillClient(
    autofill::WebViewAutofillClientIOS* client,
    id<CWVAutofillClientIOSBridge> bridge,
    web::WebState* web_state)
    : client_(CHECK_DEREF(client)),
      bridge_(bridge),
      payments_network_interface_(
          std::make_unique<payments::PaymentsNetworkInterface>(
              base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
                  web_state->GetBrowserState()->GetURLLoaderFactory()),
              client->GetIdentityManager(),
              &client->GetPersonalDataManager().payments_data_manager(),
              web_state->GetBrowserState()->IsOffTheRecord())),
      web_state_(CHECK_DEREF(web_state)) {
  GetPaymentsDataManager().CleanupForCrbug445879524();
}

IOSWebViewPaymentsAutofillClient::~IOSWebViewPaymentsAutofillClient() = default;

void IOSWebViewPaymentsAutofillClient::LoadRiskData(
    base::OnceCallback<void(const std::string&)> callback) {
  [bridge_ loadRiskData:std::move(callback)];
}

void IOSWebViewPaymentsAutofillClient::ConfirmAccountNameFixFlow(
    base::OnceCallback<void(const std::u16string&)> callback) {}

void IOSWebViewPaymentsAutofillClient::ConfirmExpirationDateFixFlow(
    const CreditCard& card,
    base::OnceCallback<void(const std::u16string&, const std::u16string&)>
        callback) {}

bool IOSWebViewPaymentsAutofillClient::HasCreditCardScanFeature() const {
  return false;
}

void IOSWebViewPaymentsAutofillClient::ScanCreditCard(
    CreditCardScanCallback callback) {}

bool IOSWebViewPaymentsAutofillClient::LocalCardSaveIsSupported() {
  return false;
}

void IOSWebViewPaymentsAutofillClient::ShowSaveCreditCardLocally(
    const CreditCard& card,
    SaveCreditCardOptions options,
    LocalSaveCardPromptCallback callback) {}

void IOSWebViewPaymentsAutofillClient::ShowSaveCreditCardToCloud(
    const CreditCard& card,
    const LegalMessageLines& legal_message_lines,
    SaveCreditCardOptions options,
    UploadSaveCardPromptCallback callback) {
  DCHECK(options.show_prompt);
  [bridge_ showSaveCreditCardToCloud:card
                   legalMessageLines:legal_message_lines
               saveCreditCardOptions:options
                            callback:std::move(callback)];
}

void IOSWebViewPaymentsAutofillClient::CreditCardUploadCompleted(
    payments::PaymentsAutofillClient::PaymentsRpcResult result,
    std::optional<OnConfirmationClosedCallback>
        on_confirmation_closed_callback) {
  const bool card_saved =
      result == payments::PaymentsAutofillClient::PaymentsRpcResult::kSuccess;
  [bridge_
      handleCreditCardUploadCompleted:card_saved
                             callback:std::move(on_confirmation_closed_callback)
                                          .value_or(base::DoNothing())];
}

void IOSWebViewPaymentsAutofillClient::HideSaveCardPrompt() {}

void IOSWebViewPaymentsAutofillClient::ShowVirtualCardEnrollDialog(
    const VirtualCardEnrollmentFields& virtual_card_enrollment_fields,
    base::OnceClosure accept_virtual_card_callback,
    base::OnceClosure decline_virtual_card_callback) {
  if (GetPrefService()->GetBoolean(ios_web_view::kCWVAutofillVCNUsageEnabled)) {
    [bridge_
        showVirtualCardEnrollmentWithEnrollmentFields:
            virtual_card_enrollment_fields
                                       acceptCallback:
                                           std::move(
                                               accept_virtual_card_callback)
                                      declineCallback:
                                          std::move(
                                              decline_virtual_card_callback)];
  }
}

void IOSWebViewPaymentsAutofillClient::VirtualCardEnrollCompleted(
    PaymentsRpcResult result) {
  BOOL success = result == PaymentsRpcResult::kSuccess;

  [bridge_ handleVirtualCardEnrollmentResult:success];
}

void IOSWebViewPaymentsAutofillClient::OnCardDataAvailable(
    const FilledCardInformationBubbleOptions& options) {}

void IOSWebViewPaymentsAutofillClient::ConfirmSaveIbanLocally(
    const Iban& iban,
    bool should_show_prompt,
    SaveIbanPromptCallback callback) {}

void IOSWebViewPaymentsAutofillClient::ConfirmUploadIbanToCloud(
    const Iban& iban,
    LegalMessageLines legal_message_lines,
    bool should_show_prompt,
    SaveIbanPromptCallback callback) {}

void IOSWebViewPaymentsAutofillClient::IbanUploadCompleted(
    bool iban_saved,
    bool hit_max_strikes) {}

void IOSWebViewPaymentsAutofillClient::ShowAutofillProgressDialog(
    autofill::AutofillProgressDialogType autofill_progress_dialog_type,
    base::OnceClosure cancel_callback) {
  [bridge_ showAutofillProgressDialogOfType:autofill_progress_dialog_type
                             cancelCallback:std::move(cancel_callback)];
}

void IOSWebViewPaymentsAutofillClient::CloseAutofillProgressDialog(
    bool show_confirmation_before_closing,
    base::OnceClosure no_interactive_authentication_callback) {
  [bridge_
      closeAutofillProgressDialogWithConfirmation:
          show_confirmation_before_closing
                               completionCallback:
                                   std::move(
                                       no_interactive_authentication_callback)];
}

void IOSWebViewPaymentsAutofillClient::ShowCardUnmaskOtpInputDialog(
    CreditCard::RecordType card_type,
    const CardUnmaskChallengeOption& challenge_option,
    base::WeakPtr<OtpUnmaskDelegate> delegate) {
  if (GetPrefService()->GetBoolean(ios_web_view::kCWVAutofillVCNUsageEnabled)) {
    [bridge_ showCardUnmaskOtpInputDialogForCardType:card_type
                                     challengeOption:challenge_option
                                            delegate:delegate];
  }
}

void IOSWebViewPaymentsAutofillClient::OnUnmaskOtpVerificationResult(
    OtpUnmaskResult unmask_result) {
  if (GetPrefService()->GetBoolean(ios_web_view::kCWVAutofillVCNUsageEnabled)) {
    [bridge_ didReceiveUnmaskOtpVerificationResult:unmask_result];
  }
}

void IOSWebViewPaymentsAutofillClient::ShowUnmaskAuthenticatorSelectionDialog(
    const std::vector<CardUnmaskChallengeOption>& challenge_options,
    base::OnceCallback<void(const std::string&)>
        confirm_unmask_challenge_option_callback,
    base::OnceClosure cancel_unmasking_closure) {
  if (GetPrefService()->GetBoolean(ios_web_view::kCWVAutofillVCNUsageEnabled)) {
    [bridge_
        showUnmaskAuthenticatorSelectorWithOptions:challenge_options
                                    acceptCallback:
                                        std::move(
                                            confirm_unmask_challenge_option_callback)
                                    cancelCallback:
                                        std::move(cancel_unmasking_closure)];
  }
}

void IOSWebViewPaymentsAutofillClient::
    DismissUnmaskAuthenticatorSelectionDialog(bool server_success) {}

payments::PaymentsNetworkInterface*
IOSWebViewPaymentsAutofillClient::GetPaymentsNetworkInterface() {
  return payments_network_interface_.get();
}

MultipleRequestPaymentsNetworkInterface*
IOSWebViewPaymentsAutofillClient::GetMultipleRequestPaymentsNetworkInterface() {
  if (GetPrefService()->GetBoolean(ios_web_view::kCWVAutofillVCNUsageEnabled)) {
    if (!multiple_request_payments_network_interface_) {
      multiple_request_payments_network_interface_ =
          std::make_unique<payments::MultipleRequestPaymentsNetworkInterface>(
              client_->GetURLLoaderFactory(), *client_->GetIdentityManager(),
              web_state_->GetBrowserState()->IsOffTheRecord());
    }
    return multiple_request_payments_network_interface_.get();
  }
  return nullptr;
}

void IOSWebViewPaymentsAutofillClient::ShowAutofillErrorDialog(
    AutofillErrorDialogContext context) {}

PaymentsWindowManager*
IOSWebViewPaymentsAutofillClient::GetPaymentsWindowManager() {
  return nullptr;
}

void IOSWebViewPaymentsAutofillClient::ShowUnmaskPrompt(
    const CreditCard& card,
    const CardUnmaskPromptOptions& card_unmask_prompt_options,
    base::WeakPtr<CardUnmaskDelegate> delegate) {
  [bridge_ showUnmaskPromptForCard:card
           cardUnmaskPromptOptions:card_unmask_prompt_options
                          delegate:delegate];
}

void IOSWebViewPaymentsAutofillClient::OnUnmaskVerificationResult(
    payments::PaymentsAutofillClient::PaymentsRpcResult result) {
  [bridge_ didReceiveUnmaskVerificationResult:result];
}

std::unique_ptr<AutofillProgressDialogController>
IOSWebViewPaymentsAutofillClient::ExtractProgressDialogModel() {
  return nullptr;
}

std::unique_ptr<CardUnmaskOtpInputDialogController>
IOSWebViewPaymentsAutofillClient::ExtractOtpInputDialogModel() {
  return nullptr;
}

CardUnmaskPromptController*
IOSWebViewPaymentsAutofillClient::GetCardUnmaskPromptModel() {
  return nullptr;
}

VirtualCardEnrollmentManager*
IOSWebViewPaymentsAutofillClient::GetVirtualCardEnrollmentManager() {
  if (GetPrefService()->GetBoolean(ios_web_view::kCWVAutofillVCNUsageEnabled)) {
    if (!virtual_card_enrollment_manager_) {
      PaymentsNetworkInterfaceVariation payments_network_interface;
      if (base::FeatureList::IsEnabled(
              features::
                  kAutofillEnableMultipleRequestInVirtualCardDownstreamEnrollment)) {
        payments_network_interface =
            GetMultipleRequestPaymentsNetworkInterface();
      } else {
        payments_network_interface = GetPaymentsNetworkInterface();
      }
      virtual_card_enrollment_manager_ =
          std::make_unique<VirtualCardEnrollmentManager>(
              &client_->GetPersonalDataManager().payments_data_manager(),
              payments_network_interface, &client_.get());
    }
    return virtual_card_enrollment_manager_.get();
  }
  return nullptr;
}

CreditCardCvcAuthenticator&
IOSWebViewPaymentsAutofillClient::GetCvcAuthenticator() {
  if (!cvc_authenticator_) {
    cvc_authenticator_ =
        std::make_unique<CreditCardCvcAuthenticator>(&client_.get());
  }
  return *cvc_authenticator_;
}

CreditCardOtpAuthenticator*
IOSWebViewPaymentsAutofillClient::GetOtpAuthenticator() {
  if (GetPrefService()->GetBoolean(ios_web_view::kCWVAutofillVCNUsageEnabled)) {
    if (!otp_authenticator_) {
      otp_authenticator_ =
          std::make_unique<CreditCardOtpAuthenticator>(&client_.get());
    }
    return otp_authenticator_.get();
  }
  return nullptr;
}

CreditCardRiskBasedAuthenticator*
IOSWebViewPaymentsAutofillClient::GetRiskBasedAuthenticator() {
  if (!risk_based_authenticator_) {
    risk_based_authenticator_ =
        std::make_unique<CreditCardRiskBasedAuthenticator>(&client_.get());
  }
  return risk_based_authenticator_.get();
}

bool IOSWebViewPaymentsAutofillClient::IsRiskBasedAuthEffectivelyAvailable()
    const {
  return GetPrefService()->GetBoolean(
      ios_web_view::kCWVAutofillVCNUsageEnabled);
}

bool IOSWebViewPaymentsAutofillClient::IsMandatoryReauthEnabled() {
  return false;
}

void IOSWebViewPaymentsAutofillClient::ShowMandatoryReauthOptInPrompt(
    base::OnceClosure accept_mandatory_reauth_callback,
    base::OnceClosure cancel_mandatory_reauth_callback,
    base::RepeatingClosure close_mandatory_reauth_callback) {}

void IOSWebViewPaymentsAutofillClient::ShowMandatoryReauthOptInConfirmation() {}

bool IOSWebViewPaymentsAutofillClient::IsAutofillPaymentMethodsEnabled() const {
  return autofill::prefs::IsAutofillPaymentMethodsEnabled(GetPrefService());
}

void IOSWebViewPaymentsAutofillClient::DisablePaymentsAutofill() {
  NOTIMPLEMENTED();
}

IbanManager* IOSWebViewPaymentsAutofillClient::GetIbanManager() {
  return nullptr;
}

IbanAccessManager* IOSWebViewPaymentsAutofillClient::GetIbanAccessManager() {
  return nullptr;
}

MerchantPromoCodeManager*
IOSWebViewPaymentsAutofillClient::GetMerchantPromoCodeManager() {
  return nullptr;
}

void IOSWebViewPaymentsAutofillClient::OpenPromoCodeOfferDetailsURL(
    const GURL& url) {
  web_state_->OpenURL(web::WebState::OpenURLParams(
      url, web::Referrer(), WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui::PageTransition::PAGE_TRANSITION_AUTO_TOPLEVEL,
      /*is_renderer_initiated=*/false));
}

AutofillOfferManager*
IOSWebViewPaymentsAutofillClient::GetAutofillOfferManager() {
  return nullptr;
}

void IOSWebViewPaymentsAutofillClient::UpdateOfferNotification(
    const AutofillOfferData& offer,
    const OfferNotificationOptions& options) {}

void IOSWebViewPaymentsAutofillClient::DismissOfferNotification() {}

bool IOSWebViewPaymentsAutofillClient::ShowTouchToFillCreditCard(
    base::WeakPtr<TouchToFillDelegate> delegate,
    base::span<const Suggestion> suggestions) {
  return false;
}

bool IOSWebViewPaymentsAutofillClient::ShowTouchToFillIban(
    base::WeakPtr<TouchToFillDelegate> delegate,
    base::span<const Iban> ibans_to_suggest) {
  return false;
}

bool IOSWebViewPaymentsAutofillClient::ShowTouchToFillLoyaltyCard(
    base::WeakPtr<TouchToFillDelegate> delegate,
    std::vector<LoyaltyCard> loyalty_cards_to_suggest) {
  return false;
}

bool IOSWebViewPaymentsAutofillClient::OnPurchaseAmountExtracted(
    base::span<const payments::BnplIssuerContext> bnpl_issuer_contexts,
    std::optional<int64_t> extracted_amount,
    bool is_amount_supported_by_any_issuer,
    const std::optional<std::string>& app_locale,
    base::OnceCallback<void(BnplIssuer)> selected_issuer_callback,
    base::OnceClosure cancel_callback) {
  NOTREACHED();
}

bool IOSWebViewPaymentsAutofillClient::ShowTouchToFillProgress(
    base::OnceClosure cancel_callback) {
  return false;
}

bool IOSWebViewPaymentsAutofillClient::ShowTouchToFillBnplIssuers(
    base::span<const payments::BnplIssuerContext> bnpl_issuer_contexts,
    const std::string& app_locale,
    base::OnceCallback<void(BnplIssuer)> selected_issuer_callback,
    base::OnceClosure cancel_callback) {
  return false;
}

bool IOSWebViewPaymentsAutofillClient::ShowTouchToFillBnplTos(
    BnplTosModel bnpl_tos_model,
    base::OnceClosure accept_callback,
    base::OnceClosure cancel_callback) {
  return false;
}

bool IOSWebViewPaymentsAutofillClient::ShowTouchToFillError(
    const AutofillErrorDialogContext& context) {
  return false;
}

void IOSWebViewPaymentsAutofillClient::HideTouchToFillPaymentMethod() {}

void IOSWebViewPaymentsAutofillClient::SetTouchToFillVisible(bool visible) {}

PaymentsDataManager&
IOSWebViewPaymentsAutofillClient::GetPaymentsDataManager() {
  return client_->GetPersonalDataManager().payments_data_manager();
}

payments::MandatoryReauthManager*
IOSWebViewPaymentsAutofillClient::GetOrCreatePaymentsMandatoryReauthManager() {
  if (!payments_reauth_manager_) {
    payments_reauth_manager_ =
        std::make_unique<payments::MandatoryReauthManager>(&client_.get());
  }
  return payments_reauth_manager_.get();
}

payments::SaveAndFillManager*
IOSWebViewPaymentsAutofillClient::GetSaveAndFillManager() {
  return nullptr;
}

void IOSWebViewPaymentsAutofillClient::ShowCreditCardLocalSaveAndFillDialog(
    CardSaveAndFillDialogCallback callback) {}

void IOSWebViewPaymentsAutofillClient::ShowCreditCardUploadSaveAndFillDialog(
    const LegalMessageLines& legal_message_lines,
    CardSaveAndFillDialogCallback callback) {}

void IOSWebViewPaymentsAutofillClient::
    ShowCreditCardSaveAndFillPendingDialog() {}

void IOSWebViewPaymentsAutofillClient::HideCreditCardSaveAndFillDialog() {}

bool IOSWebViewPaymentsAutofillClient::IsTabModalPopupDeprecated() const {
  return false;
}

BnplStrategy* IOSWebViewPaymentsAutofillClient::GetBnplStrategy() {
  return nullptr;
}

BnplUiDelegate* IOSWebViewPaymentsAutofillClient::GetBnplUiDelegate() {
  return nullptr;
}

PrefService* IOSWebViewPaymentsAutofillClient::GetPrefService() const {
  return ios_web_view::WebViewBrowserState::FromBrowserState(
             web_state_->GetBrowserState())
      ->GetPrefs();
}

}  // namespace autofill::payments
