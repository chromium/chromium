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
#import "components/autofill/core/browser/autocomplete_history_manager.h"
#import "components/autofill/core/browser/autofill_ablation_study.h"
#import "components/autofill/core/browser/autofill_client.h"
#import "components/autofill/core/browser/country_type.h"
#import "components/autofill/core/browser/crowdsourcing/autofill_crowdsourcing_manager.h"
#import "components/autofill/core/browser/password_form_classification.h"
#import "components/autofill/core/browser/payments/card_unmask_delegate.h"
#import "components/autofill/core/browser/personal_data_manager.h"
#import "components/autofill/core/browser/strike_databases/strike_database.h"
#import "components/autofill/core/browser/ui/payments/card_unmask_prompt_options.h"
#import "components/autofill/core/browser/webdata/autofill_webdata_service.h"
#import "components/autofill/ios/browser/autofill_client_ios_bridge.h"
#import "components/infobars/core/infobar_manager.h"
#import "components/plus_addresses/plus_address_types.h"
#import "components/prefs/pref_service.h"
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

enum class SuggestionType;

// Chrome iOS implementation of AutofillClient.
class ChromeAutofillClientIOS : public AutofillClient {
 public:
  ChromeAutofillClientIOS(ProfileIOS* profile,
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
  AutofillDriverFactory& GetAutofillDriverFactory() override;
  AutofillCrowdsourcingManager* GetCrowdsourcingManager() override;
  PersonalDataManager* GetPersonalDataManager() override;
  AutocompleteHistoryManager* GetAutocompleteHistoryManager() override;
  PrefService* GetPrefs() override;
  const PrefService* GetPrefs() const override;
  syncer::SyncService* GetSyncService() override;
  signin::IdentityManager* GetIdentityManager() override;
  const signin::IdentityManager* GetIdentityManager() const override;
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
  void ConfirmSaveAddressProfile(
      const AutofillProfile& profile,
      const AutofillProfile* original_profile,
      bool is_migration_to_account,
      AddressProfileSavePromptCallback callback) override;
  void ShowEditAddressProfileDialog(
      const AutofillProfile& profile,
      AddressProfileSavePromptCallback on_user_decision_callback) override;
  void ShowDeleteAddressProfileDialog(
      const AutofillProfile& profile,
      AddressProfileDeleteDialogCallback delete_dialog_callback) override;
  SuggestionUiSessionId ShowAutofillSuggestions(
      const PopupOpenArgs& open_args,
      base::WeakPtr<AutofillSuggestionDelegate> delegate) override;
  void ShowPlusAddressEmailOverrideNotification(
      const std::string& original_email,
      EmailOverrideUndoCallback email_override_undo_callback) override;
  void UpdateAutofillDataListValues(
      base::span<const autofill::SelectOption> datalist) override;
  void PinAutofillSuggestions() override;
  void HideAutofillSuggestions(SuggestionHidingReason reason) override;
  bool IsAutocompleteEnabled() const override;
  bool IsPasswordManagerEnabled() override;
  void DidFillOrPreviewForm(mojom::ActionPersistence action_persistence,
                            AutofillTriggerSource trigger_source,
                            bool is_refill) override;
  bool IsContextSecure() const override;
  FormInteractionsFlowId GetCurrentFormInteractionsFlowId() override;
  LogManager* GetLogManager() const override;
  const AutofillAblationStudy& GetAblationStudy() const override;
  bool IsLastQueriedField(FieldGlobalId field_id) override;
  bool ShouldFormatForLargeKeyboardAccessory() const override;
  AutofillPlusAddressDelegate* GetPlusAddressDelegate() override;
  void OfferPlusAddressCreation(const url::Origin& main_frame_origin,
                                PlusAddressCallback callback) override;
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

 private:
  // Returns the account email of the signed-in user, or nullopt if there is no
  // signed-in user.
  std::optional<std::u16string> GetUserEmail();

  raw_ptr<PrefService> pref_service_;
  raw_ptr<syncer::SyncService> sync_service_;
  std::unique_ptr<AutofillCrowdsourcingManager> crowdsourcing_manager_;
  raw_ptr<PersonalDataManager> personal_data_manager_;
  raw_ptr<AutocompleteHistoryManager> autocomplete_history_manager_;
  raw_ptr<ProfileIOS> profile_;
  raw_ptr<web::WebState> web_state_;
  __weak id<AutofillClientIOSBridge> bridge_;
  raw_ptr<signin::IdentityManager> identity_manager_;
  std::unique_ptr<FormDataImporter> form_data_importer_;
  scoped_refptr<AutofillWebDataService> autofill_web_data_service_;
  raw_ptr<infobars::InfoBarManager> infobar_manager_;
  std::unique_ptr<LogManager> log_manager_;
  const AutofillAblationStudy ablation_study_;

  // Order matters for this initialization. This initialization must happen
  // after all of the members passed into the constructor of
  // `payments_autofill_client_` are initialized, other than `this`.
  payments::IOSChromePaymentsAutofillClient payments_autofill_client_{
      this, web_state_, infobar_manager_, pref_service_};

  // A weak reference to the view controller used to present UI.
  __weak UIViewController* base_view_controller_;

  __weak id<AutofillCommands> commands_handler_;
};

}  // namespace autofill

#endif  // IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_CHROME_AUTOFILL_CLIENT_IOS_H_
