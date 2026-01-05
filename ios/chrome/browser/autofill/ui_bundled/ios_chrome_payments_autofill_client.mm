// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/ui_bundled/ios_chrome_payments_autofill_client.h"

#import <optional>
#import <variant>

#import "base/check_deref.h"
#import "base/containers/span.h"
#import "base/functional/callback.h"
#import "base/functional/callback_helpers.h"
#import "base/memory/ptr_util.h"
#import "base/memory/raw_ref.h"
#import "base/memory/weak_ptr.h"
#import "base/notimplemented.h"
#import "base/strings/sys_string_conversions.h"
#import "components/autofill/core/browser/autofill_progress_dialog_type.h"
#import "components/autofill/core/browser/data_manager/payments/payments_data_manager.h"
#import "components/autofill/core/browser/data_model/payments/bnpl_issuer.h"
#import "components/autofill/core/browser/field_types.h"
#import "components/autofill/core/browser/metrics/payments/credit_card_save_metrics.h"
#import "components/autofill/core/browser/payments/autofill_error_dialog_context.h"
#import "components/autofill/core/browser/payments/autofill_save_card_delegate.h"
#import "components/autofill/core/browser/payments/autofill_save_card_ui_info.h"
#import "components/autofill/core/browser/payments/bnpl_util.h"
#import "components/autofill/core/browser/payments/card_unmask_challenge_option.h"
#import "components/autofill/core/browser/payments/credit_card_cvc_authenticator.h"
#import "components/autofill/core/browser/payments/credit_card_otp_authenticator.h"
#import "components/autofill/core/browser/payments/credit_card_risk_based_authenticator.h"
#import "components/autofill/core/browser/payments/mandatory_reauth_manager.h"
#import "components/autofill/core/browser/payments/otp_unmask_delegate.h"
#import "components/autofill/core/browser/payments/otp_unmask_result.h"
#import "components/autofill/core/browser/payments/payments_autofill_client.h"
#import "components/autofill/core/browser/payments/payments_network_interface.h"
#import "components/autofill/core/browser/payments/virtual_card_enroll_metrics_logger.h"
#import "components/autofill/core/browser/payments/virtual_card_enrollment_manager.h"
#import "components/autofill/core/browser/ui/payments/autofill_progress_dialog_controller.h"
#import "components/autofill/core/browser/ui/payments/autofill_progress_dialog_controller_impl.h"
#import "components/autofill/core/browser/ui/payments/bnpl_tos_controller.h"
#import "components/autofill/core/browser/ui/payments/card_unmask_authentication_selection_dialog_controller_impl.h"
#import "components/autofill/core/browser/ui/payments/card_unmask_otp_input_dialog_controller.h"
#import "components/autofill/core/browser/ui/payments/card_unmask_otp_input_dialog_controller_impl.h"
#import "components/autofill/core/browser/ui/payments/card_unmask_prompt_controller_impl.h"
#import "components/autofill/core/browser/ui/payments/card_unmask_prompt_view.h"
#import "components/autofill/core/browser/ui/payments/virtual_card_enroll_ui_model.h"
#import "components/autofill/core/common/autofill_payments_features.h"
#import "components/autofill/core/common/autofill_prefs.h"
#import "components/autofill/ios/browser/credit_card_save_metrics_ios.h"
#import "ios/chrome/browser/autofill/model/bottom_sheet/autofill_bottom_sheet_tab_helper.h"
#import "ios/chrome/browser/autofill/model/credit_card/autofill_save_card_infobar_delegate_ios.h"
#import "ios/chrome/browser/autofill/ui_bundled/card_expiration_date_fix_flow_view_bridge.h"
#import "ios/chrome/browser/autofill/ui_bundled/card_name_fix_flow_view_bridge.h"
#import "ios/chrome/browser/autofill/ui_bundled/card_unmask_prompt_view_bridge.h"
#import "ios/chrome/browser/autofill/ui_bundled/chrome_autofill_client_ios.h"
#import "ios/chrome/browser/infobars/model/infobar_ios.h"
#import "ios/chrome/browser/shared/public/commands/autofill_commands.h"
#import "ios/public/provider/chrome/browser/risk_data/risk_data_api.h"
#import "ios/web/public/web_state.h"
#import "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#import "url/gurl.h"

namespace autofill::payments {
namespace {

using PaymentsRpcResult = PaymentsAutofillClient::PaymentsRpcResult;

// Creates and returns an infobar for saving credit cards.
std::unique_ptr<infobars::InfoBar> CreateSaveCardInfoBarMobile(
    std::unique_ptr<AutofillSaveCardInfoBarDelegateIOS> delegate,
    InfobarType infobar_type) {
  return std::make_unique<InfoBarIOS>(infobar_type, std::move(delegate));
}
}  // namespace

IOSChromePaymentsAutofillClient::IOSChromePaymentsAutofillClient(
    autofill::ChromeAutofillClientIOS* client,
    web::WebState* web_state,
    infobars::InfoBarManager* infobar_manager,
    PrefService* pref_service)
    : client_(CHECK_DEREF(client)),
      infobar_manager_(CHECK_DEREF(infobar_manager)),
      payments_network_interface_(
          std::make_unique<payments::PaymentsNetworkInterface>(
              base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
                  web_state->GetBrowserState()->GetURLLoaderFactory()),
              client->GetIdentityManager(),
              &client->GetPersonalDataManager().payments_data_manager(),
              web_state->GetBrowserState()->IsOffTheRecord())),
      pref_service_(pref_service),
      web_state_(web_state) {}

IOSChromePaymentsAutofillClient::~IOSChromePaymentsAutofillClient() = default;

void IOSChromePaymentsAutofillClient::LoadRiskData(
    base::OnceCallback<void(const std::string&)> callback) {
  std::move(callback).Run(
      base::SysNSStringToUTF8(ios::provider::GetRiskData()));
}

void IOSChromePaymentsAutofillClient::ConfirmAccountNameFixFlow(
    base::OnceCallback<void(const std::u16string&)> callback) {
  std::u16string account_name = base::UTF8ToUTF16(
      client_->GetIdentityManager()
          ->FindExtendedAccountInfo(
              client_->GetIdentityManager()->GetPrimaryAccountInfo(
                  signin::ConsentLevel::kSignin))
          .full_name);

  card_name_fix_flow_controller_.Show(
      // CardNameFixFlowViewBridge manages its own lifetime, so
      // do not use std::unique_ptr<> here.
      new CardNameFixFlowViewBridge(&card_name_fix_flow_controller_,
                                    client_->base_view_controller()),
      account_name, std::move(callback));
}

void IOSChromePaymentsAutofillClient::ConfirmExpirationDateFixFlow(
    const CreditCard& card,
    base::OnceCallback<void(const std::u16string&, const std::u16string&)>
        callback) {
  card_expiration_date_fix_flow_controller_.Show(
      // CardExpirationDateFixFlowViewBridge manages its own lifetime,
      // so do not use std::unique_ptr<> here.
      new CardExpirationDateFixFlowViewBridge(
          &card_expiration_date_fix_flow_controller_,
          client_->base_view_controller()),
      card, std::move(callback));
}

bool IOSChromePaymentsAutofillClient::HasCreditCardScanFeature() const {
  return false;
}

void IOSChromePaymentsAutofillClient::ScanCreditCard(
    CreditCardScanCallback callback) {}

bool IOSChromePaymentsAutofillClient::LocalCardSaveIsSupported() {
  return true;
}

void IOSChromePaymentsAutofillClient::ShowSaveCreditCardLocally(
    const CreditCard& card,
    SaveCreditCardOptions options,
    LocalSaveCardPromptCallback callback) {
  DCHECK(options.show_prompt);
  CHECK(!card.GetInfo(CREDIT_CARD_EXP_MONTH, client_->GetAppLocale()).empty());
  CHECK(!card.GetInfo(CREDIT_CARD_EXP_4_DIGIT_YEAR, client_->GetAppLocale())
             .empty());

  ShowSaveCreditCard(
      AutofillSaveCardUiInfo::CreateForLocalSave(options, card),
      std::make_unique<AutofillSaveCardDelegate>(std::move(callback), options));
}


void IOSChromePaymentsAutofillClient::ShowSaveCreditCardToCloud(
    const CreditCard& card,
    const LegalMessageLines& legal_message_lines,
    SaveCreditCardOptions options,
    UploadSaveCardPromptCallback callback) {
  DCHECK(options.show_prompt);

  AccountInfo account_info =
      client_->GetIdentityManager()->FindExtendedAccountInfo(
          client_->GetIdentityManager()->GetPrimaryAccountInfo(
              signin::ConsentLevel::kSignin));

  ShowSaveCreditCard(
      AutofillSaveCardUiInfo::CreateForUploadSave(
          options, card, legal_message_lines, account_info),
      std::make_unique<AutofillSaveCardDelegate>(std::move(callback), options));
}

// TODO(crbug.com/413418918): Remove optional from
// `on_confirmation_closed_callback`.
void IOSChromePaymentsAutofillClient::CreditCardUploadCompleted(
    PaymentsRpcResult result,
    std::optional<OnConfirmationClosedCallback>
        on_confirmation_closed_callback) {
  const bool card_saved = result == PaymentsRpcResult::kSuccess;
  OnConfirmationClosedCallback callback =
      on_confirmation_closed_callback
          ? *std::exchange(on_confirmation_closed_callback, std::nullopt)
          : base::DoNothing();
  if (client_->GetAutofillSaveCardInfoBarDelegateIOS()) {
    client_->GetAutofillSaveCardInfoBarDelegateIOS()->CreditCardUploadCompleted(
        card_saved, std::move(callback));
  } else if (save_card_bottom_sheet_model_) {
    save_card_bottom_sheet_model_->CreditCardUploadCompleted(
        card_saved, std::move(callback));
  } else if (show_save_card_bottom_sheet_for_upload_) {
    // If a bottomsheet was showing before and was dismissed before getting the
    // save card result, the weak ref to save card bottomsheet model would be
    // invalid since model's lifecycle is same as the UI's and, the callback
    // would never be executed. Ensure callback runs if it is still pending.
    std::move(callback).Run();
  }

  if (!card_saved) {
    // At this point, infobar would be dismissed (if showing earlier) but the
    // omnibox icon could still be tapped to re-show the infobar. Since the card
    // upload has failed, the save card infobar should not be re-shown, so the
    // infobar is removed here to remove the associated omnibox icon. If a
    // bottomsheet was showing before, there wouldn't be an omnibox icon at all.
    client_->RemoveAutofillSaveCardInfoBar();

    // Here, `PaymentsRpcResult::kClientSideTimeout` indicates that the card
    // upload request is taking longer to finish. After the save card infobar
    // has been removed, no need to show an error dialog in this case since the
    // request may succeed on the server side.
    if (result == PaymentsRpcResult::kClientSideTimeout) {
      return;
    }
    autofill_metrics::LogCreditCardUploadConfirmationViewShownMetric(
        /*is_shown=*/true, /*is_card_uploaded=*/false);
    AutofillErrorDialogContext error_context;
    error_context.type = AutofillErrorDialogType::kCreditCardUploadError;
    ShowAutofillErrorDialog(std::move(error_context));
  }
}

void IOSChromePaymentsAutofillClient::HideSaveCardPrompt() {}

void IOSChromePaymentsAutofillClient::ShowVirtualCardEnrollDialog(
    const VirtualCardEnrollmentFields& virtual_card_enrollment_fields,
    base::OnceClosure accept_virtual_card_callback,
    base::OnceClosure decline_virtual_card_callback) {
  AutofillBottomSheetTabHelper* bottom_sheet_tab_helper =
      AutofillBottomSheetTabHelper::FromWebState(web_state_);
  std::unique_ptr<VirtualCardEnrollUiModel> model =
      std::make_unique<VirtualCardEnrollUiModel>(
          virtual_card_enrollment_fields);
  virtual_card_enroll_ui_model_ = model->GetWeakPtr();
  bottom_sheet_tab_helper->ShowVirtualCardEnrollmentBottomSheet(
      std::move(model),
      VirtualCardEnrollmentCallbacks(std::move(accept_virtual_card_callback),
                                     std::move(decline_virtual_card_callback)));
}

void IOSChromePaymentsAutofillClient::VirtualCardEnrollCompleted(
    PaymentsRpcResult result) {
  if (virtual_card_enroll_ui_model_) {
    virtual_card_enroll_ui_model_->SetEnrollmentProgress(
        result == PaymentsRpcResult::kSuccess
            ? VirtualCardEnrollUiModel::EnrollmentProgress::kEnrolled
            : VirtualCardEnrollUiModel::EnrollmentProgress::kFailed);
  }
  if (result != PaymentsRpcResult::kSuccess &&
      result != PaymentsRpcResult::kClientSideTimeout) {
    AutofillErrorDialogContext autofill_error_dialog_context;
    autofill_error_dialog_context.type =
        AutofillErrorDialogType::kVirtualCardEnrollmentTemporaryError;
    ShowAutofillErrorDialog(std::move(autofill_error_dialog_context));
  }
}

void IOSChromePaymentsAutofillClient::OnCardDataAvailable(
    const FilledCardInformationBubbleOptions& options) {}

void IOSChromePaymentsAutofillClient::ConfirmSaveIbanLocally(
    const Iban& iban,
    bool should_show_prompt,
    SaveIbanPromptCallback callback) {}

void IOSChromePaymentsAutofillClient::ConfirmUploadIbanToCloud(
    const Iban& iban,
    LegalMessageLines legal_message_lines,
    bool should_show_prompt,
    SaveIbanPromptCallback callback) {}

void IOSChromePaymentsAutofillClient::IbanUploadCompleted(
    bool iban_saved,
    bool hit_max_strikes) {}

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
    CreditCard::RecordType card_type,
    const CardUnmaskChallengeOption& challenge_option,
    base::WeakPtr<OtpUnmaskDelegate> delegate) {
  otp_input_dialog_controller_ =
      std::make_unique<CardUnmaskOtpInputDialogControllerImpl>(
          card_type, challenge_option, delegate);
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

PaymentsNetworkInterface*
IOSChromePaymentsAutofillClient::GetPaymentsNetworkInterface() {
  return payments_network_interface_.get();
}

MultipleRequestPaymentsNetworkInterface*
IOSChromePaymentsAutofillClient::GetMultipleRequestPaymentsNetworkInterface() {
  if (!multiple_request_payments_network_interface_) {
    multiple_request_payments_network_interface_ =
        std::make_unique<payments::MultipleRequestPaymentsNetworkInterface>(
            client_->GetURLLoaderFactory(), *client_->GetIdentityManager(),
            web_state_->GetBrowserState()->IsOffTheRecord());
  }
  return multiple_request_payments_network_interface_.get();
}

void IOSChromePaymentsAutofillClient::ShowAutofillErrorDialog(
    AutofillErrorDialogContext error_context) {
  [client_->commands_handler()
      showAutofillErrorDialog:std::move(error_context)];
}

PaymentsWindowManager*
IOSChromePaymentsAutofillClient::GetPaymentsWindowManager() {
  return nullptr;
}

void IOSChromePaymentsAutofillClient::ShowUnmaskPrompt(
    const CreditCard& card,
    const CardUnmaskPromptOptions& card_unmask_prompt_options,
    base::WeakPtr<CardUnmaskDelegate> delegate) {
  unmask_controller_ = std::make_unique<CardUnmaskPromptControllerImpl>(
      pref_service_, card, card_unmask_prompt_options, delegate);
  [client_->commands_handler() continueCardUnmaskWithCvcAuth];
}


void IOSChromePaymentsAutofillClient::OnUnmaskVerificationResult(
    PaymentsRpcResult result) {
  if (unmask_controller_) {
    unmask_controller_->OnVerificationResult(result);
  }

  // For VCN-related errors, on iOS we show a new error dialog instead of
  // updating the CVC unmask prompt with the error message.
  switch (result) {
    case PaymentsRpcResult::kVcnRetrievalPermanentFailure:
      ShowAutofillErrorDialog(
          AutofillErrorDialogContext::WithVirtualCardPermanentOrTemporaryError(
              /*is_permanent_error=*/true));
      break;
    case PaymentsRpcResult::kVcnRetrievalTryAgainFailure:
      ShowAutofillErrorDialog(
          AutofillErrorDialogContext::WithVirtualCardPermanentOrTemporaryError(
              /*is_permanent_error=*/false));
      break;
    case PaymentsRpcResult::kSuccess:
    case PaymentsRpcResult::kTryAgainFailure:
    case PaymentsRpcResult::kPermanentFailure:
    case PaymentsRpcResult::kNetworkError:
    case PaymentsRpcResult::kClientSideTimeout:
      // Do nothing
      break;
    case PaymentsRpcResult::kNone:
      NOTREACHED();
  }
}

std::unique_ptr<AutofillProgressDialogController>
IOSChromePaymentsAutofillClient::ExtractProgressDialogModel() {
  return std::move(progress_dialog_controller_);
}

std::unique_ptr<CardUnmaskOtpInputDialogController>
IOSChromePaymentsAutofillClient::ExtractOtpInputDialogModel() {
  return std::move(otp_input_dialog_controller_);
}

CardUnmaskPromptController*
IOSChromePaymentsAutofillClient::GetCardUnmaskPromptModel() {
  return unmask_controller_.get();
}

VirtualCardEnrollmentManager*
IOSChromePaymentsAutofillClient::GetVirtualCardEnrollmentManager() {
  if (!virtual_card_enrollment_manager_) {
    PaymentsNetworkInterfaceVariation payments_network_interface;
    if (base::FeatureList::IsEnabled(
            features::
                kAutofillEnableMultipleRequestInVirtualCardDownstreamEnrollment)) {
      payments_network_interface = GetMultipleRequestPaymentsNetworkInterface();
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

bool IOSChromePaymentsAutofillClient::IsRiskBasedAuthEffectivelyAvailable()
    const {
  return true;
}

bool IOSChromePaymentsAutofillClient::IsMandatoryReauthEnabled() {
  return GetPaymentsDataManager().IsPaymentMethodsMandatoryReauthEnabled();
}

bool IOSChromePaymentsAutofillClient::IsUsingCustomCardIconEnabled() const {
  return true;
}

void IOSChromePaymentsAutofillClient::ShowMandatoryReauthOptInPrompt(
    base::OnceClosure accept_mandatory_reauth_callback,
    base::OnceClosure cancel_mandatory_reauth_callback,
    base::RepeatingClosure close_mandatory_reauth_callback) {}

void IOSChromePaymentsAutofillClient::ShowMandatoryReauthOptInConfirmation() {}

bool IOSChromePaymentsAutofillClient::IsAutofillPaymentMethodsEnabled() const {
  return autofill::prefs::IsAutofillPaymentMethodsEnabled(pref_service_);
}

void IOSChromePaymentsAutofillClient::DisablePaymentsAutofill() {
  NOTIMPLEMENTED();
}

IbanManager* IOSChromePaymentsAutofillClient::GetIbanManager() {
  return nullptr;
}

IbanAccessManager* IOSChromePaymentsAutofillClient::GetIbanAccessManager() {
  return nullptr;
}

MerchantPromoCodeManager*
IOSChromePaymentsAutofillClient::GetMerchantPromoCodeManager() {
  return nullptr;
}

void IOSChromePaymentsAutofillClient::OpenPromoCodeOfferDetailsURL(
    const GURL& url) {
  web_state_->OpenURL(web::WebState::OpenURLParams(
      url, web::Referrer(), WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui::PageTransition::PAGE_TRANSITION_AUTO_TOPLEVEL,
      /*is_renderer_initiated=*/false));
}

AutofillOfferManager*
IOSChromePaymentsAutofillClient::GetAutofillOfferManager() {
  return nullptr;
}

void IOSChromePaymentsAutofillClient::UpdateOfferNotification(
    const AutofillOfferData& offer,
    const OfferNotificationOptions& options) {}

void IOSChromePaymentsAutofillClient::DismissOfferNotification() {}

bool IOSChromePaymentsAutofillClient::ShowTouchToFillCreditCard(
    base::WeakPtr<TouchToFillDelegate> delegate,
    base::span<const Suggestion> suggestions) {
  return false;
}

bool IOSChromePaymentsAutofillClient::ShowTouchToFillIban(
    base::WeakPtr<TouchToFillDelegate> delegate,
    base::span<const Iban> ibans_to_suggest) {
  return false;
}

bool IOSChromePaymentsAutofillClient::ShowTouchToFillLoyaltyCard(
    base::WeakPtr<TouchToFillDelegate> delegate,
    std::vector<LoyaltyCard> loyalty_cards_to_suggest) {
  return false;
}

bool IOSChromePaymentsAutofillClient::OnPurchaseAmountExtracted(
    base::span<const payments::BnplIssuerContext> bnpl_issuer_contexts,
    std::optional<int64_t> extracted_amount,
    bool is_amount_supported_by_any_issuer,
    const std::optional<std::string>& app_locale,
    base::OnceCallback<void(BnplIssuer)> selected_issuer_callback,
    base::OnceClosure cancel_callback) {
  NOTREACHED();
}

bool IOSChromePaymentsAutofillClient::ShowTouchToFillProgress(
    base::OnceClosure cancel_callback) {
  return false;
}

bool IOSChromePaymentsAutofillClient::ShowTouchToFillBnplIssuers(
    base::span<const payments::BnplIssuerContext> bnpl_issuer_contexts,
    const std::string& app_locale,
    base::OnceCallback<void(BnplIssuer)> selected_issuer_callback,
    base::OnceClosure cancel_callback) {
  return false;
}

bool IOSChromePaymentsAutofillClient::ShowTouchToFillBnplTos(
    BnplTosModel model,
    base::OnceClosure accept_callback,
    base::OnceClosure cancel_callback) {
  return false;
}

bool IOSChromePaymentsAutofillClient::ShowTouchToFillError(
    const AutofillErrorDialogContext& context) {
  return false;
}

void IOSChromePaymentsAutofillClient::HideTouchToFillPaymentMethod() {}

void IOSChromePaymentsAutofillClient::SetTouchToFillVisible(bool visible) {}

PaymentsDataManager& IOSChromePaymentsAutofillClient::GetPaymentsDataManager() {
  return client_->GetPersonalDataManager().payments_data_manager();
}

payments::MandatoryReauthManager*
IOSChromePaymentsAutofillClient::GetOrCreatePaymentsMandatoryReauthManager() {
  if (!payments_reauth_manager_) {
    payments_reauth_manager_ =
        std::make_unique<payments::MandatoryReauthManager>(&client_.get());
  }
  return payments_reauth_manager_.get();
}

payments::SaveAndFillManager*
IOSChromePaymentsAutofillClient::GetSaveAndFillManager() {
  return nullptr;
}

void IOSChromePaymentsAutofillClient::ShowCreditCardLocalSaveAndFillDialog(
    CardSaveAndFillDialogCallback callback) {}

void IOSChromePaymentsAutofillClient::ShowCreditCardUploadSaveAndFillDialog(
    const LegalMessageLines& legal_message_lines,
    CardSaveAndFillDialogCallback callback) {}

void IOSChromePaymentsAutofillClient::ShowCreditCardSaveAndFillPendingDialog() {
}

void IOSChromePaymentsAutofillClient::HideCreditCardSaveAndFillDialog() {}

bool IOSChromePaymentsAutofillClient::IsTabModalPopupDeprecated() const {
  return false;
}

BnplStrategy* IOSChromePaymentsAutofillClient::GetBnplStrategy() {
  return nullptr;
}

BnplUiDelegate* IOSChromePaymentsAutofillClient::GetBnplUiDelegate() {
  return nullptr;
}

void IOSChromePaymentsAutofillClient::ShowSaveCreditCard(
    AutofillSaveCardUiInfo ui_info,
    std::unique_ptr<AutofillSaveCardDelegate> save_card_delegate) {
  // For upload saves, cache whether a bottom sheet was shown. This state is
  // needed to correctly handle the asynchronous response in
  // CreditCardUploadCompleted.
  if (save_card_delegate->is_for_upload()) {
    show_save_card_bottom_sheet_for_upload_ = ui_info.is_for_bottom_sheet;
  }
  if (ui_info.is_for_bottom_sheet) {
    AutofillBottomSheetTabHelper* bottom_sheet_tab_helper =
        AutofillBottomSheetTabHelper::FromWebState(web_state_);
    std::unique_ptr<SaveCardBottomSheetModel> model =
        std::make_unique<SaveCardBottomSheetModel>(
            std::move(ui_info), std::move(save_card_delegate));
    save_card_bottom_sheet_model_ = model->GetWeakPtr();
    bottom_sheet_tab_helper->ShowSaveCardBottomSheet(std::move(model));
    return;
  }
  const bool is_cvc_save_only =
      save_card_delegate->GetSaveCreditCardOptions().card_save_type ==
      CardSaveType::kCvcSaveOnly;

  if (save_card_delegate->is_for_upload()
          ? base::FeatureList::IsEnabled(features::kAutofillSaveCardBottomSheet)
          : base::FeatureList::IsEnabled(
                features::kAutofillLocalSaveCardBottomSheet)) {
    if (!is_cvc_save_only) {
      // Logs the decision to not show the bottomsheet for users with flag
      // enabled.
      autofill_metrics::LogSaveCreditCardPromptResultIOS(
          autofill::autofill_metrics::SaveCreditCardPromptResultIOS::kNotShown,
          save_card_delegate->is_for_upload(),
          save_card_delegate->GetSaveCreditCardOptions(),
          autofill::autofill_metrics::SaveCreditCardPromptOverlayType::
              kBottomSheet);
    }
  }
  InfobarType infobar_type = is_cvc_save_only
                                 ? InfobarType::kInfobarTypeSaveCvc
                                 : InfobarType::kInfobarTypeSaveCard;

  infobar_manager_->AddInfoBar(CreateSaveCardInfoBarMobile(
      std::make_unique<AutofillSaveCardInfoBarDelegateIOS>(
          std::move(ui_info), std::move(save_card_delegate)),
      infobar_type));
}

}  // namespace autofill::payments
