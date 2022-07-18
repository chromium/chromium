// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/autofill/chrome_autofill_client_ios.h"

#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/check.h"
#include "base/feature_list.h"
#include "base/memory/ptr_util.h"
#include "base/notreached.h"
#include "base/strings/string_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/autofill_save_update_address_profile_delegate_ios.h"
#include "components/autofill/core/browser/form_data_importer.h"
#include "components/autofill/core/browser/logging/log_manager.h"
#include "components/autofill/core/browser/payments/autofill_credit_card_filling_infobar_delegate_mobile.h"
#include "components/autofill/core/browser/payments/autofill_save_card_infobar_delegate_mobile.h"
#include "components/autofill/core/browser/payments/payments_client.h"
#include "components/autofill/core/browser/ui/payments/card_unmask_prompt_view.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/autofill/ios/browser/autofill_util.h"
#include "components/infobars/core/infobar.h"
#include "components/keyed_service/core/service_access_type.h"
#include "components/password_manager/core/browser/password_generation_frame_helper.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/security_state/ios/security_state_utils.h"
#include "components/sync/driver/sync_service.h"
#include "components/translate/core/browser/translate_manager.h"
#include "components/ukm/ios/ukm_url_recorder.h"
#include "components/variations/service/variations_service.h"
#include "ios/chrome/browser/application_context.h"
#include "ios/chrome/browser/autofill/address_normalizer_factory.h"
#include "ios/chrome/browser/autofill/autocomplete_history_manager_factory.h"
#import "ios/chrome/browser/autofill/autofill_log_router_factory.h"
#include "ios/chrome/browser/autofill/personal_data_manager_factory.h"
#include "ios/chrome/browser/autofill/strike_database_factory.h"
#include "ios/chrome/browser/infobars/infobar_ios.h"
#include "ios/chrome/browser/infobars/infobar_utils.h"
#import "ios/chrome/browser/passwords/password_tab_helper.h"
#include "ios/chrome/browser/signin/identity_manager_factory.h"
#include "ios/chrome/browser/sync/sync_service_factory.h"
#include "ios/chrome/browser/translate/chrome_ios_translate_client.h"
#include "ios/chrome/browser/ui/autofill/card_expiration_date_fix_flow_view_bridge.h"
#include "ios/chrome/browser/ui/autofill/card_name_fix_flow_view_bridge.h"
#include "ios/chrome/browser/ui/autofill/card_unmask_prompt_view_bridge.h"
#include "ios/chrome/browser/webdata_services/web_data_service_factory.h"
#include "ios/chrome/common/channel_info.h"
#import "ios/public/provider/chrome/browser/risk_data/risk_data_api.h"
#import "ios/web/public/web_state.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace autofill {

namespace {

// Creates and returns an infobar for saving credit cards.
std::unique_ptr<infobars::InfoBar> CreateSaveCardInfoBarMobile(
    std::unique_ptr<AutofillSaveCardInfoBarDelegateMobile> delegate) {
  return std::make_unique<InfoBarIOS>(InfobarType::kInfobarTypeSaveCard,
                                      std::move(delegate));
}

CardUnmaskPromptView* CreateCardUnmaskPromptViewBridge(
    CardUnmaskPromptControllerImpl* unmask_controller,
    UIViewController* base_view_controller) {
  return new CardUnmaskPromptViewBridge(unmask_controller,
                                        base_view_controller);
}

}  // namespace

ChromeAutofillClientIOS::ChromeAutofillClientIOS(
    ChromeBrowserState* browser_state,
    web::WebState* web_state,
    infobars::InfoBarManager* infobar_manager,
    id<AutofillClientIOSBridge> bridge,
    password_manager::PasswordManager* password_manager)
    : pref_service_(browser_state->GetPrefs()),
      sync_service_(SyncServiceFactory::GetForBrowserState(browser_state)),
      personal_data_manager_(PersonalDataManagerFactory::GetForBrowserState(
          browser_state->GetOriginalChromeBrowserState())),
      autocomplete_history_manager_(
          AutocompleteHistoryManagerFactory::GetForBrowserState(browser_state)),
      browser_state_(browser_state),
      web_state_(web_state),
      bridge_(bridge),
      identity_manager_(IdentityManagerFactory::GetForBrowserState(
          browser_state->GetOriginalChromeBrowserState())),
      payments_client_(std::make_unique<payments::PaymentsClient>(
          base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
              web_state_->GetBrowserState()->GetURLLoaderFactory()),
          identity_manager_,
          personal_data_manager_,
          web_state_->GetBrowserState()->IsOffTheRecord())),
      form_data_importer_(std::make_unique<FormDataImporter>(
          this,
          payments_client_.get(),
          personal_data_manager_,
          GetApplicationContext()->GetApplicationLocale())),
      infobar_manager_(infobar_manager),
      password_manager_(password_manager),
      unmask_controller_(browser_state->GetPrefs()),
      // TODO(crbug.com/928595): Replace the closure with a callback to the
      // renderer that indicates if log messages should be sent from the
      // renderer.
      log_manager_(LogManager::Create(
          AutofillLogRouterFactory::GetForBrowserState(browser_state),
          base::RepeatingClosure())) {}

ChromeAutofillClientIOS::~ChromeAutofillClientIOS() {
  HideAutofillPopup(PopupHidingReason::kTabGone);
}

void ChromeAutofillClientIOS::SetBaseViewController(
    UIViewController* base_view_controller) {
  base_view_controller_ = base_view_controller;
}

version_info::Channel ChromeAutofillClientIOS::GetChannel() const {
  return ::GetChannel();
}

PersonalDataManager* ChromeAutofillClientIOS::GetPersonalDataManager() {
  return personal_data_manager_;
}

AutocompleteHistoryManager*
ChromeAutofillClientIOS::GetAutocompleteHistoryManager() {
  return autocomplete_history_manager_;
}

PrefService* ChromeAutofillClientIOS::GetPrefs() {
  return const_cast<PrefService*>(base::as_const(*this).GetPrefs());
}

const PrefService* ChromeAutofillClientIOS::GetPrefs() const {
  return pref_service_;
}

syncer::SyncService* ChromeAutofillClientIOS::GetSyncService() {
  return sync_service_;
}

signin::IdentityManager* ChromeAutofillClientIOS::GetIdentityManager() {
  return identity_manager_;
}

FormDataImporter* ChromeAutofillClientIOS::GetFormDataImporter() {
  return form_data_importer_.get();
}

payments::PaymentsClient* ChromeAutofillClientIOS::GetPaymentsClient() {
  return payments_client_.get();
}

StrikeDatabase* ChromeAutofillClientIOS::GetStrikeDatabase() {
  return StrikeDatabaseFactory::GetForBrowserState(
      browser_state_->GetOriginalChromeBrowserState());
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

const GURL& ChromeAutofillClientIOS::GetLastCommittedURL() const {
  return web_state_->GetLastCommittedURL();
}

security_state::SecurityLevel
ChromeAutofillClientIOS::GetSecurityLevelForUmaHistograms() {
  return security_state::GetSecurityLevelForWebState(web_state_);
}

const translate::LanguageState* ChromeAutofillClientIOS::GetLanguageState() {
  // TODO(crbug.com/912597): iOS vs other platforms extracts language from
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

std::string ChromeAutofillClientIOS::GetVariationConfigCountryCode() const {
  variations::VariationsService* variation_service =
      GetApplicationContext()->GetVariationsService();
  // Retrieves the country code from variation service and converts it to upper
  // case.
  return variation_service
             ? base::ToUpperASCII(variation_service->GetLatestCountry())
             : std::string();
}

void ChromeAutofillClientIOS::ShowAutofillSettings(
    bool show_credit_card_settings) {
  NOTREACHED();
}

void ChromeAutofillClientIOS::ShowUnmaskPrompt(
    const CreditCard& card,
    UnmaskCardReason reason,
    base::WeakPtr<CardUnmaskDelegate> delegate) {
  unmask_controller_.ShowPrompt(
      base::BindOnce(&CreateCardUnmaskPromptViewBridge,
                     base::Unretained(&unmask_controller_),
                     base::Unretained(base_view_controller_)),
      card, reason, delegate);
}

void ChromeAutofillClientIOS::OnUnmaskVerificationResult(
    PaymentsRpcResult result) {
  unmask_controller_.OnVerificationResult(result);
}

void ChromeAutofillClientIOS::ConfirmSaveCreditCardLocally(
    const CreditCard& card,
    SaveCreditCardOptions options,
    LocalSaveCardPromptCallback callback) {
  DCHECK(options.show_prompt);
  infobar_manager_->AddInfoBar(CreateSaveCardInfoBarMobile(
      std::make_unique<AutofillSaveCardInfoBarDelegateMobile>(
          /*upload=*/false, options, card, LegalMessageLines(),
          /*upload_save_card_callback=*/UploadSaveCardPromptCallback(),
          /*local_save_card_callback=*/std::move(callback), AccountInfo())));
}

void ChromeAutofillClientIOS::ConfirmAccountNameFixFlow(
    base::OnceCallback<void(const std::u16string&)> callback) {
  std::u16string account_name = base::UTF8ToUTF16(
      identity_manager_
          ->FindExtendedAccountInfo(identity_manager_->GetPrimaryAccountInfo(
              signin::ConsentLevel::kSync))
          .full_name);

  card_name_fix_flow_controller_.Show(
      // CardNameFixFlowViewBridge manages its own lifetime, so
      // do not use std::unique_ptr<> here.
      new CardNameFixFlowViewBridge(&card_name_fix_flow_controller_,
                                    base_view_controller_),
      account_name, std::move(callback));
}

void ChromeAutofillClientIOS::ConfirmExpirationDateFixFlow(
    const CreditCard& card,
    base::OnceCallback<void(const std::u16string&, const std::u16string&)>
        callback) {
  card_expiration_date_fix_flow_controller_.Show(
      // CardExpirationDateFixFlowViewBridge manages its own lifetime,
      // so do not use std::unique_ptr<> here.
      new CardExpirationDateFixFlowViewBridge(
          &card_expiration_date_fix_flow_controller_, base_view_controller_),
      card, std::move(callback));
}

void ChromeAutofillClientIOS::ConfirmSaveCreditCardToCloud(
    const CreditCard& card,
    const LegalMessageLines& legal_message_lines,
    SaveCreditCardOptions options,
    UploadSaveCardPromptCallback callback) {
  DCHECK(options.show_prompt);

  AccountInfo account_info = identity_manager_->FindExtendedAccountInfo(
      identity_manager_->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin));
  infobar_manager_->AddInfoBar(CreateSaveCardInfoBarMobile(
      std::make_unique<AutofillSaveCardInfoBarDelegateMobile>(
          /*upload=*/true, options, card, legal_message_lines,
          /*upload_save_card_callback=*/std::move(callback),
          LocalSaveCardPromptCallback(), account_info)));
}

void ChromeAutofillClientIOS::CreditCardUploadCompleted(bool card_saved) {
  NOTIMPLEMENTED();
}

void ChromeAutofillClientIOS::ConfirmCreditCardFillAssist(
    const CreditCard& card,
    base::OnceClosure callback) {
  auto infobar_delegate =
      std::make_unique<AutofillCreditCardFillingInfoBarDelegateMobile>(
          card, std::move(callback));
  auto* raw_delegate = infobar_delegate.get();
  if (infobar_manager_->AddInfoBar(
          ::CreateConfirmInfoBar(std::move(infobar_delegate)))) {
    raw_delegate->set_was_shown();
  }
}

void ChromeAutofillClientIOS::ConfirmSaveAddressProfile(
    const AutofillProfile& profile,
    const AutofillProfile* original_profile,
    SaveAddressProfilePromptOptions options,
    AddressProfileSavePromptCallback callback) {
  DCHECK(base::FeatureList::IsEnabled(
      features::kAutofillAddressProfileSavePrompt));
    // TODO(crbug.com/1167062): Respect SaveAddressProfilePromptOptions.
    auto delegate =
        std::make_unique<AutofillSaveUpdateAddressProfileDelegateIOS>(
            profile, original_profile,
            GetApplicationContext()->GetApplicationLocale(),
            std::move(callback));
    infobar_manager_->AddInfoBar(std::make_unique<InfoBarIOS>(
        InfobarType::kInfobarTypeSaveAutofillAddressProfile,
        std::move(delegate)));
}

bool ChromeAutofillClientIOS::HasCreditCardScanFeature() {
  return false;
}

void ChromeAutofillClientIOS::ScanCreditCard(CreditCardScanCallback callback) {
  NOTREACHED();
}

void ChromeAutofillClientIOS::ShowAutofillPopup(
    const AutofillClient::PopupOpenArgs& open_args,
    base::WeakPtr<AutofillPopupDelegate> delegate) {
  [bridge_ showAutofillPopup:open_args.suggestions popupDelegate:delegate];
}

void ChromeAutofillClientIOS::UpdateAutofillPopupDataListValues(
    const std::vector<std::u16string>& values,
    const std::vector<std::u16string>& labels) {
  // No op. ios/web_view does not support display datalist.
}

base::span<const Suggestion> ChromeAutofillClientIOS::GetPopupSuggestions()
    const {
  NOTIMPLEMENTED();
  return base::span<const Suggestion>();
}

void ChromeAutofillClientIOS::PinPopupView() {
  NOTIMPLEMENTED();
}

AutofillClient::PopupOpenArgs ChromeAutofillClientIOS::GetReopenPopupArgs()
    const {
  NOTIMPLEMENTED();
  return {};
}

void ChromeAutofillClientIOS::UpdatePopup(
    const std::vector<Suggestion>& suggestions,
    PopupType popup_type) {
  NOTIMPLEMENTED();
}

void ChromeAutofillClientIOS::HideAutofillPopup(PopupHidingReason reason) {
  [bridge_ hideAutofillPopup];
}

bool ChromeAutofillClientIOS::IsAutocompleteEnabled() {
  return prefs::IsAutocompleteEnabled(GetPrefs());
}

bool ChromeAutofillClientIOS::IsPasswordManagerEnabled() {
  return GetPrefs()->GetBoolean(
      password_manager::prefs::kCredentialsEnableService);
}

void ChromeAutofillClientIOS::PropagateAutofillPredictions(
    AutofillDriver* driver,
    const std::vector<FormStructure*>& forms) {
  if (!PasswordTabHelper::FromWebState(web_state_)) {
    return;
  }
  password_manager_->ProcessAutofillPredictions(
      PasswordTabHelper::FromWebState(web_state_)->GetPasswordManagerDriver(),
      forms);
}

void ChromeAutofillClientIOS::DidFillOrPreviewField(
    const std::u16string& autofilled_value,
    const std::u16string& profile_full_name) {}

bool ChromeAutofillClientIOS::IsContextSecure() const {
  return IsContextSecureForWebState(web_state_);
}

bool ChromeAutofillClientIOS::ShouldShowSigninPromo() {
  return false;
}

bool ChromeAutofillClientIOS::AreServerCardsSupported() const {
  return true;
}

void ChromeAutofillClientIOS::ExecuteCommand(int id) {
  NOTIMPLEMENTED();
}

void ChromeAutofillClientIOS::OpenPromoCodeOfferDetailsURL(const GURL& url) {
  web_state_->OpenURL(web::WebState::OpenURLParams(
      url, web::Referrer(), WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui::PageTransition::PAGE_TRANSITION_AUTO_TOPLEVEL,
      /*is_renderer_initiated=*/false));
}

void ChromeAutofillClientIOS::LoadRiskData(
    base::OnceCallback<void(const std::string&)> callback) {
  std::move(callback).Run(
      base::SysNSStringToUTF8(ios::provider::GetRiskData()));
}

LogManager* ChromeAutofillClientIOS::GetLogManager() const {
  return log_manager_.get();
}

bool ChromeAutofillClientIOS::IsQueryIDRelevant(int query_id) {
  return [bridge_ isQueryIDRelevant:query_id];
}

}  // namespace autofill
