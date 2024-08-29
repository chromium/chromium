// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/ui_bundled/ios_chrome_payments_autofill_client.h"

#import <optional>

#import "base/check_deref.h"
#import "base/functional/callback.h"
#import "base/memory/ptr_util.h"
#import "base/memory/raw_ref.h"
#import "base/memory/weak_ptr.h"
#import "base/strings/sys_string_conversions.h"
#import "components/autofill/core/browser/autofill_progress_dialog_type.h"
#import "components/autofill/core/browser/metrics/payments/credit_card_save_metrics.h"
#import "components/autofill/core/browser/payments/autofill_error_dialog_context.h"
#import "components/autofill/core/browser/payments/autofill_save_card_delegate.h"
#import "components/autofill/core/browser/payments/autofill_save_card_ui_info.h"
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
#import "components/autofill/core/browser/ui/payments/autofill_progress_dialog_controller_impl.h"
#import "components/autofill/core/browser/ui/payments/card_unmask_authentication_selection_dialog_controller_impl.h"
#import "components/autofill/core/browser/ui/payments/card_unmask_prompt_view.h"
#import "components/autofill/core/browser/ui/payments/virtual_card_enroll_ui_model.h"
#import "components/autofill/core/common/autofill_payments_features.h"
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
    std::unique_ptr<AutofillSaveCardInfoBarDelegateIOS> delegate) {
  return std::make_unique<InfoBarIOS>(InfobarType::kInfobarTypeSaveCard,
                                      std::move(delegate));
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
              &client->GetPersonalDataManager()->payments_data_manager(),
              web_state->GetBrowserState()->IsOffTheRecord())),
      pref_service_(pref_service),
      web_state_(web_state) {}

IOSChromePaymentsAutofillClient::~IOSChromePaymentsAutofillClient() = default;

void IOSChromePaymentsAutofillClient::LoadRiskData(
    base::OnceCallback<void(const std::string&)> callback) {
  std::move(callback).Run(
      base::SysNSStringToUTF8(ios::provider::GetRiskData()));
}

void IOSChromePaymentsAutofillClient::ConfirmSaveCreditCardLocally(
    const CreditCard& card,
    SaveCreditCardOptions options,
    LocalSaveCardPromptCallback callback) {
  DCHECK(options.show_prompt);
  infobar_manager_->AddInfoBar(CreateSaveCardInfoBarMobile(
      std::make_unique<AutofillSaveCardInfoBarDelegateIOS>(
          AutofillSaveCardUiInfo::CreateForLocalSave(options, card),
          std::make_unique<AutofillSaveCardDelegate>(std::move(callback),
                                                     options))));
}

void IOSChromePaymentsAutofillClient::ConfirmSaveCreditCardToCloud(
    const CreditCard& card,
    const LegalMessageLines& legal_message_lines,
    SaveCreditCardOptions options,
    UploadSaveCardPromptCallback callback) {
  DCHECK(options.show_prompt);

  AccountInfo account_info =
      client_->GetIdentityManager()->FindExtendedAccountInfo(
          client_->GetIdentityManager()->GetPrimaryAccountInfo(
              signin::ConsentLevel::kSignin));
  infobar_manager_->AddInfoBar(CreateSaveCardInfoBarMobile(
      std::make_unique<AutofillSaveCardInfoBarDelegateIOS>(
          AutofillSaveCardUiInfo::CreateForUploadSave(
              options, card, legal_message_lines, account_info),
          std::make_unique<AutofillSaveCardDelegate>(std::move(callback),
                                                     options))));
}

void IOSChromePaymentsAutofillClient::CreditCardUploadCompleted(
    PaymentsRpcResult result,
    std::optional<OnConfirmationClosedCallback>
        on_confirmation_closed_callback) {
  const bool card_saved = result == PaymentsRpcResult::kSuccess;
  if (!base::FeatureList::IsEnabled(
          features::kAutofillEnableSaveCardLoadingAndConfirmation)) {
    autofill_metrics::LogCreditCardUploadConfirmationViewShownMetric(
        /*is_shown=*/false, card_saved);
    return;
  }
  if (client_->GetAutofillSaveCardInfoBarDelegateIOS()) {
    client_->GetAutofillSaveCardInfoBarDelegateIOS()->CreditCardUploadCompleted(
        card_saved, std::move(on_confirmation_closed_callback));
  }
  if (!card_saved) {
    // At this point, infobar would be dismissed but the omnibox icon could
    // still be tapped to re-show the infobar. Since the card upload has
    // failed, the save card infobar should not be re-shown, so the infobar is
    // removed here to remove the associated omnibox icon.
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

void IOSChromePaymentsAutofillClient::ShowVirtualCardEnrollDialog(
    const VirtualCardEnrollmentFields& virtual_card_enrollment_fields,
    base::OnceClosure accept_virtual_card_callback,
    base::OnceClosure decline_virtual_card_callback) {
  AutofillBottomSheetTabHelper* bottom_sheet_tab_helper =
      AutofillBottomSheetTabHelper::FromWebState(web_state_);
  std::unique_ptr<VirtualCardEnrollUiModel> model =
      std::make_unique<VirtualCardEnrollUiModel>(
          virtual_card_enrollment_fields);
  if (base::FeatureList::IsEnabled(
          features::kAutofillEnableVcnEnrollLoadingAndConfirmation)) {
    virtual_card_enroll_ui_model_ = model->GetWeakPtr();
  }
  bottom_sheet_tab_helper->ShowVirtualCardEnrollmentBottomSheet(
      std::move(model),
      VirtualCardEnrollmentCallbacks(std::move(accept_virtual_card_callback),
                                     std::move(decline_virtual_card_callback)));
}

void IOSChromePaymentsAutofillClient::VirtualCardEnrollCompleted(
    PaymentsRpcResult result) {
  if (!base::FeatureList::IsEnabled(
          features::kAutofillEnableVcnEnrollLoadingAndConfirmation)) {
    return;
  }
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
      pref_service_, card, card_unmask_prompt_options, delegate);
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
      NOTREACHED_IN_MIGRATION();
      return;
  }
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

void IOSChromePaymentsAutofillClient::OpenPromoCodeOfferDetailsURL(
    const GURL& url) {
  web_state_->OpenURL(web::WebState::OpenURLParams(
      url, web::Referrer(), WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui::PageTransition::PAGE_TRANSITION_AUTO_TOPLEVEL,
      /*is_renderer_initiated=*/false));
}

payments::MandatoryReauthManager*
IOSChromePaymentsAutofillClient::GetOrCreatePaymentsMandatoryReauthManager() {
  if (!payments_reauth_manager_) {
    payments_reauth_manager_ =
        std::make_unique<payments::MandatoryReauthManager>(&client_.get());
  }
  return payments_reauth_manager_.get();
}

}  // namespace autofill::payments
