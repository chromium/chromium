// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTOFILL_CHROME_AUTOFILL_CLIENT_IOS_H_
#define IOS_CHROME_BROWSER_UI_AUTOFILL_CHROME_AUTOFILL_CLIENT_IOS_H_

#include <memory>
#include <string>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "components/autofill/core/browser/autocomplete_history_manager.h"
#include "components/autofill/core/browser/autofill_client.h"
#include "components/autofill/core/browser/autofill_download_manager.h"
#include "components/autofill/core/browser/payments/card_unmask_delegate.h"
#include "components/autofill/core/browser/payments/legal_message_line.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/browser/strike_databases/strike_database.h"
#include "components/autofill/core/browser/ui/payments/card_expiration_date_fix_flow_controller_impl.h"
#include "components/autofill/core/browser/ui/payments/card_name_fix_flow_controller_impl.h"
#include "components/autofill/core/browser/ui/payments/card_unmask_prompt_controller_impl.h"
#include "components/autofill/core/browser/ui/payments/card_unmask_prompt_options.h"
#include "components/autofill/core/browser/ui/popup_item_ids.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service.h"
#import "components/autofill/ios/browser/autofill_client_ios_bridge.h"
#include "components/infobars/core/infobar_manager.h"
#include "components/password_manager/core/browser/password_manager.h"
#include "components/prefs/pref_service.h"
#include "components/sync/service/sync_service.h"
#include "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"

@class UIViewController;

namespace web {
class WebState;
}

namespace autofill {

// Chrome iOS implementation of AutofillClient.
class ChromeAutofillClientIOS : public AutofillClient {
 public:
  ChromeAutofillClientIOS(ChromeBrowserState* browser_state,
                          web::WebState* web_state,
                          infobars::InfoBarManager* infobar_manager,
                          id<AutofillClientIOSBridge> bridge,
                          password_manager::PasswordManager* password_manager);

  ChromeAutofillClientIOS(const ChromeAutofillClientIOS&) = delete;
  ChromeAutofillClientIOS& operator=(const ChromeAutofillClientIOS&) = delete;

  ~ChromeAutofillClientIOS() override;

  // Sets a weak reference to the view controller used to present UI.
  void SetBaseViewController(UIViewController* base_view_controller);

  // AutofillClient:
  version_info::Channel GetChannel() const override;
  bool IsOffTheRecord() override;
  scoped_refptr<network::SharedURLLoaderFactory> GetURLLoaderFactory() override;
  AutofillDownloadManager* GetDownloadManager() override;
  PersonalDataManager* GetPersonalDataManager() override;
  AutocompleteHistoryManager* GetAutocompleteHistoryManager() override;
  CreditCardCvcAuthenticator* GetCvcAuthenticator() override;
  CreditCardOtpAuthenticator* GetOtpAuthenticator() override;
  PrefService* GetPrefs() override;
  const PrefService* GetPrefs() const override;
  syncer::SyncService* GetSyncService() override;
  signin::IdentityManager* GetIdentityManager() override;
  FormDataImporter* GetFormDataImporter() override;
  payments::PaymentsClient* GetPaymentsClient() override;
  StrikeDatabase* GetStrikeDatabase() override;
  ukm::UkmRecorder* GetUkmRecorder() override;
  ukm::SourceId GetUkmSourceId() override;
  AddressNormalizer* GetAddressNormalizer() override;
  const GURL& GetLastCommittedPrimaryMainFrameURL() const override;
  url::Origin GetLastCommittedPrimaryMainFrameOrigin() const override;
  security_state::SecurityLevel GetSecurityLevelForUmaHistograms() override;
  const translate::LanguageState* GetLanguageState() override;
  translate::TranslateDriver* GetTranslateDriver() override;
  std::string GetVariationConfigCountryCode() const override;
  void ShowAutofillSettings(PopupType popup_type) override;
  void ShowUnmaskPrompt(
      const CreditCard& card,
      const CardUnmaskPromptOptions& card_unmask_prompt_options,
      base::WeakPtr<CardUnmaskDelegate> delegate) override;
  void OnUnmaskVerificationResult(PaymentsRpcResult result) override;
  void ConfirmAccountNameFixFlow(
      base::OnceCallback<void(const std::u16string&)> callback) override;
  void ConfirmExpirationDateFixFlow(
      const CreditCard& card,
      base::OnceCallback<void(const std::u16string&, const std::u16string&)>
          callback) override;
  void ConfirmSaveCreditCardLocally(
      const CreditCard& card,
      SaveCreditCardOptions options,
      LocalSaveCardPromptCallback callback) override;
  void ConfirmSaveCreditCardToCloud(
      const CreditCard& card,
      const LegalMessageLines& legal_message_lines,
      SaveCreditCardOptions options,
      UploadSaveCardPromptCallback callback) override;
  void CreditCardUploadCompleted(bool card_saved) override;
  void ConfirmCreditCardFillAssist(const CreditCard& card,
                                   base::OnceClosure callback) override;
  void ConfirmSaveAddressProfile(
      const AutofillProfile& profile,
      const AutofillProfile* original_profile,
      SaveAddressProfilePromptOptions options,
      AddressProfileSavePromptCallback callback) override;
  void ShowDeleteAddressProfileDialog() override;
  bool HasCreditCardScanFeature() override;
  void ScanCreditCard(CreditCardScanCallback callback) override;
  bool IsTouchToFillCreditCardSupported() override;
  bool ShowTouchToFillCreditCard(
      base::WeakPtr<TouchToFillDelegate> delegate,
      base::span<const CreditCard> cards_to_suggest) override;
  void HideTouchToFillCreditCard() override;
  void ShowAutofillPopup(
      const PopupOpenArgs& open_args,
      base::WeakPtr<AutofillPopupDelegate> delegate) override;
  void UpdateAutofillPopupDataListValues(
      const std::vector<std::u16string>& values,
      const std::vector<std::u16string>& labels) override;
  std::vector<Suggestion> GetPopupSuggestions() const override;
  void PinPopupView() override;
  PopupOpenArgs GetReopenPopupArgs(
      AutofillSuggestionTriggerSource trigger_source) const override;
  void UpdatePopup(const std::vector<Suggestion>& suggestions,
                   PopupType popup_type,
                   AutofillSuggestionTriggerSource trigger_source) override;
  void HideAutofillPopup(PopupHidingReason reason) override;
  bool IsAutocompleteEnabled() const override;
  bool IsPasswordManagerEnabled() override;
  void PropagateAutofillPredictionsDeprecated(
      AutofillDriver* driver,
      const std::vector<FormStructure*>& forms) override;
  void DidFillOrPreviewForm(mojom::AutofillActionPersistence action_persistence,
                            AutofillTriggerSource trigger_source,
                            bool is_refill) override;
  void DidFillOrPreviewField(const std::u16string& autofilled_value,
                             const std::u16string& profile_full_name) override;
  bool IsContextSecure() const override;
  void OpenPromoCodeOfferDetailsURL(const GURL& url) override;
  FormInteractionsFlowId GetCurrentFormInteractionsFlowId() override;
  LogManager* GetLogManager() const override;
  bool IsLastQueriedField(FieldGlobalId field_id) override;

  // RiskDataLoader:
  void LoadRiskData(
      base::OnceCallback<void(const std::string&)> callback) override;

 private:
  // Returns the account email of the signed-in user, or nullopt if there is no
  // signed-in user.
  absl::optional<std::u16string> GetUserEmail();

  PrefService* pref_service_;
  syncer::SyncService* sync_service_;
  std::unique_ptr<AutofillDownloadManager> download_manager_;
  PersonalDataManager* personal_data_manager_;
  AutocompleteHistoryManager* autocomplete_history_manager_;
  ChromeBrowserState* browser_state_;
  web::WebState* web_state_;
  __weak id<AutofillClientIOSBridge> bridge_;
  signin::IdentityManager* identity_manager_;
  std::unique_ptr<payments::PaymentsClient> payments_client_;
  std::unique_ptr<CreditCardCvcAuthenticator> cvc_authenticator_;
  std::unique_ptr<CreditCardOtpAuthenticator> otp_authenticator_;
  std::unique_ptr<FormDataImporter> form_data_importer_;
  scoped_refptr<AutofillWebDataService> autofill_web_data_service_;
  infobars::InfoBarManager* infobar_manager_;
  password_manager::PasswordManager* password_manager_;
  CardUnmaskPromptControllerImpl unmask_controller_;
  std::unique_ptr<LogManager> log_manager_;
  CardNameFixFlowControllerImpl card_name_fix_flow_controller_;
  CardExpirationDateFixFlowControllerImpl
      card_expiration_date_fix_flow_controller_;

  // A weak reference to the view controller used to present UI.
  __weak UIViewController* base_view_controller_;
};

}  // namespace autofill

#endif  // IOS_CHROME_BROWSER_UI_AUTOFILL_CHROME_AUTOFILL_CLIENT_IOS_H_
