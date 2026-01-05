// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_IOS_CHROME_PAYMENTS_AUTOFILL_CLIENT_H_
#define IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_IOS_CHROME_PAYMENTS_AUTOFILL_CLIENT_H_

#import <memory>
#include <optional>

#import "base/containers/span.h"
#import "base/functional/callback.h"
#import "base/memory/raw_ref.h"
#import "base/memory/weak_ptr.h"
#import "components/autofill/core/browser/autofill_progress_dialog_type.h"
#import "components/autofill/core/browser/payments/autofill_save_card_delegate.h"
#import "components/autofill/core/browser/payments/autofill_save_card_ui_info.h"
#import "components/autofill/core/browser/payments/multiple_request_payments_network_interface.h"
#import "components/autofill/core/browser/payments/payments_autofill_client.h"
#import "components/autofill/core/browser/ui/payments/card_expiration_date_fix_flow_controller_impl.h"
#import "components/autofill/core/browser/ui/payments/card_name_fix_flow_controller_impl.h"
#import "components/infobars/core/infobar_manager.h"

class GURL;

namespace web {
class WebState;
}  // namespace web

namespace autofill {

struct AutofillErrorDialogContext;
class AutofillProgressDialogController;
class AutofillProgressDialogControllerImpl;
class BnplIssuer;
struct BnplTosModel;
struct CardUnmaskChallengeOption;
class CardUnmaskAuthenticationSelectionDialogControllerImpl;
class CardUnmaskOtpInputDialogController;
class CardUnmaskOtpInputDialogControllerImpl;
class CardUnmaskPromptController;
class CardUnmaskPromptControllerImpl;
class ChromeAutofillClientIOS;
class CreditCardCvcAuthenticator;
class CreditCardOtpAuthenticator;
class CreditCardRiskBasedAuthenticator;
class OtpUnmaskDelegate;
enum class OtpUnmaskResult;
class PaymentsDataManager;
class SaveCardBottomSheetModel;
struct VirtualCardEnrollmentFields;
class VirtualCardEnrollmentManager;
class VirtualCardEnrollUiModel;

namespace payments {

struct BnplIssuerContext;
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
  void ConfirmAccountNameFixFlow(
      base::OnceCallback<void(const std::u16string&)> callback) override;
  void ConfirmExpirationDateFixFlow(
      const CreditCard& card,
      base::OnceCallback<void(const std::u16string&, const std::u16string&)>
          callback) override;
  bool HasCreditCardScanFeature() const override;
  void ScanCreditCard(CreditCardScanCallback callback) override;
  bool LocalCardSaveIsSupported() override;
  void ShowSaveCreditCardLocally(const CreditCard& card,
                                 SaveCreditCardOptions options,
                                 LocalSaveCardPromptCallback callback) override;
  void ShowSaveCreditCardToCloud(
      const CreditCard& card,
      const LegalMessageLines& legal_message_lines,
      SaveCreditCardOptions options,
      UploadSaveCardPromptCallback callback) override;
  void CreditCardUploadCompleted(
      payments::PaymentsAutofillClient::PaymentsRpcResult result,
      std::optional<OnConfirmationClosedCallback>
          on_confirmation_closed_callback) override;
  void HideSaveCardPrompt() override;
  void ShowVirtualCardEnrollDialog(
      const VirtualCardEnrollmentFields& virtual_card_enrollment_fields,
      base::OnceClosure accept_virtual_card_callback,
      base::OnceClosure decline_virtual_card_callback) override;
  void VirtualCardEnrollCompleted(PaymentsRpcResult result) override;
  void OnCardDataAvailable(
      const FilledCardInformationBubbleOptions& options) override;
  void ConfirmSaveIbanLocally(const Iban& iban,
                              bool should_show_prompt,
                              SaveIbanPromptCallback callback) override;
  void ConfirmUploadIbanToCloud(const Iban& iban,
                                LegalMessageLines legal_message_lines,
                                bool should_show_prompt,
                                SaveIbanPromptCallback callback) override;
  void IbanUploadCompleted(bool iban_saved, bool hit_max_strikes) override;
  void ShowAutofillProgressDialog(
      AutofillProgressDialogType autofill_progress_dialog_type,
      base::OnceClosure cancel_callback) override;
  void CloseAutofillProgressDialog(
      bool show_confirmation_before_closing,
      base::OnceClosure no_interactive_authentication_callback) override;
  void ShowCardUnmaskOtpInputDialog(
      CreditCard::RecordType card_type,
      const CardUnmaskChallengeOption& challenge_option,
      base::WeakPtr<OtpUnmaskDelegate> delegate) override;
  void OnUnmaskOtpVerificationResult(OtpUnmaskResult unmask_result) override;
  void ShowUnmaskAuthenticatorSelectionDialog(
      const std::vector<CardUnmaskChallengeOption>& challenge_options,
      base::OnceCallback<void(const std::string&)>
          confirm_unmask_challenge_option_callback,
      base::OnceClosure cancel_unmasking_closure) override;
  void DismissUnmaskAuthenticatorSelectionDialog(bool server_success) override;
  PaymentsNetworkInterface* GetPaymentsNetworkInterface() override;
  MultipleRequestPaymentsNetworkInterface*
  GetMultipleRequestPaymentsNetworkInterface() override;
  void ShowAutofillErrorDialog(
      AutofillErrorDialogContext error_context) override;
  PaymentsWindowManager* GetPaymentsWindowManager() override;
  void ShowUnmaskPrompt(
      const CreditCard& card,
      const CardUnmaskPromptOptions& card_unmask_prompt_options,
      base::WeakPtr<CardUnmaskDelegate> delegate) override;
  void OnUnmaskVerificationResult(
      payments::PaymentsAutofillClient::PaymentsRpcResult result) override;
  std::unique_ptr<AutofillProgressDialogController> ExtractProgressDialogModel()
      override;
  std::unique_ptr<CardUnmaskOtpInputDialogController>
  ExtractOtpInputDialogModel() override;
  CardUnmaskPromptController* GetCardUnmaskPromptModel() override;
  VirtualCardEnrollmentManager* GetVirtualCardEnrollmentManager() override;
  CreditCardCvcAuthenticator& GetCvcAuthenticator() override;
  CreditCardOtpAuthenticator* GetOtpAuthenticator() override;
  CreditCardRiskBasedAuthenticator* GetRiskBasedAuthenticator() override;
  bool IsRiskBasedAuthEffectivelyAvailable() const override;
  bool IsMandatoryReauthEnabled() override;
  bool IsUsingCustomCardIconEnabled() const override;
  void ShowMandatoryReauthOptInPrompt(
      base::OnceClosure accept_mandatory_reauth_callback,
      base::OnceClosure cancel_mandatory_reauth_callback,
      base::RepeatingClosure close_mandatory_reauth_callback) override;
  void ShowMandatoryReauthOptInConfirmation() override;
  bool IsAutofillPaymentMethodsEnabled() const final;
  void DisablePaymentsAutofill() final;
  IbanManager* GetIbanManager() override;
  IbanAccessManager* GetIbanAccessManager() override;
  MerchantPromoCodeManager* GetMerchantPromoCodeManager() override;
  void OpenPromoCodeOfferDetailsURL(const GURL& url) override;
  AutofillOfferManager* GetAutofillOfferManager() override;
  void UpdateOfferNotification(
      const AutofillOfferData& offer,
      const OfferNotificationOptions& options) override;
  void DismissOfferNotification() override;
  bool ShowTouchToFillCreditCard(
      base::WeakPtr<TouchToFillDelegate> delegate,
      base::span<const Suggestion> suggestions) override;
  bool ShowTouchToFillIban(
      base::WeakPtr<TouchToFillDelegate> delegate,
      base::span<const autofill::Iban> ibans_to_suggest) override;
  bool ShowTouchToFillLoyaltyCard(
      base::WeakPtr<TouchToFillDelegate> delegate,
      std::vector<autofill::LoyaltyCard> loyalty_cards_to_suggest) override;
  bool OnPurchaseAmountExtracted(
      base::span<const payments::BnplIssuerContext> bnpl_issuer_contexts,
      std::optional<int64_t> extracted_amount,
      bool is_amount_supported_by_any_issuer,
      const std::optional<std::string>& app_locale,
      base::OnceCallback<void(autofill::BnplIssuer)> selected_issuer_callback,
      base::OnceClosure cancel_callback) override;
  bool ShowTouchToFillProgress(base::OnceClosure cancel_callback) override;
  bool ShowTouchToFillBnplIssuers(
      base::span<const payments::BnplIssuerContext> bnpl_issuer_contexts,
      const std::string& app_locale,
      base::OnceCallback<void(autofill::BnplIssuer)> selected_issuer_callback,
      base::OnceClosure cancel_callback) override;
  bool ShowTouchToFillError(const AutofillErrorDialogContext& context) override;
  bool ShowTouchToFillBnplTos(BnplTosModel model,
                              base::OnceClosure accept_callback,
                              base::OnceClosure cancel_callback) override;
  void HideTouchToFillPaymentMethod() override;
  void SetTouchToFillVisible(bool visible) override;
  PaymentsDataManager& GetPaymentsDataManager() final;
  payments::MandatoryReauthManager* GetOrCreatePaymentsMandatoryReauthManager()
      override;
  payments::SaveAndFillManager* GetSaveAndFillManager() override;
  void ShowCreditCardLocalSaveAndFillDialog(
      CardSaveAndFillDialogCallback callback) override;
  void ShowCreditCardUploadSaveAndFillDialog(
      const LegalMessageLines& legal_message_lines,
      CardSaveAndFillDialogCallback callback) override;
  void ShowCreditCardSaveAndFillPendingDialog() override;
  void HideCreditCardSaveAndFillDialog() override;
  bool IsTabModalPopupDeprecated() const override;
  BnplStrategy* GetBnplStrategy() override;
  BnplUiDelegate* GetBnplUiDelegate() override;

  // Begin IOSChromePaymentsAutofillClient-specific section.

 private:
  // Shows save card UI offering upload or local save.
  void ShowSaveCreditCard(
      AutofillSaveCardUiInfo ui_info,
      std::unique_ptr<AutofillSaveCardDelegate> save_card_delegate);

  const raw_ref<autofill::ChromeAutofillClientIOS, DanglingUntriaged> client_;

  const raw_ref<infobars::InfoBarManager, DanglingUntriaged> infobar_manager_;

  std::unique_ptr<PaymentsNetworkInterface> payments_network_interface_;
  std::unique_ptr<MultipleRequestPaymentsNetworkInterface>
      multiple_request_payments_network_interface_;

  // TODO(crbug.com/40937065): Make these member variables as const raw_refs.
  const raw_ptr<PrefService, DanglingUntriaged> pref_service_;
  const raw_ptr<web::WebState, DanglingUntriaged> web_state_;
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

  base::WeakPtr<SaveCardBottomSheetModel> save_card_bottom_sheet_model_;

  // Indicates whether the save card bottom sheet should be presented instead of
  // the infobar for uploading the card to server.
  bool show_save_card_bottom_sheet_for_upload_;

};

}  // namespace payments

}  // namespace autofill

#endif  // IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_IOS_CHROME_PAYMENTS_AUTOFILL_CLIENT_H_
