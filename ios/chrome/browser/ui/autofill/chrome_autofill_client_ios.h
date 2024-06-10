// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTOFILL_CHROME_AUTOFILL_CLIENT_IOS_H_
#define IOS_CHROME_BROWSER_UI_AUTOFILL_CHROME_AUTOFILL_CLIENT_IOS_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#import "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/autofill/core/browser/autocomplete_history_manager.h"
#include "components/autofill/core/browser/autofill_client.h"
#include "components/autofill/core/browser/country_type.h"
#include "components/autofill/core/browser/crowdsourcing/autofill_crowdsourcing_manager.h"
#include "components/autofill/core/browser/payments/card_unmask_delegate.h"
#include "components/autofill/core/browser/payments/legal_message_line.h"
#include "components/autofill/core/browser/payments/mandatory_reauth_manager.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/browser/strike_databases/strike_database.h"
#include "components/autofill/core/browser/ui/payments/card_expiration_date_fix_flow_controller_impl.h"
#include "components/autofill/core/browser/ui/payments/card_unmask_prompt_options.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service.h"
#import "components/autofill/ios/browser/autofill_client_ios_bridge.h"
#include "components/infobars/core/infobar_manager.h"
#include "components/plus_addresses/plus_address_types.h"
#include "components/prefs/pref_service.h"
#include "components/sync/service/sync_service.h"
#include "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/ui/autofill/ios_chrome_payments_autofill_client.h"

@protocol AutofillCommands;
@class UIViewController;

namespace web {
class WebState;
}

namespace autofill {

enum class SuggestionType;

// Chrome iOS implementation of AutofillClient.
class ChromeAutofillClientIOS : public AutofillClient {
 public:
  ChromeAutofillClientIOS(ChromeBrowserState* browser_state,
                          web::WebState* web_state,
                          infobars::InfoBarManager* infobar_manager,
                          id<AutofillClientIOSBridge> bridge);

  ChromeAutofillClientIOS(const ChromeAutofillClientIOS&) = delete;
  ChromeAutofillClientIOS& operator=(const ChromeAutofillClientIOS&) = delete;

  ~ChromeAutofillClientIOS() override;

  // Sets a weak reference to the view controller used to present UI.
  void SetBaseViewController(UIViewController* base_view_controller);
  UIViewController* base_view_controller() { return base_view_controller_; }

  void set_commands_handler(id<AutofillCommands> commands_handler) {
    commands_handler_ = commands_handler;
  }
  id<AutofillCommands> commands_handler() const { return commands_handler_; }

  // AutofillClient:
  version_info::Channel GetChannel() const override;
  bool IsOffTheRecord() const override;
  scoped_refptr<network::SharedURLLoaderFactory> GetURLLoaderFactory() override;
  AutofillCrowdsourcingManager* GetCrowdsourcingManager() override;
  PersonalDataManager* GetPersonalDataManager() override;
  AutocompleteHistoryManager* GetAutocompleteHistoryManager() override;
  PrefService* GetPrefs() override;
  const PrefService* GetPrefs() const override;
  syncer::SyncService* GetSyncService() override;
  signin::IdentityManager* GetIdentityManager() override;
  FormDataImporter* GetFormDataImporter() override;
  payments::IOSChromePaymentsAutofillClient* GetPaymentsAutofillClient()
      override;
  StrikeDatabase* GetStrikeDatabase() override;
  ukm::UkmRecorder* GetUkmRecorder() override;
  ukm::SourceId GetUkmSourceId() override;
  AddressNormalizer* GetAddressNormalizer() override;
  const GURL& GetLastCommittedPrimaryMainFrameURL() const override;
  url::Origin GetLastCommittedPrimaryMainFrameOrigin() const override;
  security_state::SecurityLevel GetSecurityLevelForUmaHistograms() override;
  const translate::LanguageState* GetLanguageState() override;
  translate::TranslateDriver* GetTranslateDriver() override;
  GeoIpCountryCode GetVariationConfigCountryCode() const override;
  void ShowAutofillSettings(SuggestionType suggestion_type) override;
  payments::MandatoryReauthManager* GetOrCreatePaymentsMandatoryReauthManager()
      override;
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
  void ConfirmSaveAddressProfile(
      const AutofillProfile& profile,
      const AutofillProfile* original_profile,
      SaveAddressProfilePromptOptions options,
      AddressProfileSavePromptCallback callback) override;
  void ShowEditAddressProfileDialog(
      const AutofillProfile& profile,
      AddressProfileSavePromptCallback on_user_decision_callback) override;
  void ShowDeleteAddressProfileDialog(
      const AutofillProfile& profile,
      AddressProfileDeleteDialogCallback delete_dialog_callback) override;
  bool HasCreditCardScanFeature() const override;
  void ScanCreditCard(CreditCardScanCallback callback) override;
  bool ShowTouchToFillCreditCard(
      base::WeakPtr<TouchToFillDelegate> delegate,
      base::span<const CreditCard> cards_to_suggest,
      const std::vector<bool>& card_acceptabilities) override;
  void HideTouchToFillCreditCard() override;
  void ShowAutofillSuggestions(
      const PopupOpenArgs& open_args,
      base::WeakPtr<AutofillSuggestionDelegate> delegate) override;
  void UpdateAutofillDataListValues(
      base::span<const autofill::SelectOption> datalist) override;
  void PinAutofillSuggestions() override;
  void UpdatePopup(const std::vector<Suggestion>& suggestions,
                   FillingProduct main_filling_product,
                   AutofillSuggestionTriggerSource trigger_source) override;
  void HideAutofillSuggestions(SuggestionHidingReason reason) override;
  bool IsAutocompleteEnabled() const override;
  bool IsPasswordManagerEnabled() override;
  void DidFillOrPreviewForm(mojom::ActionPersistence action_persistence,
                            AutofillTriggerSource trigger_source,
                            bool is_refill) override;
  void DidFillOrPreviewField(const std::u16string& autofilled_value,
                             const std::u16string& profile_full_name) override;
  bool IsContextSecure() const override;
  void OpenPromoCodeOfferDetailsURL(const GURL& url) override;
  FormInteractionsFlowId GetCurrentFormInteractionsFlowId() override;
  LogManager* GetLogManager() const override;
  bool IsLastQueriedField(FieldGlobalId field_id) override;
  bool ShouldFormatForLargeKeyboardAccessory() const override;
  AutofillPlusAddressDelegate* GetPlusAddressDelegate() override;
  void OfferPlusAddressCreation(const url::Origin& main_frame_origin,
                                PlusAddressCallback callback) override;
  std::unique_ptr<device_reauth::DeviceAuthenticator> GetDeviceAuthenticator()
      override;
  PasswordFormType ClassifyAsPasswordForm(
      AutofillManager& manager,
      FormGlobalId form_id,
      FieldGlobalId field_id) const override;

 private:
  // Returns the account email of the signed-in user, or nullopt if there is no
  // signed-in user.
  std::optional<std::u16string> GetUserEmail();

  raw_ptr<PrefService> pref_service_;
  raw_ptr<syncer::SyncService> sync_service_;
  std::unique_ptr<AutofillCrowdsourcingManager> crowdsourcing_manager_;
  raw_ptr<PersonalDataManager> personal_data_manager_;
  raw_ptr<AutocompleteHistoryManager> autocomplete_history_manager_;
  raw_ptr<ChromeBrowserState> browser_state_;
  raw_ptr<web::WebState> web_state_;
  __weak id<AutofillClientIOSBridge> bridge_;
  raw_ptr<signin::IdentityManager> identity_manager_;
  std::unique_ptr<payments::IOSChromePaymentsAutofillClient>
      payments_autofill_client_;
  std::unique_ptr<FormDataImporter> form_data_importer_;
  scoped_refptr<AutofillWebDataService> autofill_web_data_service_;
  raw_ptr<infobars::InfoBarManager> infobar_manager_;
  std::unique_ptr<LogManager> log_manager_;
  CardExpirationDateFixFlowControllerImpl
      card_expiration_date_fix_flow_controller_;
  std::unique_ptr<payments::MandatoryReauthManager> payments_reauth_manager_;

  // A weak reference to the view controller used to present UI.
  __weak UIViewController* base_view_controller_;

  __weak id<AutofillCommands> commands_handler_;
};

}  // namespace autofill

#endif  // IOS_CHROME_BROWSER_UI_AUTOFILL_CHROME_AUTOFILL_CLIENT_IOS_H_
