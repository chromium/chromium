// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_IOS_CHROME_PAYMENTS_AUTOFILL_CLIENT_H_
#define IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_IOS_CHROME_PAYMENTS_AUTOFILL_CLIENT_H_

#import <memory>
#include <optional>

#import "base/functional/callback.h"
#import "base/memory/raw_ref.h"
#import "base/memory/weak_ptr.h"
#import "components/autofill/core/browser/autofill_progress_dialog_type.h"
#import "components/autofill/core/browser/payments/payments_autofill_client.h"
#import "components/autofill/core/browser/ui/payments/autofill_progress_dialog_controller_impl.h"
#include "components/autofill/core/browser/ui/payments/card_expiration_date_fix_flow_controller_impl.h"
#include "components/autofill/core/browser/ui/payments/card_name_fix_flow_controller_impl.h"
#import "components/autofill/core/browser/ui/payments/card_unmask_otp_input_dialog_controller_impl.h"
#import "components/autofill/core/browser/ui/payments/card_unmask_prompt_controller_impl.h"
#import "components/infobars/core/infobar_manager.h"

class GURL;

namespace web {
class WebState;
}  // namespace web

namespace autofill {

struct AutofillErrorDialogContext;
struct CardUnmaskChallengeOption;
class CardUnmaskAuthenticationSelectionDialogControllerImpl;
class ChromeAutofillClientIOS;
class CreditCardCvcAuthenticator;
class CreditCardOtpAuthenticator;
class CreditCardRiskBasedAuthenticator;
class OtpUnmaskDelegate;
enum class OtpUnmaskResult;
struct VirtualCardEnrollmentFields;
class VirtualCardEnrollmentManager;
class VirtualCardEnrollUiModel;

namespace payments {

class MandatoryReauthManager;

// Chrome iOS implementation of PaymentsAutofillClient. Owned by the
// ChromeAutofillClientIOS. Created lazily in the ChromeAutofillClientIOS when
// it is needed.
class IOSChromePaymentsAutofillClient : public PaymentsAutofillClient {
 public:
  explicit IOSChromePaymentsAutofillClient(
      autofill::ChromeAutofillClientIOS* client,
      web::WebState* web_state,
      infobars::InfoBarManager* infobar_manager,
      PrefService* pref_service);
  IOSChromePaymentsAutofillClient(const IOSChromePaymentsAutofillClient&) =
      delete;
  IOSChromePaymentsAutofillClient& operator=(
      const IOSChromePaymentsAutofillClient&) = delete;
  ~IOSChromePaymentsAutofillClient() override;

  // RiskDataLoader:
  void LoadRiskData(
      base::OnceCallback<void(const std::string&)> callback) override;

  // PaymentsAutofillClient:
  void ConfirmSaveCreditCardLocally(
      const CreditCard& card,
      SaveCreditCardOptions options,
      LocalSaveCardPromptCallback callback) override;
  void ConfirmSaveCreditCardToCloud(
      const CreditCard& card,
      const LegalMessageLines& legal_message_lines,
      SaveCreditCardOptions options,
      UploadSaveCardPromptCallback callback) override;
  void CreditCardUploadCompleted(
      payments::PaymentsAutofillClient::PaymentsRpcResult result,
      std::optional<OnConfirmationClosedCallback>
          on_confirmation_closed_callback) override;
  void ShowVirtualCardEnrollDialog(
      const VirtualCardEnrollmentFields& virtual_card_enrollment_fields,
      base::OnceClosure accept_virtual_card_callback,
      base::OnceClosure decline_virtual_card_callback) override;
  void VirtualCardEnrollCompleted(PaymentsRpcResult result) override;
  void ShowCardUnmaskOtpInputDialog(
      const CardUnmaskChallengeOption& challenge_option,
      base::WeakPtr<OtpUnmaskDelegate> delegate) override;
  void OnUnmaskOtpVerificationResult(OtpUnmaskResult unmask_result) override;
  void ShowAutofillErrorDialog(
      AutofillErrorDialogContext error_context) override;
  PaymentsNetworkInterface* GetPaymentsNetworkInterface() override;
  void ShowAutofillProgressDialog(
      AutofillProgressDialogType autofill_progress_dialog_type,
      base::OnceClosure cancel_callback) override;
  void CloseAutofillProgressDialog(
      bool show_confirmation_before_closing,
      base::OnceClosure no_interactive_authentication_callback) override;
  void ShowUnmaskPrompt(
      const CreditCard& card,
      const CardUnmaskPromptOptions& card_unmask_prompt_options,
      base::WeakPtr<CardUnmaskDelegate> delegate) override;

  void ShowUnmaskAuthenticatorSelectionDialog(
      const std::vector<CardUnmaskChallengeOption>& challenge_options,
      base::OnceCallback<void(const std::string&)>
          confirm_unmask_challenge_option_callback,
      base::OnceClosure cancel_unmasking_closure) override;
  void DismissUnmaskAuthenticatorSelectionDialog(bool server_success) override;
  void OnUnmaskVerificationResult(
      payments::PaymentsAutofillClient::PaymentsRpcResult result) override;
  void ConfirmAccountNameFixFlow(
      base::OnceCallback<void(const std::u16string&)> callback) override;
  void ConfirmExpirationDateFixFlow(
      const CreditCard& card,
      base::OnceCallback<void(const std::u16string&, const std::u16string&)>
          callback) override;
  VirtualCardEnrollmentManager* GetVirtualCardEnrollmentManager() override;
  CreditCardCvcAuthenticator& GetCvcAuthenticator() override;
  CreditCardOtpAuthenticator* GetOtpAuthenticator() override;
  CreditCardRiskBasedAuthenticator* GetRiskBasedAuthenticator() override;
  void OpenPromoCodeOfferDetailsURL(const GURL& url) override;
  payments::MandatoryReauthManager* GetOrCreatePaymentsMandatoryReauthManager()
      override;

  std::unique_ptr<AutofillProgressDialogControllerImpl>
  GetProgressDialogModel() {
    return std::move(progress_dialog_controller_);
  }
  CardUnmaskPromptControllerImpl* GetCardUnmaskPromptModel() {
    return unmask_controller_.get();
  }

  std::unique_ptr<CardUnmaskOtpInputDialogControllerImpl>
  GetOtpInputDialogModel() {
    return std::move(otp_input_dialog_controller_);
  }

 private:
  const raw_ref<autofill::ChromeAutofillClientIOS> client_;

  const raw_ref<infobars::InfoBarManager> infobar_manager_;

  std::unique_ptr<PaymentsNetworkInterface> payments_network_interface_;

  // TODO(crbug.com/40937065): Make these member variables as const raw_refs.
  const raw_ptr<PrefService> pref_service_;
  const raw_ptr<web::WebState> web_state_;
  std::unique_ptr<CardUnmaskPromptControllerImpl> unmask_controller_;

  // The unique_ptr reference is only temporarily valid until the corresponding
  // coordinator takes the ownership of the model controller from this class.
  // The WeakPtr reference should be used to invoke the model controller from
  // this class.
  std::unique_ptr<AutofillProgressDialogControllerImpl>
      progress_dialog_controller_;
  base::WeakPtr<AutofillProgressDialogControllerImpl>
      progress_dialog_controller_weak_;

  std::unique_ptr<CardUnmaskOtpInputDialogControllerImpl>
      otp_input_dialog_controller_;
  base::WeakPtr<CardUnmaskOtpInputDialogControllerImpl>
      otp_input_dialog_controller_weak_;

  std::unique_ptr<VirtualCardEnrollmentManager>
      virtual_card_enrollment_manager_;

  base::WeakPtr<VirtualCardEnrollUiModel> virtual_card_enroll_ui_model_;

  std::unique_ptr<CreditCardCvcAuthenticator> cvc_authenticator_;

  std::unique_ptr<CreditCardOtpAuthenticator> otp_authenticator_;

  base::WeakPtr<CardUnmaskAuthenticationSelectionDialogControllerImpl>
      card_unmask_authentication_selection_controller_;

  std::unique_ptr<CreditCardRiskBasedAuthenticator> risk_based_authenticator_;

  CardNameFixFlowControllerImpl card_name_fix_flow_controller_;

  CardExpirationDateFixFlowControllerImpl
      card_expiration_date_fix_flow_controller_;

  std::unique_ptr<payments::MandatoryReauthManager> payments_reauth_manager_;
};

}  // namespace payments

}  // namespace autofill

#endif  // IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_IOS_CHROME_PAYMENTS_AUTOFILL_CLIENT_H_
