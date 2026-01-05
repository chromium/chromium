// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_INTERNAL_AUTOFILL_IOS_WEB_VIEW_PAYMENTS_AUTOFILL_CLIENT_H_
#define IOS_WEB_VIEW_INTERNAL_AUTOFILL_IOS_WEB_VIEW_PAYMENTS_AUTOFILL_CLIENT_H_

#include <optional>

#include "base/containers/span.h"
#include "base/functional/callback.h"
#include "components/autofill/core/browser/payments/card_unmask_delegate.h"
#include "components/autofill/core/browser/payments/multiple_request_payments_network_interface.h"
#include "components/autofill/core/browser/payments/payments_autofill_client.h"
#include "components/autofill/core/browser/ui/payments/card_unmask_prompt_options.h"
#include "ios/web_view/internal/autofill/cwv_autofill_client_ios_bridge.h"

class GURL;

namespace web {
class WebState;
}  // namespace web

namespace autofill {

class AutofillProgressDialogController;
class BnplIssuer;
struct BnplTosModel;
class CardUnmaskOtpInputDialogController;
class CardUnmaskPromptController;
class CreditCardCvcAuthenticator;
class CreditCardOtpAuthenticator;
class CreditCardRiskBasedAuthenticator;
class WebViewAutofillClientIOS;
class PaymentsDataManager;
struct CardUnmaskChallengeOption;
struct VirtualCardEnrollmentFields;
class VirtualCardEnrollmentManager;

namespace payments {

struct BnplIssuerContext;
class MandatoryReauthManager;

// iOS WebView implementation of PaymentsAutofillClient. Owned by the
// WebViewAutofillClientIOS. Created lazily in the WebViewAutofillClientIOS when
// it is needed.
class IOSWebViewPaymentsAutofillClient : public PaymentsAutofillClient {
 public:
  explicit IOSWebViewPaymentsAutofillClient(
      autofill::WebViewAutofillClientIOS* client,
      id<CWVAutofillClientIOSBridge> bridge,
      web::WebState* web_state);
  IOSWebViewPaymentsAutofillClient(const IOSWebViewPaymentsAutofillClient&) =
      delete;
  IOSWebViewPaymentsAutofillClient& operator=(
      const IOSWebViewPaymentsAutofillClient&) = delete;
  ~IOSWebViewPaymentsAutofillClient() override;

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
  void ShowAutofillErrorDialog(AutofillErrorDialogContext context) override;
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

  // Begin IOSWebViewPaymentsAutofillClient-specific section.

 private:
  const raw_ref<autofill::WebViewAutofillClientIOS> client_;

  __weak id<CWVAutofillClientIOSBridge> bridge_;

  std::unique_ptr<PaymentsNetworkInterface> payments_network_interface_;

  std::unique_ptr<MultipleRequestPaymentsNetworkInterface>
      multiple_request_payments_network_interface_;

  std::unique_ptr<CreditCardCvcAuthenticator> cvc_authenticator_;

  std::unique_ptr<CreditCardRiskBasedAuthenticator> risk_based_authenticator_;

  std::unique_ptr<payments::MandatoryReauthManager> payments_reauth_manager_;

  std::unique_ptr<CreditCardOtpAuthenticator> otp_authenticator_;

  std::unique_ptr<VirtualCardEnrollmentManager>
      virtual_card_enrollment_manager_;

  const raw_ref<web::WebState> web_state_;

  PrefService* GetPrefService() const;
};

}  // namespace payments

}  // namespace autofill

#endif  // IOS_WEB_VIEW_INTERNAL_AUTOFILL_IOS_WEB_VIEW_PAYMENTS_AUTOFILL_CLIENT_H_
