// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/internal/autofill/web_view_autofill_client_ios.h"

#import <utility>
#import <vector>

#import "base/check.h"
#import "base/check_deref.h"
#import "base/functional/bind.h"
#import "base/functional/callback.h"
#import "base/memory/ptr_util.h"
#import "base/notreached.h"
#import "components/autofill/core/browser/form_import/form_data_importer.h"
#import "components/autofill/core/browser/logging/log_router.h"
#import "components/autofill/core/browser/suggestions/suggestion_type.h"
#import "components/autofill/core/common/autofill_prefs.h"
#import "components/autofill/ios/browser/autofill_driver_ios_factory.h"
#import "components/autofill/ios/browser/autofill_util.h"
#import "components/password_manager/core/common/password_manager_pref_names.h"
#import "components/security_state/ios/security_state_utils.h"
#import "ios/web/public/browser_state.h"
#import "ios/web/public/web_state.h"
#import "ios/web_view/internal/app/application_context.h"
#import "ios/web_view/internal/autofill/ios_web_view_payments_autofill_client.h"
#import "ios/web_view/internal/autofill/web_view_autocomplete_history_manager_factory.h"
#import "ios/web_view/internal/autofill/web_view_autofill_log_router_factory.h"
#import "ios/web_view/internal/autofill/web_view_personal_data_manager_factory.h"
#import "ios/web_view/internal/autofill/web_view_strike_database_factory.h"
#import "ios/web_view/internal/signin/web_view_identity_manager_factory.h"
#import "ios/web_view/internal/sync/web_view_sync_service_factory.h"
#import "services/network/public/cpp/shared_url_loader_factory.h"
#import "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"

namespace autofill {

// static
std::unique_ptr<WebViewAutofillClientIOS> WebViewAutofillClientIOS::Create(
    web::WebState* web_state,
    id<CWVAutofillClientIOSBridge, AutofillDriverIOSBridge> bridge) {
  auto* browser_state = ios_web_view::WebViewBrowserState::FromBrowserState(
      web_state->GetBrowserState());
  return std::make_unique<autofill::WebViewAutofillClientIOS>(
      browser_state->GetPrefs(),
      ios_web_view::WebViewPersonalDataManagerFactory::GetForBrowserState(
          browser_state->GetRecordingBrowserState()),
      ios_web_view::WebViewAutocompleteHistoryManagerFactory::
          GetForBrowserState(browser_state),
      web_state, bridge,
      ios_web_view::WebViewIdentityManagerFactory::GetForBrowserState(
          browser_state->GetRecordingBrowserState()),
      ios_web_view::WebViewStrikeDatabaseFactory::GetForBrowserState(
          browser_state->GetRecordingBrowserState()),
      ios_web_view::WebViewSyncServiceFactory::GetForBrowserState(
          browser_state),
      autofill::WebViewAutofillLogRouterFactory::GetForBrowserState(
          browser_state));
}

WebViewAutofillClientIOS::WebViewAutofillClientIOS(
    PrefService* pref_service,
    PersonalDataManager* personal_data_manager,
    AutocompleteHistoryManager* autocomplete_history_manager,
    web::WebState* web_state,
    id<CWVAutofillClientIOSBridge, AutofillDriverIOSBridge> bridge,
    signin::IdentityManager* identity_manager,
    strike_database::StrikeDatabase* strike_database,
    syncer::SyncService* sync_service,
    LogRouter* log_router)
    : AutofillClientIOS(web_state, bridge),
      bridge_(bridge),
      pref_service_(pref_service),
      personal_data_manager_(personal_data_manager),
      autocomplete_history_manager_(autocomplete_history_manager),
      identity_manager_(identity_manager),
      strike_database_(strike_database),
      sync_service_(sync_service),
      log_router_(log_router) {}

WebViewAutofillClientIOS::~WebViewAutofillClientIOS() {
  HideAutofillSuggestions(SuggestionHidingReason::kTabGone);
  if (web_state()) {
    // If web_state() is still valid, WebStateDestroyed() possibly hasn't been
    // called yet. To meet the AutofillClientIOS contract, we call it. See the
    // WebViewAutofillClientIOS class-level documentation.
    static_cast<web::WebStateObserver&>(GetAutofillDriverFactory())
        .WebStateDestroyed(web_state());
  }
}

base::WeakPtr<AutofillClient> WebViewAutofillClientIOS::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

const std::string& WebViewAutofillClientIOS::GetAppLocale() const {
  return ios_web_view::ApplicationContext::GetInstance()
      ->GetApplicationLocale();
}

bool WebViewAutofillClientIOS::IsOffTheRecord() const {
  return web_state()->GetBrowserState()->IsOffTheRecord();
}

scoped_refptr<network::SharedURLLoaderFactory>
WebViewAutofillClientIOS::GetURLLoaderFactory() {
  return base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
      web_state()->GetBrowserState()->GetURLLoaderFactory());
}

AutofillCrowdsourcingManager&
WebViewAutofillClientIOS::GetCrowdsourcingManager() {
  if (!crowdsourcing_manager_) {
    // Lazy initialization to avoid virtual function calls in the constructor.
    crowdsourcing_manager_ =
        std::make_unique<AutofillCrowdsourcingManager>(this, GetChannel());
  }
  return *crowdsourcing_manager_;
}

VotesUploader& WebViewAutofillClientIOS::GetVotesUploader() {
  return votes_uploader_;
}

PersonalDataManager& WebViewAutofillClientIOS::GetPersonalDataManager() {
  return CHECK_DEREF(personal_data_manager_);
}

ValuablesDataManager* WebViewAutofillClientIOS::GetValuablesDataManager() {
  return nullptr;
}

EntityDataManager* WebViewAutofillClientIOS::GetEntityDataManager() {
  return nullptr;
}

SingleFieldFillRouter& WebViewAutofillClientIOS::GetSingleFieldFillRouter() {
  return single_field_fill_router_;
}

AutocompleteHistoryManager*
WebViewAutofillClientIOS::GetAutocompleteHistoryManager() {
  return autocomplete_history_manager_;
}

PrefService* WebViewAutofillClientIOS::GetPrefs() {
  return const_cast<PrefService*>(std::as_const(*this).GetPrefs());
}

const PrefService* WebViewAutofillClientIOS::GetPrefs() const {
  return pref_service_;
}

syncer::SyncService* WebViewAutofillClientIOS::GetSyncService() {
  return sync_service_;
}

signin::IdentityManager* WebViewAutofillClientIOS::GetIdentityManager() {
  return const_cast<signin::IdentityManager*>(
      std::as_const(*this).GetIdentityManager());
}

const signin::IdentityManager* WebViewAutofillClientIOS::GetIdentityManager()
    const {
  return identity_manager_;
}

FormDataImporter* WebViewAutofillClientIOS::GetFormDataImporter() {
  if (!form_data_importer_) {
    form_data_importer_ =
        std::make_unique<FormDataImporter>(this, /*history_service=*/nullptr);
  }
  return form_data_importer_.get();
}

payments::PaymentsAutofillClient*
WebViewAutofillClientIOS::GetPaymentsAutofillClient() {
  return &payments_autofill_client_;
}

strike_database::StrikeDatabase* WebViewAutofillClientIOS::GetStrikeDatabase() {
  return strike_database_;
}

ukm::UkmRecorder* WebViewAutofillClientIOS::GetUkmRecorder() {
  // UKM recording is not supported for WebViews.
  return nullptr;
}

AddressNormalizer* WebViewAutofillClientIOS::GetAddressNormalizer() {
  return nullptr;
}

const GURL& WebViewAutofillClientIOS::GetLastCommittedPrimaryMainFrameURL()
    const {
  return web_state()->GetLastCommittedURL();
}

url::Origin WebViewAutofillClientIOS::GetLastCommittedPrimaryMainFrameOrigin()
    const {
  return url::Origin::Create(GetLastCommittedPrimaryMainFrameURL());
}

security_state::SecurityLevel
WebViewAutofillClientIOS::GetSecurityLevelForUmaHistograms() {
  return security_state::GetSecurityLevelForWebState(web_state());
}

const translate::LanguageState* WebViewAutofillClientIOS::GetLanguageState() {
  return nullptr;
}

translate::TranslateDriver* WebViewAutofillClientIOS::GetTranslateDriver() {
  return nullptr;
}

void WebViewAutofillClientIOS::ShowAutofillSettings(
    SuggestionType suggestion_type) {
  NOTREACHED();
}

void WebViewAutofillClientIOS::ConfirmSaveAddressProfile(
    const AutofillProfile& profile,
    const AutofillProfile* original_profile,
    SaveAddressBubbleType save_address_bubble_type,
    AddressProfileSavePromptCallback callback) {
  [bridge_ confirmSaveAddressProfile:profile
                     originalProfile:original_profile
                            callback:std::move(callback)];
}

AutofillClient::SuggestionUiSessionId
WebViewAutofillClientIOS::ShowAutofillSuggestions(
    const AutofillClient::PopupOpenArgs& open_args,
    base::WeakPtr<AutofillSuggestionDelegate> delegate) {
  [bridge_ showAutofillPopup:open_args.suggestions suggestionDelegate:delegate];
  return SuggestionUiSessionId();
}

void WebViewAutofillClientIOS::UpdateAutofillDataListValues(
    base::span<const autofill::SelectOption> datalist) {
  // No op. ios/web_view does not support display datalist.
}

void WebViewAutofillClientIOS::HideAutofillSuggestions(
    SuggestionHidingReason reason) {
  [bridge_ hideAutofillPopup];
}

bool WebViewAutofillClientIOS::IsAutofillEnabled() const {
  return IsAutofillProfileEnabled() ||
         AutofillClient::GetPaymentsAutofillClient()
             ->IsAutofillPaymentMethodsEnabled();
}

bool WebViewAutofillClientIOS::IsAutofillProfileEnabled() const {
  return prefs::IsAutofillProfileEnabled(GetPrefs());
}

bool WebViewAutofillClientIOS::IsWalletStorageEnabled() const {
  return false;
}

bool WebViewAutofillClientIOS::IsAutocompleteEnabled() const {
  return false;
}

bool WebViewAutofillClientIOS::IsPasswordManagerEnabled() const {
  return GetPrefs()->GetBoolean(
      password_manager::prefs::kCredentialsEnableService);
}

void WebViewAutofillClientIOS::DidFillForm(AutofillTriggerSource trigger_source,
                                           bool is_refill) {}

bool WebViewAutofillClientIOS::IsContextSecure() const {
  return IsContextSecureForWebState(web_state());
}

bool WebViewAutofillClientIOS::IsCvcSavingSupported() const {
  return false;
}

autofill::FormInteractionsFlowId
WebViewAutofillClientIOS::GetCurrentFormInteractionsFlowId() {
  // Currently not in use here. See `ChromeAutofillClient` for a proper
  // implementation.
  return {};
}

bool WebViewAutofillClientIOS::IsLastQueriedField(FieldGlobalId field_id) {
  return [bridge_ isLastQueriedField:field_id];
}

LogManager* WebViewAutofillClientIOS::GetCurrentLogManager() {
  if (!log_manager_ && log_router_ && log_router_->HasReceivers()) {
    // TODO(crbug.com/40612524): Replace the closure with a callback to the
    // renderer that indicates if log messages should be sent from the
    // renderer.
    log_manager_ = LogManager::Create(log_router_, base::RepeatingClosure());
  }
  return log_manager_.get();
}

autofill_metrics::FormInteractionsUkmLogger&
WebViewAutofillClientIOS::GetFormInteractionsUkmLogger() {
  return form_interactions_ukm_logger_;
}

}  // namespace autofill
