// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_CHROME_AUTOFILL_CLIENT_IOS_H_
#define IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_CHROME_AUTOFILL_CLIENT_IOS_H_

#import <memory>
#import <optional>
#import <string>
#import <vector>

#import "base/memory/raw_ptr.h"
#import "base/memory/weak_ptr.h"
#import "components/autofill/core/browser/country_type.h"
#import "components/autofill/core/browser/crowdsourcing/autofill_crowdsourcing_manager.h"
#import "components/autofill/core/browser/crowdsourcing/votes_uploader.h"
#import "components/autofill/core/browser/data_manager/personal_data_manager.h"
#import "components/autofill/core/browser/data_manager/valuables/valuables_data_manager.h"
#import "components/autofill/core/browser/foundations/autofill_client.h"
#import "components/autofill/core/browser/integrators/password_form_classification.h"
#import "components/autofill/core/browser/metrics/form_interactions_ukm_logger.h"
#import "components/autofill/core/browser/payments/card_unmask_delegate.h"
#import "components/autofill/core/browser/single_field_fillers/autocomplete/autocomplete_history_manager.h"
#import "components/autofill/core/browser/studies/autofill_ablation_study.h"
#import "components/autofill/core/browser/ui/payments/card_unmask_prompt_options.h"
#import "components/autofill/core/browser/webdata/autofill_webdata_service.h"
#import "components/autofill/ios/browser/autofill_agent.h"
#import "components/autofill/ios/browser/autofill_client_ios.h"
#import "components/autofill/ios/browser/autofill_client_ios_bridge.h"
#import "components/infobars/core/infobar_manager.h"
#import "components/plus_addresses/core/browser/plus_address_types.h"
#import "components/prefs/pref_service.h"
#import "components/strike_database/strike_database.h"
#import "components/sync/service/sync_service.h"
#import "ios/chrome/browser/autofill/model/credit_card/autofill_save_card_infobar_delegate_ios.h"
#import "ios/chrome/browser/autofill/ui_bundled/ios_chrome_payments_autofill_client.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"

@protocol AutofillCommands;
@class UIViewController;

namespace web {
class WebState;
}

namespace autofill {

class LogRouter;

enum class SuggestionType;

// Chrome iOS implementation of AutofillClient.
//
// Satisfies the AutofillClientIOS contract for the following reason.
// ChromeAutofillClientIOS is owned by web::WebStateUserData, so
// - first ~WebStateImpl() notifies web::WebStateObserver::WebStateDestroyed()
// - then ~WebStateImpl() invalidates weak_ptr(), and
// - then ~SupportsUserData() destroys WebStateUserData and thus
//   ChromeAutofillClientIOS.
class ChromeAutofillClientIOS : public AutofillClientIOS {
 public:
  ChromeAutofillClientIOS(
      ProfileIOS* profile,
      web::WebState* web_state,
      infobars::InfoBarManager* infobar_manager,
      id<AutofillClientIOSBridge, AutofillDriverIOSBridge> bridge);

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
  base::WeakPtr<AutofillClient> GetWeakPtr() override;
  const std::string& GetAppLocale() const override;
  version_info::Channel GetChannel() const override;
  bool IsOffTheRecord() const override;
  scoped_refptr<network::SharedURLLoaderFactory> GetURLLoaderFactory() override;
  AutofillCrowdsourcingManager& GetCrowdsourcingManager() override;
  VotesUploader& GetVotesUploader() override;
  PersonalDataManager& GetPersonalDataManager() override;
  ValuablesDataManager* GetValuablesDataManager() override;
  EntityDataManager* GetEntityDataManager() override;
  FieldClassificationModelHandler*
  GetAutofillFieldClassificationModelHandler() override;
  FieldClassificationModelHandler*
  GetPasswordManagerFieldClassificationModelHandler() override;
  SingleFieldFillRouter& GetSingleFieldFillRouter() override;
  AutocompleteHistoryManager* GetAutocompleteHistoryManager() override;
  PrefService* GetPrefs() override;
  const PrefService* GetPrefs() const override;
  syncer::SyncService* GetSyncService() override;
  signin::IdentityManager* GetIdentityManager() override;
  const signin::IdentityManager* GetIdentityManager() const override;
  FormDataImporter* GetFormDataImporter() override;
  payments::IOSChromePaymentsAutofillClient* GetPaymentsAutofillClient()
      override;
  strike_database::StrikeDatabase* GetStrikeDatabase() override;
  ukm::UkmRecorder* GetUkmRecorder() override;
  AddressNormalizer* GetAddressNormalizer() override;
  const GURL& GetLastCommittedPrimaryMainFrameURL() const override;
  url::Origin GetLastCommittedPrimaryMainFrameOrigin() const override;
  security_state::SecurityLevel GetSecurityLevelForUmaHistograms() override;
  const translate::LanguageState* GetLanguageState() override;
  translate::TranslateDriver* GetTranslateDriver() override;
  GeoIpCountryCode GetVariationConfigCountryCode() const override;
  void ShowAutofillSettings(SuggestionType suggestion_type) override;
  void ConfirmSaveAddressProfile(
      const AutofillProfile& profile,
      const AutofillProfile* original_profile,
      AutofillClient::SaveAddressBubbleType save_address_bubble_type,
      AddressProfileSavePromptCallback callback) override;
  SuggestionUiSessionId ShowAutofillSuggestions(
      const PopupOpenArgs& open_args,
      base::WeakPtr<AutofillSuggestionDelegate> delegate) override;
  void ShowPlusAddressEmailOverrideNotification(
      const std::string& original_email,
      EmailOverrideUndoCallback email_override_undo_callback) override;
  void UpdateAutofillDataListValues(
      base::span<const autofill::SelectOption> datalist) override;
  void HideAutofillSuggestions(SuggestionHidingReason reason) override;
  bool IsAutofillEnabled() const override;
  bool IsAutofillProfileEnabled() const override;
  bool IsWalletStorageEnabled() const override;
  bool IsAutocompleteEnabled() const override;
  bool IsPasswordManagerEnabled() const override;
  void DidFillForm(AutofillTriggerSource trigger_source,
                   bool is_refill) override;
  bool IsContextSecure() const override;
  FormInteractionsFlowId GetCurrentFormInteractionsFlowId() override;
  LogManager* GetCurrentLogManager() override;
  autofill_metrics::FormInteractionsUkmLogger& GetFormInteractionsUkmLogger()
      override;
  const AutofillAblationStudy& GetAblationStudy() const override;
  bool IsLastQueriedField(FieldGlobalId field_id) override;
  bool ShouldFormatForLargeKeyboardAccessory() const override;
  AutofillPlusAddressDelegate* GetPlusAddressDelegate() override;
  std::unique_ptr<device_reauth::DeviceAuthenticator> GetDeviceAuthenticator()
      override;
  PasswordFormClassification ClassifyAsPasswordForm(
      AutofillManager& manager,
      FormGlobalId form_id,
      FieldGlobalId field_id) const override;

  // Searches infobars managed by the infobar_manager_ for infobar of the type
  // AutofillSaveCardInfoBarDelegateIOS and returns it if found else returns a
  // nullptr.
  virtual AutofillSaveCardInfoBarDelegateIOS*
  GetAutofillSaveCardInfoBarDelegateIOS();

  // Removes the save card infobar if it exists.
  virtual void RemoveAutofillSaveCardInfoBar();

  void ConsiderAsSecureForTesting() { consider_as_secure_for_testing_ = true; }

 private:
  // Returns the account email of the signed-in user, or nullopt if there is no
  // signed-in user.
  std::optional<std::u16string> GetUserEmail();

  raw_ptr<PrefService, DanglingUntriaged> pref_service_;
  raw_ptr<syncer::SyncService, DanglingUntriaged> sync_service_;
  std::unique_ptr<AutofillCrowdsourcingManager> crowdsourcing_manager_;
  VotesUploader votes_uploader_{this};
  raw_ptr<PersonalDataManager, DanglingUntriaged> personal_data_manager_;
  raw_ptr<AutocompleteHistoryManager, DanglingUntriaged>
      autocomplete_history_manager_;
  raw_ptr<ProfileIOS> profile_;
  __weak id<AutofillClientIOSBridge> bridge_;
  raw_ptr<signin::IdentityManager, DanglingUntriaged> identity_manager_;
  std::unique_ptr<FormDataImporter> form_data_importer_;
  scoped_refptr<AutofillWebDataService> autofill_web_data_service_;
  raw_ptr<infobars::InfoBarManager, DanglingUntriaged> infobar_manager_;
  const raw_ptr<LogRouter> log_router_;
  std::unique_ptr<LogManager> log_manager_;
  autofill_metrics::FormInteractionsUkmLogger form_interactions_ukm_logger_{
      this};
  const AutofillAblationStudy ablation_study_;

  // Order matters for this initialization. This initialization must happen
  // after all of the members passed into the constructor of
  // `payments_autofill_client_` are initialized, other than `this`.
  payments::IOSChromePaymentsAutofillClient payments_autofill_client_{
      this, web_state(), infobar_manager_, pref_service_};
  SingleFieldFillRouter single_field_fill_router_{
      autocomplete_history_manager_, payments_autofill_client_.GetIbanManager(),
      payments_autofill_client_.GetMerchantPromoCodeManager()};

  // A weak reference to the view controller used to present UI.
  __weak UIViewController* base_view_controller_;

  __weak id<AutofillCommands> commands_handler_;

  // If this is true, we consider the form to be secure.
  // Only use this for testing purposes!
  bool consider_as_secure_for_testing_ = false;

  base::WeakPtrFactory<ChromeAutofillClientIOS> weak_ptr_factory_{this};
};

}  // namespace autofill

#endif  // IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_CHROME_AUTOFILL_CLIENT_IOS_H_
