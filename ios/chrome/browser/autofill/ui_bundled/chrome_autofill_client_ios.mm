// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/ui_bundled/chrome_autofill_client_ios.h"

#import <optional>
#import <utility>
#import <vector>

#import "base/check.h"
#import "base/check_deref.h"
#import "base/functional/bind.h"
#import "base/functional/callback.h"
#import "base/memory/ptr_util.h"
#import "base/notreached.h"
#import "base/ranges/algorithm.h"
#import "base/strings/string_util.h"
#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "components/autofill/core/browser/autofill_plus_address_delegate.h"
#import "components/autofill/core/browser/autofill_save_update_address_profile_delegate_ios.h"
#import "components/autofill/core/browser/form_data_importer.h"
#import "components/autofill/core/browser/logging/log_manager.h"
#import "components/autofill/core/browser/payments/payments_network_interface.h"
#import "components/autofill/core/browser/ui/suggestion_type.h"
#import "components/autofill/core/common/autofill_features.h"
#import "components/autofill/core/common/autofill_prefs.h"
#import "components/autofill/ios/browser/autofill_driver_ios_factory.h"
#import "components/autofill/ios/browser/autofill_util.h"
#import "components/infobars/core/infobar.h"
#import "components/infobars/core/infobar_manager.h"
#import "components/keyed_service/core/service_access_type.h"
#import "components/password_manager/core/browser/form_parsing/form_data_parser.h"
#import "components/password_manager/core/browser/password_form.h"
#import "components/password_manager/core/common/password_manager_pref_names.h"
#import "components/plus_addresses/plus_address_service.h"
#import "components/security_state/ios/security_state_utils.h"
#import "components/sync/service/sync_service.h"
#import "components/translate/core/browser/translate_manager.h"
#import "components/ukm/ios/ukm_url_recorder.h"
#import "components/variations/service/variations_service.h"
#import "ios/chrome/browser/autofill/model/address_normalizer_factory.h"
#import "ios/chrome/browser/autofill/model/autocomplete_history_manager_factory.h"
#import "ios/chrome/browser/autofill/model/autofill_log_router_factory.h"
#import "ios/chrome/browser/autofill/model/bottom_sheet/autofill_bottom_sheet_tab_helper.h"
#import "ios/chrome/browser/autofill/model/personal_data_manager_factory.h"
#import "ios/chrome/browser/autofill/model/strike_database_factory.h"
#import "ios/chrome/browser/autofill/ui_bundled/scoped_autofill_payment_reauth_module_override.h"
#import "ios/chrome/browser/device_reauth/ios_device_authenticator.h"
#import "ios/chrome/browser/device_reauth/ios_device_authenticator_factory.h"
#import "ios/chrome/browser/history/model/history_service_factory.h"
#import "ios/chrome/browser/infobars/model/infobar_ios.h"
#import "ios/chrome/browser/infobars/model/infobar_utils.h"
#import "ios/chrome/browser/passwords/model/password_tab_helper.h"
#import "ios/chrome/browser/plus_addresses/model/plus_address_service_factory.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/public/commands/autofill_commands.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
#import "ios/chrome/browser/translate/model/chrome_ios_translate_client.h"
#import "ios/chrome/browser/webdata_services/model/web_data_service_factory.h"
#import "ios/chrome/common/channel_info.h"
#import "ios/web/public/web_state.h"
#import "services/network/public/cpp/shared_url_loader_factory.h"
#import "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"

namespace autofill {

ChromeAutofillClientIOS::ChromeAutofillClientIOS(
    ProfileIOS* profile,
    web::WebState* web_state,
    infobars::InfoBarManager* infobar_manager,
    id<AutofillClientIOSBridge> bridge)
    : pref_service_(profile->GetPrefs()),
      sync_service_(SyncServiceFactory::GetForBrowserState(profile)),
      personal_data_manager_(PersonalDataManagerFactory::GetForProfile(
          profile->GetOriginalProfile())),
      autocomplete_history_manager_(
          AutocompleteHistoryManagerFactory::GetForProfile(profile)),
      profile_(profile),
      web_state_(web_state),
      bridge_(bridge),
      identity_manager_(
          IdentityManagerFactory::GetForProfile(profile->GetOriginalProfile())),
      infobar_manager_(infobar_manager),
      // TODO(crbug.com/40612524): Replace the closure with a callback to the
      // renderer that indicates if log messages should be sent from the
      // renderer.
      log_manager_(
          LogManager::Create(AutofillLogRouterFactory::GetForProfile(profile),
                             base::RepeatingClosure())),
      ablation_study_(GetApplicationContext()->GetLocalState()) {}

ChromeAutofillClientIOS::~ChromeAutofillClientIOS() {
  HideAutofillSuggestions(SuggestionHidingReason::kTabGone);
}

void ChromeAutofillClientIOS::SetBaseViewController(
    UIViewController* base_view_controller) {
  base_view_controller_ = base_view_controller;
}

version_info::Channel ChromeAutofillClientIOS::GetChannel() const {
  return ::GetChannel();
}

bool ChromeAutofillClientIOS::IsOffTheRecord() const {
  return web_state_->GetBrowserState()->IsOffTheRecord();
}

scoped_refptr<network::SharedURLLoaderFactory>
ChromeAutofillClientIOS::GetURLLoaderFactory() {
  return base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
      web_state_->GetBrowserState()->GetURLLoaderFactory());
}

AutofillDriverFactory& ChromeAutofillClientIOS::GetAutofillDriverFactory() {
  return CHECK_DEREF(AutofillDriverIOSFactory::FromWebState(web_state_));
}

AutofillCrowdsourcingManager*
ChromeAutofillClientIOS::GetCrowdsourcingManager() {
  if (!crowdsourcing_manager_) {
    // Lazy initialization to avoid virtual function calls in the constructor.
    crowdsourcing_manager_ = std::make_unique<AutofillCrowdsourcingManager>(
        this, GetChannel(), GetLogManager());
  }
  return crowdsourcing_manager_.get();
}

PersonalDataManager* ChromeAutofillClientIOS::GetPersonalDataManager() {
  return personal_data_manager_;
}

AutocompleteHistoryManager*
ChromeAutofillClientIOS::GetAutocompleteHistoryManager() {
  return autocomplete_history_manager_;
}

PrefService* ChromeAutofillClientIOS::GetPrefs() {
  return const_cast<PrefService*>(std::as_const(*this).GetPrefs());
}

const PrefService* ChromeAutofillClientIOS::GetPrefs() const {
  return pref_service_;
}

syncer::SyncService* ChromeAutofillClientIOS::GetSyncService() {
  return sync_service_;
}

signin::IdentityManager* ChromeAutofillClientIOS::GetIdentityManager() {
  return const_cast<signin::IdentityManager*>(
      std::as_const(*this).GetIdentityManager());
}

const signin::IdentityManager* ChromeAutofillClientIOS::GetIdentityManager()
    const {
  return identity_manager_;
}

FormDataImporter* ChromeAutofillClientIOS::GetFormDataImporter() {
  if (!form_data_importer_) {
    form_data_importer_ = std::make_unique<FormDataImporter>(
        this,
        ios::HistoryServiceFactory::GetForBrowserState(
            profile_, ServiceAccessType::EXPLICIT_ACCESS),
        GetApplicationContext()->GetApplicationLocale());
  }

  return form_data_importer_.get();
}

payments::IOSChromePaymentsAutofillClient*
ChromeAutofillClientIOS::GetPaymentsAutofillClient() {
  return &payments_autofill_client_;
}

StrikeDatabase* ChromeAutofillClientIOS::GetStrikeDatabase() {
  return StrikeDatabaseFactory::GetForProfile(profile_->GetOriginalProfile());
}

ukm::UkmRecorder* ChromeAutofillClientIOS::GetUkmRecorder() {
  return GetApplicationContext()->GetUkmRecorder();
}

ukm::SourceId ChromeAutofillClientIOS::GetUkmSourceId() {
  return ukm::GetSourceIdForWebStateDocument(web_state_);
}

AddressNormalizer* ChromeAutofillClientIOS::GetAddressNormalizer() {
  return AddressNormalizerFactory::GetInstance();
}

const GURL& ChromeAutofillClientIOS::GetLastCommittedPrimaryMainFrameURL()
    const {
  return web_state_->GetLastCommittedURL();
}

url::Origin ChromeAutofillClientIOS::GetLastCommittedPrimaryMainFrameOrigin()
    const {
  return url::Origin::Create(GetLastCommittedPrimaryMainFrameURL());
}

security_state::SecurityLevel
ChromeAutofillClientIOS::GetSecurityLevelForUmaHistograms() {
  return security_state::GetSecurityLevelForWebState(web_state_);
}

const translate::LanguageState* ChromeAutofillClientIOS::GetLanguageState() {
  // TODO(crbug.com/41430413): iOS vs other platforms extracts language from
  // the top level frame vs whatever frame directly holds the form.
  auto* translate_client = ChromeIOSTranslateClient::FromWebState(web_state_);
  if (translate_client) {
    auto* translate_manager = translate_client->GetTranslateManager();
    if (translate_manager)
      return translate_manager->GetLanguageState();
  }
  return nullptr;
}

translate::TranslateDriver* ChromeAutofillClientIOS::GetTranslateDriver() {
  auto* translate_client = ChromeIOSTranslateClient::FromWebState(web_state_);
  if (translate_client) {
    return translate_client->GetTranslateDriver();
  }
  return nullptr;
}

GeoIpCountryCode ChromeAutofillClientIOS::GetVariationConfigCountryCode()
    const {
  variations::VariationsService* variation_service =
      GetApplicationContext()->GetVariationsService();
  // Retrieves the country code from variation service and converts it to upper
  // case.
  return GeoIpCountryCode(
      variation_service
          ? base::ToUpperASCII(variation_service->GetLatestCountry())
          : std::string());
}

void ChromeAutofillClientIOS::ShowAutofillSettings(
    SuggestionType suggestion_type) {
  NOTREACHED_IN_MIGRATION();
}

void ChromeAutofillClientIOS::ConfirmSaveAddressProfile(
    const AutofillProfile& profile,
    const AutofillProfile* original_profile,
    bool is_migration_to_account,
    AddressProfileSavePromptCallback callback) {
  for (infobars::InfoBar* infobar : infobar_manager_->infobars()) {
    AutofillSaveUpdateAddressProfileDelegateIOS* existing_delegate =
        AutofillSaveUpdateAddressProfileDelegateIOS::FromInfobarDelegate(
            infobar->delegate());

    if (existing_delegate) {
      if (existing_delegate->is_infobar_visible()) {
        // AutoDecline the new prompt if the existing prompt is visible.
        std::move(callback).Run(
            AutofillClient::AddressPromptUserDecision::kAutoDeclined, profile);
        return;
      } else {
        // If the existing prompt is not visible, it means that the user has
        // closed the prompt of the previous import process using the "Cancel"
        // button or it has been accepted by the user. A fallback icon is shown
        // for the user in the omnibox to get back to the prompt. If it is
        // already accepted by the user, the save button is disabled and the
        // saved data is shown to user. In both the cases, the original prompt
        // is replaced by the new one provided that the modal view of the
        // original infobar is not visible to the user.
        infobar_manager_->RemoveInfoBar(infobar);
        break;
      }
    }
  }

  auto delegate = std::make_unique<AutofillSaveUpdateAddressProfileDelegateIOS>(
      profile, original_profile, GetUserEmail(),
      GetApplicationContext()->GetApplicationLocale(), is_migration_to_account,
      std::move(callback));

  infobar_manager_->AddInfoBar(std::make_unique<InfoBarIOS>(
      InfobarType::kInfobarTypeSaveAutofillAddressProfile,
      std::move(delegate)));
}

void ChromeAutofillClientIOS::ShowEditAddressProfileDialog(
    const AutofillProfile& profile,
    AddressProfileSavePromptCallback on_user_decision_callback) {
  NOTREACHED();
}

void ChromeAutofillClientIOS::ShowDeleteAddressProfileDialog(
    const AutofillProfile& profile,
    AddressProfileDeleteDialogCallback delete_dialog_callback) {
  NOTREACHED();
}

AutofillClient::SuggestionUiSessionId
ChromeAutofillClientIOS::ShowAutofillSuggestions(
    const AutofillClient::PopupOpenArgs& open_args,
    base::WeakPtr<AutofillSuggestionDelegate> delegate) {
  [bridge_ showAutofillPopup:open_args.suggestions suggestionDelegate:delegate];
  return SuggestionUiSessionId();
}

void ChromeAutofillClientIOS::ShowPlusAddressEmailOverrideNotification(
    const std::string& original_email,
    EmailOverrideUndoCallback email_override_undo_callback) {
  // TODO(crbug.com/324557053): Implement.
  NOTIMPLEMENTED();
}

AutofillPlusAddressDelegate* ChromeAutofillClientIOS::GetPlusAddressDelegate() {
  return PlusAddressServiceFactory::GetForProfile(profile_);
}

void ChromeAutofillClientIOS::OfferPlusAddressCreation(
    const url::Origin& main_frame_origin,
    PlusAddressCallback callback) {
  AutofillBottomSheetTabHelper* bottomSheetTabHelper =
      AutofillBottomSheetTabHelper::FromWebState(web_state_);
  bottomSheetTabHelper->ShowPlusAddressesBottomSheet(std::move(callback));
}

void ChromeAutofillClientIOS::UpdateAutofillDataListValues(
    base::span<const autofill::SelectOption> datalist) {
  // No op. ios/web_view does not support display datalist.
}

void ChromeAutofillClientIOS::PinAutofillSuggestions() {
  NOTIMPLEMENTED();
}

void ChromeAutofillClientIOS::HideAutofillSuggestions(
    SuggestionHidingReason reason) {
  [bridge_ hideAutofillPopup];
}

bool ChromeAutofillClientIOS::IsAutocompleteEnabled() const {
  return prefs::IsAutocompleteEnabled(GetPrefs());
}

bool ChromeAutofillClientIOS::IsPasswordManagerEnabled() {
  return GetPrefs()->GetBoolean(
      password_manager::prefs::kCredentialsEnableService);
}

void ChromeAutofillClientIOS::DidFillOrPreviewForm(
    mojom::ActionPersistence action_persistence,
    AutofillTriggerSource trigger_source,
    bool is_refill) {}

bool ChromeAutofillClientIOS::IsContextSecure() const {
  return IsContextSecureForWebState(web_state_);
}

FormInteractionsFlowId
ChromeAutofillClientIOS::GetCurrentFormInteractionsFlowId() {
  // Currently not in use here. See `ChromeAutofillClient` for a proper
  // implementation.
  return {};
}

LogManager* ChromeAutofillClientIOS::GetLogManager() const {
  return log_manager_.get();
}

const AutofillAblationStudy& ChromeAutofillClientIOS::GetAblationStudy() const {
  return ablation_study_;
}

bool ChromeAutofillClientIOS::IsLastQueriedField(FieldGlobalId field_id) {
  return [bridge_ isLastQueriedField:field_id];
}

bool ChromeAutofillClientIOS::ShouldFormatForLargeKeyboardAccessory() const {
  return IsKeyboardAccessoryUpgradeEnabled();
}

std::unique_ptr<device_reauth::DeviceAuthenticator>
ChromeAutofillClientIOS::GetDeviceAuthenticator() {
  device_reauth::DeviceAuthParams params(
      base::Seconds(60), device_reauth::DeviceAuthSource::kAutofill);
  id<ReauthenticationProtocol> reauthModule =
      ScopedAutofillPaymentReauthModuleOverride::Get();
  if (!reauthModule) {
    reauthModule = [[ReauthenticationModule alloc] init];
  }
  return CreateIOSDeviceAuthenticator(reauthModule, profile_, params);
}

std::optional<std::u16string> ChromeAutofillClientIOS::GetUserEmail() {
  AuthenticationService* authenticationService =
      AuthenticationServiceFactory::GetForProfile(profile_);
  DCHECK(authenticationService);
  id<SystemIdentity> identity =
      authenticationService->GetPrimaryIdentity(signin::ConsentLevel::kSignin);
  if (identity) {
    return base::SysNSStringToUTF16(identity.userEmail);
  }
  return std::nullopt;
}

PasswordFormClassification ChromeAutofillClientIOS::ClassifyAsPasswordForm(
    AutofillManager& manager,
    FormGlobalId form_id,
    FieldGlobalId field_id) const {
  FormStructure* form_structure = manager.FindCachedFormById(form_id);
  if (!form_structure) {
    return {};
  }

  FormDataAndServerPredictions form_and_predictions =
      GetFormDataAndServerPredictions(*form_structure);

  // Gets the renderer form corresponding to `field_id` when Autofill across
  // iframes is enabled.
  const auto GetRendererForm = [&]() -> std::optional<FormData> {
    const AutofillDriverRouter& router =
        AutofillDriverIOSFactory::FromWebState(web_state_)->router();

    std::vector<FormData> renderer_forms =
        router.GetRendererForms(form_and_predictions.form_data);

    // Find the form to which `field_id` belongs.
    auto renderer_forms_it =
        base::ranges::find_if(renderer_forms, [field_id](const FormData& form) {
          return base::ranges::find(form.fields(), field_id,
                                    &FormFieldData::global_id) !=
                 form.fields().end();
        });
    if (renderer_forms_it == renderer_forms.end()) {
      return std::nullopt;
    }
    return *renderer_forms_it;
  };

  const std::optional<FormData> renderer_form =
      base::FeatureList::IsEnabled(
          autofill::features::kAutofillAcrossIframesIos)
          ? GetRendererForm()
          : std::move(form_and_predictions.form_data);

  if (!renderer_form) {
    return {};
  }

  password_manager::FormDataParser parser;
  // The driver id is irrelevant here because it would only be used by password
  // manager logic that handles the `PasswordForm` returned by the parser.
  parser.set_predictions(password_manager::ConvertToFormPredictions(
      /*driver_id=*/0, *renderer_form, form_and_predictions.predictions));

  // The parser can use stored usernames to identify a filled username field by
  // the value it contains. Here it remains empty.
  std::unique_ptr<password_manager::PasswordForm> pw_form = parser.Parse(
      *renderer_form, password_manager::FormDataParser::Mode::kFilling,
      /*stored_usernames=*/{});
  if (!pw_form) {
    return {};
  }
  PasswordFormClassification result{.type = pw_form->GetPasswordFormType()};
  if (!pw_form->username_element_renderer_id.is_null()) {
    result.username_field = FieldGlobalId(
        field_id.frame_token, pw_form->username_element_renderer_id);
  }
  return result;
}

AutofillSaveCardInfoBarDelegateIOS*
ChromeAutofillClientIOS::GetAutofillSaveCardInfoBarDelegateIOS() {
  const auto save_card_infobar = base::ranges::find(
      infobar_manager_->infobars(),
      infobars::InfoBarDelegate::AUTOFILL_CC_INFOBAR_DELEGATE_MOBILE,
      &infobars::InfoBar::GetIdentifier);
  return save_card_infobar != infobar_manager_->infobars().cend()
             ? AutofillSaveCardInfoBarDelegateIOS::FromInfobarDelegate(
                   (*save_card_infobar)->delegate())
             : nullptr;
}

void ChromeAutofillClientIOS::RemoveAutofillSaveCardInfoBar() {
  const auto save_card_infobar = base::ranges::find(
      infobar_manager_->infobars(),
      infobars::InfoBarDelegate::AUTOFILL_CC_INFOBAR_DELEGATE_MOBILE,
      &infobars::InfoBar::GetIdentifier);
  if (save_card_infobar != infobar_manager_->infobars().cend()) {
    infobar_manager_->RemoveInfoBar(*save_card_infobar);
  }
}

}  // namespace autofill
